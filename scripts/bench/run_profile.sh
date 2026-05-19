#!/usr/bin/env bash
# scripts/bench/run_profile.sh
# Unified profiling entry point.
#
# Modes:
#   --cache      Spike D$ profiling
#   --memory     QEMU memory-traffic profiling
#   --trace      Spike trace + hotspot mapping
#   --histogram  Spike PC histogram pipeline
#
# Examples:
#   scripts/bench/run_profile.sh --cache --mul karatsuba_zbc --level 128
#   scripts/bench/run_profile.sh --memory --mul hybrid --level 192
#   scripts/bench/run_profile.sh --trace --mul karatsuba_zbc
#   scripts/bench/run_profile.sh --histogram --mul karatsuba_zbc --report

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${SCRIPT_DIR}/lib_bench.sh"
bench_load_implemented_variants "$REPO_ROOT"
GIT_HASH="$(git rev-parse --short HEAD 2>/dev/null || echo 'unknown')"
BENCH_TIME="$(date +'%Y-%m-%d_%H-%M')"
PROFILE_TMP_ROOT="$REPO_ROOT/.tmp"

# Populate MUL_VARIANT, KECCAK_IMPL, DECODE_IMPL via shared auto-mapping.
bench_parse_track_args "$@"

MODE=""
LEVEL="128"
LEVEL_SET=0
REPORT_FLAG=0
TOP_N=""
COMPARE_WITH=""
CONTEXT_FLAG=0
PROFILE_POLYMUL=0
PROFILE_LINES=0

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

bench_print_variant_header() {
    local variant="$1"
    local mode_label="$2"
    echo -e "\n📦 ${variant} (${mode_label})"
}

bench_print_level_branch() {
    local level="$1"
    local idx="$2"
    local total_levels="$3"
    local status="$4"
    local branch="├──"
    if [[ "$idx" -eq "$total_levels" ]]; then
        branch="└──"
    fi
    echo " ${branch} L${level} ⋯ ${status}"
}

profile_cache_mode() {
    local variant="${MUL_VARIANT:-karatsuba_zbc}"
    local dc_config="${DC_CONFIG:-512:8:64}"
    local profile_kind="KEM"
    local target_prefix="bench"
    local out_prefix="kem"
    if [[ "$PROFILE_POLYMUL" == "1" ]]; then
        profile_kind="PolyMul"
        target_prefix="bench_polymul"
        out_prefix="poly"
    fi

    local riscv="${RISCV:-$HOME/Thesis/toolchains/installed-tools}"
    local spike="${riscv}/bin/spike"
    local isa="${ISA:-rv64gcv_zbc_zbb_zbkb_zvbc_zvbb_zvl256b_zicntr_zihpm}"

    if [[ ! -x "$spike" ]]; then
        echo "[profile_cache_spike] ERROR: spike not found at $spike" >&2
        exit 2
    fi

    local track="scalar"
    if [[ "$variant" == *"zvbc"* || "$variant" == *"rvv"* ]]; then track="vector"; fi
    local out_dir="$REPO_ROOT/analysis/profiles/cache/${track}/${variant}"
    mkdir -p "$out_dir"

    local BENCH_TIME
    BENCH_TIME="$(date +'%Y-%m-%d_%H-%M')"
    local csv_out="$out_dir/${out_prefix}_${BENCH_TIME}.csv"

    echo "# date: $(date +'%Y-%m-%d %H:%M')" > "$csv_out"
    echo "# commit: ${GIT_HASH}" >> "$csv_out"
    echo "# variant: ${variant}" >> "$csv_out"
    echo "# cache_config: ${dc_config}" >> "$csv_out"
    echo "level,dc_misses,dc_read_misses,dc_write_misses,dc_writebacks,dc_bytes_read,dc_bytes_written,dc_read_accesses,dc_write_accesses" >> "$csv_out"

    echo -e "\n📦 ${variant} (cache - ${profile_kind})"

    extract_metric() {
        local pattern="$1"
        local file="$2"
        awk -F':' -v p="$pattern" '$1 ~ p {gsub(/[[:space:]]/, "", $2); print $2; exit}' "$file"
    }

    local target_levels=()
    if [[ "$LEVEL" == "all" ]]; then
        target_levels=(128 192 256)
    else
        target_levels=("$LEVEL")
    fi

    # Isolated build dir per impl pairing — configure once, build per target.
    local build_dir="$REPO_ROOT/build/spike/cacheprof_k${KECCAK_IMPL:-ref}_d${DECODE_IMPL:-ref}"
    cmake -S "$REPO_ROOT" -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
        -DRISCV="$riscv" \
        -DHQC_SIM=spike >/dev/null 2>&1

    local total_levels="${#target_levels[@]}"
    local idx=0
    for level in "${target_levels[@]}"; do
        idx=$((idx + 1))
        local target="${target_prefix}_hqc${level}_${variant}"
        local elf="$build_dir/${target}.elf"

        cmake --build "$build_dir" --target "$target" -j"$(nproc)" >/dev/null 2>&1

        local raw_stdout="$out_dir/${out_prefix}_cache_spike_stdout_hqc${level}_${variant}_${BENCH_TIME}.log"
        local raw_stderr="$out_dir/${out_prefix}_cache_spike_stderr_hqc${level}_${variant}_${BENCH_TIME}.log"

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

        echo "${level},${dc_misses},${dc_read_misses},${dc_write_misses},${dc_writebacks},${dc_bytes_read},${dc_bytes_written},${dc_read_accesses},${dc_write_accesses}" >> "$csv_out"
        local branch="├──"
        if [[ "$idx" -eq "$total_levels" ]]; then branch="└──"; fi
        echo " ${branch} L${level} ⋯ $(format_int "$dc_misses") misses (R=$(format_int "$dc_read_misses"), W=$(format_int "$dc_write_misses")) | WB=$(format_int "$dc_writebacks")"

        # Auto-delete raw log files
        rm -f "$raw_stdout" "$raw_stderr"
    done

    bench_refresh_tracked_csv "$csv_out" "${out_dir}/${out_prefix}_latest.csv" || true
    echo ""
}

profile_memory_mode() {
    local variant="${MUL_VARIANT:-karatsuba_zbc}"
    local profile_kind="KEM"
    local target_prefix="bench"
    local out_prefix="kem"
    if [[ "$PROFILE_POLYMUL" == "1" ]]; then
        profile_kind="PolyMul"
        target_prefix="bench_polymul"
        out_prefix="poly"
    fi

    local riscv="${RISCV:-$HOME/Thesis/toolchains/installed-tools}"
    local qemu="${riscv}/bin/qemu-system-riscv64"

    if [[ ! -x "$qemu" ]]; then
        echo "[profile_memory] ERROR: qemu-system-riscv64 not found at $qemu" >&2
        exit 2
    fi

    local track="scalar"
    if [[ "$variant" == *"zvbc"* || "$variant" == *"rvv"* ]]; then track="vector"; fi
    local out_dir="$REPO_ROOT/analysis/profiles/memory/${track}/${variant}"
    mkdir -p "$out_dir"

    local BENCH_TIME
    BENCH_TIME="$(date +'%Y-%m-%d_%H-%M')"
    local csv_out="$out_dir/${out_prefix}_${BENCH_TIME}.csv"

    echo "# date: $(date +'%Y-%m-%d %H:%M')" > "$csv_out"
    echo "# commit: ${GIT_HASH}" >> "$csv_out"
    echo "# variant: ${variant}" >> "$csv_out"
    echo "level,load_instructions,store_instructions,load_bytes,store_bytes,est_cache_lines_load,est_cache_lines_store" >> "$csv_out"

    echo -e "\n📦 ${variant} (memory - ${profile_kind})"

    local target_levels=()
    if [[ "$LEVEL" == "all" ]]; then
        target_levels=(128 192 256)
    else
        target_levels=("$LEVEL")
    fi

    # Isolated build dir per impl pairing — configure once, build per target.
    # Pass impl flags so the correct keccak/decode sources are compiled.
    local build_dir="$REPO_ROOT/build/qemu/memprof_k${KECCAK_IMPL:-ref}_d${DECODE_IMPL:-ref}"
    cmake -S "$REPO_ROOT" -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
        -DRISCV="${riscv}" \
        -DHQC_SIM=qemu \
        -DHQC_KECCAK_IMPL="${KECCAK_IMPL:-ref}" \
        -DHQC_DECODE_IMPL="${DECODE_IMPL:-ref}" >/dev/null 2>&1

    local total_levels="${#target_levels[@]}"
    local idx=0
    for level in "${target_levels[@]}"; do
        idx=$((idx + 1))
        local target="${target_prefix}_hqc${level}_${variant}"
        local elf="$build_dir/${target}.elf"

        cmake --build "$build_dir" --target "$target" -j"$(nproc)" >/dev/null 2>&1

        local qemu_log="$out_dir/${out_prefix}_qemu_trace_hqc${level}_${variant}_${BENCH_TIME}.log"

        "$qemu" \
            -machine virt \
            -cpu rv64,v=true,vlen=256,elen=64,vext_spec=v1.0,zbkb=true,zbc=true,zvbc=true,zvbb=true \
            -m 128M \
            -nographic \
            -monitor none \
            -bios none \
            -kernel "$elf" \
            -serial stdio \
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

        echo "${level},${load_ins},${store_ins},${load_bytes},${store_bytes},${load_lines},${store_lines}" >> "$csv_out"
        local branch="├──"
        if [[ "$idx" -eq "$total_levels" ]]; then branch="└──"; fi
        echo " ${branch} L${level} ⋯ LdIns=$(format_int "$load_ins") StIns=$(format_int "$store_ins") | LdB=$(format_int "$load_bytes") StB=$(format_int "$store_bytes")"

        # Auto-delete the massive trace file
        rm -f "$qemu_log"
    done

    bench_refresh_tracked_csv "$csv_out" "${out_dir}/${out_prefix}_latest.csv" || true
    echo ""
}

profile_trace_mode() {
    local variant="${MUL_VARIANT:-karatsuba_zbc}"
    local track="scalar"
    local profile_kind="KEM"
    local target_prefix="bench"
    local out_prefix="kem"
    local latest_status_tmp=""
    local trace_tmp_dir=""
    if [[ "$PROFILE_POLYMUL" == "1" ]]; then
        profile_kind="PolyMul"
        target_prefix="bench_polymul"
        out_prefix="poly"
    fi
    cleanup_profile_trace_mode() {
        rm -rf "${trace_tmp_dir:-}"
        rmdir "$PROFILE_TMP_ROOT" 2>/dev/null || true
        rm -f "${latest_status_tmp:-}"
    }
    local riscv="${RISCV:-$HOME/Thesis/toolchains/installed-tools}"
    local spike="${riscv}/bin/spike"
    local isa="${ISA:-rv64gcv_zbc_zbb_zbkb_zvbc_zvbb_zvl256b_zicntr_zihpm}"
    local build_dir="$REPO_ROOT/build/spike/trace"

    if [[ "$variant" == *"zvbc"* || "$variant" == *"rvv"* ]]; then
        track="vector"
    fi

    if [[ ! -x "$spike" ]]; then
        echo "ERROR: spike not found at $spike" >&2
        exit 2
    fi

    local out_dir="$REPO_ROOT/analysis/profiles/traces/${track}/${variant}"
    mkdir -p "$out_dir"
    mkdir -p "$PROFILE_TMP_ROOT"
    trace_tmp_dir="$PROFILE_TMP_ROOT/run_profile_trace_${variant}_$$"
    mkdir -p "$trace_tmp_dir"
    trap cleanup_profile_trace_mode EXIT INT TERM

    local map_pcs_lines_arg=""
    local trace_suffix="hotspots"
    if [[ "$PROFILE_LINES" == "1" ]]; then
        map_pcs_lines_arg="--lines"
        trace_suffix="lines"
    fi

    cmake -S "$REPO_ROOT" -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
        -DRISCV="$riscv" \
        -DHQC_SIM=spike \
        -DHQC_TRACE_MODE=1 \
        -DHQC_KECCAK_IMPL="${KECCAK_IMPL:-ref}" \
        -DHQC_DECODE_IMPL="${DECODE_IMPL:-ref}" >/dev/null 2>&1

    echo -e "\n📦 ${variant} (trace - ${profile_kind})"

    local target_levels=()
    if [[ "$LEVEL" == "all" ]]; then
        target_levels=(128 192 256)
    else
        target_levels=("$LEVEL")
    fi

    local total_levels="${#target_levels[@]}"
    local idx=0
    latest_status_tmp="$(mktemp)"

    for level in "${target_levels[@]}"; do
        idx=$((idx + 1))
        local target="${target_prefix}_hqc${level}_${variant}"
        bench_build_target_if_configured "$build_dir" "$target" "trace" >/dev/null 2>&1 || true

        local elf="$build_dir/${target}.elf"
        if [[ ! -f "$elf" ]]; then
            # Auto-configure and build if ELF is missing
            if ! bench_auto_configure_and_build "$build_dir" "$REPO_ROOT" "$target" "spike" "1" "trace"; then
                echo " [trace] SKIP L${level}: failed to auto-configure and build ELF" >&2
                continue
            fi
            if [[ ! -f "$elf" ]]; then
                echo " [trace] SKIP L${level}: missing ELF ${elf}" >&2
                continue
            fi
        fi

        local csv_filename="${out_prefix}_hqc${level}_hotspots_${BENCH_TIME}.csv"
        if [[ "$PROFILE_LINES" == "1" ]]; then
            csv_filename="${out_prefix}_hqc${level}_lines_${BENCH_TIME}.csv"
        fi
        local csv_out="$out_dir/$csv_filename"

        echo "# date: $(date +'%Y-%m-%d %H:%M')" > "$csv_out"
        echo "# commit: ${GIT_HASH}" >> "$csv_out"
        echo "# variant: ${variant}" >> "$csv_out"
        echo "# level: ${level}" >> "$csv_out"
        echo "function,instructions,percentage" >> "$csv_out"

        local start_time=$(date +%s)

        "$spike" -l --isa="$isa" "$elf" 2>&1 \
            | awk '/core   0:/ {print $3}' \
            | sed 's/^(//' \
            | sed 's/)//' \
            | sort -T "$trace_tmp_dir" \
            | uniq -c \
            | python3 "$REPO_ROOT/scripts/report/map_pcs.py" ${map_pcs_lines_arg:+$map_pcs_lines_arg} "$elf" - >> "$csv_out"

        local end_time=$(date +%s)
        local elapsed=$((end_time - start_time))

        local branch="├──"
        if [[ "$idx" -eq "$total_levels" ]]; then branch="└──"; fi
        if [[ "$PROFILE_LINES" == "1" ]]; then
            echo " ${branch} L${level} ⋯ generated trace (lines) in ${elapsed}s"
        else
            echo " ${branch} L${level} ⋯ generated trace in ${elapsed}s"
        fi

        if ! bench_refresh_tracked_csv_quiet "$csv_out" "${out_dir}/${out_prefix}_hqc${level}_${trace_suffix}_latest.csv" >> "$latest_status_tmp"; then
            true
        fi

    done

    echo ""
    if grep -q '^UPDATED|' "$latest_status_tmp"; then
        grep '^UPDATED|' "$latest_status_tmp" | tail -n 1 | cut -d'|' -f2-
    elif [[ -s "$latest_status_tmp" ]]; then
        tail -n 1 "$latest_status_tmp" | cut -d'|' -f2-
    fi
    trap - EXIT INT TERM
    cleanup_profile_trace_mode
}

profile_histogram_mode() {
    if [[ -z "$MUL_VARIANT" ]]; then
    echo "ERROR: --histogram requires --mul <variant>" >&2
        exit 2
    fi

    bench_load_implemented_variants "$REPO_ROOT"

    local riscv="${RISCV:-$HOME/Thesis/installed-tools}"
    local spike="${riscv}/bin/spike"
    local isa="rv64gcv_zbc_zbb_zbkb_zvbc_zvbb_zvl256b_zicntr_zihpm"
    local build_dir="$REPO_ROOT/build/spike/trace"
    local target_levels=()
    if [[ "$LEVEL" == "all" ]]; then
        target_levels=(128 192 256)
    else
        target_levels=("$LEVEL")
    fi

    local BENCH_TIME
    BENCH_TIME="$(date +'%Y-%m-%d_%H-%M')"
    local profile_kind="KEM"
    local target_prefix="bench"
    local out_prefix="kem"
    if [[ "$PROFILE_POLYMUL" == "1" ]]; then
        profile_kind="PolyMul"
        target_prefix="bench_polymul"
        out_prefix="poly"
    fi

    local track
    if [[ "${MUL_VARIANT,,}" == *"zvbc"* || "${MUL_VARIANT,,}" == *"rvv"* ]]; then
        track="vector"
    else
        track="scalar"
    fi
    local variant="${MUL_VARIANT:-karatsuba_zbc}"
    local out_dir="${REPO_ROOT}/analysis/profiles/histograms/${track}/${variant}"

    if ! bench_require_executable "$spike" "Spike not found"; then
        echo "       Source toolchain/toolchain.env or set RISCV" >&2
        exit 2
    fi

    mkdir -p "$out_dir"

    cmake -S "$REPO_ROOT" -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
        -DRISCV="$riscv" \
        -DHQC_SIM=spike \
        -DHQC_TRACE_MODE=1 \
        -DHQC_KECCAK_IMPL="${KECCAK_IMPL:-ref}" \
        -DHQC_DECODE_IMPL="${DECODE_IMPL:-ref}" >/dev/null 2>&1

    echo -e "\n📦 ${variant} (histogram - ${profile_kind})"

    local total_levels="${#target_levels[@]}"
    local idx=0
    local latest_status_tmp
    latest_status_tmp="$(mktemp)"

    for level in "${target_levels[@]}"; do
        idx=$((idx + 1))
        local target="${target_prefix}_hqc${level}_${variant}"
        bench_build_target_if_configured "$build_dir" "$target" "histogram" >/dev/null 2>&1 || true

        local elf="${build_dir}/${target}.elf"
        if [[ ! -f "$elf" ]]; then
            # Auto-configure and build if ELF is missing
            if ! bench_auto_configure_and_build "$build_dir" "$REPO_ROOT" "$target" "spike" "1" "histogram"; then
                echo "ERROR: Failed to auto-configure and build $elf" >&2
                continue
            fi
            if [[ ! -f "$elf" ]]; then
                echo "ERROR: Missing ELF $elf" >&2
                continue
            fi
        fi

        local hist="${out_dir}/${out_prefix}_hqc${level}_${variant}_hist.txt"
        local mix_file="${out_dir}/${out_prefix}_hqc${level}_${BENCH_TIME}.csv"
        local objdump_bin="${RISCV:-$HOME/Thesis/installed-tools}/bin/llvm-objdump"

        "$spike" --isa="$isa" -g "$elf" >/dev/null 2> "$hist"

        # Write CSV preamble, then parse mnemonics and auto-delete raw log
        echo "# date: $(date +'%Y-%m-%d %H:%M')" > "$mix_file"
        echo "# commit: ${GIT_HASH}" >> "$mix_file"
        echo "# variant: ${variant}" >> "$mix_file"
        echo "# level: ${level}" >> "$mix_file"
        python3 "$REPO_ROOT/scripts/report/parse_histogram.py" "$hist" "$elf" "$objdump_bin" >> "$mix_file"
        rm -f "$hist"
        local prefix=" ├──"
        if [[ "$idx" -eq "$total_levels" ]]; then prefix=" └──"; fi
        echo "${prefix} L${level} ⋯ $(basename "$mix_file")"
        if ! bench_refresh_tracked_csv_quiet "$mix_file" "${out_dir}/${out_prefix}_hqc${level}_latest.csv" >> "$latest_status_tmp"; then
            true
        fi
        if [[ "$REPORT_FLAG" -eq 1 ]]; then
            cat "$mix_file" | head -n "$(( ${TOP_N:-20} + 3 ))"
            echo ""
        fi
    done

    echo ""
    if grep -q '^UPDATED|' "$latest_status_tmp"; then
        grep '^UPDATED|' "$latest_status_tmp" | tail -n 1 | cut -d'|' -f2-
    elif [[ -s "$latest_status_tmp" ]]; then
        tail -n 1 "$latest_status_tmp" | cut -d'|' -f2-
    fi
    rm -f "$latest_status_tmp"

    echo ""
    echo "✔️ Saved: ${out_dir}"
}

usage() {
    cat <<'EOF'
Usage:
    run_profile.sh --cache [--mul V] [--level 128|192|256|all] [--polymul]
    run_profile.sh --memory [--mul V] [--level 128|192|256|all] [--polymul]
    run_profile.sh --trace [--mul V] [--level 128|192|256|all] [--polymul] [--lines]
    run_profile.sh --histogram --mul <variant> [--polymul] [--report]
             [--top N] [--compare-with set] [--context]
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --cache|--memory|--trace|--histogram)
            MODE="${1#--}"
            shift
            ;;
        --variant|--mul)
            MUL_VARIANT="$2"
            shift 2
            ;;
        --keccak|--decode)
            shift 2
            ;;
        --level)
            LEVEL="$2"
            LEVEL_SET=1
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
        --polymul)
            PROFILE_POLYMUL=1
            shift
            ;;
        --lines)
            PROFILE_LINES=1
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

if [[ "$LEVEL_SET" -eq 0 ]] && [[ "$MODE" == "cache" || "$MODE" == "memory" ]]; then
    LEVEL="all"
fi

run_mode_for_variant() {
    MUL_VARIANT="$1"
    case "$MODE" in
        cache)     profile_cache_mode ;;
        memory)    profile_memory_mode ;;
        trace)     profile_trace_mode ;;
        histogram) profile_histogram_mode ;;
    esac
}

profile_require_parallel() {
    if ! command -v parallel >/dev/null 2>&1; then
        echo "ERROR: GNU parallel is required for batched profile runs" >&2
        echo "       Install parallel and retry" >&2
        exit 2
    fi
}

profile_select_parallel_jobs() {
    local jobs=1
    local mem_available_kb

    if [[ -r /proc/meminfo ]]; then
        mem_available_kb="$(awk '/MemAvailable:/ {print $2; exit}' /proc/meminfo)"
        if [[ "$mem_available_kb" =~ ^[0-9]+$ ]] && [[ "$mem_available_kb" -ge 16777216 ]]; then
            jobs=2
        fi
    fi

    echo "$jobs"
}

declare -a batch_variants=()
declare -a batch_levels=()

if [[ "$MUL_VARIANT" == "all" ]]; then
    batch_variants=("${IMPLEMENTED_VARIANTS[@]}")
elif [[ -n "$MUL_VARIANT" ]]; then
    batch_variants=("$MUL_VARIANT")
elif [[ "$MODE" == "histogram" ]]; then
    batch_variants=("")
else
    batch_variants=("karatsuba_zbc")
fi

if [[ "$LEVEL" == "all" ]]; then
    batch_levels=(128 192 256)
else
    batch_levels=("$LEVEL")
fi

if [[ "${#batch_variants[@]}" -gt 1 ]]; then
    if [[ "$MODE" == "cache" || "$MODE" == "memory" ]]; then
        for v in "${batch_variants[@]}"; do
            MUL_VARIANT="$v"
            run_mode_for_variant "$v"
        done
    else
        profile_require_parallel

        jobs="$(profile_select_parallel_jobs)"
        declare -a cmds=()
        v=""
        l=""
        cmd=""

        for v in "${batch_variants[@]}"; do
            cmd="${SCRIPT_DIR}/run_profile.sh --${MODE} --mul ${v} --level ${LEVEL}"
            if [[ "$REPORT_FLAG" -eq 1 ]]; then cmd+=" --report"; fi
            if [[ -n "$TOP_N" ]]; then cmd+=" --top ${TOP_N}"; fi
            if [[ -n "$COMPARE_WITH" ]]; then cmd+=" --compare-with ${COMPARE_WITH}"; fi
            if [[ "$CONTEXT_FLAG" -eq 1 ]]; then cmd+=" --context"; fi
            if [[ "$PROFILE_POLYMUL" -eq 1 ]]; then cmd+=" --polymul"; fi
            if [[ "$PROFILE_LINES" -eq 1 ]]; then cmd+=" --lines"; fi
            cmds+=("$cmd")
        done

        printf '%s\n' "${cmds[@]}" | parallel --jobs "$jobs"
    fi
else
    run_mode_for_variant "${batch_variants[0]}"
fi
