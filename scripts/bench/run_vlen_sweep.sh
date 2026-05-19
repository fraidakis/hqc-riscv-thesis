#!/usr/bin/env bash
# scripts/bench/run_vlen_sweep.sh
# VLEN sweep benchmark — groups the VLEN scaling curve (256, 512, 1024, 2048)
# per security level so each output file captures all VLEN points for one level.
#
# Output layout:
#   analysis/benchmarks/vlen_sweep/vector/<variant>/level<L>_latest.csv
#   analysis/benchmarks/vlen_sweep/vector/<variant>/archive/<timestamp>.csv
#
# CSV schema: level,vlen,op,instructions
#
# Usage:
#   ./scripts/bench/run_vlen_sweep.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${SCRIPT_DIR}/lib_bench.sh"

RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
SPIKE="${RISCV}/bin/spike"

VARIANTS=("ref_vector" "karatsuba_rvv" "karatsuba_zvbc")
LEVELS=(128 192 256)
VLENS=(256 512 1024 2048)

GIT_HASH="$(git rev-parse --short HEAD 2>/dev/null || echo 'unknown')"

# Verify Spike is available.
if [[ ! -x "$SPIKE" ]]; then
    echo "ERROR: Spike not found at ${SPIKE}" >&2
    echo "       Source toolchain/toolchain.env or set RISCV" >&2
    exit 2
fi

for variant in "${VARIANTS[@]}"; do
    # Determine keccak/decode impls for this variant via the shared mapper.
    bench_parse_track_args --mul "$variant"

    local_keccak="$KECCAK_IMPL"
    local_decode="$DECODE_IMPL"

    BUILD_DIR="${REPO_ROOT}/build/spike/bench_k${local_keccak}_d${local_decode}"

    echo ""
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "  🔬 VLEN SWEEP  variant=${variant}  keccak=${local_keccak}  decode=${local_decode}"
    echo "╚══════════════════════════════════════════════════════════════╝"

    for level in "${LEVELS[@]}"; do
        out_dir="${REPO_ROOT}/analysis/benchmarks/vlen_sweep/vector/${variant}"
        csv_out="${out_dir}/level${level}_latest.csv"

        tmp_csv="$(mktemp)"

        # Write metadata header + column header into temp CSV.
        {
            echo "# date: $(date +'%Y-%m-%d %H:%M')"
            echo "# commit: ${GIT_HASH}"
            echo "# variant: ${variant}"
            echo "# level: ${level}"
            echo "level,vlen,op,instructions"
        } > "$tmp_csv"

        echo ""
        echo "  ── HQC-${level} ──"

        for vlen in "${VLENS[@]}"; do
            target="bench_polymul_hqc${level}_${variant}_vlen${vlen}"
            elf="${BUILD_DIR}/${target}.elf"

            # ISA must include the zvl constraint matching the VLEN being tested.
            isa="rv64gcv_zbc_zbb_zbkb_zvbc_zvbb_zvl${vlen}b_zicntr_zihpm"

            printf "    [VLEN=%4s] Build ..." "${vlen}"

            # Auto-configure and build the VLEN-parameterized ELF.
            if ! bench_auto_configure_and_build \
                    "$BUILD_DIR" "$REPO_ROOT" "$target" "spike" "0" "vlen_sweep"; then
                echo " SKIP (build failed)"
                continue
            fi

            if [[ ! -f "$elf" ]]; then
                echo " SKIP (ELF missing after build)"
                continue
            fi

            printf " Run ..."

            # Run Spike; capture raw bench output.
            tmp_log="$(mktemp)"
            if ! "$SPIKE" --isa="$isa" "$elf" > "$tmp_log" 2>/dev/null; then
                echo " SKIP (Spike error)"
                rm -f "$tmp_log"
                continue
            fi

            # Sum median_instret (col 7) across all ops (keypair + encaps + decaps OR
            # vect_mul) to produce a single "total KEM instructions" figure.
            total_instructions="$(awk -F',' '
                /^#/ { next }
                NR == 1 { next }        # skip header row
                /^(keypair|encaps|decaps|vect_mul),/ {
                    sum += $7
                }
                END { print (sum > 0 ? sum : 0) }
            ' "$tmp_log")"

            rm -f "$tmp_log"

            echo "${level},${vlen},total_kem,${total_instructions}" >> "$tmp_csv"
            echo " ${total_instructions} instret"
        done

        # Commit the temp CSV to the archival system.
        bench_refresh_tracked_csv "$tmp_csv" "$csv_out" || true
        echo "  → ${csv_out}"
    done
done

echo ""
echo "✔ VLEN sweep complete."
