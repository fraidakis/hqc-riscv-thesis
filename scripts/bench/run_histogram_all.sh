#!/usr/bin/env bash
# scripts/bench/run_histogram_all.sh
#
# Run Spike -g (PC histogram) for polymul micro-bench ELFs (launch/wait mode),
# or print mnemonic frequency tables from existing histogram files (report mode).
#
# In report mode, raw PC addresses are resolved to instruction mnemonics via
# objdump, aggregated by mnemonic, and displayed with execution percentages.
# Both the bench CSV (stdout) and PC histogram (stderr) are captured in a
# single Spike run, so you get both metrics simultaneously.
#
# Usage:
#   scripts/bench/run_histogram_all.sh <bench_set> [options]
#
# Options:
#   --variant <grep-pattern>   Filter variants (e.g. "karatsuba_zbc")
#   --launch                   (default) Launch Spike -g jobs in background
#   --wait                     Launch jobs, block until complete, then report
#   --report                   Print mnemonic table from existing hist files
#   --top <N>                  Show top N entries in report (default: 20)
#   --compare-with <bench_set> Side-by-side delta vs another benchmark set in report
#   --context                  Show disassembly context for 3 hottest PCs
#
# Examples:
#   scripts/bench/run_histogram_all.sh baseline_ref
#   scripts/bench/run_histogram_all.sh polymul_zbc --wait
#   scripts/bench/run_histogram_all.sh polymul_zbc --report
#   scripts/bench/run_histogram_all.sh polymul_zbc --report --top 30
#   scripts/bench/run_histogram_all.sh polymul_zbc --report --compare-with baseline_ref
#   scripts/bench/run_histogram_all.sh polymul_zbc --report --context
#   scripts/bench/run_histogram_all.sh polymul_zbc --variant karatsuba_zbc --report
#
# Output files (--launch/--wait mode):
#   analysis/benchmarks/<bench_set>/hqc{level}_{variant}_polymul_N1.csv
#   analysis/benchmarks/<bench_set>/hqc{level}_{variant}_hist.txt
#
# Note: -g (PC histogram) adds negligible overhead vs. a plain bench run.
#       The CSV output is usable for extract_results.py just like regular bench output.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Implemented variants come from scripts/implemented_variants.txt — the same
# file CMakeLists.txt reads, so CMake and scripts are always in sync.
mapfile -t IMPLEMENTED_VARIANTS < "${REPO_ROOT}/scripts/implemented_variants.txt"

RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
SPIKE="${RISCV}/bin/spike"
OBJDUMP="${RISCV}/bin/riscv64-unknown-elf-objdump"
ISA="rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm"
BUILD_DIR="${REPO_ROOT}/build-spike"

LEVELS=(128 192 256)

# ── Argument parsing ──────────────────────────────────────────────────────────
if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <bench_set> [--variant <pattern>] [--launch|--report|--wait]" >&2
    echo "       [--top N] [--compare-with BENCH_SET] [--context]" >&2
    exit 2
fi

BENCH_SET="$1"
shift

VARIANT_FILTER=""
MODE="launch"
TOP_N=20
COMPARE_WITH=""
SHOW_CONTEXT=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --variant)
            VARIANT_FILTER="$2"; shift 2 ;;
        --launch)
            MODE="launch"; shift ;;
        --report)
            MODE="report"; shift ;;
        --wait)
            MODE="wait"; shift ;;
        --top)
            TOP_N="$2"; shift 2 ;;
        --compare-with)
            COMPARE_WITH="$2"; shift 2 ;;
        --context)
            SHOW_CONTEXT=1; shift ;;
        *)
            echo "ERROR: Unknown option: $1" >&2; exit 2 ;;
    esac
done

OUT_DIR="${REPO_ROOT}/analysis/benchmarks/${BENCH_SET}"

# ── Helper: apply variant filter ──────────────────────────────────────────────
variant_matches() {
    local v="$1"
    [[ -z "$VARIANT_FILTER" ]] && return 0
    echo "$v" | grep -qE "$VARIANT_FILTER"
}

# ── PC resolution helpers ─────────────────────────────────────────────────────

# Build a PC→mnemonic lookup file from an ELF using objdump.
# Prints the path to a temp file containing "pc mnemonic" lines (one per insn),
# or "" if the ELF or objdump is unavailable.
build_pc_map() {
    local elf="$1"
    if [[ ! -f "$elf" || ! -x "$OBJDUMP" ]]; then
        echo ""
        return
    fi
    local tmp
    tmp=$(mktemp /tmp/hqc_pcmap.XXXXXX)
    "$OBJDUMP" -d "$elf" 2>/dev/null | \
        awk '/^[[:blank:]]+[0-9a-f]+:/ {
            pc = $1; sub(/:$/, "", pc); gsub(/^[[:blank:]]+/, "", pc)
            print pc, $3
        }' > "$tmp"
    echo "$tmp"
}

# Resolve hist PCs to mnemonics and aggregate counts by mnemonic.
# Prints "mnemonic count" lines sorted by count descending.
# Falls back to raw PC keys if map_file is empty or unavailable.
resolve_and_aggregate() {
    local hist="$1" map_file="${2:-}"
    if [[ -n "$map_file" && -s "$map_file" ]]; then
        awk 'NR==FNR         { map[$1]=$2; next }
             /^PC Histogram/ { next }
             /^[0-9a-f]/ {
                 key = ($1 in map) ? map[$1] : ("PC:" $1)
                 sum[key] += $2
             }
             END { for (k in sum) print k, sum[k] }' \
            "$map_file" "$hist" | sort -k2 -rn
    else
        grep -v '^PC Histogram' "$hist" | \
            awk 'NF==2 { sum[$1]+=$2 } END { for (k in sum) print k, sum[k] }' | \
            sort -k2 -rn
    fi
}

# Print a formatted mnemonic frequency table for one histogram file.
# Args: hist_file map_file top_n [compare_hist compare_label]
print_hist_report() {
    local hist="$1" map_file="${2:-}" top_n="$3"
    local compare_hist="${4:-}" compare_label="${5:-}"

    local n_data
    n_data=$(grep -vc '^PC Histogram' "$hist" 2>/dev/null || echo 0)
    if [[ "$n_data" -eq 0 ]]; then
        echo "  (empty or malformed histogram file)"
        return
    fi

    local tmp_data
    tmp_data=$(mktemp /tmp/hqc_hdata.XXXXXX)
    resolve_and_aggregate "$hist" "$map_file" > "$tmp_data"

    local total
    total=$(awk '{ s += $2 } END { print s+0 }' "$tmp_data")
    if [[ "$total" -eq 0 ]]; then
        echo "  (no execution data)"
        rm -f "$tmp_data"
        return
    fi

    if [[ -n "$compare_hist" && -f "$compare_hist" ]]; then
        # ── Side-by-side comparison ──────────────────────────────────────────
        local tmp_cdata
        tmp_cdata=$(mktemp /tmp/hqc_cdata.XXXXXX)
        resolve_and_aggregate "$compare_hist" "$map_file" > "$tmp_cdata"

        local ctotal
        ctotal=$(awk '{ s += $2 } END { print s+0 }' "$tmp_cdata")

        printf "  %-28s  %12s  %6s  |  %12s  %6s  %8s\n" \
            "mnemonic" "${BENCH_SET}" "%" "${compare_label}" "%" "Δ%"
        printf "  %-28s  %12s  %6s  |  %12s  %6s  %8s\n" \
            "--------" "-------" "---" "-------" "---" "---"

        awk -v total="$total" -v ctotal="${ctotal:-0}" '
            NR==FNR { cmap[$1]=$2+0; next }
            {
                insn=$1; cnt=$2
                pct  = 100.0*cnt/total
                ccnt = (insn in cmap) ? cmap[insn] : 0
                cpct = (ctotal>0) ? 100.0*ccnt/ctotal : 0
                delta = pct - cpct
                printf "  %-28s  %12d  %5.1f%%  |  %12d  %5.1f%%  %+7.2f%%\n",
                    insn, cnt, pct, ccnt, cpct, delta
            }
        ' "$tmp_cdata" "$tmp_data" | head -"$top_n"

        rm -f "$tmp_cdata"
    else
        # ── Single-phase report ──────────────────────────────────────────────
        printf "  %-28s  %14s  %s\n" "mnemonic" "count" "%"
        printf "  %-28s  %14s  %s\n" "--------" "-----" "-"
        awk -v total="$total" '{
            printf "  %-28s  %14d  %5.1f%%\n", $1, $2, 100.0*$2/total
        }' "$tmp_data" | head -"$top_n"
    fi

    rm -f "$tmp_data"
}

# Print disassembly context for the 3 hottest PCs in a histogram file.
show_context() {
    local hist="$1" elf="$2"
    if [[ ! -f "$elf" ]]; then
        echo "  (ELF not available for context — build-spike may be stale)"
        return
    fi

    local top_pcs
    mapfile -t top_pcs < <(
        grep -v '^PC Histogram' "$hist" | sort -k2 -rn | head -3 | awk '{print $1}'
    )
    [[ ${#top_pcs[@]} -eq 0 ]] && return

    echo ""
    echo "  ── Disassembly context (top-3 hottest PCs):"
    for pc in "${top_pcs[@]}"; do
        echo ""
        printf "  [0x%s]\n" "$pc"
        "$OBJDUMP" -d "$elf" 2>/dev/null | \
            grep -B 3 -A 8 "^[[:blank:]]*${pc}:" || \
            echo "    (PC not found in disassembly)"
    done
}

# ─────────────────────────────────────────────────────────────────────────────
# REPORT: print per-variant mnemonic frequency tables
# ─────────────────────────────────────────────────────────────────────────────
run_report() {
    if [[ ! -d "$OUT_DIR" ]]; then
        echo "[histogram] No benchmark-set directory found: $OUT_DIR" >&2
        echo "            Run --launch first to generate histogram files." >&2
        return 0
    fi

    local found=0
    for LEVEL in "${LEVELS[@]}"; do
        for VARIANT in "${IMPLEMENTED_VARIANTS[@]}"; do
            variant_matches "$VARIANT" || continue

            local hist="${OUT_DIR}/hqc${LEVEL}_${VARIANT}_hist.txt"
            if [[ ! -f "$hist" ]]; then
                echo "[histogram] SKIP  hqc${LEVEL}_${VARIANT}: hist not found" >&2
                continue
            fi

            found=$((found + 1))
            echo ""
            echo "=== hqc${LEVEL}_${VARIANT} (${BENCH_SET}) ==="

            local elf="${BUILD_DIR}/bench_polymul_hqc${LEVEL}_${VARIANT}.elf"
            local map_file
            map_file=$(build_pc_map "$elf")
            if [[ -z "$map_file" ]]; then
                echo "  note: objdump unavailable — showing raw PC addresses"
            fi

            local compare_hist=""
            if [[ -n "$COMPARE_WITH" ]]; then
                compare_hist="${REPO_ROOT}/analysis/benchmarks/${COMPARE_WITH}/hqc${LEVEL}_${VARIANT}_hist.txt"
                if [[ ! -f "$compare_hist" ]]; then
                    echo "  [compare: no hist for ${COMPARE_WITH} — skipping delta]" >&2
                    compare_hist=""
                fi
            fi

            print_hist_report "$hist" "${map_file:-}" "$TOP_N" "$compare_hist" "$COMPARE_WITH"

            if [[ "$SHOW_CONTEXT" -eq 1 ]]; then
                show_context "$hist" "$elf"
            fi

            [[ -n "${map_file:-}" && -f "$map_file" ]] && rm -f "$map_file"
        done
    done

    if [[ $found -eq 0 ]]; then
        echo ""
        echo "[histogram] No histogram files found in ${OUT_DIR}/"
        echo "            Run: $0 ${BENCH_SET} --launch"
    fi
}

# ─────────────────────────────────────────────────────────────────────────────
# REPORT MODE
# ─────────────────────────────────────────────────────────────────────────────
if [[ "$MODE" == "report" ]]; then
    run_report
    exit 0
fi

# ─────────────────────────────────────────────────────────────────────────────
# LAUNCH / WAIT MODE: start Spike -g jobs in background
# ─────────────────────────────────────────────────────────────────────────────
if [[ ! -x "$SPIKE" ]]; then
    echo "ERROR: Spike not found: $SPIKE" >&2
    echo "       Source toolchain/toolchain.env or set RISCV" >&2
    exit 2
fi

mkdir -p "$OUT_DIR"
PID_FILE="${OUT_DIR}/pids.txt"

LAUNCHED=0
declare -a JOB_LINES=()
declare -a JOB_PIDS=()

for LEVEL in "${LEVELS[@]}"; do
    for VARIANT in "${IMPLEMENTED_VARIANTS[@]}"; do
        variant_matches "$VARIANT" || continue

        ELF="${BUILD_DIR}/bench_polymul_hqc${LEVEL}_${VARIANT}.elf"
        TARGET="bench_polymul_hqc${LEVEL}_${VARIANT}"
        PREFIX="hqc${LEVEL}_${VARIANT}"
        CSV="${OUT_DIR}/${PREFIX}_polymul_N1.csv"
        HIST="${OUT_DIR}/${PREFIX}_hist.txt"

        # Always do an incremental cmake build before running — no-op if nothing
        # changed, but catches stale ELFs when sources (e.g. bench.c) are updated.
        if [[ -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
            cmake --build "$BUILD_DIR" --target "$TARGET" -j"$(nproc)" >&2 || {
                echo "[histogram] BUILD FAIL ${TARGET}" >&2; continue
            }
        fi
        if [[ ! -f "$ELF" ]]; then
            echo "[histogram] SKIP  ${TARGET}: ELF not found and build dir not configured" >&2
            echo "              cmake -S . -B build-spike -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake -DHQC_SIM=spike -DBENCH_N_ITER=1" >&2
            continue
        fi

        # Spike -g: stdout → CSV bench output, stderr → PC histogram text.
        nohup "$SPIKE" --isa="$ISA" -g "$ELF" \
            > "$CSV" \
            2> "$HIST" &

        PID=$!
        JOB_PIDS+=("$PID")
        echo "$(date '+%Y-%m-%dT%H:%M:%S') PID=$PID ${TARGET} (histogram)" >> "$PID_FILE"
        JOB_LINES+=("  PID ${PID}  ${TARGET}  →  $(basename "$CSV") + $(basename "$HIST")")
        LAUNCHED=$((LAUNCHED + 1))
    done
done

# ── Summary ───────────────────────────────────────────────────────────────────
echo ""
if [[ $LAUNCHED -eq 0 ]]; then
    echo "[histogram] No jobs launched. Check that polymul ELFs are built in ${BUILD_DIR}/"
    echo "            cmake --build build-spike -j\$(nproc)"
else
    echo "[histogram] Launched ${LAUNCHED} Spike -g job(s) for ${BENCH_SET}:"
    for line in "${JOB_LINES[@]}"; do
        echo "$line"
    done
    echo ""

    if [[ "$MODE" == "wait" ]]; then
        echo "[histogram] Waiting for ${LAUNCHED} job(s) to complete..."
        for pid in "${JOB_PIDS[@]}"; do
            wait "$pid" || true
        done
        echo "[histogram] All jobs done."
        run_report
    else
        echo "Monitor:"
        echo "  ps aux | grep '[s]pike.*bench_polymul'"
        echo "  watch -n 30 \"wc -c ${OUT_DIR}/*_hist.txt\""
        echo ""
        echo "When done, view top mnemonics:"
        echo "  $0 ${BENCH_SET} --report"
        echo "  $0 ${BENCH_SET} --report --context"
        echo "  $0 ${BENCH_SET} --report --compare-with <other_bench_set>"
    fi
fi
