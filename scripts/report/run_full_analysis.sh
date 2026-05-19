#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${SCRIPT_DIR}/../bench/lib_bench.sh"

usage() {
    cat <<'EOF'
Usage:
        scripts/report/run_full_analysis.sh --mul <name> [--level <128|192|256|all>]

Arguments:
  --mul <name>         Required benchmark/profiler variant name.
    --level <value>       Optional level: 128, 192, 256, or all (default: 128).
  -h, --help            Show this help message.
EOF
}

LEVEL="128"
MUL_VARIANT=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --mul)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --mul requires a value" >&2
                usage
                exit 2
            fi
            MUL_VARIANT="$2"
            shift 2
            ;;
        --keccak|--decode)
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

if [[ -z "$MUL_VARIANT" ]]; then
    usage
    exit 1
fi

if [[ "$LEVEL" != "128" && "$LEVEL" != "192" && "$LEVEL" != "256" && "$LEVEL" != "all" ]]; then
    echo "ERROR: --level must be one of 128, 192, 256, or all" >&2
    usage
    exit 2
fi

mkdir -p "${REPO_ROOT}/analysis/reports"

run_step_with_timer() {
    local step="$1"
    local total="$2"
    local msg="$3"
    shift 3

    printf "[%s/%s] ⏱️  %s ... " "$step" "$total" "$msg"
    local t_start t_end elapsed exit_code
    t_start="$(date +%s%N)"
    { "$@" >/dev/null 2>&1; } && exit_code=0 || exit_code=$?
    t_end="$(date +%s%N)"
    elapsed=$(awk -v s="$t_start" -v e="$t_end" 'BEGIN { printf "%.1f", (e - s) / 1000000000 }')
    if [[ $exit_code -eq 0 ]]; then
        echo "✔️ DONE (${elapsed}s)"
    else
        echo "❌ FAILED"
        return $exit_code
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
    local report_file="${REPO_ROOT}/analysis/reports/${MUL_VARIANT}_L${REPORT_LEVEL}_report.md"

    local poly_csv="${REPO_ROOT}/analysis/benchmarks/isolated/${TRACK}/${MUL_VARIANT}/polymul_latest.csv"
    local kem_csv="${REPO_ROOT}/analysis/benchmarks/isolated/${TRACK}/${MUL_VARIANT}/kem_latest.csv"

    local cache_csv="${REPO_ROOT}/analysis/profiles/cache/${TRACK}/${MUL_VARIANT}/kem_latest.csv"
    local memory_csv="${REPO_ROOT}/analysis/profiles/memory/${TRACK}/${MUL_VARIANT}/kem_latest.csv"

    local trace_hotspots_csv="${REPO_ROOT}/analysis/profiles/traces/${TRACK}/${MUL_VARIANT}/kem_hqc${REPORT_LEVEL}_hotspots_latest.csv"
    local trace_lines_csv="${REPO_ROOT}/analysis/profiles/traces/${TRACK}/${MUL_VARIANT}/kem_hqc${REPORT_LEVEL}_lines_latest.csv"
    local histogram_csv="${REPO_ROOT}/analysis/profiles/histograms/${TRACK}/${MUL_VARIANT}/kem_hqc${REPORT_LEVEL}_latest.csv"

    local tmp_poly
    local tmp_kem
    local tmp_cache
    local tmp_memory
    local tmp_trace_hot
    local tmp_trace_lines
    local tmp_histogram

    tmp_poly="$(mktemp)"
    tmp_kem="$(mktemp)"
    tmp_cache="$(mktemp)"
    tmp_memory="$(mktemp)"
    tmp_trace_hot="$(mktemp)"
    tmp_trace_lines="$(mktemp)"
    tmp_histogram="$(mktemp)"

    trap 'rm -f "${tmp_poly:-}" "${tmp_kem:-}" "${tmp_cache:-}" "${tmp_memory:-}" "${tmp_trace_hot:-}" "${tmp_trace_lines:-}" "${tmp_histogram:-}"' RETURN

    if [[ -f "$poly_csv" ]]; then
        awk -F',' '
            BEGIN { OFS=","; hdr=0 }
            /^#/ { next }
            !hdr { print $1,$2,$3,$4; hdr=1; next }
            { print $1,$2,$3,$4 }
        ' "$poly_csv" > "$tmp_poly"
    fi
    if [[ ! -s "$tmp_poly" ]]; then
        rm -f "$tmp_poly"
    fi

    {
        echo "level,variant,op,instructions,codesize"
        if [[ -f "$kem_csv" ]]; then
            awk -F',' '
                BEGIN { OFS=","; hdr=0 }
                /^#/ { next }
                !hdr { hdr=1; next }
                { print $1,$2,$3,$4,$5 }
            ' "$kem_csv"
        fi
    } > "$tmp_kem"
    if [[ ! -s "$tmp_kem" ]]; then
        rm -f "$tmp_kem"
    fi

    if [[ -f "$cache_csv" ]]; then
        awk -F',' '
            BEGIN { OFS=","; hdr=0 }
            /^#/ { next }
            !hdr { print $0; hdr=1; next }
            { print $0 }
        ' "$cache_csv" | head -n 11 > "$tmp_cache" || true
    fi
    if [[ ! -s "$tmp_cache" ]]; then
        rm -f "$tmp_cache"
    fi

    if [[ -f "$memory_csv" ]]; then
        awk -F',' '
            BEGIN { OFS=","; hdr=0 }
            /^#/ { next }
            !hdr { print $0; hdr=1; next }
            { print $0 }
        ' "$memory_csv" > "$tmp_memory"
    fi
    if [[ ! -s "$tmp_memory" ]]; then
        rm -f "$tmp_memory"
    fi

    if [[ -f "$trace_hotspots_csv" ]]; then
        awk -F',' '
            BEGIN { OFS=","; hdr=0 }
            /^#/ { next }
            !hdr { print $0; hdr=1; next }
            { print $0 }
        ' "$trace_hotspots_csv" | head -n 21 > "$tmp_trace_hot" || true
    fi
    if [[ ! -s "$tmp_trace_hot" ]]; then
        rm -f "$tmp_trace_hot"
    fi

    if [[ -f "$trace_lines_csv" ]]; then
        awk -F',' '
            BEGIN { OFS=","; hdr=0 }
            /^#/ { next }
            !hdr { print $0; hdr=1; next }
            { print $0 }
        ' "$trace_lines_csv" | head -n 21 > "$tmp_trace_lines" || true
    fi
    if [[ ! -s "$tmp_trace_lines" ]]; then
        rm -f "$tmp_trace_lines"
    fi

    if [[ -f "$histogram_csv" ]]; then
        awk -F',' '
            BEGIN { OFS=","; hdr=0 }
            /^#/ { next }
            !hdr { print $0; hdr=1; next }
            { print $0 }
        ' "$histogram_csv" | head -n 21 > "$tmp_histogram" || true
    fi
    if [[ ! -s "$tmp_histogram" ]]; then
        rm -f "$tmp_histogram"
    fi

    {
        echo "# Full Analysis Report"
        echo ""
        echo "- Variant: ${MUL_VARIANT}"
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
    append_table_or_note "$report_file" "Basic Block Histogram (Top 20)" "$tmp_histogram"

    echo "[done] Report written: ${report_file}"
}

run_variant_analysis() {
    local variant="$1"
    export MUL_VARIANT="$variant"

    TRACK="scalar"
    if [[ "${MUL_VARIANT}" == *"zvbc"* || "${MUL_VARIANT}" == *"rvv"* ]]; then
        TRACK="vector"
    fi

    bench_parse_track_args --mul "$MUL_VARIANT"

    REPORT_LEVEL="$LEVEL"
    if [[ "$REPORT_LEVEL" == "all" ]]; then
        REPORT_LEVEL="128"
    fi

    echo ""
    echo "============================================================"
    echo "         STARTING FULL ANALYSIS: ${MUL_VARIANT}"
    echo "============================================================"
    echo ""

    cd "$REPO_ROOT"

    run_step_with_timer 1 8 "Isolated polymul benchmark" \
        ./scripts/bench/run_benchmarks.sh --isolated --mul "$MUL_VARIANT" --keccak "$KECCAK_IMPL" --decode "$DECODE_IMPL" --level all

    run_step_with_timer 2 8 "Isolated KEM benchmark" \
        ./scripts/bench/run_benchmarks.sh --isolated --kem --mul "$MUL_VARIANT" --keccak "$KECCAK_IMPL" --decode "$DECODE_IMPL" --level all

    run_step_with_timer 3 8 "Cache profiler" \
        ./scripts/bench/run_profile.sh --cache --mul "$MUL_VARIANT" --keccak "$KECCAK_IMPL" --decode "$DECODE_IMPL" --level all

    run_step_with_timer 4 8 "Memory profiler" \
        ./scripts/bench/run_profile.sh --memory --mul "$MUL_VARIANT" --keccak "$KECCAK_IMPL" --decode "$DECODE_IMPL" --level all

    run_step_with_timer 5 8 "Trace profiler (function-level)" \
        ./scripts/bench/run_profile.sh --trace --mul "$MUL_VARIANT" --keccak "$KECCAK_IMPL" --decode "$DECODE_IMPL" --level "$LEVEL"

    run_step_with_timer 6 8 "Trace profiler (line-level)" \
        ./scripts/bench/run_profile.sh --trace --lines --mul "$MUL_VARIANT" --keccak "$KECCAK_IMPL" --decode "$DECODE_IMPL" --level "$LEVEL"

    run_step_with_timer 7 8 "Histogram profiler" \
        ./scripts/bench/run_profile.sh --histogram --mul "$MUL_VARIANT" --keccak "$KECCAK_IMPL" --decode "$DECODE_IMPL" --level "$LEVEL"

    run_step_with_timer 8 8 "Generate Markdown report" \
        generate_report
}

bench_load_implemented_variants "$REPO_ROOT"

if [[ "$MUL_VARIANT" == "all" ]]; then
    for variant in "${IMPLEMENTED_VARIANTS[@]}"; do
        run_variant_analysis "$variant"
    done
else
    run_variant_analysis "$MUL_VARIANT"
fi
