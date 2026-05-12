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