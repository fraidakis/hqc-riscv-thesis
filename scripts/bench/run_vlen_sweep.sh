#!/usr/bin/env bash
# scripts/bench/run_vlen_sweep.sh
# VLEN sweep benchmark — groups the VLEN scaling curve (256, 512, 1024, 2048)
# per security level so each output file captures all VLEN points for one level.
#
# Output layout:
#   analysis/benchmarks/vlen_sweep/vector/<variant>/level<L>_latest.csv
#   analysis/benchmarks/vlen_sweep/vector/<variant>/archive/<timestamp>.csv
#
# CSV schema: vlen,op,instructions
#
# Usage:
#   ./scripts/bench/run_vlen_sweep.sh [--mul <regex>] [--level <N|"128 192 256"|all>]
#
# Notes:
#   - Uses full KEM benchmark targets (bench_hqc<level>_<variant>)
#   - Sweeps runtime VLEN via Spike ISA (`zvl${vlen}b`)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${SCRIPT_DIR}/lib_bench.sh"

RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
SPIKE="${RISCV}/bin/spike"

VARIANTS=("ref_vector" "karatsuba_rvv" "karatsuba_zvbc")
LEVELS=(128 192 256)
VLENS=(256 512 1024 2048)
VLEN_LIST_CMAKE="$(IFS=';'; echo "${VLENS[*]}")"

GIT_HASH="$(git rev-parse --short HEAD 2>/dev/null || echo 'unknown')"

MUL_FILTER=""
LEVEL_FILTER="all"

usage() {
    cat <<'EOF'
Usage:
  run_vlen_sweep.sh [--mul <regex>] [--level <N|"128 192 256"|all>]

Options:
  --mul, --variant <regex>   Filter polymul variant(s) using regex.
  --level, --levels <value>  Select level(s): 128 | 192 | 256 | "128 192" | all.
  --help                     Show this help.

Env:
    RISCV                      Toolchain root (default: $HOME/Thesis/installed-tools)
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --mul|--variant)
            MUL_FILTER="${2:-}"
            shift 2
            ;;
        --level|--levels)
            LEVEL_FILTER="${2:-all}"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --*)
            echo "ERROR: Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
        *)
            echo "ERROR: Unexpected positional argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ "$MUL_FILTER" == "all" ]]; then
    MUL_FILTER=""
fi

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

format_int_dot() {
    local n="${1:-0}"
    if [[ ! "$n" =~ ^[0-9]+$ ]]; then
        echo "$n"
        return
    fi
    awk -v n="$n" 'BEGIN {
        out = ""
        while (n >= 1000) {
            out = sprintf(".%03d", n % 1000) out
            n = int(n / 1000)
        }
        printf "%d%s", n, out
    }'
}

selected_levels=()
if [[ "$LEVEL_FILTER" == "all" ]]; then
    selected_levels=("${LEVELS[@]}")
else
    level_tokens="${LEVEL_FILTER//,/ }"
    for lvl in $level_tokens; do
        case "$lvl" in
            128|192|256) selected_levels+=("$lvl") ;;
            *)
                echo "ERROR: Unsupported level '$lvl' (allowed: 128, 192, 256, all)" >&2
                exit 2
                ;;
        esac
    done
fi

if [[ ${#selected_levels[@]} -eq 0 ]]; then
    echo "ERROR: No levels selected." >&2
    exit 2
fi

# Verify Spike is available.
if [[ ! -x "$SPIKE" ]]; then
    echo "ERROR: Spike not found at ${SPIKE}" >&2
    echo "       Source toolchain/toolchain.env or set RISCV" >&2
    exit 2
fi

for variant in "${VARIANTS[@]}"; do
    if ! bench_variant_matches "$variant" "$MUL_FILTER"; then
        continue
    fi

    # Determine keccak/decode impls for this variant via the shared mapper.
    bench_parse_track_args --mul "$variant"

    local_keccak="$KECCAK_IMPL"
    local_decode="$DECODE_IMPL"
    variant_slug="${variant//_/-}"

    # Full-KEM VLEN sweep should isolate polymul scaling. The RVV decoder path
    # currently hangs for VLEN > 256 on this benchmark flow, so pin decode to ref.
    if [[ "$local_decode" == "rvv" ]]; then
        local_decode="ref"
    fi

    BUILD_DIR="${REPO_ROOT}/build/spike/bench_k${local_keccak}_d${local_decode}"

    # Ensure this build dir exposes the full requested VLEN target matrix.
    if ! cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
            -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
            -DCMAKE_BUILD_TYPE=Release \
            -DHQC_SIM=spike \
            -DHQC_KECCAK_IMPL="$local_keccak" \
            -DHQC_DECODE_IMPL="$local_decode" \
            -DVLEN_LIST="$VLEN_LIST_CMAKE" >/dev/null 2>&1; then
        echo "[vlen_sweep] AUTO-CONFIG FAIL ${BUILD_DIR}" >&2
        continue
    fi

    for level in "${selected_levels[@]}"; do
        out_dir="${REPO_ROOT}/analysis/benchmarks/vlen_sweep/vector/${variant}"
        csv_latest="${out_dir}/vlen_sweep_L${level}_${variant_slug}_latest.csv"
        csv_run="${out_dir}/vlen_sweep_L${level}_${variant_slug}_${GIT_HASH}_$(date +'%Y-%m-%d_%H-%M').csv"

        tmp_csv="$(mktemp)"

        # Write metadata header + column header into temp CSV.
        {
            echo "# date: $(date +'%Y-%m-%d %H:%M')"
            echo "# commit: ${GIT_HASH}"
            echo "# variant: ${variant}"
            echo "# level: ${level}"
            echo "vlen,op,instructions"
        } > "$tmp_csv"

        echo ""
        echo "📦 ${variant} (HQC-${level})"

        target="bench_hqc${level}_${variant}"
        elf="${BUILD_DIR}/${target}.elf"

        # Build the full KEM bench ELF once per (variant, level).
        if ! bench_auto_configure_and_build \
                "$BUILD_DIR" "$REPO_ROOT" "$target" "spike" "0" "vlen_sweep"; then
            echo "    [HQC-${level}] SKIP (build failed for ${target})"
            continue
        fi

        if [[ ! -f "$elf" ]]; then
            echo "    [HQC-${level}] SKIP (ELF missing after build for ${target})"
            continue
        fi

        for i in "${!VLENS[@]}"; do
            vlen="${VLENS[$i]}"

            # ISA must include the zvl constraint matching the VLEN being tested.
            isa="rv64gcv_zbc_zbb_zbkb_zvbc_zvbb_zvl${vlen}b_zicntr_zihpm"

            branch=" ├──"
            if [[ "$i" -eq $((${#VLENS[@]} - 1)) ]]; then
                branch=" └──"
            fi
            printf "%s VLEN=%4s ⋯" "${branch}" "${vlen}"

            # Run Spike; capture raw bench output.
            tmp_log="$(mktemp)"
            if ! "$SPIKE" --isa="$isa" "$elf" > "$tmp_log" 2>/dev/null; then
                echo " SKIP (Spike error)"
                rm -f "$tmp_log"
                continue
            fi

            kp_inst="$(awk -F',' '$1=="keypair" {print $7; exit}' "$tmp_log")"
            enc_inst="$(awk -F',' '$1=="encaps"  {print $7; exit}' "$tmp_log")"
            dec_inst="$(awk -F',' '$1=="decaps"  {print $7; exit}' "$tmp_log")"

            kp_inst="${kp_inst:-0}"
            enc_inst="${enc_inst:-0}"
            dec_inst="${dec_inst:-0}"
            total_instructions=$((kp_inst + enc_inst + dec_inst))

            rm -f "$tmp_log"

            echo "${vlen},keypair,${kp_inst}" >> "$tmp_csv"
            echo "${vlen},encaps,${enc_inst}" >> "$tmp_csv"
            echo "${vlen},decaps,${dec_inst}" >> "$tmp_csv"
            echo "${vlen},total,${total_instructions}" >> "$tmp_csv"

            kp_fmt="$(format_int_dot "$kp_inst")"
            enc_fmt="$(format_int_dot "$enc_inst")"
            dec_fmt="$(format_int_dot "$dec_inst")"
            total_fmt="$(format_int_dot "$total_instructions")"

            printf " Kp: %s | Enc: %11s | Dec: %11s | Total: %11s\n" \
                "$kp_fmt" "$enc_fmt" "$dec_fmt" "$total_fmt"
        done

        # Commit the temp CSV to the archival system.
        bench_refresh_tracked_csv "$tmp_csv" "$csv_latest" || true
        cp -f "$csv_latest" "$csv_run" >/dev/null 2>&1 || true
        echo "  → ${csv_latest}"
    done
done

if [[ -n "$MUL_FILTER" ]]; then
    matched=0
    for variant in "${VARIANTS[@]}"; do
        if bench_variant_matches "$variant" "$MUL_FILTER"; then
            matched=1
            break
        fi
    done
    if [[ "$matched" -eq 0 ]]; then
        echo "WARNING: no variants matched --mul '$MUL_FILTER'"
    fi
fi

echo ""
echo "✔ VLEN sweep complete."
