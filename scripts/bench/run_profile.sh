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
#   scripts/bench/run_profile.sh --cache --variant karatsuba_ref --level 128 --iter 1
#   scripts/bench/run_profile.sh --memory --variant hybrid --level 192 --iter 1
#   scripts/bench/run_profile.sh --trace --elf build/spike/bench_polymul_hqc128_karatsuba_zbc.elf
#   scripts/bench/run_profile.sh --histogram --bench-set polymul_zbc --wait

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${SCRIPT_DIR}/lib_bench.sh"

MODE=""
VARIANT=""
LEVEL="128"
ITER="1"
ELF=""
BENCH_SET=""
WAIT_FLAG=0
REPORT_FLAG=0
TOP_N=""
COMPARE_WITH=""
CONTEXT_FLAG=0

# Histogram bench-set -> output directory track mapping.
declare -A BENCH_SET_TRACK_MAP
BENCH_SET_TRACK_MAP=(
    [baseline_ref]="scalar"
    [polymul_zbc]="scalar"
    [fafft_zbc]="scalar"
    [hybrid]="scalar"
    [hybrid_zbc]="scalar"
    [hybrid_zbc_v2]="scalar"
    [hybrid_2way_zbc]="scalar"
    [polymul_zvbc]="vector"
    [polymul_zvbc_v2]="vector"
    [fafft_zvbc]="vector"
)

profile_cache_mode() {
    local variant="${VARIANT:-karatsuba_ref}"
    local level="$LEVEL"
    local iter="$ITER"
    local dc_config="${DC_CONFIG:-512:8:64}"

    if ! [[ "$level" =~ ^(128|192|256)$ ]]; then
        echo "[profile_cache_spike] ERROR: level must be 128, 192, or 256" >&2
        exit 2
    fi
    if ! [[ "$iter" =~ ^[1-9][0-9]*$ ]]; then
        echo "[profile_cache_spike] ERROR: iter must be a positive integer" >&2
        exit 2
    fi

    local riscv="${RISCV:-$HOME/Thesis/toolchains/installed-tools}"
    local spike="${riscv}/bin/spike"
    local isa="${ISA:-rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm}"

    if [[ ! -x "$spike" ]]; then
        echo "[profile_cache_spike] ERROR: spike not found at $spike" >&2
        exit 2
    fi

    local build_dir="$REPO_ROOT/build/spike_cacheprof_hqc${level}_${variant}"
    local target="bench_polymul_hqc${level}_${variant}"
    local elf="$build_dir/${target}.elf"

    cmake -S "$REPO_ROOT" -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
        -DRISCV="$riscv" \
        -DHQC_SIM=spike \
        -DBENCH_N_ITER="$iter" >/dev/null

    cmake --build "$build_dir" --target "$target" -j"$(nproc)" >/dev/null

    local out_dir="$REPO_ROOT/analysis/benchmarks/memory_profile"
    mkdir -p "$out_dir"

    local stamp="$(date +%Y%m%d_%H%M%S)"
    local run_mode="direct"
    local raw_stdout="$out_dir/cache_spike_stdout_hqc${level}_${variant}_${stamp}.log"
    local raw_stderr="$out_dir/cache_spike_stderr_hqc${level}_${variant}_${stamp}.log"
    local csv_out="$out_dir/cache_profile_hqc${level}_${variant}.csv"

    "$spike" --dc="$dc_config" --isa="$isa" "$elf" >"$raw_stdout" 2>"$raw_stderr"

    extract_metric() {
        local pattern="$1"
        local file="$2"
        awk -F':' -v p="$pattern" '$1 ~ p {gsub(/[[:space:]]/, "", $2); print $2; exit}' "$file"
    }

    local read_misses write_misses writebacks dc_misses
    read_misses="$(extract_metric 'D\\$ Read Misses' "$raw_stdout")"
    write_misses="$(extract_metric 'D\\$ Write Misses' "$raw_stdout")"
    writebacks="$(extract_metric 'D\\$ Writebacks' "$raw_stdout")"

    if [[ -z "$read_misses" || -z "$write_misses" || -z "$writebacks" ]]; then
        echo "[profile_cache_spike] ERROR: failed to parse D$ cache metrics from $raw_stdout" >&2
        echo "[profile_cache_spike] tail of output:" >&2
        tail -n 40 "$raw_stdout" >&2 || true
        exit 1
    fi

    dc_misses=$((read_misses + write_misses))

    if [[ ! -f "$csv_out" ]]; then
        echo "timestamp,level,variant,iter,dc_config,dc_misses,dc_read_misses,dc_write_misses,dc_writebacks,run_mode,elf,stdout_log,stderr_log" > "$csv_out"
    fi

    echo "${stamp},${level},${variant},${iter},${dc_config},${dc_misses},${read_misses},${write_misses},${writebacks},${run_mode},${elf},${raw_stdout},${raw_stderr}" >> "$csv_out"

    echo "[profile_cache_spike] variant=${variant} level=${level} iter=${iter} dc=${dc_config} run_mode=${run_mode}"
    echo "[profile_cache_spike] D$ Misses=${dc_misses} (Read=${read_misses}, Write=${write_misses})"
    echo "[profile_cache_spike] D$ Writebacks=${writebacks}"
    echo "[profile_cache_spike] csv=${csv_out}"
    echo "[profile_cache_spike] stdout_log=${raw_stdout}"
}

profile_memory_mode() {
    local variant="${VARIANT:-karatsuba_ref}"
    local level="$LEVEL"
    local iter="$ITER"

    if ! [[ "$level" =~ ^(128|192|256)$ ]]; then
        echo "[profile_memory] ERROR: level must be 128, 192, or 256" >&2
        exit 2
    fi

    local riscv="${RISCV:-$HOME/Thesis/toolchains/installed-tools}"
    local qemu="${riscv}/bin/qemu-system-riscv64"

    if [[ ! -x "$qemu" ]]; then
        echo "[profile_memory] ERROR: qemu-system-riscv64 not found at $qemu" >&2
        exit 2
    fi

    local build_dir="$REPO_ROOT/build/qemu_memprof_hqc${level}_${variant}"
    local target="bench_polymul_hqc${level}_${variant}"
    local elf="$build_dir/${target}.elf"

    cmake -S "$REPO_ROOT" -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
        -DHQC_SIM=qemu \
        -DBENCH_N_ITER="$iter" >/dev/null

    cmake --build "$build_dir" --target "$target" -j"$(nproc)" >/dev/null

    local stamp="$(date +%Y%m%d_%H%M%S)"
    local out_dir="$REPO_ROOT/analysis/benchmarks/memory_profile"
    mkdir -p "$out_dir"
    local qemu_log="$out_dir/qemu_trace_hqc${level}_${variant}_${stamp}.log"
    local uart_log="$out_dir/qemu_stdout_hqc${level}_${variant}_${stamp}.txt"
    local csv_out="$out_dir/memory_profile_hqc${level}_${variant}_${stamp}.csv"

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
        -D "$qemu_log" > "$uart_log" 2>&1

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

    echo "level,variant,iter,load_instructions,store_instructions,load_bytes,store_bytes,est_cache_lines_load,est_cache_lines_store,qemu_log,uart_log" > "$csv_out"
    echo "${level},${variant},${iter},${load_ins},${store_ins},${load_bytes},${store_bytes},${load_lines},${store_lines},${qemu_log},${uart_log}" >> "$csv_out"

    echo "[profile_memory] variant=${variant} level=${level} iter=${iter}"
    echo "[profile_memory] load_instructions=${load_ins} store_instructions=${store_ins}"
    echo "[profile_memory] load_bytes=${load_bytes} store_bytes=${store_bytes}"
    echo "[profile_memory] est_cache_lines_load=${load_lines} est_cache_lines_store=${store_lines}"
    echo "[profile_memory] csv=${csv_out}"
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
    outdir="$REPO_ROOT/analysis/benchmarks/hotspot_profile/${basename}"
    mix_dir="$REPO_ROOT/analysis/benchmarks/instruction_mix"

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
    if [[ -z "$BENCH_SET" ]]; then
        echo "ERROR: --histogram requires --bench-set <set>" >&2
        exit 2
    fi

    bench_load_implemented_variants "$REPO_ROOT"

    local riscv="${RISCV:-$HOME/Thesis/installed-tools}"
    local spike="${riscv}/bin/spike"
    local isa="rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm"
    local build_dir="${REPO_ROOT}/build/spike"
    local levels=(128 192 256)
    local mode="launch"

    if [[ "$REPORT_FLAG" -eq 1 ]]; then
        mode="report"
    elif [[ "$WAIT_FLAG" -eq 1 ]]; then
        mode="wait"
    fi

    local bench_track="${BENCH_SET_TRACK_MAP[$BENCH_SET]:-}"
    local out_dir
    if [[ -n "$bench_track" ]]; then
        out_dir="${REPO_ROOT}/analysis/benchmarks/${bench_track}/${BENCH_SET}"
    else
        out_dir="${REPO_ROOT}/analysis/benchmarks/${BENCH_SET}"
    fi

    # Report flow is handled directly in this unified entrypoint.
    if [[ "$mode" == "report" ]]; then
        if [[ ! -d "$out_dir" ]]; then
            echo "[histogram] No benchmark-set directory found: $out_dir" >&2
            echo "            Run --histogram --bench-set $BENCH_SET first to generate files." >&2
            return 0
        fi

        local found=0
        for level in "${levels[@]}"; do
            for variant in "${IMPLEMENTED_VARIANTS[@]}"; do
                bench_variant_matches "$variant" "$VARIANT" || continue
                local hist="${out_dir}/hqc${level}_${variant}_hist.txt"
                if [[ ! -f "$hist" ]]; then
                    continue
                fi
                found=$((found + 1))
                echo "=== hqc${level}_${variant} (${BENCH_SET}) ==="
                head -n "${TOP_N:-20}" "$hist" || true
                if [[ "$CONTEXT_FLAG" -eq 1 ]]; then
                    local elf_ctx="${build_dir}/bench_polymul_hqc${level}_${variant}.elf"
                    if [[ -f "$elf_ctx" ]]; then
                        echo "[histogram] context requested; run run_profile.sh --histogram --bench-set ${BENCH_SET} --report --context for full context output."
                    fi
                fi
                echo ""
            done
        done
        if [[ "$found" -eq 0 ]]; then
            echo "[histogram] No histogram files found in ${out_dir}/"
        fi
        return 0
    fi

    if ! bench_require_executable "$spike" "Spike not found"; then
        echo "       Source toolchain/toolchain.env or set RISCV" >&2
        exit 2
    fi

    mkdir -p "$out_dir"

    local launched=0
    local -a job_lines=()
    local -a job_pids=()

    for level in "${levels[@]}"; do
        for variant in "${IMPLEMENTED_VARIANTS[@]}"; do
            bench_variant_matches "$variant" "$VARIANT" || continue

            local elf="${build_dir}/bench_polymul_hqc${level}_${variant}.elf"
            local target="bench_polymul_hqc${level}_${variant}"
            local prefix="hqc${level}_${variant}"
            local csv="${out_dir}/${prefix}_polymul_N1.csv"
            local hist="${out_dir}/${prefix}_hist.txt"

            bench_build_target_if_configured "$build_dir" "$target" "histogram" || continue
            if ! bench_require_elf_or_hint "$elf" "$target" \
                "cmake -S . -B build/spike -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake -DHQC_SIM=spike -DBENCH_N_ITER=1" \
                "histogram"; then
                continue
            fi

            nohup "$spike" --isa="$isa" -g "$elf" > "$csv" 2> "$hist" &
            local pid=$!
            job_pids+=("$pid")
            job_lines+=("  PID ${pid}  ${target}  ->  $(basename "$csv") + $(basename "$hist")")
            launched=$((launched + 1))
        done
    done

    echo ""
    if [[ "$launched" -eq 0 ]]; then
        echo "[histogram] No jobs launched. Check that polymul ELFs are built in ${build_dir}/"
        echo "            cmake --build build/spike -j$(nproc)"
        return 0
    fi

    echo "[histogram] Launched ${launched} Spike -g job(s) for ${BENCH_SET}:"
    printf "%s\n" "${job_lines[@]}"
    echo ""

    if [[ "$mode" == "wait" ]]; then
        echo "[histogram] Waiting for ${launched} job(s) to complete..."
        for pid in "${job_pids[@]}"; do
            wait "$pid" || true
        done
        echo "[histogram] All jobs done."
    else
        echo "Monitor:"
        echo "  ps aux | grep '[s]pike.*bench_polymul'"
        echo "  watch -n 30 \"wc -c ${out_dir}/*_hist.txt\""
    fi
}

usage() {
    cat <<'EOF'
Usage:
    run_profile.sh --cache [--variant V] [--level 128|192|256] [--iter N]
    run_profile.sh --memory [--variant V] [--level 128|192|256] [--iter N]
    run_profile.sh --trace --elf <path-to-elf>
    run_profile.sh --histogram --bench-set <set> [--variant regex] [--wait|--report]
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
        --iter)
            ITER="$2"
            shift 2
            ;;
        --elf)
            ELF="$2"
            shift 2
            ;;
        --bench-set)
            BENCH_SET="$2"
            shift 2
            ;;
        --wait)
            WAIT_FLAG=1
            shift
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

case "$MODE" in
    cache)
        profile_cache_mode
        ;;
    memory)
        profile_memory_mode
        ;;
    trace)
        profile_trace_mode
        ;;
    histogram)
        profile_histogram_mode
        ;;
esac
