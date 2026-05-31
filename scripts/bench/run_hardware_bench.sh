#!/usr/bin/env bash
# scripts/bench/run_hardware_bench.sh
#
# Hardware benchmark collection script for BPI-F3 (SpacemiT K1 X60).
# Runs NATIVELY on the board — collects cycle + instruction measurements
# for thesis §8.5 using perf_event_open.
#
# Protocol: 100 warmup iterations (discarded) + 10,000 measured iterations.
# Reports: median, mean, stddev, min, max per metric.
#
# Output structure:
#   results/hardware/
#   ├── raw/                    # Per-iteration dumps (10k lines each)
#   │   ├── ref_scalar_128_mul_cycles.txt
#   │   ├── karatsuba_zbc_128_mul_cycles.txt
#   │   ├── karatsuba_rvv_128_kem_keypair_cycles.txt
#   │   └── ...
#   ├── kem/                    # Summary CSVs for thesis charts
#   │   ├── hqc128_cycles.csv
#   │   ├── hqc192_cycles.csv
#   │   └── hqc256_cycles.csv
#   ├── micro/                  # Isolated kernel summaries
#   │   ├── polymul_cycles.csv
#   │   ├── keccak_cycles.csv
#   │   └── decode_cycles.csv
#   └── validation/             # Spike vs HW comparison
#       └── spike_vs_hw_speedups.csv
#
# Usage:
#   ./scripts/bench/run_hardware_bench.sh              # Full suite
#   ./scripts/bench/run_hardware_bench.sh --bench mul  # Only polymul
#   ./scripts/bench/run_hardware_bench.sh --bench kem  # Only KEM
#   ./scripts/bench/run_hardware_bench.sh --bench keccak
#   ./scripts/bench/run_hardware_bench.sh --bench decode
#   ./scripts/bench/run_hardware_bench.sh --variant karatsuba_rvv --level 128

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# ── Defaults ─────────────────────────────────────────────────────────────────
BENCH_DIR="${REPO_ROOT}/tests/bench"
OUT_DIR="${REPO_ROOT}/results/hardware"
RAW_DIR="${OUT_DIR}/raw"
KEM_DIR="${OUT_DIR}/kem"
MICRO_DIR="${OUT_DIR}/micro"
VALIDATION_DIR="${OUT_DIR}/validation"

CORE=7
N_ITER=10000
N_WARMUP=100
VARIANTS="ref_scalar karatsuba_zbc karatsuba_rvv"
LEVELS="128 192 256"
BENCH="all"  # all | mul | kem | keccak | decode
SINGLE_VARIANT=""
SINGLE_LEVEL=""

# ── Parse arguments ──────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --bench)     BENCH="$2"; shift 2 ;;
        --variant)   SINGLE_VARIANT="$2"; shift 2 ;;
        --level)     SINGLE_LEVEL="$2"; shift 2 ;;
        --core)      CORE="$2"; shift 2 ;;
        --iter)      N_ITER="$2"; shift 2 ;;
        --warmup)    N_WARMUP="$2"; shift 2 ;;
        -h|--help)
            echo "Usage: run_hardware_bench.sh [--bench all|mul|kem|keccak|decode]"
            echo "       [--variant VARIANT] [--level LEVEL] [--core N]"
            echo "       [--iter N] [--warmup N]"
            exit 0 ;;
        *)  echo "ERROR: Unknown option: $1" >&2; exit 2 ;;
    esac
done

if [[ -n "$SINGLE_VARIANT" ]]; then VARIANTS="$SINGLE_VARIANT"; fi
if [[ -n "$SINGLE_LEVEL" ]]; then LEVELS="$SINGLE_LEVEL"; fi

# ══════════════════════════════════════════════════════════════════════════════
# PRE-FLIGHT CHECKS
# ══════════════════════════════════════════════════════════════════════════════

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  Hardware Benchmark Collection — BPI-F3 (SpacemiT X60)     ║"
echo "║  Protocol: ${N_WARMUP} warmup + ${N_ITER} measured iterations            ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# 1. Verify running on RISC-V
echo "── Pre-flight checks ──────────────────────────────────────────"
ARCH="$(uname -m)"
if [[ "$ARCH" != "riscv64" ]]; then
    echo "ERROR: This script must run natively on riscv64 (got: $ARCH)" >&2
    echo "       Deploy to the BPI-F3 board and run there." >&2
    exit 1
fi
echo "  ✓ Architecture: $ARCH"

# 2. Set CPU governor to performance
echo "  Setting CPU governor to performance..."
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null 2>&1 || {
    echo "  ⚠ WARNING: Could not set CPU governor (continuing anyway)" >&2
}
GOV="$(cat /sys/devices/system/cpu/cpu${CORE}/cpufreq/scaling_governor 2>/dev/null || echo "unknown")"
if [[ "$GOV" == "performance" ]]; then
    echo "  ✓ CPU governor: performance"
else
    echo "  ⚠ CPU governor: $GOV (expected: performance)" >&2
fi

# 3. Verify frequency is stable
FREQ="$(cat /sys/devices/system/cpu/cpu${CORE}/cpufreq/scaling_cur_freq 2>/dev/null || echo "unknown")"
if [[ "$FREQ" != "unknown" ]]; then
    FREQ_MHZ=$((FREQ / 1000))
    echo "  ✓ CPU${CORE} frequency: ${FREQ_MHZ} MHz"
else
    echo "  ⚠ Could not read CPU frequency" >&2
fi

# 4. Check perf_event_open works
echo "  Testing perf_event_open..."
PERF_TEST_SRC=$(mktemp /tmp/perf_test_XXXXXX.c)
PERF_TEST_BIN=$(mktemp /tmp/perf_test_XXXXXX)
cat > "$PERF_TEST_SRC" << 'PERF_EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
int main(void) {
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(pe));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(pe);
    pe.config = PERF_COUNT_HW_CPU_CYCLES;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    int fd = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
    if (fd < 0) { printf("FAIL\n"); return 1; }
    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
    volatile int x = 0;
    for (int i = 0; i < 10000; i++) x += i;
    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    long long count = 0;
    read(fd, &count, sizeof(count));
    close(fd);
    if (count > 0) { printf("OK %lld\n", count); return 0; }
    printf("FAIL zero\n"); return 1;
}
PERF_EOF
gcc -O2 -static -o "$PERF_TEST_BIN" "$PERF_TEST_SRC" 2>/dev/null
PERF_RESULT="$("$PERF_TEST_BIN" 2>/dev/null || echo "FAIL")"
rm -f "$PERF_TEST_SRC" "$PERF_TEST_BIN"
if [[ "$PERF_RESULT" == OK* ]]; then
    echo "  ✓ perf_event_open: working (test read: ${PERF_RESULT#OK })"
else
    echo "ERROR: perf_event_open not working. Check /proc/sys/kernel/perf_event_paranoid" >&2
    echo "       Try: echo 1 | sudo tee /proc/sys/kernel/perf_event_paranoid" >&2
    exit 1
fi
echo ""

# ══════════════════════════════════════════════════════════════════════════════
# BUILD PHASE
# ══════════════════════════════════════════════════════════════════════════════

echo "── Build phase ────────────────────────────────────────────────"

build_variant() {
    local level="$1" variant="$2" target="$3"
    echo "  Building: ${target} (${variant}, HQC-${level})..."
    make -f Makefile.hw LEVEL="$level" POLYMUL="$variant" MODE=publish \
         N_ITER="$N_ITER" N_WARMUP="$N_WARMUP" "$target" 2>&1 | tail -3
}

verify_instructions() {
    local binary="$1" variant="$2"
    local clmul_count vle_count
    clmul_count=$(objdump -d "$binary" | grep -cE "clmul|clmulh" || echo "0")
    vle_count=$(objdump -d "$binary" | grep -cE "vle64|vxor|vslide" || echo "0")

    case "$variant" in
        karatsuba_zbc)
            if [[ "$clmul_count" -gt 0 ]]; then
                echo "    ✓ Verified: ${clmul_count} clmul/clmulh instructions"
            else
                echo "    ⚠ WARNING: No clmul instructions found!" >&2
            fi
            ;;
        karatsuba_rvv)
            if [[ "$vle_count" -gt 0 ]]; then
                echo "    ✓ Verified: ${vle_count} vector instructions (vle64/vxor/vslide)"
            else
                echo "    ⚠ WARNING: No vector instructions found!" >&2
            fi
            if [[ "$clmul_count" -gt 0 ]]; then
                echo "    ✓ Verified: ${clmul_count} clmul/clmulh instructions"
            fi
            ;;
    esac
}

cd "$BENCH_DIR"

# Build all needed binaries
for variant in $VARIANTS; do
    for level in $LEVELS; do
        if [[ "$BENCH" == "all" || "$BENCH" == "mul" ]]; then
            build_variant "$level" "$variant" "mul"
            verify_instructions "bench_hw_mul_hqc${level}_${variant}" "$variant"
        fi
        if [[ "$BENCH" == "all" || "$BENCH" == "kem" ]]; then
            build_variant "$level" "$variant" "kem"
            verify_instructions "bench_hw_kem_hqc${level}_${variant}" "$variant"
        fi
    done
done

# Keccak and decode don't depend on polymul variant
if [[ "$BENCH" == "all" || "$BENCH" == "keccak" ]]; then
    for level in $LEVELS; do
        build_variant "$level" "ref_scalar" "keccak"
    done
fi
if [[ "$BENCH" == "all" || "$BENCH" == "decode" ]]; then
    for level in $LEVELS; do
        build_variant "$level" "ref_scalar" "decode"
    done
fi

echo ""

# ══════════════════════════════════════════════════════════════════════════════
# EXECUTION PHASE
# ══════════════════════════════════════════════════════════════════════════════

echo "── Execution phase ────────────────────────────────────────────"

# Create output directories
mkdir -p "$RAW_DIR" "$KEM_DIR" "$MICRO_DIR" "$VALIDATION_DIR"

run_bench() {
    local binary="$1"
    shift
    sudo nice -n -20 taskset -c "$CORE" "./$binary" "$@"
}

# ── Polymul benchmarks ───────────────────────────────────────────────────────
if [[ "$BENCH" == "all" || "$BENCH" == "mul" ]]; then
    echo ""
    echo "  ━━━ Polynomial Multiplication (isolated vect_mul) ━━━"

    # Collect all mul results for the micro/polymul_cycles.csv summary
    POLYMUL_CSV_HEADER="variant,level,iterations,min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,min_instret,median_instret,max_instret,mean_instret,stddev_instret"
    POLYMUL_CSV_ROWS=""

    for variant in $VARIANTS; do
        for level in $LEVELS; do
            echo "    [${variant}/HQC-${level}] Running ${N_ITER} iterations..."
            local_raw_dir="${RAW_DIR}"
            output="$(run_bench "bench_hw_mul_hqc${level}_${variant}" --raw-dir "$local_raw_dir")"

            # Extract data row (skip comment and header lines)
            data_row="$(echo "$output" | grep "^vect_mul," || true)"
            if [[ -n "$data_row" ]]; then
                # Parse median cycles for display
                median_cyc="$(echo "$data_row" | cut -d',' -f4)"
                median_ins="$(echo "$data_row" | cut -d',' -f10)"
                ipc=$(awk "BEGIN {printf \"%.3f\", $median_ins / $median_cyc}" 2>/dev/null || echo "?")
                printf "      %-20s median=%s cycles, instret=%s, IPC=%s\n" \
                    "${variant}" "$median_cyc" "$median_ins" "$ipc"

                # Accumulate for summary CSV
                POLYMUL_CSV_ROWS="${POLYMUL_CSV_ROWS}${variant},${level},${data_row#vect_mul,}"$'\n'
            else
                echo "      ⚠ No data for ${variant}/HQC-${level}" >&2
            fi
        done
    done

    # Write micro/polymul_cycles.csv
    {
        echo "# Hardware polymul benchmark — BPI-F3 (SpacemiT X60)"
        echo "# date: $(date +'%Y-%m-%d %H:%M')"
        echo "# protocol: ${N_WARMUP} warmup + ${N_ITER} measured iterations"
        echo "# core: ${CORE}, governor: performance"
        echo "$POLYMUL_CSV_HEADER"
        echo -n "$POLYMUL_CSV_ROWS"
    } > "${MICRO_DIR}/polymul_cycles.csv"
    echo "    → ${MICRO_DIR}/polymul_cycles.csv"
fi

# ── Keccak benchmarks ────────────────────────────────────────────────────────
if [[ "$BENCH" == "all" || "$BENCH" == "keccak" ]]; then
    echo ""
    echo "  ━━━ Keccak (full decaps symmetric pattern) ━━━"

    KECCAK_CSV_HEADER="level,iterations,min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,min_instret,median_instret,max_instret,mean_instret,stddev_instret"
    KECCAK_CSV_ROWS=""

    for level in $LEVELS; do
        echo "    [HQC-${level}] Running ${N_ITER} iterations..."
        output="$(run_bench "bench_hw_keccak_hqc${level}" --raw-dir "$RAW_DIR")"

        data_row="$(echo "$output" | grep "^keccak," || true)"
        if [[ -n "$data_row" ]]; then
            median_cyc="$(echo "$data_row" | cut -d',' -f4)"
            printf "      HQC-%-3s  median=%s cycles\n" "$level" "$median_cyc"
            KECCAK_CSV_ROWS="${KECCAK_CSV_ROWS}${level},${data_row#keccak,}"$'\n'
        fi
    done

    {
        echo "# Hardware Keccak benchmark — BPI-F3 (SpacemiT X60)"
        echo "# date: $(date +'%Y-%m-%d %H:%M')"
        echo "# protocol: ${N_WARMUP} warmup + ${N_ITER} measured iterations"
        echo "# measures: full decaps Keccak call pattern (all hash/xof operations)"
        echo "$KECCAK_CSV_HEADER"
        echo -n "$KECCAK_CSV_ROWS"
    } > "${MICRO_DIR}/keccak_cycles.csv"
    echo "    → ${MICRO_DIR}/keccak_cycles.csv"
fi

# ── Decode benchmarks ────────────────────────────────────────────────────────
if [[ "$BENCH" == "all" || "$BENCH" == "decode" ]]; then
    echo ""
    echo "  ━━━ Decode (RM + RS) ━━━"

    DECODE_CSV_HEADER="level,iterations,min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,min_instret,median_instret,max_instret,mean_instret,stddev_instret"
    DECODE_CSV_ROWS=""

    for level in $LEVELS; do
        echo "    [HQC-${level}] Running ${N_ITER} iterations..."
        output="$(run_bench "bench_hw_decode_hqc${level}" --raw-dir "$RAW_DIR")"

        data_row="$(echo "$output" | grep "^decode," || true)"
        if [[ -n "$data_row" ]]; then
            median_cyc="$(echo "$data_row" | cut -d',' -f4)"
            printf "      HQC-%-3s  median=%s cycles\n" "$level" "$median_cyc"
            DECODE_CSV_ROWS="${DECODE_CSV_ROWS}${level},${data_row#decode,}"$'\n'
        fi
    done

    {
        echo "# Hardware decode benchmark — BPI-F3 (SpacemiT X60)"
        echo "# date: $(date +'%Y-%m-%d %H:%M')"
        echo "# protocol: ${N_WARMUP} warmup + ${N_ITER} measured iterations"
        echo "# measures: reed_muller_decode + reed_solomon_decode"
        echo "$DECODE_CSV_HEADER"
        echo -n "$DECODE_CSV_ROWS"
    } > "${MICRO_DIR}/decode_cycles.csv"
    echo "    → ${MICRO_DIR}/decode_cycles.csv"
fi

# ── KEM benchmarks ───────────────────────────────────────────────────────────
if [[ "$BENCH" == "all" || "$BENCH" == "kem" ]]; then
    echo ""
    echo "  ━━━ Full KEM (keypair + encaps + decaps) ━━━"

    for level in $LEVELS; do
        KEM_CSV_HEADER="variant,op,iterations,min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,min_instret,median_instret,max_instret,mean_instret,stddev_instret"
        KEM_CSV_ROWS=""

        for variant in $VARIANTS; do
            echo "    [${variant}/HQC-${level}] Running ${N_ITER} iterations..."
            output="$(run_bench "bench_hw_kem_hqc${level}_${variant}" --raw-dir "$RAW_DIR")"

            # Extract keypair, encaps, decaps rows
            for op in keypair encaps decaps; do
                data_row="$(echo "$output" | grep "^${op}," || true)"
                if [[ -n "$data_row" ]]; then
                    median_cyc="$(echo "$data_row" | cut -d',' -f4)"
                    printf "      %-20s %-8s median=%s cycles\n" "$variant" "$op" "$median_cyc"
                    KEM_CSV_ROWS="${KEM_CSV_ROWS}${variant},${data_row}"$'\n'
                fi
            done
        done

        # Write kem/hqcXXX_cycles.csv
        {
            echo "# Hardware KEM benchmark — BPI-F3 (SpacemiT X60)"
            echo "# date: $(date +'%Y-%m-%d %H:%M')"
            echo "# level: HQC-${level}"
            echo "# protocol: ${N_WARMUP} warmup + ${N_ITER} measured iterations"
            echo "# core: ${CORE}, governor: performance"
            echo "$KEM_CSV_HEADER"
            echo -n "$KEM_CSV_ROWS"
        } > "${KEM_DIR}/hqc${level}_cycles.csv"
        echo "    → ${KEM_DIR}/hqc${level}_cycles.csv"
        echo ""
    done
fi

# ══════════════════════════════════════════════════════════════════════════════
# VALIDATION: Spike vs HW comparison CSV
# ══════════════════════════════════════════════════════════════════════════════

if [[ "$BENCH" == "all" || "$BENCH" == "mul" ]]; then
    echo "── Generating Spike vs HW comparison ──────────────────────────"

    SPIKE_DIR="${REPO_ROOT}/results/simulation/benchmarks/components/poly_mul"
    VALIDATION_CSV="${VALIDATION_DIR}/spike_vs_hw_speedups.csv"

    # Check if Spike data exists
    if [[ -d "$SPIKE_DIR" ]]; then
        {
            echo "# Spike ISS (instret) vs Hardware (cycles) comparison"
            echo "# date: $(date +'%Y-%m-%d %H:%M')"
            echo "# Spike: deterministic instruction count (IPC=1 assumption)"
            echo "# Hardware: median cycles from ${N_ITER} iterations on BPI-F3"
            echo "variant,level,spike_instret,hw_median_cycles,ipc,spike_speedup_vs_ref,hw_speedup_vs_ref,hw_spike_ratio"
        } > "$VALIDATION_CSV"

        # Read Spike ref_scalar baseline
        for level in $LEVELS; do
            # Read Spike instret for each variant
            spike_ref=""
            spike_file="${SPIKE_DIR}/scalar_ref/isolated_mul_ref-scalar_latest.csv"
            if [[ -f "$spike_file" ]]; then
                spike_ref="$(awk -F',' -v lvl="$level" '$1==lvl && $2=="vect_mul" {print $3}' "$spike_file")"
            fi

            hw_ref=""
            if [[ -f "${MICRO_DIR}/polymul_cycles.csv" ]]; then
                hw_ref="$(awk -F',' -v lvl="$level" '$1=="ref_scalar" && $2==lvl {print $4}' "${MICRO_DIR}/polymul_cycles.csv")"
            fi

            for variant in $VARIANTS; do
                # Map variant to Spike directory name
                case "$variant" in
                    ref_scalar)    spike_var_dir="scalar_ref"; spike_slug="ref-scalar" ;;
                    karatsuba_zbc) spike_var_dir="scalar_Zb"; spike_slug="karatsuba-zbc" ;;
                    karatsuba_rvv) spike_var_dir="vector_rvv"; spike_slug="karatsuba-rvv" ;;
                    *)             spike_var_dir="$variant"; spike_slug="${variant//_/-}" ;;
                esac

                # Read Spike instret
                spike_instret=""
                spike_csv="${SPIKE_DIR}/${spike_var_dir}/isolated_mul_${spike_slug}_latest.csv"
                if [[ -f "$spike_csv" ]]; then
                    spike_instret="$(awk -F',' -v lvl="$level" '$1==lvl && $2=="vect_mul" {print $3}' "$spike_csv")"
                fi

                # Read HW median cycles
                hw_cycles=""
                if [[ -f "${MICRO_DIR}/polymul_cycles.csv" ]]; then
                    hw_cycles="$(awk -F',' -v var="$variant" -v lvl="$level" '$1==var && $2==lvl {print $4}' "${MICRO_DIR}/polymul_cycles.csv")"
                fi

                # Compute IPC
                ipc=""
                if [[ -n "$spike_instret" && -n "$hw_cycles" && "$hw_cycles" != "0" ]]; then
                    ipc="$(awk "BEGIN {printf \"%.3f\", $spike_instret / $hw_cycles}")"
                fi

                # Compute speedups vs ref
                spike_speedup=""
                if [[ -n "$spike_ref" && -n "$spike_instret" && "$spike_instret" != "0" ]]; then
                    spike_speedup="$(awk "BEGIN {printf \"%.3f\", $spike_ref / $spike_instret}")"
                fi

                hw_speedup=""
                if [[ -n "$hw_ref" && -n "$hw_cycles" && "$hw_cycles" != "0" ]]; then
                    hw_speedup="$(awk "BEGIN {printf \"%.3f\", $hw_ref / $hw_cycles}")"
                fi

                # Compute HW/Spike ratio
                ratio=""
                if [[ -n "$hw_speedup" && -n "$spike_speedup" && "$spike_speedup" != "0" && "$spike_speedup" != "0.000" ]]; then
                    ratio="$(awk "BEGIN {printf \"%.3f\", $hw_speedup / $spike_speedup}")"
                fi

                echo "${variant},${level},${spike_instret:-—},${hw_cycles:-—},${ipc:-—},${spike_speedup:-—},${hw_speedup:-—},${ratio:-—}" >> "$VALIDATION_CSV"
            done
        done

        echo "  → ${VALIDATION_CSV}"
    else
        echo "  ⚠ No Spike data found at ${SPIKE_DIR} — skipping comparison"
    fi
fi

# ══════════════════════════════════════════════════════════════════════════════
# SUMMARY
# ══════════════════════════════════════════════════════════════════════════════

echo ""
echo "═══════════════════════════════════════════════════════════════"
echo "  Collection complete."
echo ""
echo "  Output:"
if [[ "$BENCH" == "all" || "$BENCH" == "mul" ]]; then
    echo "    ${MICRO_DIR}/polymul_cycles.csv"
fi
if [[ "$BENCH" == "all" || "$BENCH" == "keccak" ]]; then
    echo "    ${MICRO_DIR}/keccak_cycles.csv"
fi
if [[ "$BENCH" == "all" || "$BENCH" == "decode" ]]; then
    echo "    ${MICRO_DIR}/decode_cycles.csv"
fi
if [[ "$BENCH" == "all" || "$BENCH" == "kem" ]]; then
    for level in $LEVELS; do
        echo "    ${KEM_DIR}/hqc${level}_cycles.csv"
    done
fi
echo ""
echo "  Raw per-iteration data: ${RAW_DIR}/"
RAW_COUNT="$(find "$RAW_DIR" -name "*.txt" 2>/dev/null | wc -l)"
echo "    ${RAW_COUNT} raw dump files"
echo "═══════════════════════════════════════════════════════════════"
