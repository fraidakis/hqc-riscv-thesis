#!/usr/bin/env bash
# scripts/bench/run_spike_bench_phase.sh
#
# Launch Spike benchmark jobs in the background (nohup) for all polymul
# variants of a given phase.  Prints PIDs and monitoring hints, then exits.
#
# Default: polymul micro-bench only (~20-150 min each).
# Use --full at phase milestones to also launch full KEM benches.
#
# Known phases and their canonical variants:
#   phase0      -> karatsuba_ref   (vendor baseline, unmodified)
#   phase1a     -> karatsuba_zbc   (clmul base-case + lazy reduction)
#   phase1b     -> fafft_zbc       (FAFFT with clmul GF(2^m) butterflies + RVV XOR)
#   phase1c     -> hybrid_zbc      (Karatsuba outer levels -> FAFFT inner, CRT-based)
#   phase1plus  -> fafft_zvbc      (vclmul.vv vectorised butterflies, Spike only)
#
# Passing a known phase automatically restricts benchmarks to its canonical
# variant.  Use --variant to override (e.g. for cross-phase comparisons).
#
# Usage:
#   scripts/bench/run_spike_bench_phase.sh <phase> [options]
#
# Options:
#   --variant <grep-pattern>   Override the canonical variant filter
#                              e.g. "karatsuba_zbc" or "fafft_zbc|hybrid_zbc"
#   --full                     Also launch full KEM benchmarks (keypair/encaps/decaps)
#   --no-polymul               Skip polymul micro-benchmarks (requires --full)
#   --iter <N>                 Number of iterations baked into the ELF (default: 1)
#                              Used to name output CSVs as _N{N}.csv
#
# Examples:
#   scripts/bench/run_spike_bench_phase.sh phase0
#   scripts/bench/run_spike_bench_phase.sh phase1a
#   scripts/bench/run_spike_bench_phase.sh phase1a --full
#   scripts/bench/run_spike_bench_phase.sh phase1plus --full
#   scripts/bench/run_spike_bench_phase.sh phase1a --variant "karatsuba_zbc|karatsuba_ref"
#
# Output files written to analysis/benchmarks/<phase>/:
#   hqc{level}_{variant}_polymul_N{N}.csv  polymul micro-bench CSV
#   hqc{level}_{variant}_polymul.log       Spike stderr (errors/warnings)
#   hqc{level}_{variant}_N{N}.csv          full KEM bench CSV (--full only)
#   hqc{level}_{variant}_spike.log         full KEM Spike stderr (--full only)
#   run_events.log                         unified START/END event log (PID + timing)

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Implemented variants come from scripts/implemented_variants.txt -- the same
# file CMakeLists.txt reads, so CMake and scripts are always in sync.
mapfile -t IMPLEMENTED_VARIANTS < "${REPO_ROOT}/scripts/implemented_variants.txt"

RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
SPIKE="${RISCV}/bin/spike"
ISA="rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm"
BUILD_DIR="${REPO_ROOT}/build-spike"

LEVELS=(128 192 256)

# ── Phase -> canonical variant mapping ───────────────────────────────────────
# Each phase has exactly one canonical polymul variant.  This table is the
# single source of truth; update it whenever a new phase lands.
declare -A PHASE_VARIANT_MAP
PHASE_VARIANT_MAP=(
    [phase0]="karatsuba_ref"
    [phase1a]="karatsuba_zbc"
    [phase1b]="fafft_zbc"
    [phase1c]="hybrid_zbc"
    [phase1plus]="fafft_zvbc"
)

# ── Argument parsing ──────────────────────────────────────────────────────────
if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <phase> [--variant <pattern>] [--full] [--no-polymul]" >&2
    exit 2
fi

PHASE="$1"
shift

VARIANT_FILTER=""
DO_FULL=0
DO_POLYMUL=1
ITER=1

while [[ $# -gt 0 ]]; do
    case "$1" in
        --variant)
            VARIANT_FILTER="$2"
            shift 2
            ;;
        --full)
            DO_FULL=1
            shift
            ;;
        --no-polymul)
            DO_POLYMUL=0
            shift
            ;;
        --iter)
            ITER="$2"
            if ! [[ "$ITER" =~ ^[1-9][0-9]*$ ]]; then
                echo "ERROR: --iter requires a positive integer, got: $ITER" >&2
                exit 2
            fi
            shift 2
            ;;
        *)
            echo "ERROR: Unknown option: $1" >&2
            exit 2
            ;;
    esac
done

if [[ $DO_POLYMUL -eq 0 && $DO_FULL -eq 0 ]]; then
    echo "ERROR: --no-polymul requires --full (nothing to do otherwise)" >&2
    exit 2
fi

# Resolve variant filter: explicit --variant overrides the canonical mapping.
if [[ -z "$VARIANT_FILTER" ]]; then
    if [[ -v PHASE_VARIANT_MAP["$PHASE"] ]]; then
        VARIANT_FILTER="${PHASE_VARIANT_MAP[$PHASE]}"
        echo "[bench] Phase '${PHASE}' -> canonical variant '${VARIANT_FILTER}'"
    else
        echo "WARNING: Unknown phase '${PHASE}' -- no canonical variant; running all implemented variants." >&2
        echo "         Known phases: ${!PHASE_VARIANT_MAP[*]}" >&2
    fi
else
    echo "[bench] Phase '${PHASE}' -> variant override '${VARIANT_FILTER}'"
fi

# ── Preflight checks ──────────────────────────────────────────────────────────
if [[ ! -x "$SPIKE" ]]; then
    echo "ERROR: Spike not found: $SPIKE" >&2
    echo "       Source toolchain/toolchain.env or set RISCV" >&2
    exit 2
fi

OUT_DIR="${REPO_ROOT}/analysis/benchmarks/${PHASE}"
mkdir -p "$OUT_DIR"

EVENTS_LOG="${OUT_DIR}/run_events.log"

# ── Helper: apply variant filter ─────────────────────────────────────────────
variant_matches() {
    local v="$1"
    if [[ -z "$VARIANT_FILTER" ]]; then
        return 0
    fi
    echo "$v" | grep -qE "$VARIANT_FILTER"
}

# ── Helper: launch a timed Spike job in the background ───────────────────────
# The main script synchronously writes the START event with the job PID.
# The background subshell calculates elapsed time and uses flock to safely
# append the END event, preventing race conditions from concurrent writes.
# Usage: launch_timed_job <elf> <csv> <log> <label>
launch_timed_job() {
    local elf="$1" csv="$2" log="$3" label="$4"

    # Capture variables needed inside the subshell by value.
    local spike="$SPIKE" isa="$ISA" events_log="$EVENTS_LOG"

    nohup bash -c "
        start=\$(date +%s)
        \"${spike}\" --isa=\"${isa}\" \"${elf}\"
        exit_code=\$?
        end=\$(date +%s)
        elapsed=\$(( end - start ))
        mins=\$(( elapsed / 60 ))
        secs=\$(( elapsed % 60 ))
        status=OK
        [[ \$exit_code -ne 0 ]] && status=\"FAIL(exit=\${exit_code})\"
        msg=\"\$(date '+%Y-%m-%dT%H:%M:%S') END    ${label}  elapsed=\${mins}m\${secs}s  status=\${status}\"
        ( flock -x 9; printf '%s\n' \"\${msg}\" >&9 ) 9>>\"${events_log}\"
    " > "$csv" 2> "$log" &

    local pid=$!
    printf '%s START  pid=%-6s %s\n' \
        "$(date '+%Y-%m-%dT%H:%M:%S')" "$pid" "$label" >> "$EVENTS_LOG"
    echo "$pid"
}

# ── Launch jobs ───────────────────────────────────────────────────────────────
LAUNCHED=0
declare -a JOB_LINES=()

for LEVEL in "${LEVELS[@]}"; do
    for VARIANT in "${IMPLEMENTED_VARIANTS[@]}"; do
        variant_matches "$VARIANT" || continue

        # ── Polymul micro-bench ───────────────────────────────────────────────
        if [[ $DO_POLYMUL -eq 1 ]]; then
            ELF="${BUILD_DIR}/bench_polymul_hqc${LEVEL}_${VARIANT}.elf"
            TARGET="bench_polymul_hqc${LEVEL}_${VARIANT}"
            PREFIX="hqc${LEVEL}_${VARIANT}"
            CSV="${OUT_DIR}/${PREFIX}_polymul_N${ITER}.csv"
            LOG="${OUT_DIR}/${PREFIX}_polymul.log"

            # Always do an incremental cmake build before running — no-op if nothing
            # changed, but catches stale ELFs when sources (e.g. bench.c) are updated.
            if [[ -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
                cmake --build "$BUILD_DIR" --target "$TARGET" -j"$(nproc)" >&2 || {
                    echo "[bench] BUILD FAIL ${TARGET}" >&2; continue
                }
            fi
            if [[ ! -f "$ELF" ]]; then
                echo "[bench] SKIP  ${TARGET}: ELF not found and build dir not configured" >&2
                echo "              cmake -S . -B build-spike -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake -DHQC_SIM=spike -DBENCH_N_ITER=${ITER}" >&2
                continue
            fi

            LABEL="polymul_hqc${LEVEL}_${VARIANT}"
            PID=$(launch_timed_job "$ELF" "$CSV" "$LOG" "$LABEL")
            JOB_LINES+=("  PID ${PID}  ${LABEL}  ->  $(basename "$CSV")")
            LAUNCHED=$((LAUNCHED + 1))
        fi

        # ── Full KEM bench ────────────────────────────────────────────────────
        if [[ $DO_FULL -eq 1 ]]; then
            ELF="${BUILD_DIR}/bench_hqc${LEVEL}_${VARIANT}.elf"
            TARGET="bench_hqc${LEVEL}_${VARIANT}"
            PREFIX="hqc${LEVEL}_${VARIANT}"
            CSV="${OUT_DIR}/${PREFIX}_N${ITER}.csv"
            LOG="${OUT_DIR}/${PREFIX}_spike.log"

            # Always do an incremental cmake build before running — no-op if nothing
            # changed, but catches stale ELFs when sources (e.g. bench.c) are updated.
            if [[ -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
                cmake --build "$BUILD_DIR" --target "$TARGET" -j"$(nproc)" >&2 || {
                    echo "[bench] BUILD FAIL ${TARGET}" >&2; continue
                }
            fi
            if [[ ! -f "$ELF" ]]; then
                echo "[bench] SKIP  ${TARGET}: ELF not found and build dir not configured" >&2
                echo "              cmake -S . -B build-spike -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake -DHQC_SIM=spike -DBENCH_N_ITER=${ITER}" >&2
                continue
            fi

            LABEL="kem_hqc${LEVEL}_${VARIANT}"
            PID=$(launch_timed_job "$ELF" "$CSV" "$LOG" "$LABEL")
            JOB_LINES+=("  PID ${PID}  ${LABEL}  ->  $(basename "$CSV")")
            LAUNCHED=$((LAUNCHED + 1))
        fi
    done
done

# ── Summary ───────────────────────────────────────────────────────────────────
echo ""
if [[ $LAUNCHED -eq 0 ]]; then
    echo "[bench] No jobs launched. Check that ELFs are built in ${BUILD_DIR}/"
    echo "        cmake -S . -B build-spike -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake -DHQC_SIM=spike -DBENCH_N_ITER=${ITER}"
    echo "        cmake --build build-spike -j\$(nproc)"
else
    echo "[bench] Launched ${LAUNCHED} Spike job(s) for ${PHASE}:"
    for line in "${JOB_LINES[@]}"; do
        echo "$line"
    done
    echo ""
    echo "Monitor:"
    echo "  watch -n 30 \"wc -c ${OUT_DIR}/*.csv\""
    echo "  ps aux | grep '[s]pike.*bench'"
    echo "  tail -f ${EVENTS_LOG}"
    echo ""
    echo "When done, extract results:"
    echo "  python3 scripts/report/extract_results.py --phase ${PHASE}"
fi
