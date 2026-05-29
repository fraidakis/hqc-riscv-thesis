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

mkdir -p "${REPO_ROOT}/results/reports/summaries"
mkdir -p "${REPO_ROOT}/results/reports/variants"

# Map old variant names to new directory names (extension-family tier naming).
variant_to_dir() {
    case "$1" in
        ref_scalar)      echo "scalar_ref" ;;
        karatsuba_zbc)   echo "scalar_Zb" ;;
        ref_vector)      echo "vector_ref" ;;
        karatsuba_rvv)   echo "vector_rvv" ;;
        karatsuba_zvbc)  echo "vector_Zvk" ;;
        *)               echo "$1" ;;  # fallback
    esac
}

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
    [[ -f "$csv_file" ]] || return 1

    awk -F',' '
    function fmt_int(n,    s, r) {
        s = sprintf("%d", n + 0)
        r = ""
        while (length(s) > 3) {
            r = "," substr(s, length(s) - 2) r
            s = substr(s, 1, length(s) - 3)
        }
        return s r
    }
    function humanize(col,    m) {
        m["instructions"]          = "Instructions"
        m["codesize"]              = "Code (B)"
        m["percentage"]            = "% Total"
        m["level"]                 = "Level"
        m["op"]                    = "Operation"
        m["vlen"]                  = "VLEN"
        m["compiler"]              = "Compiler"
        m["opt"]                   = "Opt"
        m["variant"]               = "Variant"
        m["status"]                = "Status"
        m["text_bytes"]            = "Text (B)"
        m["function"]              = "Function"
        m["dc_misses"]             = "D$ Misses"
        m["dc_read_misses"]        = "D$ Read Miss"
        m["dc_write_misses"]       = "D$ Write Miss"
        m["dc_writebacks"]         = "D$ Writebacks"
        m["dc_bytes_read"]         = "D$ Bytes Read"
        m["dc_bytes_written"]      = "D$ Bytes Written"
        m["dc_read_accesses"]      = "D$ Read Acc."
        m["dc_write_accesses"]     = "D$ Write Acc."
        m["load_instructions"]     = "Load Insns"
        m["store_instructions"]    = "Store Insns"
        m["load_bytes"]            = "Load Bytes"
        m["store_bytes"]           = "Store Bytes"
        m["est_cache_lines_load"]  = "CL Load"
        m["est_cache_lines_store"] = "CL Store"
        return (col in m) ? m[col] : col
    }
    BEGIN { have_hdr = 0; printed = 0; ncols = 0 }
    /^#/ { next }
    !have_hdr {
        have_hdr = 1
        ncols = NF
        for (i = 1; i <= ncols; i++) {
            gsub(/\r/, "", $i)
            hdr[i] = $i
        }
        next
    }
    !printed {
        for (i = 1; i <= ncols; i++) {
            gsub(/\r/, "", $i)
            is_num[i] = ($i ~ /^[0-9]+(\.[0-9]*)?$/ && $i != "") ? 1 : 0
        }
        printf "|"
        for (i = 1; i <= ncols; i++) printf " %s |", humanize(hdr[i])
        printf "\n|"
        for (i = 1; i <= ncols; i++) printf " %s |", (is_num[i] ? "---:" : "---")
        printf "\n"
        printed = 1
    }
    have_hdr {
        printf "|"
        for (i = 1; i <= ncols; i++) {
            gsub(/\r/, "", $i)
            val = $i
            if (is_num[i] == 1 && val ~ /^[0-9]+$/ && length(val) > 4) val = fmt_int(val + 0)
            printf " %s |", val
        }
        printf "\n"
    }
    END { if (!printed) exit 1 }
    ' "$csv_file"
}

append_table_or_note() {
    local report_file="$1"
    local title="$2"
    local csv_file="$3"

    printf "### %s\n\n" "${title}" >> "$report_file"

    if [[ ! -f "$csv_file" ]]; then
        printf "> _No data available._\n\n" >> "$report_file"
        return
    fi

    if ! csv_to_markdown "$csv_file" >> "$report_file"; then
        printf "> _No data rows._\n\n" >> "$report_file"
    else
        printf "\n" >> "$report_file"
    fi
}

generate_report() {
    local new_variant_dir
    new_variant_dir="$(variant_to_dir "$MUL_VARIANT")"
    local report_file="${REPO_ROOT}/results/reports/variants/${new_variant_dir}_L${REPORT_LEVEL}_report.md"
    local variant_slug
    local level_tag
    variant_slug="${MUL_VARIANT//_/-}"
    level_tag="Lall"
    if [[ "$LEVEL" != "all" ]]; then
        level_tag="L${LEVEL}"
    fi

    local poly_csv="${REPO_ROOT}/results/simulation/benchmarks/components/poly_mul/${new_variant_dir}/isolated_polymul_${variant_slug}_latest.csv"
    local kem_csv="${REPO_ROOT}/results/simulation/benchmarks/components/poly_mul/${new_variant_dir}/isolated_kem_${variant_slug}_latest.csv"

    local cache_csv="${REPO_ROOT}/results/simulation/profiling/cache/${new_variant_dir}/cache_kem_Lall_${variant_slug}_latest.csv"
    local memory_csv="${REPO_ROOT}/results/simulation/profiling/mem_access/${new_variant_dir}/memory_kem_Lall_${variant_slug}_latest.csv"

    local trace_hotspots_csv="${REPO_ROOT}/results/simulation/profiling/traces/${new_variant_dir}/trace_kem_L${REPORT_LEVEL}_${variant_slug}_hotspots_latest.csv"
    local trace_lines_csv="${REPO_ROOT}/results/simulation/profiling/traces/${new_variant_dir}/trace_kem_L${REPORT_LEVEL}_${variant_slug}_lines_latest.csv"
    local histogram_csv="${REPO_ROOT}/results/simulation/profiling/histograms/${new_variant_dir}/histogram_kem_L${REPORT_LEVEL}_${variant_slug}_latest.csv"

    local compiler_csv="${REPO_ROOT}/results/simulation/benchmarks/compiler_compare/${new_variant_dir}/compiler_compare_Lall_${variant_slug}_latest.csv"

    local vlen_csv_128="${REPO_ROOT}/results/simulation/benchmarks/vlen_scaling/${new_variant_dir}/vlen_sweep_L128_${variant_slug}_latest.csv"
    local vlen_csv_192="${REPO_ROOT}/results/simulation/benchmarks/vlen_scaling/${new_variant_dir}/vlen_sweep_L192_${variant_slug}_latest.csv"
    local vlen_csv_256="${REPO_ROOT}/results/simulation/benchmarks/vlen_scaling/${new_variant_dir}/vlen_sweep_L256_${variant_slug}_latest.csv"

    local speedups_md="${REPO_ROOT}/results/reports/summaries/speedups_simulation.md"
    local compiler_analysis_md="${REPO_ROOT}/results/reports/summaries/compiler_analysis.md"

    local tmp_poly
    local tmp_kem
    local tmp_cache
    local tmp_memory
    local tmp_trace_hot
    local tmp_trace_lines
    local tmp_histogram
    local tmp_compiler
    local tmp_vlen_128
    local tmp_vlen_192
    local tmp_vlen_256
    local tmp_cc_128
    local tmp_cc_192
    local tmp_cc_256

    tmp_poly="$(mktemp)"
    tmp_kem="$(mktemp)"
    tmp_cache="$(mktemp)"
    tmp_memory="$(mktemp)"
    tmp_trace_hot="$(mktemp)"
    tmp_trace_lines="$(mktemp)"
    tmp_histogram="$(mktemp)"
    tmp_compiler="$(mktemp)"
    tmp_vlen_128="$(mktemp)"
    tmp_vlen_192="$(mktemp)"
    tmp_vlen_256="$(mktemp)"
    tmp_cc_128="$(mktemp)"
    tmp_cc_192="$(mktemp)"
    tmp_cc_256="$(mktemp)"

    trap 'rm -f "${tmp_poly:-}" "${tmp_kem:-}" "${tmp_cache:-}" "${tmp_memory:-}" "${tmp_trace_hot:-}" "${tmp_trace_lines:-}" "${tmp_histogram:-}" "${tmp_compiler:-}" "${tmp_vlen_128:-}" "${tmp_vlen_192:-}" "${tmp_vlen_256:-}" "${tmp_cc_128:-}" "${tmp_cc_192:-}" "${tmp_cc_256:-}"' RETURN

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
        echo "level,op,instructions,codesize"
        if [[ -f "$kem_csv" ]]; then
            awk -F',' '
                BEGIN { OFS=","; hdr=0 }
                /^#/ { next }
                !hdr { hdr=1; next }
                $2 == "total" { print $1, "**" $2 "**", $3, $4; next }
                { print $1,$2,$3,$4 }
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

    # compiler compare CSV — drop elf path (col 8) and error message (col 9)
    if [[ -f "$compiler_csv" ]]; then
        awk -F',' '
            BEGIN { OFS=","; hdr=0 }
            /^#/ { next }
            !hdr { print $1,$2,$3,$4,$5,$6,$7; hdr=1; next }
            { print $1,$2,$3,$4,$5,$6,$7 }
        ' "$compiler_csv" > "$tmp_compiler"
    fi
    if [[ ! -s "$tmp_compiler" ]]; then
        rm -f "$tmp_compiler"
    fi

    # per-level compiler compare splits (level col dropped — redundant with section title)
    for _level in 128 192 256; do
        local _cc_tmp
        eval "_cc_tmp=\"\$tmp_cc_${_level}\""
        if [[ -f "$compiler_csv" ]]; then
            awk -F',' -v lvl="$_level" '
                BEGIN { OFS=","; hdr=0 }
                /^#/ { next }
                !hdr { print $1,$2,$4,$5,$6,$7; hdr=1; next }
                $3 == lvl { print $1,$2,$4,$5,$6,$7 }
            ' "$compiler_csv" > "$_cc_tmp"
        fi
        if [[ ! -s "$_cc_tmp" ]]; then
            rm -f "$_cc_tmp"
        fi
    done

    # VLEN sweep CSVs (vector variants only)
    for _lvl in 128 192 256; do
        local _vlen_csv _tmp_var
        _vlen_csv="${REPO_ROOT}/results/simulation/benchmarks/vlen_scaling/${new_variant_dir}/vlen_sweep_L${_lvl}_${variant_slug}_latest.csv"
        eval "_tmp_var=\"\$tmp_vlen_${_lvl}\""
        if [[ -f "$_vlen_csv" ]]; then
            awk -F',' '
                BEGIN { OFS=","; hdr=0 }
                /^#/ { next }
                !hdr { print $0; hdr=1; next }
                $1 == "total" { print $0 }
            ' "$_vlen_csv" > "$_tmp_var"
        fi
        if [[ ! -s "$_tmp_var" ]]; then
            rm -f "$_tmp_var"
        fi
    done

    local _git_sha
    _git_sha="$(git -C "${REPO_ROOT}" rev-parse --short HEAD 2>/dev/null || echo "unknown")"
    {
        printf "# HQC-RISC-V Analysis: \`%s\`\n\n" "${MUL_VARIANT}"
        printf "| Property | Value |\n"
        printf "| --- | --- |\n"
        printf "| Track | %s |\n" "${TRACK}"
        printf "| Security levels | 128 · 192 · 256 |\n"
        printf "| Report level (profiles) | %s |\n" "${REPORT_LEVEL}"
        printf "| Generated | %s |\n" "$(date +'%Y-%m-%d %H:%M:%S')"
        printf "| Commit | \`%s\` |\n" "${_git_sha}"
        printf "\n---\n\n"
        printf "## 1. Cycle Counts\n\n"
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

    if [[ "$TRACK" == "vector" ]]; then
        {
            echo "## 4. VLEN Scaling Sweep"
            echo ""
        } >> "$report_file"

        append_table_or_note "$report_file" "VLEN Sweep — HQC-128" "$tmp_vlen_128"
        append_table_or_note "$report_file" "VLEN Sweep — HQC-192" "$tmp_vlen_192"
        append_table_or_note "$report_file" "VLEN Sweep — HQC-256" "$tmp_vlen_256"
    fi

    {
        echo "## 5. Compiler Comparison"
        echo ""
    } >> "$report_file"

    append_table_or_note "$report_file" "HQC-128" "$tmp_cc_128"
    append_table_or_note "$report_file" "HQC-192" "$tmp_cc_192"
    append_table_or_note "$report_file" "HQC-256" "$tmp_cc_256"

    {
        echo "## 6. Cross-Variant Reports"
        echo ""
        echo "The following reports aggregate data across all variants and are regenerated"
        echo "each time a full analysis runs:"
        echo ""
        echo "- Speedup summary: \`results/reports/summaries/speedups_simulation.md\`"
        echo "- Compiler showdown: \`results/reports/summaries/compiler_analysis.md\`"
        echo ""
    } >> "$report_file"

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

    run_step_with_timer 1 14 "Isolated polymul benchmark" \
        ./scripts/bench/run_benchmarks.sh --mode mul --mul "$MUL_VARIANT" --keccak "$KECCAK_IMPL" --decode "$DECODE_IMPL"

    run_step_with_timer 2 14 "Isolated KEM benchmark" \
        ./scripts/bench/run_benchmarks.sh --mode kem --mul "$MUL_VARIANT" --keccak "$KECCAK_IMPL" --decode "$DECODE_IMPL"

    run_step_with_timer 3 14 "Cache profiler" \
        ./scripts/bench/run_profile.sh --cache --mul "$MUL_VARIANT" --keccak "$KECCAK_IMPL" --decode "$DECODE_IMPL" --level all

    run_step_with_timer 4 14 "Memory profiler" \
        ./scripts/bench/run_profile.sh --memory --mul "$MUL_VARIANT" --keccak "$KECCAK_IMPL" --decode "$DECODE_IMPL" --level all

    run_step_with_timer 5 14 "Trace profiler (function-level)" \
        ./scripts/bench/run_profile.sh --trace --mul "$MUL_VARIANT" --keccak "$KECCAK_IMPL" --decode "$DECODE_IMPL" --level "$LEVEL"

    run_step_with_timer 6 14 "Trace profiler (line-level)" \
        ./scripts/bench/run_profile.sh --trace --lines --mul "$MUL_VARIANT" --keccak "$KECCAK_IMPL" --decode "$DECODE_IMPL" --level "$LEVEL"

    run_step_with_timer 7 14 "Histogram profiler" \
        ./scripts/bench/run_profile.sh --histogram --mul "$MUL_VARIANT" --keccak "$KECCAK_IMPL" --decode "$DECODE_IMPL" --level "$LEVEL"

    if [[ "$TRACK" == "vector" ]]; then
        run_step_with_timer 8 14 "VLEN scaling sweep (256/512/1024/2048)" \
            ./scripts/bench/run_benchmarks.sh --mode vlen --mul "$MUL_VARIANT"
    else
        printf "[8/14] ⏭  VLEN scaling sweep (skipped: scalar variant)\n"
    fi

    run_step_with_timer 9 14 "Compiler comparison (Clang vs GCC)" \
        ./scripts/bench/run_benchmarks.sh --mode compiler --mul "$MUL_VARIANT"

    run_step_with_timer 10 14 "Generate speedups report" \
        python3 ./scripts/report/generate_speedups.py

    run_step_with_timer 11 14 "Generate compiler analysis report" \
        python3 ./scripts/report/generate_compiler_compare.py

    run_step_with_timer 12 14 "Generate Markdown report" \
        generate_report

    # Step 13: Generate hardware speedups report (conditional on hardware data)
    local hw_data_dir="${REPO_ROOT}/results/hardware/benchmarks"
    if find "$hw_data_dir" -path '*/*_latest.csv' -print -quit 2>/dev/null | grep -q .; then
        run_step_with_timer 13 14 "Generate hardware speedups report" \
            python3 ./scripts/report/generate_hw_speedups.py
    else
        printf "[13/14] ⏭  Hardware speedups report (skipped: no hardware data)\n"
    fi

    # Step 14: Generate cross-platform comparison report (conditional on both Spike + HW data)
    local spike_data_dir="${REPO_ROOT}/results/simulation/benchmarks/components/poly_mul"
    if find "$hw_data_dir" -path '*/*_latest.csv' -print -quit 2>/dev/null | grep -q . \
       && find "$spike_data_dir" -path '*/*_latest.csv' -print -quit 2>/dev/null | grep -q .; then
        run_step_with_timer 14 14 "Generate cross-platform comparison report" \
            python3 ./scripts/report/generate_comparison.py
    else
        printf "[14/14] ⏭  Cross-platform comparison report (skipped: missing Spike or hardware data)\n"
    fi
}

bench_load_implemented_variants "$REPO_ROOT"

if [[ "$MUL_VARIANT" == "all" ]]; then
    for variant in "${IMPLEMENTED_VARIANTS[@]}"; do
        run_variant_analysis "$variant"
    done
else
    run_variant_analysis "$MUL_VARIANT"
fi
