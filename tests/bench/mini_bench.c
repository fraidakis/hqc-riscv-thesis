/* tests/bench/mini_bench.c
 *
 * Minimal standalone RISC-V benchmark for Spike vs QEMU comparison.
 *
 * Exercises three kernels relevant to HQC polynomial multiplication:
 *   1. Scalar GF(2^128) multiply via Zbc clmul/clmulh (2-limb schoolbook)
 *   2. RVV-accelerated 256-byte XOR (vxor.vv, e64/m4)
 *   3. Scalar XOR-rotate chain (control baseline)
 *
 * Output (one CSV line per kernel, to HTIF/UART console):
 *   kernel,iters,cycles,cyc_per_iter
 *
 * ── Compile (Spike HTIF backend) ──────────────────────────────────────
 *   make -f tests/bench/mini_bench.mk SIM=spike
 *   # → build-mini/mini_bench_spike.elf
 *
 * ── Compile (QEMU virt backend) ───────────────────────────────────────
 *   make -f tests/bench/mini_bench.mk SIM=qemu
 *   # → build-mini/mini_bench_qemu.elf
 *
 * ── Run on Spike (no logging!) ────────────────────────────────────────
 *   ./sim/spike/run_bench.sh build-mini/mini_bench_spike.elf
 *
 * ── Run on QEMU ───────────────────────────────────────────────────────
 *   ./sim/qemu/run_kat.sh build-mini/mini_bench_qemu.elf
 */

#include <stdint.h>
#include "bench.h"

/*
 * Read the machine cycle counter via CSR 0xb00 (mcycle).
 * mcycle is always accessible in M-mode; no ISA extension required.
 * bench.h exposes the same value through hqc_rdcycle() after the bench.c fix,
 * but we keep a local alias here so this file compiles standalone.
 */
static inline uint64_t read_mcycle(void)
{
    uint64_t c;
    __asm__ volatile ("csrr %0, 0xb00" : "=r"(c));
    return c;
}

/* Number of iterations per kernel. 10 000 keeps total Spike time < 60 s. */
#define N_ITER 10000

/* ── Scalar Zbc carryless multiply ────────────────────────────────────── */

static inline uint64_t clmul64(uint64_t a, uint64_t b)
{
    uint64_t r;
    __asm__ volatile ("clmul %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
    return r;
}

static inline uint64_t clmulh64(uint64_t a, uint64_t b)
{
    uint64_t r;
    __asm__ volatile ("clmulh %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
    return r;
}

/* 2-limb GF(2)[x] multiply: (a_hi·x^64 + a_lo) × (b_hi·x^64 + b_lo)
 * Returns the 128-bit low half of the 256-bit product.                     */
typedef struct { uint64_t lo, hi; } u128_t;

static inline u128_t gf2_mul128(u128_t a, u128_t b)
{
    uint64_t ll_lo = clmul64 (a.lo, b.lo);
    uint64_t ll_hi = clmulh64(a.lo, b.lo);
    uint64_t lh_lo = clmul64 (a.lo, b.hi);
    uint64_t hl_lo = clmul64 (a.hi, b.lo);
    u128_t r;
    r.lo = ll_lo;
    r.hi = ll_hi ^ lh_lo ^ hl_lo;
    return r;
}

/* ── RVV 256-byte XOR (e64, m4) ────────────────────────────────────────── */

#define BUF_WORDS 32   /* 32 × 8 bytes = 256 bytes */

/* Use separate section so the linker keeps these even with --gc-sections. */
static uint64_t __attribute__((used)) a_buf[BUF_WORDS];
static uint64_t __attribute__((used)) b_buf[BUF_WORDS];
static uint64_t __attribute__((used)) c_buf[BUF_WORDS];

/* XOR 256 bytes: c[] = a[] ^ b[] using a single vxor.vv with m4 grouping. */
static void rvv_xor256(void)
{
    __asm__ volatile (
        /* Set vl = 32 elements of e64 with LMUL=m4 */
        "li      t0, 32\n"
        "vsetvli t1, t0, e64, m4, ta, ma\n"
        /* Load a[] into v0-v3, b[] into v4-v7 */
        "vle64.v v0, (%0)\n"
        "vle64.v v4, (%1)\n"
        /* c[] = a[] ^ b[] into v8-v11 */
        "vxor.vv v8, v0, v4\n"
        /* Store result */
        "vse64.v v8, (%2)\n"
        :
        : "r"(a_buf), "r"(b_buf), "r"(c_buf)
        : "t0", "t1", "memory"
    );
}

/* ── Helper: print one result line ────────────────────────────────────── */

static void print_result(const char *kernel, uint64_t iters, uint64_t cycles)
{
    hqc_htif_puts(kernel);
    hqc_htif_puts(",");
    hqc_htif_putu64(iters);
    hqc_htif_puts(",");
    hqc_htif_putu64(cycles);
    hqc_htif_puts(",");
    hqc_htif_putu64(cycles / (iters ? iters : 1));
    hqc_htif_puts("\n");
}

/* ── main ──────────────────────────────────────────────────────────────── */

int main(void)
{
    uint64_t t0, t1;

    /* Print CSV header */
    hqc_htif_puts("kernel,iters,cycles,cyc_per_iter\n");

    /* Initialise buffers (non-trivial values prevent DCE). */
    for (int i = 0; i < BUF_WORDS; i++) {
        a_buf[i] = (uint64_t)(i + 1) * 0xDEADBEEFCAFEBABEULL;
        b_buf[i] = (uint64_t)(BUF_WORDS - i) * 0x0123456789ABCDEFULL;
    }

    /* ── Kernel 1: Scalar GF(2^128) multiply chain ──────────────────── */
    u128_t acc = { 0xFEDCBA9876543210ULL, 0x0123456789ABCDEFULL };
    u128_t mul = { 0xDEADBEEFCAFEBABEULL, 0xCAFED00DBEEF1234ULL };

    t0 = read_mcycle();
    for (int i = 0; i < N_ITER; i++) {
        acc = gf2_mul128(acc, mul);
        /* Feed back result to prevent loop being collapsed by optimiser. */
        mul.lo ^= acc.hi;
        mul.hi ^= acc.lo;
    }
    t1 = read_mcycle();

    /* Prevent acc from being optimised away after the loop. */
    __asm__ volatile ("" : : "r"(acc.lo), "r"(acc.hi) : );

    print_result("clmul128", N_ITER, t1 - t0);

    /* ── Kernel 2: RVV 256-byte XOR ─────────────────────────────────── */
    t0 = read_mcycle();
    for (int i = 0; i < N_ITER; i++) {
        rvv_xor256();
        /* Use one output word so the call is not DCE'd. */
        a_buf[i & (BUF_WORDS - 1)] ^= c_buf[0];
    }
    t1 = read_mcycle();

    print_result("rvv_xor256", N_ITER, t1 - t0);

    /* ── Kernel 3: Scalar XOR-rotate chain (control) ─────────────────── */
    uint64_t x = 1;
    const uint64_t scalar_iters = (uint64_t)N_ITER * BUF_WORDS;

    t0 = read_mcycle();
    for (uint64_t i = 0; i < scalar_iters; i++) {
        x ^= a_buf[i & (BUF_WORDS - 1)];
        x = (x << 1) | (x >> 63);
    }
    t1 = read_mcycle();

    __asm__ volatile ("" : : "r"(x) : );

    print_result("scalar_xor", scalar_iters, t1 - t0);

    hqc_htif_exit(0);
    return 0;
}
