/* tests/bench/perf_hw.h
 *
 * Linux perf_event_open wrapper for hardware benchmarking on BPI-F3.
 *
 * Provides the same measurement semantics as the bare-metal bench.h
 * (cycle + instruction counts per region) but uses the Linux perf
 * subsystem instead of direct CSR access.
 *
 * Two API layers:
 *
 *   1. Basic (perf_hw_ctx / perf_hw_sample):
 *      Counts cycles + instructions per region.  Used by the main benchmark
 *      harness (bench_kem.c, bench_mul.c).
 *
 *   2. Extended (perf_hw_ext_ctx / perf_hw_ext_sample):
 *      Adds L1d misses, DTLB misses, branch mispredictions, frontend/backend
 *      stalls, and vector instruction count.  Uses SpacemiT X60 platform-
 *      specific raw event codes (type=4, the cpu PMU device).
 *      Enable by compiling with -DPERF_HW_EXTENDED=1.
 *
 * Usage (basic):
 *   perf_hw_ctx ctx;
 *   perf_hw_init(&ctx);
 *   perf_hw_start(&ctx);
 *   ... code under test ...
 *   perf_hw_stop(&ctx);
 *   perf_hw_read(&ctx, &sample);
 *   perf_hw_close(&ctx);
 *
 * Usage (extended):
 *   perf_hw_ext_ctx ectx;
 *   perf_hw_ext_init(&ectx);
 *   perf_hw_ext_start(&ectx);
 *   ... code under test ...
 *   perf_hw_ext_stop(&ectx);
 *   perf_hw_ext_read(&ectx, &esample);
 *   perf_hw_ext_close(&ectx);
 *
 * Designed for native compilation on riscv64 Linux with GCC 14.2:
 *   gcc -O3 -static -march=rv64imafdcv_zba_zbb_zbc_zbs ...
 */

#ifndef PERF_HW_H
#define PERF_HW_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>

/* ══════════════════════════════════════════════════════════════════════════
 * SpacemiT X60 Raw PMU Event Codes
 *
 * Discovered via: perf stat -vvv -e <name> taskset -c 7 true
 * These use type=4 (the "cpu" PMU device on this kernel) with the raw
 * event code in pe.config.  The generic PERF_TYPE_HARDWARE mappings
 * (type=0) do NOT work for cache/TLB/stall counters on this SoC.
 * ══════════════════════════════════════════════════════════════════════════ */

/* Cache hierarchy */
#define PERF_X60_L1D_LOAD_MISS       0x05
#define PERF_X60_L1D_STORE_MISS      0x09
#define PERF_X60_L1D_ACCESS          0xAA
#define PERF_X60_L1D_LOAD_ACCESS     0x06  /* from perf list: l1d_load_access */
#define PERF_X60_L1D_STORE_ACCESS    0x0A  /* l1d_store_access */
#define PERF_X60_L1I_LOAD_MISS       0x0B
#define PERF_X60_L1I_LOAD_ACCESS     0x0C
#define PERF_X60_L2_LOAD_MISS        0xB9
#define PERF_X60_L2_LOAD_ACCESS      0xB8

/* TLB */
#define PERF_X60_DTLB_LOAD_MISS      0x15
#define PERF_X60_DTLB_STORE_MISS     0x19
#define PERF_X60_ITLB_LOAD_MISS      0x1B
#define PERF_X60_JTLB_MISS           0xA3

/* Branch prediction */
#define PERF_X60_BR_MISPRED          0x45
#define PERF_X60_COND_BR_MISPRED     0x02
#define PERF_X60_INDIRECT_BR_MISPRED 0x42

/* Pipeline stalls */
#define PERF_X60_STALL_FRONTEND      0x03
#define PERF_X60_STALL_BACKEND       0x04
#define PERF_X60_PIPELINE_FLUSH      0x3F
#define PERF_X60_EU_STALL            0x67
#define PERF_X60_RF_RAW_STALL        0x64

/* Vector unit */
#define PERF_X60_VECTOR_INST         0x3B
#define PERF_X60_VECTOR_LOAD_INST    0x39
#define PERF_X60_VECTOR_STORE_INST   0x3A
#define PERF_X60_VPU_STALL_PIPE0     0x81
#define PERF_X60_VPU_STALL_PIPE1     0x82

/* Instruction mix */
#define PERF_X60_LOAD_INST           0x2D
#define PERF_X60_STORE_INST          0x2C

/* PMU device type (read from /sys/bus/event_source/devices/cpu/type) */
#define PERF_X60_PMU_TYPE            4

/* ── Data types ──────────────────────────────────────────────────────────── */

typedef struct {
    int fd_cycles;
    int fd_instret;
} perf_hw_ctx;

typedef struct {
    uint64_t cycles;
    uint64_t instret;
} perf_hw_sample;

/* ── Extended context: cycles + instret + microarchitectural counters ────── */

#define PERF_HW_EXT_NUM_COUNTERS 8

typedef struct {
    int fd_cycles;
    int fd_instret;
    int fd_ext[PERF_HW_EXT_NUM_COUNTERS];
    int n_ext;  /* number of successfully opened extended counters */
} perf_hw_ext_ctx;

typedef struct {
    uint64_t cycles;
    uint64_t instret;
    uint64_t l1d_load_miss;
    uint64_t l1d_access;
    uint64_t dtlb_load_miss;
    uint64_t br_mispred;
    uint64_t stall_frontend;
    uint64_t stall_backend;
    uint64_t vector_inst;
    uint64_t vpu_stall;
} perf_hw_ext_sample;

/* ── Internal syscall wrapper ────────────────────────────────────────────── */

static inline long perf_event_open_(struct perf_event_attr *attr, pid_t pid,
                                    int cpu, int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

static inline int perf_hw_init(perf_hw_ctx *ctx)
{
    struct perf_event_attr pe;

    memset(&pe, 0, sizeof(pe));
    pe.type           = PERF_TYPE_HARDWARE;
    pe.size           = sizeof(pe);
    pe.disabled       = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv     = 1;

    /* CPU cycles counter */
    pe.config = PERF_COUNT_HW_CPU_CYCLES;
    ctx->fd_cycles = (int)perf_event_open_(&pe, 0, -1, -1, 0);
    if (ctx->fd_cycles < 0) {
        perror("perf_event_open(cycles)");
        return -1;
    }

    /* Instructions retired counter */
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    ctx->fd_instret = (int)perf_event_open_(&pe, 0, -1, -1, 0);
    if (ctx->fd_instret < 0) {
        perror("perf_event_open(instructions)");
        close(ctx->fd_cycles);
        return -1;
    }

    return 0;
}

static inline void perf_hw_start(perf_hw_ctx *ctx)
{
    ioctl(ctx->fd_cycles,  PERF_EVENT_IOC_RESET,  0);
    ioctl(ctx->fd_instret, PERF_EVENT_IOC_RESET,  0);
    ioctl(ctx->fd_cycles,  PERF_EVENT_IOC_ENABLE, 0);
    ioctl(ctx->fd_instret, PERF_EVENT_IOC_ENABLE, 0);
}

static inline void perf_hw_stop(perf_hw_ctx *ctx)
{
    ioctl(ctx->fd_cycles,  PERF_EVENT_IOC_DISABLE, 0);
    ioctl(ctx->fd_instret, PERF_EVENT_IOC_DISABLE, 0);
}

static inline void perf_hw_read(perf_hw_ctx *ctx, perf_hw_sample *s)
{
    read(ctx->fd_cycles,  &s->cycles,  sizeof(s->cycles));
    read(ctx->fd_instret, &s->instret, sizeof(s->instret));
}

static inline void perf_hw_close(perf_hw_ctx *ctx)
{
    close(ctx->fd_cycles);
    close(ctx->fd_instret);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Extended PMU API — SpacemiT X60 platform-specific counters
 *
 * Opens up to 8 additional hardware counters using the platform PMU
 * (type=4) with raw event codes.  Falls back gracefully if any counter
 * fails to open (n_ext tracks how many succeeded).
 *
 * Counter layout in fd_ext[] / ext_sample fields:
 *   [0] l1d_load_miss     — L1 data cache load misses
 *   [1] l1d_access        — L1 data cache total accesses (denominator)
 *   [2] dtlb_load_miss    — Data TLB load misses
 *   [3] br_mispred        — Branch mispredictions (all types)
 *   [4] stall_frontend    — Frontend stalled cycles (I-cache, fetch)
 *   [5] stall_backend     — Backend stalled cycles (memory, execution)
 *   [6] vector_inst       — Vector instructions retired
 *   [7] vpu_stall         — VPU pipe0 stalls (vector backpressure)
 * ══════════════════════════════════════════════════════════════════════════ */

/* Raw event codes for the extended counters, in fd_ext[] order */
static const uint64_t _perf_hw_ext_events[PERF_HW_EXT_NUM_COUNTERS] = {
    PERF_X60_L1D_LOAD_MISS,
    PERF_X60_L1D_ACCESS,
    PERF_X60_DTLB_LOAD_MISS,
    PERF_X60_BR_MISPRED,
    PERF_X60_STALL_FRONTEND,
    PERF_X60_STALL_BACKEND,
    PERF_X60_VECTOR_INST,
    PERF_X60_VPU_STALL_PIPE0,
};

static const char *_perf_hw_ext_names[PERF_HW_EXT_NUM_COUNTERS] = {
    "l1d_load_miss",
    "l1d_access",
    "dtlb_load_miss",
    "br_mispred",
    "stall_frontend",
    "stall_backend",
    "vector_inst",
    "vpu_stall_pipe0",
};

static inline int perf_hw_ext_init(perf_hw_ext_ctx *ctx)
{
    struct perf_event_attr pe;

    /* First open cycles + instret using the generic PERF_TYPE_HARDWARE
     * (these always work on the X60). */
    memset(&pe, 0, sizeof(pe));
    pe.type           = PERF_TYPE_HARDWARE;
    pe.size           = sizeof(pe);
    pe.disabled       = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv     = 1;

    pe.config = PERF_COUNT_HW_CPU_CYCLES;
    ctx->fd_cycles = (int)perf_event_open_(&pe, 0, -1, -1, 0);
    if (ctx->fd_cycles < 0) {
        perror("perf_hw_ext_init(cycles)");
        return -1;
    }

    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    ctx->fd_instret = (int)perf_event_open_(&pe, 0, -1, -1, 0);
    if (ctx->fd_instret < 0) {
        perror("perf_hw_ext_init(instructions)");
        close(ctx->fd_cycles);
        return -1;
    }

    /* Now open extended counters using the platform PMU (type=4).
     * These use raw event codes specific to the SpacemiT X60. */
    memset(&pe, 0, sizeof(pe));
    pe.type           = PERF_X60_PMU_TYPE;
    pe.size           = sizeof(struct perf_event_attr);
    pe.disabled       = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv     = 1;

    ctx->n_ext = 0;
    for (int i = 0; i < PERF_HW_EXT_NUM_COUNTERS; i++) {
        pe.config = _perf_hw_ext_events[i];
        ctx->fd_ext[i] = (int)perf_event_open_(&pe, 0, -1, -1, 0);
        if (ctx->fd_ext[i] < 0) {
            fprintf(stderr, "WARNING: perf_hw_ext_init: cannot open %s (event=0x%lx)\n",
                    _perf_hw_ext_names[i], (unsigned long)_perf_hw_ext_events[i]);
            ctx->fd_ext[i] = -1;
        } else {
            ctx->n_ext++;
        }
    }

    if (ctx->n_ext == 0) {
        fprintf(stderr, "WARNING: perf_hw_ext_init: no extended counters opened. "
                "Falling back to cycles+instret only.\n");
    }

    return 0;
}

static inline void perf_hw_ext_start(perf_hw_ext_ctx *ctx)
{
    ioctl(ctx->fd_cycles,  PERF_EVENT_IOC_RESET,  0);
    ioctl(ctx->fd_instret, PERF_EVENT_IOC_RESET,  0);
    ioctl(ctx->fd_cycles,  PERF_EVENT_IOC_ENABLE, 0);
    ioctl(ctx->fd_instret, PERF_EVENT_IOC_ENABLE, 0);

    for (int i = 0; i < PERF_HW_EXT_NUM_COUNTERS; i++) {
        if (ctx->fd_ext[i] >= 0) {
            ioctl(ctx->fd_ext[i], PERF_EVENT_IOC_RESET,  0);
            ioctl(ctx->fd_ext[i], PERF_EVENT_IOC_ENABLE, 0);
        }
    }
}

static inline void perf_hw_ext_stop(perf_hw_ext_ctx *ctx)
{
    ioctl(ctx->fd_cycles,  PERF_EVENT_IOC_DISABLE, 0);
    ioctl(ctx->fd_instret, PERF_EVENT_IOC_DISABLE, 0);

    for (int i = 0; i < PERF_HW_EXT_NUM_COUNTERS; i++) {
        if (ctx->fd_ext[i] >= 0) {
            ioctl(ctx->fd_ext[i], PERF_EVENT_IOC_DISABLE, 0);
        }
    }
}

static inline void perf_hw_ext_read(perf_hw_ext_ctx *ctx, perf_hw_ext_sample *s)
{
    read(ctx->fd_cycles,  &s->cycles,  sizeof(s->cycles));
    read(ctx->fd_instret, &s->instret, sizeof(s->instret));

    /* Read extended counters into their respective fields.
     * Zero-initialize first so missing counters read as 0. */
    uint64_t ext_vals[PERF_HW_EXT_NUM_COUNTERS] = {0};
    for (int i = 0; i < PERF_HW_EXT_NUM_COUNTERS; i++) {
        if (ctx->fd_ext[i] >= 0) {
            read(ctx->fd_ext[i], &ext_vals[i], sizeof(ext_vals[i]));
        }
    }

    s->l1d_load_miss  = ext_vals[0];
    s->l1d_access     = ext_vals[1];
    s->dtlb_load_miss = ext_vals[2];
    s->br_mispred     = ext_vals[3];
    s->stall_frontend = ext_vals[4];
    s->stall_backend  = ext_vals[5];
    s->vector_inst    = ext_vals[6];
    s->vpu_stall      = ext_vals[7];
}

static inline void perf_hw_ext_close(perf_hw_ext_ctx *ctx)
{
    close(ctx->fd_cycles);
    close(ctx->fd_instret);
    for (int i = 0; i < PERF_HW_EXT_NUM_COUNTERS; i++) {
        if (ctx->fd_ext[i] >= 0) {
            close(ctx->fd_ext[i]);
        }
    }
}

/* ── Extended sample CSV header (for file output) ────────────────────────── */

static inline void perf_hw_ext_print_csv_header(void)
{
    printf("op,iterations,"
           "min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,"
           "min_instret,median_instret,max_instret,mean_instret,stddev_instret,"
           "median_l1d_load_miss,median_l1d_access,l1d_miss_rate,"
           "median_dtlb_load_miss,"
           "median_br_mispred,"
           "median_stall_frontend,median_stall_backend,"
           "median_vector_inst,median_vpu_stall\n");
}

/* ── Extended sample: names array for programmatic access ────────────────── */

static inline const char *perf_hw_ext_counter_name(int idx)
{
    if (idx < 0 || idx >= PERF_HW_EXT_NUM_COUNTERS)
        return "unknown";
    return _perf_hw_ext_names[idx];
}

static inline int perf_hw_ext_counter_count(void)
{
    return PERF_HW_EXT_NUM_COUNTERS;
}

/* ── Statistics helpers ──────────────────────────────────────────────────── */

static inline void perf_hw_sort_u64(uint64_t *arr, int n)
{
    /* Insertion sort — fine for N <= 10000 in a benchmark harness */
    for (int i = 1; i < n; i++) {
        uint64_t key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

static inline uint64_t perf_hw_median(const uint64_t *sorted, int n)
{
    if (n % 2 == 1)
        return sorted[n / 2];
    return (sorted[n / 2 - 1] + sorted[n / 2]) / 2;
}

static inline double perf_hw_mean(const uint64_t *arr, int n)
{
    double sum = 0.0;
    for (int i = 0; i < n; i++)
        sum += (double)arr[i];
    return sum / (double)n;
}

static inline double perf_hw_stddev(const uint64_t *arr, int n, double mean)
{
    double sum_sq = 0.0;
    for (int i = 0; i < n; i++) {
        double d = (double)arr[i] - mean;
        sum_sq += d * d;
    }
    return __builtin_sqrt(sum_sq / (double)(n - 1));
}

/* ── System checks (call once at startup) ────────────────────────────────── */

static inline void perf_hw_check_system(void)
{
    /* Check CPU frequency governor */
    FILE *f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "r");
    if (f) {
        char gov[64] = {0};
        if (fgets(gov, sizeof(gov), f)) {
            /* Strip newline */
            char *nl = strchr(gov, '\n');
            if (nl) *nl = '\0';
            if (strcmp(gov, "performance") != 0) {
                fprintf(stderr, "WARNING: CPU governor is '%s', not 'performance'.\n"
                        "  Run: echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor\n",
                        gov);
            }
        }
        fclose(f);
    }

    /* Verify perf counters give non-zero values */
    perf_hw_ctx test_ctx;
    if (perf_hw_init(&test_ctx) == 0) {
        perf_hw_start(&test_ctx);
        volatile int x = 0;
        for (int i = 0; i < 1000; i++) x += i;
        perf_hw_stop(&test_ctx);
        perf_hw_sample s;
        perf_hw_read(&test_ctx, &s);
        perf_hw_close(&test_ctx);
        if (s.cycles == 0) {
            fprintf(stderr, "ERROR: perf_event_open returns 0 cycles. "
                    "Check /proc/sys/kernel/perf_event_paranoid\n");
            exit(1);
        }
        (void)x;
    }
}

#endif /* PERF_HW_H */
