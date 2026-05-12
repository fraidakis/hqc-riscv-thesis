#!/usr/bin/env bash
# scripts/bench/run_profile.sh
# Unified profiling entry point.
#
# Modes:
#   --cache      Spike D$ profiling
#   --memory     QEMU memory-traffic profiling
#   --trace      Spike trace + hotspot + instruction mix
#   --histogram  Spike PC histogram pipeline
#
# Examples:
#   scripts/bench/run_profile.sh --cache --variant karatsuba_ref --level 128
#   scripts/bench/run_profile.sh --memory --variant hybrid --level 192
#   scripts/bench/run_profile.sh --trace --elf build/spike/bench_polymul_hqc128_karatsuba_zbc.elf
#   scripts/bench/run_profile.sh --histogram --variant karatsuba_zbc --report

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${SCRIPT_DIR}/lib_bench.sh"
bench_load_implemented_variants "$REPO_ROOT"
GIT_HASH="$(git rev-parse --short HEAD 2>/dev/null || echo 'unknown')"

MODE=""
VARIANT=""
LEVEL="128"
ELF=""
REPORT_FLAG=0
TOP_N=""
COMPARE_WITH=""
CONTEXT_FLAG=0

format_int() {
    local n="${1:-0}"
    if [[ ! "$n" =~ ^[0-9]+$ ]]; then
        echo "$n"
        return
    fi
    awk -v n="$n" 'BEGIN {
        out = ""
        while (n >= 1000) {
            out = sprintf(",%03d", n % 1000) out
            n = int(n / 1000)
        }
        printf "%d%s", n, out
    }'
}

profile_cache_mode() {
    local variant="${VARIANT:-karatsuba_ref}"
    local dc_config="${DC_CONFIG:-512:8:64}"

    local riscv="${RISCV:-$HOME/Thesis/toolchains/installed-tools}"
    local spike="${riscv}/bin/spike"
    local isa="${ISA:-rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm}"

    if [[ ! -x "$spike" ]]; then
        echo "[profile_cache_spike] ERROR: spike not found at $spike" >&2
        exit 2
    fi

    local track="scalar"
    if [[ "$variant" == *"zvbc"* ]]; then track="vector"; fi
    local out_dir="$REPO_ROOT/analysis/profiles/cache/${track}/${variant}"
    mkdir -p "$out_dir"

    local BENCH_TIME
    BENCH_TIME="$(date +'%Y-%m-%d_%H-%M')"
    local csv_out="$out_dir/${BENCH_TIME}.csv"

    echo "# date: $(date +'%Y-%m-%d %H:%M')" > "$csv_out"
    echo "# commit: ${GIT_HASH}" >> "$csv_out"
    echo "# variant: ${variant}" >> "$csv_out"
    echo "# cache_config: ${dc_config}" >> "$csv_out"
    echo "dc_misses,dc_read_misses,dc_write_misses,dc_writebacks,dc_bytes_read,dc_bytes_written,dc_read_accesses,dc_write_accesses" >> "$csv_out"

    echo -e "\n📦 ${variant} (cache)"

    extract_metric() {
        local pattern="$1"
        local file="$2"
        awk -F':' -v p="$pattern" '$1 ~ p {gsub(/[[:space:]]/, "", $2); print $2; exit}' "$file"
    }

    # Loop through all levels
    for level in 128 192 256; do
        local build_dir="$REPO_ROOT/build/spike_cacheprof_hqc${level}_${variant}"
        local target="bench_polymul_hqc${level}_${variant}"
        local elf="$build_dir/${target}.elf"

        cmake -S "$REPO_ROOT" -B "$build_dir" \
            -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
            -DRISCV="$riscv" \
            -DHQC_SIM=spike >/dev/null 2>&1

        cmake --build "$build_dir" --target "$target" -j"$(nproc)" >/dev/null 2>&1

        local raw_stdout="$out_dir/cache_spike_stdout_hqc${level}_${variant}_${BENCH_TIME}.log"
        local raw_stderr="$out_dir/cache_spike_stderr_hqc${level}_${variant}_${BENCH_TIME}.log"

        "$spike" --dc="$dc_config" --isa="$isa" "$elf" >"$raw_stdout" 2>"$raw_stderr"

        local dc_read_misses dc_write_misses dc_writebacks dc_misses
        local dc_bytes_read dc_bytes_written dc_read_accesses dc_write_accesses
        
        dc_read_misses="$(extract_metric 'D\\$ Read Misses' "$raw_stdout")"
        dc_write_misses="$(extract_metric 'D\\$ Write Misses' "$raw_stdout")"
        dc_writebacks="$(extract_metric 'D\\$ Writebacks' "$raw_stdout")"
        dc_bytes_read="$(extract_metric 'D\\$ Bytes Read' "$raw_stdout")"
        dc_bytes_written="$(extract_metric 'D\\$ Bytes Written' "$raw_stdout")"
        dc_read_accesses="$(extract_metric 'D\\$ Read Accesses' "$raw_stdout")"
        dc_write_accesses="$(extract_metric 'D\\$ Write Accesses' "$raw_stdout")"

        if [[ -z "$dc_read_misses" || -z "$dc_write_misses" || -z "$dc_writebacks" ]]; then
            echo "[profile_cache_spike] ERROR: failed to parse D$ cache metrics from $raw_stdout" >&2
            echo "[profile_cache_spike] tail of output:" >&2
            tail -n 40 "$raw_stdout" >&2 || true
            exit 1
        fi

        dc_misses=$((dc_read_misses + dc_write_misses))

        echo "${dc_misses},${dc_read_misses},${dc_write_misses},${dc_writebacks},${dc_bytes_read},${dc_bytes_written},${dc_read_accesses},${dc_write_accesses}" >> "$csv_out"
        local branch="├──"
        if [[ "$level" == "256" ]]; then branch="└──"; fi
        echo " ${branch} L${level} ⋯ $(format_int "$dc_misses") misses (R=$(format_int "$dc_read_misses"), W=$(format_int "$dc_write_misses")) | WB=$(format_int "$dc_writebacks")"

        # Auto-delete raw log files
        rm -f "$raw_stdout" "$raw_stderr"
    done

    if bench_refresh_tracked_csv "$csv_out" "${out_dir}/latest.csv"; then
        echo "↻ Refreshed: ${out_dir}/latest.csv"
    else
        echo "↻ Unchanged payload: ${out_dir}/latest.csv"
    fi
    echo ""
    echo "✔️ Saved: ${csv_out}"
}

profile_memory_mode() {
    local variant="${VARIANT:-karatsuba_ref}"

    local riscv="${RISCV:-$HOME/Thesis/toolchains/installed-tools}"
    local qemu="${riscv}/bin/qemu-system-riscv64"

    if [[ ! -x "$qemu" ]]; then
        echo "[profile_memory] ERROR: qemu-system-riscv64 not found at $qemu" >&2
        exit 2
    fi

    local track="scalar"
    if [[ "$variant" == *"zvbc"* ]]; then track="vector"; fi
    local out_dir="$REPO_ROOT/analysis/profiles/memory/${track}/${variant}"
    mkdir -p "$out_dir"

    local BENCH_TIME
    BENCH_TIME="$(date +'%Y-%m-%d_%H-%M')"
    local csv_out="$out_dir/${BENCH_TIME}.csv"

    echo "# date: $(date +'%Y-%m-%d %H:%M')" > "$csv_out"
    echo "# commit: ${GIT_HASH}" >> "$csv_out"
    echo "# variant: ${variant}" >> "$csv_out"
    echo "load_instructions,store_instructions,load_bytes,store_bytes,est_cache_lines_load,est_cache_lines_store" >> "$csv_out"

    echo -e "\n📦 ${variant} (memory)"

    # Loop through all levels
    for level in 128 192 256; do
        local build_dir="$REPO_ROOT/build/qemu_memprof_hqc${level}_${variant}"
        local target="bench_polymul_hqc${level}_${variant}"
        local elf="$build_dir/${target}.elf"

        cmake -S "$REPO_ROOT" -B "$build_dir" \
            -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
            -DHQC_SIM=qemu >/dev/null 2>&1

        cmake --build "$build_dir" --target "$target" -j"$(nproc)" >/dev/null 2>&1

        local qemu_log="$out_dir/qemu_trace_hqc${level}_${variant}_${BENCH_TIME}.log"

        "$qemu" \
            -machine virt \
            -cpu rv64,v=true,vlen=256,elen=64,vext_spec=v1.0,zbkb=true,zbc=true,zvbc=true \
            -m 256M \
            -nographic \
            -bios none \
            -kernel "$elf" \
            -serial mon:stdio \
            -no-reboot \
            -d in_asm,nochain \
            -D "$qemu_log" >/dev/null 2>&1

        local load_bytes store_bytes load_ins store_ins
        read -r load_bytes store_bytes load_ins store_ins <<<"$(awk '
function add_load(bytes){lb+=bytes;li++}
function add_store(bytes){sb+=bytes;si++}
{
    line=tolower($0)
    if (line ~ /\<vle8\./) { add_load(8); next }
    if (line ~ /\<vle16\./) { add_load(16); next }
    if (line ~ /\<vle32\./) { add_load(32); next }
    if (line ~ /\<vle64\./) { add_load(64); next }
    if (line ~ /\<vse8\./) { add_store(8); next }
    if (line ~ /\<vse16\./) { add_store(16); next }
    if (line ~ /\<vse32\./) { add_store(32); next }
    if (line ~ /\<vse64\./) { add_store(64); next }

    if (line ~ /\<lb\>/ || line ~ /\<lbu\>/) { add_load(1); next }
    if (line ~ /\<lh\>/ || line ~ /\<lhu\>/) { add_load(2); next }
    if (line ~ /\<lw\>/ || line ~ /\<lwu\>/ || line ~ /\<flw\>/) { add_load(4); next }
    if (line ~ /\<ld\>/ || line ~ /\<fld\>/) { add_load(8); next }

    if (line ~ /\<sb\>/) { add_store(1); next }
    if (line ~ /\<sh\>/) { add_store(2); next }
    if (line ~ /\<sw\>/ || line ~ /\<fsw\>/) { add_store(4); next }
    if (line ~ /\<sd\>/ || line ~ /\<fsd\>/) { add_store(8); next }
}
END{printf "%d %d %d %d", lb, sb, li, si}
' "$qemu_log")"

        local load_lines store_lines
        load_lines=$(( (load_bytes + 63) / 64 ))
        store_lines=$(( (store_bytes + 63) / 64 ))

        echo "${load_ins},${store_ins},${load_bytes},${store_bytes},${load_lines},${store_lines}" >> "$csv_out"
        local branch="├──"
        if [[ "$level" == "256" ]]; then branch="└──"; fi
        echo " ${branch} L${level} ⋯ LdIns=$(format_int "$load_ins") StIns=$(format_int "$store_ins") | LdB=$(format_int "$load_bytes") StB=$(format_int "$store_bytes")"

        # Auto-delete the massive trace file
        rm -f "$qemu_log"
    done

    if bench_refresh_tracked_csv "$csv_out" "${out_dir}/latest.csv"; then
        echo "↻ Refreshed: ${out_dir}/latest.csv"
    else
        echo "↻ Unchanged payload: ${out_dir}/latest.csv"
    fi
    echo ""
    echo "✔️ Saved: ${csv_out}"
}

profile_trace_mode() {
    local elf="$ELF"
    local basename outdir mix_dir mix_csv

    if [[ -z "$elf" ]]; then
        echo "ERROR: --trace requires --elf <path>" >&2
        exit 2
    fi
    if [[ ! -f "$elf" ]]; then
        echo "ERROR: ELF not found: $elf" >&2
        exit 2
    fi

    basename="$(basename "$elf" .elf)"
    outdir="$REPO_ROOT/analysis/profiles/traces/hotspots/${basename}"
    mix_dir="$REPO_ROOT/analysis/profiles/traces/instruction_mix"

    mix_csv="${mix_dir}/mix_${basename}.csv"
    if [[ "$basename" =~ hqc(128|192|256)_([a-zA-Z0-9_]+)$ ]]; then
        local parsed_level parsed_variant
        parsed_level="${BASH_REMATCH[1]}"
        parsed_variant="${BASH_REMATCH[2]}"
        mix_csv="${mix_dir}/mix_hqc${parsed_level}_${parsed_variant}.csv"
    fi

    mkdir -p "$outdir" "$mix_dir"

    if [[ -z "${RISCV:-}" ]]; then
        echo "Please source toolchain/toolchain.env first" >&2
        exit 1
    fi

    echo "=========================================="
    echo "1. Generating trace for $basename..."
    echo "=========================================="
    "$RISCV/bin/spike" -l --isa=rv64gcv_zbc_zbb_zbkb_zvbc "$elf" 2> "$outdir/spike_trace.log"

    echo "=========================================="
    echo "2. Extracting PCs..."
    echo "=========================================="
    grep "core   0:" "$outdir/spike_trace.log" | awk '{print $3}' | sed 's/^(//' | sed 's/)//' | sort | uniq -c > "$outdir/pc_counts_all.txt"

    echo "=========================================="
    echo "3. Mapping PCs to C functions..."
    echo "=========================================="
    python3 "$REPO_ROOT/scripts/report/map_pcs.py" "$elf" "$outdir"
    rm -f "$outdir/pc_counts_all.txt"

    echo "=========================================="
    echo "4. Building instruction mix CSV..."
    echo "=========================================="
    python3 "$REPO_ROOT/scripts/report/generate_instruction_mix.py" \
        "$outdir/spike_trace.log" \
        --label "$basename" \
        --output "$mix_csv"

    echo "=========================================="
    echo "Done. Results saved to $outdir/"
    echo "Top 10 Functions by executed instructions:"
    echo "=========================================="
    cat "$outdir/final_function_counts.txt" | head -n 11
    echo "Instruction mix CSV: $mix_csv"
    rm -f "$outdir/spike_trace.log"
}

profile_histogram_mode() {
    if [[ -z "$VARIANT" ]]; then
        echo "ERROR: --histogram requires --variant <variant>" >&2
        exit 2
    fi

    bench_load_implemented_variants "$REPO_ROOT"

    local riscv="${RISCV:-$HOME/Thesis/installed-tools}"
    local spike="${riscv}/bin/spike"
    local isa="rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm"
    local build_dir="${REPO_ROOT}/build/spike"
    local levels=(128 192 256)

    local BENCH_TIME
    BENCH_TIME="$(date +'%Y-%m-%d_%H-%M')"

    local track
    if [[ "${VARIANT,,}" == *"zvbc"* ]]; then
        track="vector"
    else
        track="scalar"
    fi
    local variant="${VARIANT:-karatsuba_ref}"
    local out_dir="${REPO_ROOT}/analysis/profiles/histograms/${track}/${variant}"

    if ! bench_require_executable "$spike" "Spike not found"; then
        echo "       Source toolchain/toolchain.env or set RISCV" >&2
        exit 2
    fi

    mkdir -p "$out_dir"

    echo -e "\n📦 ${variant} (histogram)"

    for level in "${levels[@]}"; do
        local target="bench_polymul_hqc${level}_${variant}"
        bench_build_target_if_configured "$build_dir" "$target" "histogram" >/dev/null 2>&1 || true

        local elf="${build_dir}/${target}.elf"
        if [[ ! -f "$elf" ]]; then
            echo "ERROR: Missing ELF $elf" >&2
            continue
        fi

        local hist="${out_dir}/hqc${level}_${variant}_hist.txt"
        local mix_file="${out_dir}/hqc${level}_${BENCH_TIME}.txt"
        local objdump_bin="${RISCV:-$HOME/Thesis/installed-tools}/bin/llvm-objdump"

        "$spike" --isa="$isa" -g "$elf" >/dev/null 2> "$hist"

        # Parse mnemonics and auto-delete raw log
        python3 "$REPO_ROOT/scripts/report/parse_histogram.py" "$hist" "$elf" "$objdump_bin" > "$mix_file"
        rm -f "$hist"
        cp -f "$mix_file" "${out_dir}/hqc${level}_latest.txt"
        local prefix=" ├──"
        if [[ "$level" == "256" ]]; then prefix=" └──"; fi
        echo "${prefix} L${level} ⋯ $(basename "$mix_file")"
        if [[ "$REPORT_FLAG" -eq 1 ]]; then
            cat "$mix_file" | head -n "$(( ${TOP_N:-20} + 3 ))"
            echo ""
        fi
    done

    echo ""
    echo "✔️ Saved: ${out_dir}"
}

usage() {
    cat <<'EOF'
Usage:
    run_profile.sh --cache [--variant V] [--level 128|192|256]
    run_profile.sh --memory [--variant V] [--level 128|192|256]
    run_profile.sh --trace --elf <path-to-elf>
    run_profile.sh --histogram --variant <variant> [--report]
             [--top N] [--compare-with set] [--context]
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --cache|--memory|--trace|--histogram)
            MODE="${1#--}"
            shift
            ;;
        --variant)
            VARIANT="$2"
            shift 2
            ;;
        --level)
            LEVEL="$2"
            shift 2
            ;;
        --elf)
            ELF="$2"
            shift 2
            ;;
        --report)
            REPORT_FLAG=1
            shift
            ;;
        --top)
            TOP_N="$2"
            shift 2
            ;;
        --compare-with)
            COMPARE_WITH="$2"
            shift 2
            ;;
        --context)
            CONTEXT_FLAG=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "ERROR: unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ -z "$MODE" ]]; then
    echo "ERROR: one of --cache|--memory|--trace|--histogram is required" >&2
    usage >&2
    exit 2
fi

run_mode_for_variant() {
    VARIANT="$1"
    case "$MODE" in
        cache)     profile_cache_mode ;;
        memory)    profile_memory_mode ;;
        trace)     profile_trace_mode ;;
        histogram) profile_histogram_mode ;;
    esac
}

if [[ "$VARIANT" == "all" ]]; then
    for v in "${IMPLEMENTED_VARIANTS[@]}"; do
        run_mode_for_variant "$v"
    done
else
    run_mode_for_variant "${VARIANT:-karatsuba_ref}"
fi
