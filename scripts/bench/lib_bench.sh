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

bench_resolve_variant() {
    local raw="$1"
    # Map legacy variant names to their canonical equivalents.
    # If the name is already canonical or unknown, return it unchanged.
    case "$raw" in
        ref_scalar)      echo "scalar_ref" ;;
        karatsuba_zbc)   echo "scalar_zb" ;;
        ref_vector)      echo "vector_ref" ;;
        karatsuba_rvv)   echo "vector_rvv" ;;
        karatsuba_zvbc)  echo "vector_zvk" ;;
        *)               echo "$raw" ;;
    esac
}

bench_variant_slug() {
    local canonical="$1"
    # Map canonical variant names to hyphenated slugs (used in CSV filenames).
    # The slug is simply the canonical name with underscores replaced by hyphens.
    case "$canonical" in
        scalar_ref)     echo "scalar-ref" ;;
        scalar_zb)      echo "scalar-zb" ;;
        vector_ref)     echo "vector-ref" ;;
        vector_rvv)     echo "vector-rvv" ;;
        vector_zvk)     echo "vector-zvk" ;;
        # For non-variant identifiers (keccak/decode impl names like "ref", "rvv",
        # "zvbc", "opt", "zvbb_x2_soa"), fall back to underscore→hyphen replacement.
        *)              echo "${canonical//_/-}" ;;
    esac
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
        vector_ref|vector_rvv|vector_zvk|*_rvv|*_zvbc) echo "vector" ;;
        *) echo "scalar" ;;
    esac
}

bench_load_implemented_variants() {
    local repo_root="$1"
    mapfile -t IMPLEMENTED_VARIANTS < "${repo_root}/scripts/implemented_variants.txt"
}

# bench_parse_track_args "$@"
# Parses --variant <variant>, --keccak <impl>, --decode <impl> from the argument list.
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
            --variant)
                MUL_VARIANT="$(bench_resolve_variant "${2:-}")"
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
            scalar_ref|scalar_zb|vector_ref) KECCAK_IMPL="ref" ;;
            vector_zvk)           KECCAK_IMPL="zvbb_x2_soa" ;;
            vector_rvv)           KECCAK_IMPL="ref"         ;;
            *)                    KECCAK_IMPL="ref"          ;;
        esac
    fi

    # Auto-map DECODE_IMPL from MUL_VARIANT suffix if not explicitly set.
    if [[ "$_decode_explicit" -eq 0 ]]; then
        case "$MUL_VARIANT" in
            scalar_ref|scalar_zb|vector_ref) DECODE_IMPL="ref" ;;
            vector_rvv)           DECODE_IMPL="rvv" ;;
            vector_zvk)           DECODE_IMPL="zvbc" ;;
            *)                    DECODE_IMPL="ref" ;;
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
    local _toolchain_file="${HQC_TOOLCHAIN_FILE:-toolchain/riscv-gcc.cmake}"
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
        cached_toolchain_base="$(awk -F'=' '/^CMAKE_TOOLCHAIN_FILE:(FILEPATH|UNINITIALIZED)=/{print $2; exit}' "$cache_file" | xargs basename 2>/dev/null || true)"
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

    # If cache looks valid but the requested target doesn't exist, force reconfigure.
    # This catches stale CMake targets (e.g. after renaming a variant)
    # where cache variables haven't changed but the generated targets are stale.
    if [[ "$need_config" -eq 0 ]] && [[ -f "${build_dir}/Makefile" ]]; then
        if ! grep -q "^${target}:" "${build_dir}/Makefile"; then
            need_config=1
            config_reason="stale targets (${target} missing)"
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

# ── bench_hw_csv_header ───────────────────────────────────────────────────────
# Generate the metadata comment block for a hardware CSV.
#
# Arguments:
#   $1 - variant    (e.g. vector_rvv)
#   $2 - n_iter     (e.g. 100)
#   $3 - n_warmup   (e.g. 10)
#   $4 - mode       (dev | publish)
#   $5 - cov_cycles (float, optional — only included in publish mode)
#
# Outputs comment lines to stdout.
bench_hw_csv_header() {
    local variant="$1"
    local n_iter="$2"
    local n_warmup="$3"
    local mode="$4"
    local cov_cycles="${5:-}"

    local date_str
    date_str="$(date +'%Y-%m-%d %H:%M')"
    local commit
    commit="$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")"

    echo "# date: ${date_str}"
    echo "# commit: ${commit}"
    echo "# variant: ${variant}"
    echo "# platform: bpi-f3"
    echo "# n_iter: ${n_iter}"
    echo "# n_warmup: ${n_warmup}"
    echo "# mode: ${mode}"
    if [[ "$mode" == "publish" && -n "$cov_cycles" ]]; then
        echo "# cov_cycles: ${cov_cycles}"
    fi
    # Include isolation provenance if the library is loaded
    if declare -f hw_isolation_provenance_comments >/dev/null 2>&1; then
        hw_isolation_provenance_comments
    fi
}

# ── bench_hw_store_result ──────────────────────────────────────────────────────
# Write CSV content directly to the benchmark category directory (flat, no
# per-variant subdirectory), then delegate to bench_refresh_tracked_csv for
# archive management.
#
# Arguments:
#   $1 - csv_content  (full CSV text including header comments + data rows)
#   $2 - variant      (e.g. vector_rvv)
#   $3 - op           (e.g. mul, kem)
#   $4 - timestamp    (e.g. 2025-06-01_14-30)
#
# The storage path model (split by operation type):
#   kem files → results/hardware/benchmarks/kem/hw_kem_{slug}_{timestamp}.csv
#   mul files → results/hardware/benchmarks/components/poly_mul/hw_mul_{slug}_{timestamp}.csv
#
# Returns: the path written (via stdout from bench_refresh_tracked_csv).
bench_hw_store_result() {
    local csv_content="$1"
    local variant="$2"
    local op="$3"
    local timestamp="$4"
    local slug_override="${5:-}"

    local slug
    if [[ -n "$slug_override" ]]; then
        slug="$slug_override"
    else
        slug="$(bench_variant_slug "$variant")"
    fi

    local repo_root
    repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

    # Route to the appropriate category directory based on operation type.
    local category_dir
    case "$op" in
        kem)     category_dir="${repo_root}/results/hardware/benchmarks/kem" ;;
        mul)     category_dir="${repo_root}/results/hardware/benchmarks/components/poly_mul" ;;
        keccak)  category_dir="${repo_root}/results/hardware/benchmarks/components/keccak" ;;
        decode)  category_dir="${repo_root}/results/hardware/benchmarks/components/decode" ;;
        *)       category_dir="${repo_root}/results/hardware/benchmarks/components/${op}" ;;
    esac
    mkdir -p "$category_dir"

    local timestamped_file latest_file
    timestamped_file="${category_dir}/$(bench_build_filename "hw" "$op" "$slug" "$timestamp")"
    latest_file="${category_dir}/$(bench_build_filename "hw" "$op" "$slug" "latest")"

    # Write the CSV content to the timestamped file.
    echo "$csv_content" > "$timestamped_file"

    # Delegate to bench_refresh_tracked_csv for archive management.
    bench_refresh_tracked_csv "$timestamped_file" "$latest_file"
}

bench_build_filename() {
    local platform="$1"
    local operation="$2"
    local variant_slug="$3"
    local suffix="$4"

    # Validate platform
    case "$platform" in
        sim|hw) ;;
        *)
            echo "ERROR: bench_build_filename: invalid platform '$platform' (expected 'sim' or 'hw')" >&2
            return 1
            ;;
    esac

    # Validate operation
    case "$operation" in
        mul|kem|keccak|decode|compiler_compare|vlen|validation) ;;
        *)
            echo "ERROR: bench_build_filename: invalid operation '$operation' (expected one of: mul, kem, keccak, decode, compiler_compare, vlen, validation)" >&2
            return 1
            ;;
    esac

    # Validate variant-slug
    if [[ ! "$variant_slug" =~ ^[a-z0-9]+(-[a-z0-9]+)*$ ]]; then
        echo "ERROR: bench_build_filename: invalid variant-slug '$variant_slug' (must match [a-z0-9]+(-[a-z0-9]+)*)" >&2
        return 1
    fi

    # Validate suffix
    if [[ "$suffix" != "latest" ]] && [[ ! "$suffix" =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}_[0-9]{2}-[0-9]{2}$ ]]; then
        echo "ERROR: bench_build_filename: invalid suffix '$suffix' (expected 'latest' or 'YYYY-MM-DD_HH-MM')" >&2
        return 1
    fi

    echo "${platform}_${operation}_${variant_slug}_${suffix}.csv"
}

bench_validate_filename() {
    local filename="$1"

    # Strip any directory component — validate only the basename
    filename="$(basename "$filename")"

    # Full regex for valid filenames
    local full_regex='^(sim|hw)_(mul|kem|keccak|decode|compiler_compare|vlen|validation)_[a-z0-9]+(-[a-z0-9]+)*_(latest|[0-9]{4}-[0-9]{2}-[0-9]{2}_[0-9]{2}-[0-9]{2})\.csv$'

    if [[ "$filename" =~ $full_regex ]]; then
        return 0
    fi

    # Determine which segment is invalid for a helpful error message.

    # Check .csv extension
    if [[ "$filename" != *.csv ]]; then
        echo "ERROR: bench_validate_filename: filename '$filename' does not end with '.csv'" >&2
        return 1
    fi

    # Strip .csv extension for segment analysis
    local stem="${filename%.csv}"

    # Split on underscores — but we need to be careful because operation can contain
    # underscores (compiler_compare) and the timestamp contains underscores.
    # Strategy: validate segments left-to-right.

    # Extract platform (first segment before first underscore)
    local platform="${stem%%_*}"
    local rest="${stem#*_}"

    case "$platform" in
        sim|hw) ;;
        *)
            echo "ERROR: bench_validate_filename: invalid platform '$platform' in '$filename' (expected 'sim' or 'hw')" >&2
            return 1
            ;;
    esac

    # Extract operation — try multi-word operations first (compiler_compare)
    local operation=""
    local after_op=""
    if [[ "$rest" == compiler_compare_* ]]; then
        operation="compiler_compare"
        after_op="${rest#compiler_compare_}"
    else
        operation="${rest%%_*}"
        after_op="${rest#*_}"
    fi

    case "$operation" in
        mul|kem|keccak|decode|compiler_compare|vlen|validation) ;;
        *)
            echo "ERROR: bench_validate_filename: invalid operation '$operation' in '$filename' (expected one of: mul, kem, keccak, decode, compiler_compare, vlen, validation)" >&2
            return 1
            ;;
    esac

    # The remaining part is: {variant-slug}_{suffix}
    # suffix is either "latest" or "YYYY-MM-DD_HH-MM" (which contains an underscore)
    # Try to extract suffix from the end.
    local variant_slug=""
    local suffix=""

    # Check if it ends with a timestamp pattern: YYYY-MM-DD_HH-MM
    local ts_regex='[0-9]{4}-[0-9]{2}-[0-9]{2}_[0-9]{2}-[0-9]{2}$'
    if [[ "$after_op" =~ _([0-9]{4}-[0-9]{2}-[0-9]{2}_[0-9]{2}-[0-9]{2})$ ]]; then
        suffix="${BASH_REMATCH[1]}"
        variant_slug="${after_op%_${suffix}}"
    elif [[ "$after_op" =~ _latest$ ]]; then
        suffix="latest"
        variant_slug="${after_op%_latest}"
    else
        echo "ERROR: bench_validate_filename: invalid suffix in '$filename' (expected 'latest' or 'YYYY-MM-DD_HH-MM')" >&2
        return 1
    fi

    # Validate variant-slug
    if [[ ! "$variant_slug" =~ ^[a-z0-9]+(-[a-z0-9]+)*$ ]]; then
        echo "ERROR: bench_validate_filename: invalid variant-slug '$variant_slug' in '$filename' (must match [a-z0-9]+(-[a-z0-9]+)*)" >&2
        return 1
    fi

    # If we get here, all segments parsed correctly but the full regex didn't match.
    # This shouldn't happen, but handle it gracefully.
    echo "ERROR: bench_validate_filename: filename '$filename' does not match expected pattern" >&2
    return 1
}
