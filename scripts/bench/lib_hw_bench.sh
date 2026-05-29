#!/usr/bin/env bash
# scripts/bench/lib_hw_bench.sh
# Hardware-specific helpers for the BPI-F3 benchmark pipeline.
# Sourced by run_hw_benchmarks.sh.

# Source shared bench library for bench_refresh_tracked_csv.
_LIB_HW_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib_bench.sh
source "${_LIB_HW_DIR}/lib_bench.sh"

# ── variant_to_dir ───────────────────────────────────────────────────────────
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

# ── hw_variant_slug ──────────────────────────────────────────────────────────
# Convert a variant name to slug format (replace '_' with '-').
#   hw_variant_slug "karatsuba_rvv"  →  "karatsuba-rvv"
hw_variant_slug() {
    local variant="$1"
    echo "${variant//_/-}"
}

# ── hw_csv_header ────────────────────────────────────────────────────────────
# Generate the metadata comment block for a hardware CSV.
#
# Arguments:
#   $1 - variant    (e.g. karatsuba_rvv)
#   $2 - n_iter     (e.g. 100)
#   $3 - n_warmup   (e.g. 10)
#   $4 - mode       (dev | publish)
#   $5 - cov_cycles (float, optional — only included in publish mode)
#
# Outputs comment lines to stdout.
hw_csv_header() {
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
}

# ── hw_store_result ──────────────────────────────────────────────────────────
# Write CSV content to the per-variant subdirectory with a timestamp, then
# delegate to bench_refresh_tracked_csv for symlink/archive management.
#
# Arguments:
#   $1 - csv_content  (full CSV text including header comments + data rows)
#   $2 - variant      (e.g. karatsuba_rvv)
#   $3 - op           (e.g. mul, kem)
#   $4 - timestamp    (e.g. 2025-06-01_14-30)
#
# The storage path model (split by operation type):
#   kem files → results/hardware/benchmarks/kem/{new_variant}/hw_kem_{slug}_{timestamp}.csv
#   mul files → results/hardware/benchmarks/components/poly_mul/{new_variant}/hw_mul_{slug}_{timestamp}.csv
#
# Returns: the path written (via stdout from bench_refresh_tracked_csv).
hw_store_result() {
    local csv_content="$1"
    local variant="$2"
    local op="$3"
    local timestamp="$4"

    local slug
    slug="$(hw_variant_slug "$variant")"

    local new_variant_dir
    new_variant_dir="$(variant_to_dir "$variant")"

    local repo_root
    repo_root="$(cd "${_LIB_HW_DIR}/../.." && pwd)"

    # Route to the appropriate subdirectory based on operation type.
    local variant_dir
    if [[ "$op" == "kem" ]]; then
        variant_dir="${repo_root}/results/hardware/benchmarks/kem/${new_variant_dir}"
    else
        variant_dir="${repo_root}/results/hardware/benchmarks/components/poly_mul/${new_variant_dir}"
    fi
    mkdir -p "$variant_dir"

    local timestamped_file="${variant_dir}/hw_${op}_${slug}_${timestamp}.csv"
    local latest_file="${variant_dir}/hw_${op}_${slug}_latest.csv"

    # Write the CSV content to the timestamped file.
    echo "$csv_content" > "$timestamped_file"

    # Delegate to bench_refresh_tracked_csv for symlink + archive management.
    bench_refresh_tracked_csv "$timestamped_file" "$latest_file"
}
