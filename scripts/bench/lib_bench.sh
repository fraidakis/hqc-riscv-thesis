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

bench_load_implemented_variants() {
    local repo_root="$1"
    mapfile -t IMPLEMENTED_VARIANTS < "${repo_root}/scripts/implemented_variants.txt"
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
    
    # If CMakeCache.txt already exists, just build the target
    if [[ -f "${build_dir}/CMakeCache.txt" ]]; then
        if ! cmake --build "$build_dir" --target "$target" -j"$(nproc)" >/dev/null 2>&1; then
            echo "[${tag}] AUTO-BUILD FAIL ${target}" >&2
            return 1
        fi
        return 0
    fi
    
    # Otherwise, auto-configure the build directory
    echo " ├── ⚙️ Auto-configuring missing build dir: ${build_dir}..."
    
    # Construct the cmake command based on sim_type and trace_mode
    local cmake_cmd="cmake -S \"$repo_root\" -B \"$build_dir\" \
        -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
        -DHQC_SIM=${sim_type}"
    
    # Add trace mode flag if needed
    if [[ "$trace_mode" == "1" ]]; then
        cmake_cmd="${cmake_cmd} -DHQC_TRACE_MODE=1"
    fi
    
    # Execute cmake configuration
    if ! eval "$cmake_cmd" >/dev/null 2>&1; then
        echo "[${tag}] AUTO-CONFIG FAIL ${build_dir}" >&2
        return 1
    fi
    
    # Now build the target
    if ! cmake --build "$build_dir" --target "$target" -j"$(nproc)" >/dev/null 2>&1; then
        echo "[${tag}] AUTO-BUILD FAIL ${target}" >&2
        return 1
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
        BEGIN { in_preamble = 1 }
        {
            if (in_preamble && $0 ~ /^#/) {
                next
            }
            in_preamble = 0
            print
        }
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