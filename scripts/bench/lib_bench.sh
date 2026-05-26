#!/usr/bin/env bash
# scripts/bench/lib_bench.sh
# Shared helpers for Spike benchmark runners.

bench_sanitize_label() {
    local raw="$1"
    raw="${raw//[^a-zA-Z0-9]/_}"
    raw="$(echo "$raw" | sed -E 's/_+/_/g; s/^_+//; s/_+$//')"
    if [[ -z "$raw" ]]; then
        raw="all"
    fi
    echo "$raw"
}

bench_variant_matches() {
    local variant="$1" filter="${2:-}"
    if [[ -z "$filter" ]]; then
        return 0
    fi
    echo "$variant" | grep -qE "$filter"
}

bench_variant_track() {
    local variant="${1:-}"
    local normalized="${variant,,}"

    case "$normalized" in
        ref_vector|*_rvv|*_zvbc) echo "vector" ;;
        *) echo "scalar" ;;
    esac
}

bench_load_implemented_variants() {
    local repo_root="$1"
    mapfile -t IMPLEMENTED_VARIANTS < "${repo_root}/scripts/implemented_variants.txt"
}

# bench_parse_track_args "$@"
# Parses --mul <variant>, --keccak <impl>, --decode <impl> from the argument list.
# Sets globals: MUL_VARIANT  KECCAK_IMPL  DECODE_IMPL
# Auto-maps KECCAK_IMPL/DECODE_IMPL from MUL_VARIANT suffix when not provided.
# Caller's $@ is not modified — remaining args are still available to the caller.
bench_parse_track_args() {
    local _keccak_explicit=0
    local _decode_explicit=0
    MUL_VARIANT=""
    KECCAK_IMPL=""
    DECODE_IMPL=""

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --mul)
                MUL_VARIANT="${2:-}"
                shift 2
                ;;
            --keccak)
                case "${2:-}" in
                    opt) KECCAK_IMPL="zvbb_x2_soa" ;;
                    ref) KECCAK_IMPL="ref" ;;
                    *)   KECCAK_IMPL="${2:-}" ;;
                esac
                _keccak_explicit=1
                shift 2
                ;;
            --decode)
                case "${2:-}" in
                    opt|zvbc|rs_zvbc_exp) DECODE_IMPL="zvbc" ;;
                    ref) DECODE_IMPL="ref" ;;
                    rvv) DECODE_IMPL="rvv" ;;
                    *)   DECODE_IMPL="${2:-}" ;;
                esac
                _decode_explicit=1
                shift 2
                ;;
            *)
                shift
                ;;
        esac
    done

    # Auto-map KECCAK_IMPL from MUL_VARIANT suffix if not explicitly set.
    if [[ "$_keccak_explicit" -eq 0 ]] && [[ -n "$MUL_VARIANT" ]]; then
        case "$MUL_VARIANT" in
            ref_scalar|ref_vector) KECCAK_IMPL="ref"         ;;
            *_zvbc)               KECCAK_IMPL="zvbb_x2_soa" ;;
            *_rvv)                KECCAK_IMPL="ref"         ;;
            *_zbc|*_ref|hybrid) KECCAK_IMPL="ref"         ;;
            *)                  KECCAK_IMPL="ref"          ;;
        esac
    fi

    # Auto-map DECODE_IMPL from MUL_VARIANT suffix if not explicitly set.
    if [[ "$_decode_explicit" -eq 0 ]]; then
        case "$MUL_VARIANT" in
            ref_scalar|ref_vector) DECODE_IMPL="ref" ;;
            *_rvv)             DECODE_IMPL="rvv" ;;
            *_zvbc)            DECODE_IMPL="zvbc" ;;
            *_zbc|*_ref|hybrid) DECODE_IMPL="ref" ;;
            *)                  DECODE_IMPL="ref" ;;
        esac
    fi

    if [[ "$KECCAK_IMPL" == "opt" ]]; then KECCAK_IMPL="zvbb_x2_soa"; fi
    if [[ "$DECODE_IMPL" == "opt" || "$DECODE_IMPL" == "rs_zvbc_exp" ]]; then DECODE_IMPL="zvbc"; fi
    export MUL_VARIANT KECCAK_IMPL DECODE_IMPL
}

bench_require_executable() {
    local exe="$1" msg="${2:-Executable not found}"
    if [[ ! -x "$exe" ]]; then
        echo "ERROR: ${msg}: $exe" >&2
        return 1
    fi
}

bench_build_target_if_configured() {
    local build_dir="$1" target="$2" tag="${3:-bench}"
    if [[ -f "${build_dir}/CMakeCache.txt" ]]; then
        if ! cmake --build "$build_dir" --target "$target" -j"$(nproc)" >/dev/null 2>&1; then
            echo "[${tag}] BUILD FAIL ${target}" >&2
            return 1
        fi
    fi
}

bench_require_elf_or_hint() {
    local elf="$1" target="$2" hint="$3" tag="${4:-bench}"
    if [[ ! -f "$elf" ]]; then
        echo "[${tag}] SKIP  ${target}: ELF not found and build dir not configured" >&2
        echo "              ${hint}" >&2
        return 1
    fi
}

bench_auto_configure_and_build() {
    local build_dir="$1" repo_root="$2" target="$3" sim_type="${4:-spike}" trace_mode="${5:-0}" tag="${6:-bench}"
    local _keccak="${KECCAK_IMPL:-ref}"
    local _decode="${DECODE_IMPL:-ref}"
    local _scalar_march="${BENCH_EXPECT_HQC_SCALAR_MARCH:-rv64gc_zbc_zbb}"
    local _vector_bits="${HQC_VECTOR_BITS:-256}"
    local _toolchain_file="${HQC_TOOLCHAIN_FILE:-toolchain/riscv-clang.cmake}"
    local _opt_level="${HQC_OPT_LEVEL:-}"
    local need_config=0
    local config_reason=""
    local lock_fd=""

    mkdir -p "$build_dir"
    if command -v flock >/dev/null 2>&1; then
        exec {lock_fd}>"${build_dir}.lock"
        flock "$lock_fd"
    fi

    if [[ ! -f "${build_dir}/CMakeCache.txt" ]]; then
        need_config=1
        config_reason="missing build dir"
    else
        local cache_file="${build_dir}/CMakeCache.txt"
        local cached_keccak cached_decode cached_scalar cached_vbits
        cached_keccak="$(awk -F'=' '/^HQC_KECCAK_IMPL:STRING=/{print $2; exit}' "$cache_file")"
        cached_decode="$(awk -F'=' '/^HQC_DECODE_IMPL:STRING=/{print $2; exit}' "$cache_file")"
        cached_scalar="$(awk -F'=' '/^HQC_SCALAR_MARCH:STRING=/{print $2; exit}' "$cache_file")"
        cached_vbits="$(awk -F'=' '/^HQC_VECTOR_BITS:STRING=/{print $2; exit}' "$cache_file")"
        cached_vbits="${cached_vbits:-256}"
        local cached_toolchain_base want_toolchain_base cached_opt
        cached_toolchain_base="$(awk -F'=' '/^CMAKE_TOOLCHAIN_FILE:FILEPATH=/{print $2; exit}' "$cache_file" | xargs basename 2>/dev/null || true)"
        want_toolchain_base="$(basename "$_toolchain_file")"
        cached_opt="$(awk -F'=' '/^RISCV_OPT_LEVEL:STRING=/{print $2; exit}' "$cache_file")"

        if [[ -z "$cached_keccak" || -z "$cached_decode" || -z "$cached_scalar" ]]; then
            need_config=1
            config_reason="incomplete cache"
        elif [[ "$cached_keccak" != "$_keccak" || "$cached_decode" != "$_decode" || \
                "$cached_scalar" != "$_scalar_march" || "$cached_vbits" != "$_vector_bits" || \
                "$cached_toolchain_base" != "$want_toolchain_base" ]]; then
            need_config=1
            config_reason="cache mismatch"
        elif [[ -n "$_opt_level" && "$cached_opt" != "$_opt_level" ]]; then
            need_config=1
            config_reason="opt-level mismatch"
        fi
    fi

    if [[ "$need_config" -eq 1 ]]; then
        echo " ├── ⚙️  Auto-configuring ${config_reason}: ${build_dir}..."
        local cmake_cmd="cmake -S \"$repo_root\" -B \"$build_dir\" \
        -DCMAKE_TOOLCHAIN_FILE=${_toolchain_file} \
        -DCMAKE_BUILD_TYPE=Release \
        -DHQC_SIM=${sim_type} \
        -DHQC_KECCAK_IMPL=${_keccak} \
        -DHQC_DECODE_IMPL=${_decode} \
        -DHQC_SCALAR_MARCH=${_scalar_march} \
        -DHQC_VECTOR_BITS=${_vector_bits}"

        if [[ -n "$_opt_level" ]]; then
            cmake_cmd="${cmake_cmd} -DRISCV_OPT_LEVEL=${_opt_level}"
        fi
        if [[ "$trace_mode" == "1" ]]; then
            cmake_cmd="${cmake_cmd} -DHQC_TRACE_MODE=1"
        fi

        if ! eval "$cmake_cmd" >/dev/null 2>&1; then
            if [[ -n "$lock_fd" ]]; then
                eval "exec ${lock_fd}>&-"
            fi
            echo "[${tag}] AUTO-CONFIG FAIL ${build_dir}" >&2
            return 1
        fi
    fi

    # Build the target.
    if ! cmake --build "$build_dir" --target "$target" -j"$(nproc)" >/dev/null 2>&1; then
        if [[ -n "$lock_fd" ]]; then
            eval "exec ${lock_fd}>&-"
        fi
        echo "[${tag}] AUTO-BUILD FAIL ${target}" >&2
        return 1
    fi

    if [[ -n "$lock_fd" ]]; then
        eval "exec ${lock_fd}>&-"
    fi

    return 0
}

bench_launch_spike_job() {
    local spike="$1" isa="$2" elf="$3" out_file="$4" err_file="${5:-/dev/null}"
    nohup "$spike" --isa="$isa" "$elf" > "$out_file" 2> "$err_file" &
    echo "$!"
}

bench_run_spike_timed() {
    local spike="$1" isa="$2" elf="$3" out_file="$4" err_file="$5"
    local t0_ns t1_ns elapsed_ms elapsed_s
    t0_ns=$(date +%s%N)
    "$spike" --isa="$isa" "$elf" > "$out_file" 2> "$err_file"
    t1_ns=$(date +%s%N)
    elapsed_ms=$(( (t1_ns - t0_ns) / 1000000 ))
    elapsed_s=$(awk -v ms="$elapsed_ms" 'BEGIN { printf "%.3f", ms/1000.0 }')
    echo "$elapsed_s"
}

bench_strip_leading_csv_comments() {
    local file="$1"
    awk '
        /^# date:/ { next }
        /^# commit:/ { next }
        { print }
    ' "$file"
}

bench_csv_payload_matches() {
    local src="$1" dst="$2"
    if [[ ! -f "$dst" ]]; then
        return 1
    fi
    cmp -s \
        <(bench_strip_leading_csv_comments "$src") \
        <(bench_strip_leading_csv_comments "$dst")
}

bench_refresh_tracked_csv() {
    local src="$1" dst="$2"
    local target_dir
    local archive_dir
    local archived_src
    local abs_dst

    target_dir="$(dirname "$dst")"
    archive_dir="${target_dir}/archive"
    archived_src="${archive_dir}/$(basename "$src")"

    if command -v realpath >/dev/null 2>&1; then
        abs_dst="$(realpath -m "$dst")"
    elif [[ "$dst" == /* ]]; then
        abs_dst="$dst"
    else
        abs_dst="$(cd "$target_dir" && pwd)/$(basename "$dst")"
    fi

    mkdir -p "$target_dir"
    mkdir -p "$archive_dir"
    if bench_csv_payload_matches "$src" "$dst"; then
        mv -f "$src" "$archived_src"
        echo ""
        echo " ⏭ Unchanged: $abs_dst"
        return 1
    fi
    cp -f "$src" "$dst"
    mv -f "$src" "$archived_src"
    echo ""
    echo " 🔄 Updated: $abs_dst"
}

bench_refresh_tracked_csv_quiet() {
    local src="$1" dst="$2"
    local target_dir
    local archive_dir
    local archived_src
    local abs_dst

    target_dir="$(dirname "$dst")"
    archive_dir="${target_dir}/archive"
    archived_src="${archive_dir}/$(basename "$src")"

    if command -v realpath >/dev/null 2>&1; then
        abs_dst="$(realpath -m "$dst")"
    elif [[ "$dst" == /* ]]; then
        abs_dst="$dst"
    else
        abs_dst="$(cd "$target_dir" && pwd)/$(basename "$dst")"
    fi

    mkdir -p "$target_dir"
    mkdir -p "$archive_dir"
    if bench_csv_payload_matches "$src" "$dst"; then
        mv -f "$src" "$archived_src"
        echo "UNCHANGED| ⏭ Unchanged: $abs_dst"
        return 1
    fi
    cp -f "$src" "$dst"
    mv -f "$src" "$archived_src"
    echo "UPDATED| 🔄 Updated: $abs_dst"
}