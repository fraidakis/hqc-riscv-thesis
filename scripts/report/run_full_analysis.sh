#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

usage() {
    cat <<'EOF'
Usage:
    scripts/report/run_full_analysis.sh --variant <name> [--level <128|192|256|all>] [--report]

Arguments:
  --variant <name>      Required benchmark/profiler variant name.
  --level <value>       Optional level: 128, 192, 256, or all (default: all).
  --report              Optional flag to aggregate results into one Markdown report.
  -h, --help            Show this help message.
EOF
}

VARIANT=""
LEVEL="all"
REPORT=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --variant)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --variant requires a value" >&2
                usage
                exit 2
            fi
            VARIANT="$2"
            shift 2
            ;;
        --level)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --level requires a value" >&2
                usage
                exit 2
            fi
            LEVEL="$2"
            shift 2
            ;;
        --report)
            REPORT=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "ERROR: unknown option: $1" >&2
            usage
            exit 2
            ;;
    esac
done

if [[ -z "$VARIANT" ]]; then
    usage
    exit 1
fi

if [[ "$LEVEL" != "128" && "$LEVEL" != "192" && "$LEVEL" != "256" && "$LEVEL" != "all" ]]; then
    echo "ERROR: --level must be one of 128, 192, 256, or all" >&2
    usage
    exit 2
fi

if [[ "$REPORT" -eq 1 ]]; then
    mkdir -p "${REPO_ROOT}/analysis/llm_reports"
fi

TRACK="scalar"
if [[ "${VARIANT}" == *"zvbc"* ]]; then
    TRACK="vector"
fi

REPORT_LEVEL="$LEVEL"
if [[ "$REPORT_LEVEL" == "all" ]]; then
    REPORT_LEVEL="128"
fi

sanitize_label() {
    echo "$1" | tr -c 'A-Za-z0-9_-' '_'
}

run_quiet_stdout() {
    local label="$1"
    shift

    echo "[run] ${label}"
    "$@" >/dev/null
}

cache_bust_rebuild() {
    echo "🧹 Cleaning previous build objects to guarantee a fresh compile..."
    cmake --build build/bench --target clean >/dev/null || true
    cmake --build build/bench >/dev/null
}

resolve_levels_for_suite() {
    if [[ "$LEVEL" == "all" ]]; then
        echo "128 192 256"
    else
        echo "$LEVEL"
    fi
}

csv_to_markdown() {
    local csv_file="$1"

    if [[ ! -f "$csv_file" ]]; then
        return 1
    fi

    awk -F',' '
        BEGIN { printed = 0 }
        NR == 1 {
            print "| " $1 " | " $2 " | " $3 " | " $4 (NF >= 5 ? " | " $5 : "") (NF >= 6 ? " | " $6 : "") (NF >= 7 ? " | " $7 : "") (NF >= 8 ? " | " $8 : "") (NF >= 9 ? " | " $9 : "") " |"
            sep = "| --- | --- | --- | ---"
            if (NF >= 5) sep = sep " | ---"
            if (NF >= 6) sep = sep " | ---"
            if (NF >= 7) sep = sep " | ---"
            if (NF >= 8) sep = sep " | ---"
            if (NF >= 9) sep = sep " | ---"
            print sep " |"
            printed = 1
            next
        }
        {
            print "| " $1 " | " $2 " | " $3 " | " $4 (NF >= 5 ? " | " $5 : "") (NF >= 6 ? " | " $6 : "") (NF >= 7 ? " | " $7 : "") (NF >= 8 ? " | " $8 : "") (NF >= 9 ? " | " $9 : "") " |"
        }
        END {
            if (printed == 0) {
                exit 1
            }
        }
    ' "$csv_file"
}

append_table_or_note() {
    local report_file="$1"
    local title="$2"
    local csv_file="$3"

    echo "### ${title}" >> "$report_file"

    if [[ ! -f "$csv_file" ]]; then
        echo "No data found: ${csv_file}" >> "$report_file"
        echo "" >> "$report_file"
        return
    fi

    if ! csv_to_markdown "$csv_file" >> "$report_file"; then
        echo "No tabular rows available in: ${csv_file}" >> "$report_file"
    fi

    echo "" >> "$report_file"
}

generate_report() {
    local report_file="${REPO_ROOT}/analysis/llm_reports/${VARIANT}_L${LEVEL}_report.md"

    local poly_csv="${REPO_ROOT}/analysis/benchmarks/isolated/${TRACK}/${VARIANT}/polymul_latest.csv"
    local suite_root="${REPO_ROOT}/analysis/benchmarks/suites/${SUITE_RUN_ID}"
    local keypair_csv="${suite_root}/keypair.csv"
    local encaps_csv="${suite_root}/encaps.csv"
    local decaps_csv="${suite_root}/decaps.csv"

    local cache_csv="${REPO_ROOT}/analysis/profiles/cache/${TRACK}/${VARIANT}/kem_latest.csv"
    local memory_csv="${REPO_ROOT}/analysis/profiles/memory/${TRACK}/${VARIANT}/kem_latest.csv"

    local trace_hotspots_csv="${REPO_ROOT}/analysis/profiles/traces/${TRACK}/${VARIANT}/kem_hqc${REPORT_LEVEL}_hotspots_latest.csv"
    local trace_lines_csv="${REPO_ROOT}/analysis/profiles/traces/${TRACK}/${VARIANT}/kem_hqc${REPORT_LEVEL}_lines_latest.csv"

    local tmp_poly
    local tmp_kem
    local tmp_cache
    local tmp_memory
    local tmp_trace_hot
    local tmp_trace_lines

    tmp_poly="$(mktemp)"
    tmp_kem="$(mktemp)"
    tmp_cache="$(mktemp)"
    tmp_memory="$(mktemp)"
    tmp_trace_hot="$(mktemp)"
    tmp_trace_lines="$(mktemp)"

    trap 'rm -f "${tmp_poly:-}" "${tmp_kem:-}" "${tmp_cache:-}" "${tmp_memory:-}" "${tmp_trace_hot:-}" "${tmp_trace_lines:-}"' RETURN

    if [[ -f "$poly_csv" ]]; then
        awk -F',' -v lvl="$REPORT_LEVEL" '
            BEGIN { OFS="," }
            /^#/ { next }
            NR == 1 { print $1,$2,$3,$4; next }
            $1 == lvl { print $1,$2,$3,$4 }
        ' "$poly_csv" > "$tmp_poly"
    fi

    {
        echo "level,variant,op,wall_seconds,instructions,codesize"
        for kem_src in "$keypair_csv" "$encaps_csv" "$decaps_csv"; do
            if [[ -f "$kem_src" ]]; then
                awk -F',' -v lvl="$REPORT_LEVEL" -v variant="$VARIANT" '
                    BEGIN { OFS="," }
                    /^#/ { next }
                    NR == 1 { next }
                    $1 == lvl && $2 == variant { print $1,$2,$4,$5,$6,$7 }
                ' "$kem_src"
            fi
        done
    } > "$tmp_kem"

    if [[ -f "$cache_csv" ]]; then
        awk '/^#/ { next } NF { print }' "$cache_csv" | head -n 11 > "$tmp_cache" || true
    fi

    if [[ -f "$memory_csv" ]]; then
        awk '/^#/ { next } NF { print }' "$memory_csv" > "$tmp_memory"
    fi

    if [[ -f "$trace_hotspots_csv" ]]; then
        awk '/^#/ { next } NF { print }' "$trace_hotspots_csv" | head -n 21 > "$tmp_trace_hot" || true
    fi

    if [[ -f "$trace_lines_csv" ]]; then
        awk '/^#/ { next } NF { print }' "$trace_lines_csv" | head -n 21 > "$tmp_trace_lines" || true
    fi

    {
        echo "# Full Analysis Report"
        echo ""
        echo "- Variant: ${VARIANT}"
        echo "- Level: ${REPORT_LEVEL}"
        echo "- Date: $(date +'%Y-%m-%d %H:%M:%S')"
        echo ""
        echo "## 1. Cycle Counts"
        echo ""
    } > "$report_file"

    append_table_or_note "$report_file" "Polymul Isolated" "$tmp_poly"
    append_table_or_note "$report_file" "Full KEM Suite" "$tmp_kem"

    {
        echo "## 2. Memory & Cache"
        echo ""
    } >> "$report_file"

    append_table_or_note "$report_file" "Cache Misses (Top 10 Rows)" "$tmp_cache"
    append_table_or_note "$report_file" "Memory Footprint" "$tmp_memory"

    {
        echo "## 3. Hotspots"
        echo ""
    } >> "$report_file"

    append_table_or_note "$report_file" "Function-Level Hotspots (Top 20 Rows)" "$tmp_trace_hot"
    append_table_or_note "$report_file" "Line-Level Hotspots (Top 20 Rows)" "$tmp_trace_lines"

    echo "[done] Report written: ${report_file}"
}

main() {
    cd "$REPO_ROOT"

    cache_bust_rebuild

    echo "[step] Running benchmarks and profilers"

    # Required command shape for isolated benchmark; fall back to repository-compatible invocation if needed.
    if ! run_quiet_stdout "Isolated polymul benchmark" \
        ./scripts/bench/run_benchmarks.sh --isolated --variant "$VARIANT" --level "$LEVEL"; then
        run_quiet_stdout "Isolated polymul benchmark (compat fallback)" \
            ./scripts/bench/run_benchmarks.sh --isolated --variant "$VARIANT"
    fi

    SUITE_RUN_ID="$(sanitize_label "${VARIANT}_L${LEVEL}_$(date +'%Y%m%d_%H%M%S')")"

    # Required command shape for suite benchmark; fall back to repository-compatible invocation if needed.
    if ! run_quiet_stdout "Full KEM suite benchmark" \
        ./scripts/bench/run_benchmarks.sh --suite --variant "$VARIANT" --level "$LEVEL" --run-id "$SUITE_RUN_ID"; then
        run_quiet_stdout "Full KEM suite benchmark (compat fallback)" \
            ./scripts/bench/run_benchmarks.sh --suite --variant "$VARIANT" --levels "$(resolve_levels_for_suite)" --run-id "$SUITE_RUN_ID"
    fi

    run_quiet_stdout "Cache profiler" \
        ./scripts/bench/run_profile.sh --cache --variant "$VARIANT" --level "$LEVEL"

    run_quiet_stdout "Memory profiler" \
        ./scripts/bench/run_profile.sh --memory --variant "$VARIANT" --level "$LEVEL"

    run_quiet_stdout "Trace profiler (function-level)" \
        ./scripts/bench/run_profile.sh --trace --variant "$VARIANT" --level "$LEVEL"

    run_quiet_stdout "Trace profiler (line-level)" \
        ./scripts/bench/run_profile.sh --trace --lines --variant "$VARIANT" --level "$LEVEL"

    if [[ "$REPORT" -eq 1 ]]; then
        echo "[step] Aggregating LLM-readable Markdown report"
        generate_report
    fi

    echo "[done] Full analysis orchestration completed"
}

main
