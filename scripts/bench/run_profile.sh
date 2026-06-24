#!/usr/bin/env bash
# scripts/bench/run_profile.sh
# Unified profiling entry point.
#
# Modes:
#   --cache      Spike D$ profiling
#   --memory     QEMU memory-traffic profiling
#   --trace      Spike trace + hotspot mapping
#
# Examples:
#   scripts/bench/run_profile.sh --cache --variant scalar_zb --level 128
#   scripts/bench/run_profile.sh --memory --variant vector_rvv --level 192
#   scripts/bench/run_profile.sh --trace --variant scalar_zb

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${SCRIPT_DIR}/lib_bench.sh"

variant_to_dir() {
    case "$1" in
        scalar_ref)      echo "scalar_ref" ;;
        scalar_zb)       echo "scalar_Zb" ;;
        vector_ref)      echo "vector_ref" ;;
        vector_rvv)      echo "vector_rvv" ;;
        vector_zvk)      echo "vector_Zvk" ;;
        *)               echo "$1" ;;  # fallback
    esac
}
bench_load_implemented_variants "$REPO_ROOT"
GIT_HASH="$(git rev-parse --short HEAD 2>/dev/null || echo 'unknown')"
BENCH_TIME="$(date +'%Y-%m-%d_%H-%M')"
PROFILE_TMP_ROOT="$REPO_ROOT/.tmp"

# Populate MUL_VARIANT, KECCAK_IMPL, DECODE_IMPL via shared auto-mapping.
bench_parse_track_args "$@"
# Resolve any legacy variant name to canonical form.
MUL_VARIANT="$(bench_resolve_variant "${MUL_VARIANT:-}")"

MODE=""
LEVEL="128"
LEVEL_SET=0
REPORT_FLAG=0
TOP_N=""
COMPARE_WITH=""
CONTEXT_FLAG=0
PROFILE_POLYMUL=0
PROFILE_LINES=0

# Hardware profiling options
PLATFORM="sim"
HW_MODE="kem"
QUALITY="dev"
REMOTE=""
REMOTE_EXPLICIT=0
LOCAL_MODE=0

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
    local variant="${MUL_VARIANT:-scalar_zb}"
    local variant_slug="$(bench_variant_slug "$variant")"
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
    local isa="${ISA:-rv64gcv_zbc_zbb_zbs_zba_zbkb_zvbc_zvbb_zvl256b}"

    if [[ ! -x "$spike" ]]; then
        echo "[profile_cache_spike] ERROR: spike not found at $spike" >&2
        exit 2
    fi

    local out_dir="$REPO_ROOT/results/simulation/profiling/cache/$(variant_to_dir "$variant")"
    mkdir -p "$out_dir"

    local BENCH_TIME
    BENCH_TIME="$(date +'%Y-%m-%d_%H-%M')"

    local level_tag="Lall"
    if [[ "$LEVEL" != "all" ]]; then
        level_tag="L${LEVEL}"
    fi
    local csv_out="$out_dir/cache_${out_prefix}_${level_tag}_${variant_slug}_${BENCH_TIME}.csv"
    local csv_latest="$out_dir/cache_${out_prefix}_${level_tag}_${variant_slug}_latest.csv"

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
        -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-gcc.cmake \
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

    bench_refresh_tracked_csv "$csv_out" "$csv_latest" || true
    echo ""
}

profile_memory_mode() {
    local variant="${MUL_VARIANT:-scalar_zb}"
    local variant_slug="$(bench_variant_slug "$variant")"
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

    local out_dir="$REPO_ROOT/results/simulation/profiling/mem_access/$(variant_to_dir "$variant")"
    mkdir -p "$out_dir"

    local BENCH_TIME
    BENCH_TIME="$(date +'%Y-%m-%d_%H-%M')"

    local level_tag="Lall"
    if [[ "$LEVEL" != "all" ]]; then
        level_tag="L${LEVEL}"
    fi
    local csv_out="$out_dir/memory_${out_prefix}_${level_tag}_${variant_slug}_${BENCH_TIME}.csv"
    local csv_latest="$out_dir/memory_${out_prefix}_${level_tag}_${variant_slug}_latest.csv"

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
        -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-gcc.cmake \
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
            -cpu rv64,v=true,vlen=256,elen=64,vext_spec=v1.0,zbkb=true,zbc=true,zbs=true,zvbc=true,zvbb=true \
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

    bench_refresh_tracked_csv "$csv_out" "$csv_latest" || true
    echo ""
}

profile_trace_mode() {
    local variant="${MUL_VARIANT:-scalar_zb}"
    local variant_slug="$(bench_variant_slug "$variant")"
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
    local isa="${ISA:-rv64gcv_zbc_zbb_zbs_zba_zbkb_zvbc_zvbb_zvl256b}"
    local build_dir="$REPO_ROOT/build/spike/trace"

    if [[ ! -x "$spike" ]]; then
        echo "ERROR: spike not found at $spike" >&2
        exit 2
    fi

    local out_dir="$REPO_ROOT/results/simulation/profiling/traces/$(variant_to_dir "$variant")"
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
        -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-gcc.cmake \
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

        local csv_filename="trace_${out_prefix}_L${level}_${variant_slug}_hotspots_${BENCH_TIME}.csv"
        if [[ "$PROFILE_LINES" == "1" ]]; then
            csv_filename="trace_${out_prefix}_L${level}_${variant_slug}_lines_${BENCH_TIME}.csv"
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
            | RISCV="$riscv" python3 "$REPO_ROOT/scripts/report/map_pcs.py" ${map_pcs_lines_arg:+$map_pcs_lines_arg} "$elf" - >> "$csv_out"

        local end_time=$(date +%s)
        local elapsed=$((end_time - start_time))

        local branch="├──"
        if [[ "$idx" -eq "$total_levels" ]]; then branch="└──"; fi
        if [[ "$PROFILE_LINES" == "1" ]]; then
            echo " ${branch} L${level} ⋯ generated trace (lines) in ${elapsed}s"
        else
            echo " ${branch} L${level} ⋯ generated trace in ${elapsed}s"
        fi

        if ! bench_refresh_tracked_csv_quiet "$csv_out" "${out_dir}/trace_${out_prefix}_L${level}_${variant_slug}_${trace_suffix}_latest.csv" >> "$latest_status_tmp"; then
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

# ══════════════════════════════════════════════════════════════════════════════
# Hardware profiling: shared SSH/deploy infrastructure
# ══════════════════════════════════════════════════════════════════════════════
_hw_profile_setup() {
    # Sets up shared variables for all hw profiling modes.
    # Called once at the top of each hw mode function.
    local creds_file="${REPO_ROOT}/docs/hardware/BPI-F3/bpi-f3-credentials.env"
    _hw_bpi_pass=""
    if [[ -f "$creds_file" ]]; then
        source "$creds_file"
        _hw_bpi_pass="${BPI_PASS:-}"
    fi

    # Auto-detect local mode on riscv64
    if [[ "$(uname -m)" == "riscv64" && "$REMOTE_EXPLICIT" -eq 0 ]]; then
        LOCAL_MODE=1
    fi
    if [[ "$REMOTE_EXPLICIT" -eq 0 && "$LOCAL_MODE" -eq 0 ]]; then
        REMOTE="${BPI_USER:-root}@${BPI_HOST:-192.168.1.50}"
    fi
    if [[ "$LOCAL_MODE" -eq 0 && -z "$REMOTE" ]]; then
        echo "ERROR: Not on riscv64 — specify --remote HOST or --local" >&2
        exit 1
    fi

    # SSH transport
    _hw_use_sshpass=0
    if [[ "$LOCAL_MODE" -eq 0 && -n "$REMOTE" ]]; then
        if ssh -o ConnectTimeout=5 -o BatchMode=yes "$REMOTE" true 2>/dev/null; then
            _hw_use_sshpass=0
        elif [[ -n "$_hw_bpi_pass" ]] && command -v sshpass &>/dev/null; then
            _hw_use_sshpass=1
        else
            echo "ERROR: Cannot connect to ${REMOTE} via SSH." >&2
            exit 2
        fi
    fi
}

_hw_ssh() {
    if [[ "$_hw_use_sshpass" -eq 1 ]]; then
        sshpass -p "$_hw_bpi_pass" ssh -o StrictHostKeyChecking=no "$@"
    else
        ssh -o StrictHostKeyChecking=no "$@"
    fi
}

_hw_rsync() {
    if [[ "$_hw_use_sshpass" -eq 1 ]]; then
        sshpass -p "$_hw_bpi_pass" rsync -e "ssh -o StrictHostKeyChecking=no" "$@"
    else
        rsync -e "ssh -o StrictHostKeyChecking=no" "$@"
    fi
}

_hw_run_cmd() {
    if [[ "$LOCAL_MODE" -eq 1 ]]; then
        eval "$1"
    else
        _hw_ssh "$REMOTE" "$1"
    fi
}

_hw_deploy() {
    if [[ "$LOCAL_MODE" -eq 1 ]]; then return; fi
    echo "[deploy] Syncing to ${REMOTE}..."
    _hw_rsync -az --delete \
        --exclude='.venv' --exclude='build*' --exclude='.git' \
        --exclude='__pycache__' --exclude='.hypothesis' \
        --exclude='results' --exclude='.tmp' \
        "${REPO_ROOT}/" "${REMOTE}:~/hqc-riscv/"
    echo "[deploy] Done."
}

_hw_set_governor() {
    _hw_run_cmd "echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null 2>&1 || true"
}

_hw_ensure_paranoid() {
    local val
    val="$(_hw_run_cmd 'cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo 3')"
    if [[ "$val" =~ ^[0-9]+$ ]] && [[ "$val" -gt 1 ]]; then
        _hw_run_cmd "sudo sysctl -w kernel.perf_event_paranoid=1 >/dev/null 2>&1 || true"
    fi
}

# ══════════════════════════════════════════════════════════════════════════════
# Hardware profiling mode: --counters (EXTENDED perf_event_open per-region)
# ══════════════════════════════════════════════════════════════════════════════
profile_counters_hw_mode() {
    local variant="${MUL_VARIANT:-scalar_zb}"
    local variant_slug="$(bench_variant_slug "$variant")"
    local core="7"

    _hw_profile_setup
    _hw_deploy
    _hw_set_governor
    _hw_ensure_paranoid

    local remote_dir="~/hqc-riscv"
    local n_iter n_warmup
    if [[ "$QUALITY" == "publish" ]]; then
        n_iter=10000; n_warmup=100
    else
        n_iter=100; n_warmup=10
    fi

    # Determine components to profile
    local components=()
    case "$HW_MODE" in
        all)     components=(kem mul keccak decode) ;;
        kem)     components=(kem) ;;
        mul)     components=(mul) ;;
        keccak)  components=(keccak) ;;
        decode)  components=(decode) ;;
        *)       components=(kem) ;;
    esac

    local out_base="$REPO_ROOT/results/hardware/profiling/counters/$(variant_to_dir "$variant")"
    mkdir -p "$out_base"

    echo -e "\n╔══════════════════════════════════════════════════════════════╗"
    printf  "║  %-60s║\n" "Hardware Profiling: Counters (EXTENDED perf_event_open)"
    printf  "║  %-60s║\n" "Variant: ${variant}  Quality: ${QUALITY}  Target: ${REMOTE:-local}"
    echo    "╚══════════════════════════════════════════════════════════════╝"
    echo ""

    local target_levels=()
    if [[ "$LEVEL" == "all" ]]; then
        target_levels=(128 192 256)
    else
        target_levels=("$LEVEL")
    fi

    for component in "${components[@]}"; do
        echo "📦 ${variant} / ${component}"

        local total_levels="${#target_levels[@]}"
        local idx=0

        for level in "${target_levels[@]}"; do
            idx=$((idx + 1))

            # Build with EXTENDED=1
            local make_base="make -f Makefile.hw SUITE=bench LEVEL=${level} POLYMUL=${variant} MODE=${QUALITY} EXTENDED=1"
            _hw_run_cmd "cd ${remote_dir} && ${make_base} clean >/dev/null 2>&1; ${make_base} ${component} >/dev/null 2>&1"

            # Determine binary name
            local bin_name
            case "$component" in
                kem)     bin_name="build-hw/bench_hw_kem_hqc${level}_${variant}" ;;
                mul)     bin_name="build-hw/bench_hw_mul_hqc${level}_${variant}" ;;
                keccak)  bin_name="build-hw/bench_hw_keccak_hqc${level}" ;;
                decode)  bin_name="build-hw/bench_hw_decode_hqc${level}_ref" ;;
            esac

            if ! _hw_run_cmd "test -f ${remote_dir}/${bin_name}" 2>/dev/null; then
                echo " ├── ❌ BUILD FAIL (${component}, L${level})"
                continue
            fi

            # Execute and capture the binary's own CSV output
            local exec_prefix="sudo nice -n -20 taskset -c ${core}"
            local raw_output
            raw_output="$(_hw_run_cmd "cd ${remote_dir} && ${exec_prefix} ./${bin_name} 2>/dev/null")"

            if [[ -z "$raw_output" ]]; then
                echo " ├── ❌ EXEC FAIL (${component}, L${level})"
                continue
            fi

            # Store raw output
            local csv_out="${out_base}/counters_${component}_L${level}_${variant_slug}_${BENCH_TIME}.csv"
            local csv_latest="${out_base}/counters_${component}_L${level}_${variant_slug}_latest.csv"

            {
                echo "# date: $(date +'%Y-%m-%d %H:%M')"
                echo "# commit: ${GIT_HASH}"
                echo "# variant: ${variant}"
                echo "# level: ${level}"
                echo "# component: ${component}"
                echo "# quality: ${QUALITY}"
                echo "# n_iter: ${n_iter}"
                echo "# n_warmup: ${n_warmup}"
                echo "# platform: bpi-f3 (SpacemiT X60, 1.6GHz, VLEN=256)"
                echo "# core: ${core}"
                echo "# governor: performance"
                echo "$raw_output"
            } > "$csv_out"

            bench_refresh_tracked_csv_quiet "$csv_out" "$csv_latest" >/dev/null 2>&1 || true

            # Print summary (extract median cycles from output)
            local med_cycles
            med_cycles="$(echo "$raw_output" | grep -v '^#' | grep -v '^op,' | head -1 | awk -F',' '{print $4}')"
            local branch="├──"
            if [[ "$idx" -eq "$total_levels" ]]; then branch="└──"; fi
            echo " ${branch} L${level} ⋯ median_cycles=${med_cycles:-?}"

        done
        echo ""
    done

    # ── Compute derived metrics ──────────────────────────────────────────
    echo "[derived] Computing miss rates, MPKI, stall%, vector%..."
    python3 "${REPO_ROOT}/scripts/report/compute_derived_counters.py" \
        --input-dir "$out_base" \
        --variant "$variant" \
        --levels "$(IFS=,; echo "${target_levels[*]}")" 2>/dev/null || \
        echo "  ⚠ Derived metric computation skipped (script not ready or parse error)"

    echo ""
    echo " Output: ${out_base}/"
    echo ""
}

# ══════════════════════════════════════════════════════════════════════════════
# Hardware profiling mode: --hotspots (task-clock sampling, flame graphs)
# ══════════════════════════════════════════════════════════════════════════════
profile_hotspots_hw_mode() {
    local variant="${MUL_VARIANT:-scalar_zb}"
    local variant_slug="$(bench_variant_slug "$variant")"
    local core="7"

    _hw_profile_setup
    _hw_deploy
    _hw_set_governor
    _hw_ensure_paranoid

    local remote_dir="~/hqc-riscv"

    # Determine components to profile
    local components=()
    case "$HW_MODE" in
        all)     components=(kem mul) ;;
        kem)     components=(kem) ;;
        mul)     components=(mul) ;;
        keccak)  components=(keccak) ;;
        decode)  components=(decode) ;;
        *)       components=(mul) ;;
    esac

    local out_base="$REPO_ROOT/results/hardware/profiling/hotspots/$(variant_to_dir "$variant")"
    mkdir -p "$out_base"

    echo -e "\n╔══════════════════════════════════════════════════════════════╗"
    printf  "║  %-60s║\n" "Hardware Profiling: Hotspots (task-clock sampling)"
    printf  "║  %-60s║\n" "Variant: ${variant}  Target: ${REMOTE:-local}"
    printf  "║  %-60s║\n" "Method: perf record -e task-clock -F 4999 --call-graph fp"
    echo    "╚══════════════════════════════════════════════════════════════╝"
    echo ""

    local target_levels=()
    if [[ "$LEVEL" == "all" ]]; then
        target_levels=(128 192 256)
    else
        target_levels=("$LEVEL")
    fi

    for component in "${components[@]}"; do
        echo "📦 ${variant} / ${component} (hotspots)"

        local total_levels="${#target_levels[@]}"
        local idx=0

        for level in "${target_levels[@]}"; do
            idx=$((idx + 1))

            # Build profile binary (harness-free, -O3 -fno-omit-frame-pointer -g)
            local profile_target="profile_${component}"
            local make_base="make -f Makefile.hw SUITE=bench LEVEL=${level} POLYMUL=${variant}"

            _hw_run_cmd "cd ${remote_dir} && ${make_base} clean >/dev/null 2>&1; ${make_base} ${profile_target} >/dev/null 2>&1"

            # Determine binary name
            local bin_name="build-hw/bench_profile_${component}_hqc${level}_${variant}"

            if ! _hw_run_cmd "test -f ${remote_dir}/${bin_name}" 2>/dev/null; then
                echo " ├── ❌ BUILD FAIL (profile_${component}, L${level})"
                continue
            fi

            # Run perf record with task-clock
            local perf_data="/tmp/perf_hotspot_${component}_${level}_${variant_slug}.data"
            local exec_cmd="cd ${remote_dir} && perf record -e task-clock -F 4999 --call-graph fp -o ${perf_data} -- sudo nice -n -20 taskset -c ${core} ./${bin_name} 2>/dev/null"

            _hw_run_cmd "$exec_cmd" >/dev/null 2>&1

            # Extract sample count
            local sample_count
            sample_count="$(_hw_run_cmd "perf report -i ${perf_data} --stdio 2>&1 | grep '# Samples:' | sed 's/.*# Samples: *//;s/ of .*//' | head -1" 2>/dev/null || echo "?")"
            sample_count="${sample_count:-?}"

            # Extract hotspot report (filter kernel/libc/ioctl/harness)
            local report_raw
            report_raw="$(_hw_run_cmd "perf report -i ${perf_data} --stdio --no-children --percent-limit 0.5 2>&1")"

            # Parse into CSV
            local csv_out="${out_base}/hotspots_${component}_L${level}_${variant_slug}_${BENCH_TIME}.csv"
            local csv_latest="${out_base}/hotspots_${component}_L${level}_${variant_slug}_latest.csv"

            {
                echo "# date: $(date +'%Y-%m-%d %H:%M')"
                echo "# commit: ${GIT_HASH}"
                echo "# variant: ${variant}"
                echo "# level: ${level}"
                echo "# component: ${component}"
                echo "# method: perf record -e task-clock -F 4999 --call-graph fp"
                echo "# binary: ${bin_name} (-O3 -fno-omit-frame-pointer -g, NO -fno-inline)"
                echo "# samples: ${sample_count}"
                echo "# platform: bpi-f3 (SpacemiT X60, 1.6GHz, VLEN=256)"
                echo "# note: harness-free binary (no perf_event_open/ioctl overhead)"
                echo "function,self_pct,samples"
            } > "$csv_out"

            # Parse perf report output into CSV rows, filtering kernel/system
            echo "$report_raw" | python3 -c "
import sys, re
for line in sys.stdin:
    line = line.strip()
    # Match lines like: '  46.14%  bench_hw_mul_hq  bench_hw_mul_hqc128_scalar_zb  [.] clmul_leaf.constprop.1'
    m = re.match(r'^\s*([\d.]+)%\s+\S+\s+\S+\s+\[(.)\]\s+(.+)', line)
    if not m:
        continue
    pct, space, func = m.group(1), m.group(2), m.group(3)
    # Filter kernel symbols [k], unknown addresses, and system functions
    if space == 'k':
        continue
    if func.startswith('0x'):
        continue
    # Filter measurement harness and system symbols
    skip_prefixes = ('perf_hw_', 'ioctl', '__syscall', '__internal_syscall',
                     '__libc_start', '_start', '__GI_', 'read', 'write',
                     '__clock_gettime', 'clock_gettime')
    if any(func.startswith(p) for p in skip_prefixes):
        continue
    if float(pct) < 0.5:
        continue
    # Estimate samples from percentage (approximate)
    print(f'{func},{pct},')
" >> "$csv_out"

            bench_refresh_tracked_csv_quiet "$csv_out" "$csv_latest" >/dev/null 2>&1 || true

            # Generate FlameGraph SVG if tools available
            local svg_out="${out_base}/hotspots_${component}_L${level}_${variant_slug}_latest.svg"
            local flamegraph_ok=0
            if _hw_run_cmd "test -x ~/FlameGraph/flamegraph.pl" 2>/dev/null; then
                local folded
                folded="$(_hw_run_cmd "perf script -i ${perf_data} 2>/dev/null | ~/FlameGraph/stackcollapse-perf.pl --kernel 2>/dev/null | grep -v -E '^(ioctl|__syscall|perf_hw_|__libc_start|_start)' || true")"
                if [[ -n "$folded" ]]; then
                    echo "$folded" | _hw_ssh "$REMOTE" "cat | ~/FlameGraph/flamegraph.pl --title='${variant} ${component} L${level} (task-clock)' --width=1200" > "$svg_out" 2>/dev/null && flamegraph_ok=1
                fi
            fi

            # Clean up remote perf data
            _hw_run_cmd "rm -f ${perf_data}" 2>/dev/null || true

            # Print summary
            local branch="├──"
            if [[ "$idx" -eq "$total_levels" ]]; then branch="└──"; fi
            local svg_note=""
            if [[ "$flamegraph_ok" -eq 1 ]]; then svg_note=" + SVG"; fi
            local sample_warning=""
            # Parse sample count — handle "21K" "1.2M" and plain numbers
            local sample_numeric="${sample_count}"
            sample_numeric="${sample_numeric//,/}"
            if [[ "$sample_numeric" =~ ^([0-9.]+)K$ ]]; then
                sample_numeric=$(awk "BEGIN{printf \"%d\", ${BASH_REMATCH[1]} * 1000}")
            elif [[ "$sample_numeric" =~ ^([0-9.]+)M$ ]]; then
                sample_numeric=$(awk "BEGIN{printf \"%d\", ${BASH_REMATCH[1]} * 1000000}")
            fi
            if [[ "$sample_numeric" =~ ^[0-9]+$ ]] && [[ "$sample_numeric" -lt 1000 ]]; then
                sample_warning=" ⚠ LOW SAMPLES"
            fi
            echo " ${branch} L${level} ⋯ ${sample_count} samples${svg_note}${sample_warning}"

        done
        echo ""
    done

    echo " Output: ${out_base}/"
    echo ""
}

# ══════════════════════════════════════════════════════════════════════════════
# Hardware profiling mode: --isa-mix (static instruction-mix from objdump)
# ══════════════════════════════════════════════════════════════════════════════
profile_isa_mix_hw_mode() {
    local variant="${MUL_VARIANT:-scalar_zb}"
    local variant_slug="$(bench_variant_slug "$variant")"

    _hw_profile_setup
    _hw_deploy

    local remote_dir="~/hqc-riscv"

    # Determine components
    local components=()
    case "$HW_MODE" in
        all)     components=(kem mul) ;;
        kem)     components=(kem) ;;
        mul)     components=(mul) ;;
        keccak)  components=(keccak) ;;
        decode)  components=(decode) ;;
        *)       components=(mul) ;;
    esac

    local out_base="$REPO_ROOT/results/hardware/profiling/isa_mix/$(variant_to_dir "$variant")"
    mkdir -p "$out_base"

    echo -e "\n╔══════════════════════════════════════════════════════════════╗"
    printf  "║  %-60s║\n" "Hardware Profiling: ISA Instruction Mix (objdump)"
    printf  "║  %-60s║\n" "Variant: ${variant}  Target: ${REMOTE:-local}"
    echo    "╚══════════════════════════════════════════════════════════════╝"
    echo ""

    local target_levels=()
    if [[ "$LEVEL" == "all" ]]; then
        target_levels=(128 192 256)
    else
        target_levels=("$LEVEL")
    fi

    for component in "${components[@]}"; do
        echo "📦 ${variant} / ${component} (ISA mix)"

        local total_levels="${#target_levels[@]}"
        local idx=0

        for level in "${target_levels[@]}"; do
            idx=$((idx + 1))

            # Build the standard -O3 binary (same as bench, no frame pointers needed)
            local make_base="make -f Makefile.hw SUITE=bench LEVEL=${level} POLYMUL=${variant} MODE=dev EXTENDED=0"
            _hw_run_cmd "cd ${remote_dir} && ${make_base} clean >/dev/null 2>&1; ${make_base} ${component} >/dev/null 2>&1"

            local bin_name
            case "$component" in
                kem)     bin_name="build-hw/bench_hw_kem_hqc${level}_${variant}" ;;
                mul)     bin_name="build-hw/bench_hw_mul_hqc${level}_${variant}" ;;
                keccak)  bin_name="build-hw/bench_hw_keccak_hqc${level}" ;;
                decode)  bin_name="build-hw/bench_hw_decode_hqc${level}_ref" ;;
            esac

            if ! _hw_run_cmd "test -f ${remote_dir}/${bin_name}" 2>/dev/null; then
                echo " ├── ❌ BUILD FAIL (${component}, L${level})"
                continue
            fi

            # Run objdump and count instruction categories on the board
            local isa_mix_raw
            isa_mix_raw="$(_hw_run_cmd "cd ${remote_dir} && objdump -d ./${bin_name} | python3 scripts/report/parse_isa_mix.py")"

            local csv_out="${out_base}/isa_mix_${component}_L${level}_${variant_slug}_${BENCH_TIME}.csv"
            local csv_latest="${out_base}/isa_mix_${component}_L${level}_${variant_slug}_latest.csv"

            {
                echo "# date: $(date +'%Y-%m-%d %H:%M')"
                echo "# commit: ${GIT_HASH}"
                echo "# variant: ${variant}"
                echo "# level: ${level}"
                echo "# component: ${component}"
                echo "# binary: ${bin_name} (-O3, same as bench)"
                echo "# platform: bpi-f3 (SpacemiT X60)"
                echo "category,mnemonic,count"
                echo "$isa_mix_raw"
            } > "$csv_out"

            bench_refresh_tracked_csv_quiet "$csv_out" "$csv_latest" >/dev/null 2>&1 || true

            # Print summary
            local zbc_count rvv_count total_count
            zbc_count="$(grep 'summary,zbc_total' "$csv_out" 2>/dev/null | cut -d',' -f3 || echo 0)"
            rvv_count="$(grep -E 'summary,rvv_(arith|load|store|config)_total' "$csv_out" 2>/dev/null | awk -F',' '{s+=$3}END{print s+0}' || echo 0)"
            total_count="$(grep 'total,all_instructions' "$csv_out" 2>/dev/null | cut -d',' -f3 || echo 0)"

            local branch_char="├──"
            if [[ "$idx" -eq "$total_levels" ]]; then branch_char="└──"; fi
            echo " ${branch_char} L${level} ⋯ total=${total_count} Zbc=${zbc_count:-0} RVV=${rvv_count:-0}"

        done
        echo ""
    done

    echo " Output: ${out_base}/"
    echo ""
}



usage() {
    cat <<'EOF'
Usage:
  Simulation modes (--platform sim, default):
    run_profile.sh --cache [--variant V] [--level 128|192|256|all] [--polymul]
    run_profile.sh --memory [--variant V] [--level 128|192|256|all] [--polymul]
    run_profile.sh --trace [--variant V] [--level 128|192|256|all] [--polymul] [--lines]

  Hardware modes (--platform hw):
    run_profile.sh --platform hw --counters [--variant V] [--level 128|192|256|all]
                   [--component kem|mul|keccak|decode|all] [--quality dev|publish]
                   [--remote HOST|--local]
    run_profile.sh --platform hw --hotspots [--variant V] [--level 128|192|256|all]
                   [--component kem|mul] [--remote HOST|--local]
    run_profile.sh --platform hw --isa-mix [--variant V] [--level 128|192|256|all]
                   [--component kem|mul] [--remote HOST|--local]

Hardware profiling (--platform hw):
  --counters         Per-region EXTENDED perf_event_open counters + derived metrics
  --hotspots         task-clock sampling hotspots (perf record -e task-clock)
  --isa-mix          Static ISA instruction-mix analysis (objdump)
  --component C      Component to profile: kem, mul, keccak, decode, all (default: kem)
  --quality Q        dev (100 iters) or publish (10000 iters) — default: dev
  --remote HOST      Deploy and run on remote HOST via SSH
  --local            Run directly on this machine (auto-detected on riscv64)
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --cache|--memory|--trace)
            MODE="${1#--}"
            shift
            ;;
        --counters)
            MODE="counters"
            shift
            ;;
        --hotspots)
            MODE="hotspots"
            shift
            ;;
        --isa-mix|--isa_mix|--isamix)
            MODE="isa_mix"
            shift
            ;;
        --platform)
            PLATFORM="$2"
            shift 2
            ;;
        --variant)
            MUL_VARIANT="$(bench_resolve_variant "$2")"
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
        --quality)
            QUALITY="$2"
            shift 2
            ;;
        --remote)
            REMOTE="$2"
            REMOTE_EXPLICIT=1
            LOCAL_MODE=0
            shift 2
            ;;
        --local)
            LOCAL_MODE=1
            shift
            ;;
        --component|--mode)
            HW_MODE="$2"
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
    echo "ERROR: one of --cache|--memory|--trace|--counters|--hotspots|--isa-mix is required" >&2
    usage >&2
    exit 2
fi

# Validate platform
if [[ "$PLATFORM" != "sim" && "$PLATFORM" != "hw" ]]; then
    echo "ERROR: --platform must be 'sim' or 'hw', got: '$PLATFORM'" >&2
    exit 2
fi

# hw modes auto-set platform
if [[ "$MODE" == "counters" || "$MODE" == "hotspots" || "$MODE" == "isa_mix" ]]; then
    PLATFORM="hw"
fi

# sim modes are only valid on sim platform
if [[ "$PLATFORM" == "hw" && "$MODE" != "counters" && "$MODE" != "hotspots" && "$MODE" != "isa_mix" ]]; then
    echo "ERROR: --platform hw supports --counters, --hotspots, --isa-mix" >&2
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
        counters)  profile_counters_hw_mode ;;
        hotspots)  profile_hotspots_hw_mode ;;
        isa_mix)   profile_isa_mix_hw_mode ;;
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
else
    batch_variants=("scalar_zb")
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
            cmd="${SCRIPT_DIR}/run_profile.sh --${MODE} --variant ${v} --level ${LEVEL}"
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
