/* tests/bench/perf_hw.h
 *
 * Linux perf_event_open wrapper for hardware benchmarking on BPI-F3.
 *
 * Provides the same measurement semantics as the bare-metal bench.h
 * (cycle + instruction counts per region) but uses the Linux perf
 * subsystem instead of direct CSR access.
 *
 * Usage:
 *   perf_hw_ctx ctx;
 *   perf_hw_init(&ctx);           // open counters (once)
 *   perf_hw_start(&ctx);          // reset + enable
 *   ... code under test ...
 *   perf_hw_stop(&ctx);           // disable
 *   perf_hw_read(&ctx, &sample);  // read cycles + instret
 *   perf_hw_close(&ctx);          // cleanup (once)
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

/* ── Data types ──────────────────────────────────────────────────────────── */

typedef struct {
    int fd_cycles;
    int fd_instret;
} perf_hw_ctx;

typedef struct {
    uint64_t cycles;
    uint64_t instret;
} perf_hw_sample;

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
