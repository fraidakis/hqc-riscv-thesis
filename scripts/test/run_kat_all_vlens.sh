#!/usr/bin/env bash
# scripts/test/run_kat_all_vlens.sh
#
# Task 9: Final checkpoint — Full KAT suite at all VLENs
#
# Runs the full KAT suite (100 vectors × 3 security levels = 300 total)
# at each VLEN in {256, 512, 1024, 2048} using the karatsuba_zvbc variant
# with zvbb_x2_soa Keccak and zvbc decode.
#
# Verifies all 1200 test vectors pass (300 per VLEN × 4 VLENs).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

SPIKE="${RISCV:-$HOME/Thesis/installed-tools}/bin/spike"
VLENS=(256 512 1024 2048)
LEVELS=(128 192 256)
POLYMUL="karatsuba_zvbc"
KECCAK_IMPL="zvbb_x2_soa"
DECODE_IMPL="zvbc"

# Results tracking
declare -A RESULTS
TOTAL_PASS=0
TOTAL_FAIL=0
TOTAL_TESTS=0

echo "╔══════════════════════════════════════════════════════════════════╗"
echo "║  Task 9: Full KAT Suite — All VLENs × All Security Levels      ║"
echo "║  Variant: karatsuba_zvbc + zvbb_x2_soa + zvbc                   ║"
echo "╚══════════════════════════════════════════════════════════════════╝"
echo ""
echo "Spike: $SPIKE"
echo "VLENs: ${VLENS[*]}"
echo "Levels: ${LEVELS[*]}"
echo ""

# Verify spike exists
if [[ ! -x "$SPIKE" ]]; then
    echo "ERROR: Spike not found at $SPIKE" >&2
    exit 1
fi

for VLEN in "${VLENS[@]}"; do
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "  VLEN = ${VLEN}"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    # ISA string with the correct zvl<VLEN>b extension
    ISA="rv64gcv_zbc_zbb_zvbc_zvbb_zvl${VLEN}b_zicntr_zihpm"

    # Build directory for this VLEN
    BUILD_DIR="${REPO_ROOT}/build/spike/kat_vlen${VLEN}_kzvbb_x2_soa_dzvbc"

    # Configure if needed
    if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]] || \
       ! grep -q "HQC_VECTOR_BITS:STRING=${VLEN}" "${BUILD_DIR}/CMakeCache.txt" 2>/dev/null; then
        echo "  ⚙️  Configuring build for VLEN=${VLEN}..."
        cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
            -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
            -DCMAKE_BUILD_TYPE=Release \
            -DHQC_SIM=spike \
            -DHQC_KECCAK_IMPL="${KECCAK_IMPL}" \
            -DHQC_DECODE_IMPL="${DECODE_IMPL}" \
            -DHQC_SCALAR_MARCH=rv64gc_zbc_zbb \
            -DHQC_VECTOR_BITS="${VLEN}" \
            -DVLEN_LIST="${VLEN}" \
            > /dev/null 2>&1
        echo "  ✓ Configured"
    else
        echo "  ✓ Build dir already configured for VLEN=${VLEN}"
    fi

    for LEVEL in "${LEVELS[@]}"; do
        # Use the per-VLEN KAT target
        TARGET="hqc${LEVEL}_${POLYMUL}_vlen${VLEN}"
        ELF="${BUILD_DIR}/${TARGET}.elf"
        RSP="${REPO_ROOT}/tests/kat/PQCkemKAT_${LEVEL}.rsp"

        TOTAL_TESTS=$((TOTAL_TESTS + 1))

        echo -n "  [HQC-${LEVEL}] Building ${TARGET}... "

        # Build the target
        if ! cmake --build "$BUILD_DIR" --target "$TARGET" -j"$(nproc)" > /dev/null 2>&1; then
            echo "BUILD FAIL"
            RESULTS["${VLEN}_${LEVEL}"]="BUILD_FAIL"
            TOTAL_FAIL=$((TOTAL_FAIL + 1))
            continue
        fi

        if [[ ! -f "$ELF" ]]; then
            echo "ELF NOT FOUND"
            RESULTS["${VLEN}_${LEVEL}"]="NO_ELF"
            TOTAL_FAIL=$((TOTAL_FAIL + 1))
            continue
        fi
        echo "OK"

        # Run on Spike with the correct ISA
        LOG="$(mktemp /tmp/hqc_kat_vlen${VLEN}_L${LEVEL}_XXXXXX.log)"
        echo -n "  [HQC-${LEVEL}] Running on Spike (ISA: ${ISA})... "

        if ! "$SPIKE" --isa="$ISA" "$ELF" > "$LOG" 2>/dev/null; then
            echo "SPIKE FAIL"
            RESULTS["${VLEN}_${LEVEL}"]="SPIKE_FAIL"
            TOTAL_FAIL=$((TOTAL_FAIL + 1))
            rm -f "$LOG"
            continue
        fi
        echo "OK"

        # Verify with kat_check.py
        echo -n "  [HQC-${LEVEL}] Verifying KAT vectors... "
        if python3 "${REPO_ROOT}/tests/kat/kat_check.py" \
            --actual "$LOG" \
            --rsp "$RSP" \
            --level "$LEVEL" 2>/dev/null; then
            RESULTS["${VLEN}_${LEVEL}"]="PASS"
            TOTAL_PASS=$((TOTAL_PASS + 1))
        else
            echo ""
            RESULTS["${VLEN}_${LEVEL}"]="FAIL"
            TOTAL_FAIL=$((TOTAL_FAIL + 1))
        fi

        rm -f "$LOG"
    done
    echo ""
done

# ── Results Table ─────────────────────────────────────────────────────────────
echo ""
echo "╔══════════════════════════════════════════════════════════════════╗"
echo "║                    KAT Results Summary                          ║"
echo "╠══════════╦══════════╦══════════╦══════════╦═════════════════════╣"
echo "║   VLEN   ║  HQC-128 ║  HQC-192 ║  HQC-256 ║  Vectors (pass)   ║"
echo "╠══════════╬══════════╬══════════╬══════════╬═════════════════════╣"

for VLEN in "${VLENS[@]}"; do
    r128="${RESULTS[${VLEN}_128]:-N/A}"
    r192="${RESULTS[${VLEN}_192]:-N/A}"
    r256="${RESULTS[${VLEN}_256]:-N/A}"

    # Count passes for this VLEN
    vlen_pass=0
    for LEVEL in "${LEVELS[@]}"; do
        if [[ "${RESULTS[${VLEN}_${LEVEL}]:-}" == "PASS" ]]; then
            vlen_pass=$((vlen_pass + 1))
        fi
    done
    vectors=$((vlen_pass * 100))

    printf "║  %4d    ║  %-6s  ║  %-6s  ║  %-6s  ║  %3d/300            ║\n" \
        "$VLEN" "$r128" "$r192" "$r256" "$vectors"
done

echo "╠══════════╬══════════╬══════════╬══════════╬═════════════════════╣"
total_vectors=$((TOTAL_PASS * 100))
printf "║  TOTAL   ║          ║          ║          ║  %4d/1200          ║\n" "$total_vectors"
echo "╚══════════╩══════════╩══════════╩══════════╩═════════════════════╝"
echo ""

if [[ "$TOTAL_FAIL" -eq 0 ]]; then
    echo "✅ ALL ${TOTAL_TESTS} KAT runs PASSED (${total_vectors}/1200 vectors verified)"
    exit 0
else
    echo "❌ ${TOTAL_FAIL}/${TOTAL_TESTS} KAT runs FAILED"
    exit 1
fi
