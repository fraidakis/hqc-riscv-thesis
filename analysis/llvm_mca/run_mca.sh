#!/usr/bin/env bash
# analysis/llvm_mca/run_mca.sh
# Run LLVM-MCA on HQC source functions to get throughput/latency estimates.
#
# Usage (single file):
#   run_mca.sh <source_file> [hqc_level=128|192|256] [extra_mca_flags...]
#   Example: ./analysis/llvm_mca/run_mca.sh vendor/hqc-ref/src/ref/gf2x.c 128
#            ./analysis/llvm_mca/run_mca.sh src/poly_mul/scalar/fafft_zbc/fafft.c 128 --mattr=+zbc
#
# Usage (all variants):
#   run_mca.sh --all [hqc_level=128|192|256] [extra_mca_flags...]
#   Example: ./analysis/llvm_mca/run_mca.sh --all 128

set -euo pipefail

# ── Configuration ────────────────────────────────────────────────────────────
RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
CLANG="$RISCV/bin/clang"
MCA="$RISCV/bin/llvm-mca"
REPORT_DIR="analysis/llvm_mca/reports"
VARIANTS_FILE="scripts/implemented_variants.txt"

mkdir -p "$REPORT_DIR"

MARCH="rv64gcv_zbc_zbb_zbkb_zvbc"
MABI="lp64d"
TARGET="riscv64-unknown-elf"

# ── Variant → Source File Mapping ───────────────────────────────────────────
declare -A VARIANT_SOURCES=(
    [karatsuba_ref]="vendor/hqc-ref/src/ref/gf2x.c"
    [karatsuba_zbc]="src/poly_mul/scalar/karatsuba_zbc/gf2x_clmul.c"
    [fafft_zbc]="src/poly_mul/scalar/fafft_zbc/fafft.c"
    [hybrid]="src/poly_mul/scalar/hybrid_zbc/hybrid.c"
    [fafft_zvbc]="src/poly_mul/vector/fafft_zvbc/fafft_vclmul.c"
    [karatsuba_zvbc]="src/poly_mul/vector/karatsuba_zvbc/gf2x_vclmul.c"
)

# ── Variant-specific compilation flags ─────────────────────────────────────
declare -A VARIANT_DEFINES=(
    [karatsuba_ref]=""
    [karatsuba_zbc]="-DHQC_POLYMUL_ZBC=1"
    [fafft_zbc]="-DHQC_POLYMUL_ZBC=1"
    [hybrid]="-DHQC_POLYMUL_ZBC=1"
    [fafft_zvbc]="-DHQC_POLYMUL_ZVBC=1"
    [karatsuba_zvbc]="-DHQC_POLYMUL_ZVBC=1"
)

# ── Helper Functions ────────────────────────────────────────────────────────
compile_and_analyze() {
    local SOURCE="$1"
    local VARIANT="$2"
    local LEVEL="$3"
    local EXTRA_FLAGS="${4:-}"

    # Map level to hqc-N directory names (hqc-1=128, hqc-3=192, hqc-5=256)
    local HQC_DIR
    case "$LEVEL" in
        128) HQC_DIR="hqc-1" ;;
        192) HQC_DIR="hqc-3" ;;
        256) HQC_DIR="hqc-5" ;;
        *)
            echo "[ERROR] Invalid security level: $LEVEL" >&2
            return 1
            ;;
    esac

    local BASENAME="${VARIANT}_hqc${LEVEL}"
    local ASM_FILE="$REPORT_DIR/${BASENAME}.s"
    local JSON_REPORT="$REPORT_DIR/${BASENAME}_mca.json"
    local TXT_REPORT="$REPORT_DIR/${BASENAME}_mca.txt"

    # Get variant-specific defines
    local VARIANT_DEFS="${VARIANT_DEFINES[$VARIANT]:-}"

    echo "[llvm-mca] Compiling: $VARIANT (HQC-$LEVEL) from $SOURCE"
    "$CLANG" \
        --target="$TARGET" \
        -march="$MARCH" \
        -mabi="$MABI" \
        -menable-experimental-extensions \
        -O3 \
        -ffreestanding -fno-builtin \
        -Iinclude \
        -Ivendor/hqc-ref/src/ref \
        -Ivendor/hqc-ref/src/ref/$HQC_DIR \
        -Ivendor/hqc-ref/src/common/$HQC_DIR \
        -DHQC_$LEVEL \
        $VARIANT_DEFS \
        -S -o "$ASM_FILE" \
        "$SOURCE"

    echo "[llvm-mca] Running MCA analysis on: $ASM_FILE"
    "$MCA" \
        -march=riscv64 \
        -mcpu=sifive-u74 \
        --timeline \
        --iterations=10 \
        --skip-unsupported-instructions=lack-sched \
        $EXTRA_FLAGS \
        "$ASM_FILE" > "$TXT_REPORT"

    # Extract key metrics and save as structured JSON
    extract_mca_metrics "$TXT_REPORT" "$JSON_REPORT" "$VARIANT" "$LEVEL"

    echo "[llvm-mca] Reports written:"
    echo "  Text:   $TXT_REPORT"
    echo "  JSON:   $JSON_REPORT"
    head -30 "$TXT_REPORT"
}

extract_mca_metrics() {
    local TXT_REPORT="$1"
    local JSON_REPORT="$2"
    local VARIANT="$3"
    local LEVEL="$4"

    # Parse key metrics from LLVM-MCA text report
    local THROUGHPUT=$(grep -oP 'Block RThroughput: \K[0-9.]+' "$TXT_REPORT" | head -1 || echo "N/A")
    local TOTAL_CYCLES=$(grep -oP 'Total Cycles:\s+\K[0-9]+' "$TXT_REPORT" | head -1 || echo "0")
    local TOTAL_UOPS=$(grep -oP 'Total uOps:\s+\K[0-9]+' "$TXT_REPORT" | head -1 || echo "0")
    
    # Create JSON report
    cat > "$JSON_REPORT" << EOF
{
  "variant": "$VARIANT",
  "hqc_level": $LEVEL,
  "timestamp": "$(date -Iseconds)",
  "metrics": {
    "block_rthroughput": "$THROUGHPUT",
    "total_cycles": $TOTAL_CYCLES,
    "total_uops": $TOTAL_UOPS,
    "source_file": "$(basename "$SOURCE")"
  },
  "analysis_tool": "llvm-mca",
  "cpu_model": "sifive-u74",
  "march": "rv64gcv_zbc_zbb_zbkb_zvbc"
}
EOF

    echo "[llvm-mca] Metrics saved to JSON: $JSON_REPORT"
}

# ── Main Entry Point ───────────────────────────────────────────────────────
if [[ $# -eq 0 ]]; then
    echo "Usage:" >&2
    echo "  $0 <source_file> [hqc_level] [extra_mca_flags...]" >&2
    echo "  $0 --all [hqc_level] [extra_mca_flags...]" >&2
    exit 1
fi

if [[ "$1" == "--all" ]]; then
    # Process all variants
    shift
    LEVEL="${1:-128}"
    if [[ ! "$LEVEL" =~ ^(128|192|256)$ ]]; then
        echo "[ERROR] Invalid security level: $LEVEL" >&2
        exit 1
    fi
    shift || true
    EXTRA_FLAGS="${@:-}"

    echo "[llvm-mca] Running analysis for all variants at HQC-$LEVEL..."
    FAILED=()
    
    while IFS= read -r VARIANT; do
        VARIANT=$(echo "$VARIANT" | xargs)  # trim whitespace
        [[ -z "$VARIANT" ]] && continue
        
        if [[ -z "${VARIANT_SOURCES[$VARIANT]:-}" ]]; then
            echo "[WARNING] Unknown variant: $VARIANT" >&2
            continue
        fi

        SOURCE="${VARIANT_SOURCES[$VARIANT]}"
        if [[ ! -f "$SOURCE" ]]; then
            echo "[ERROR] Source file not found: $SOURCE" >&2
            FAILED+=("$VARIANT")
            continue
        fi

        if ! compile_and_analyze "$SOURCE" "$VARIANT" "$LEVEL" "$EXTRA_FLAGS"; then
            FAILED+=("$VARIANT")
        fi
        echo ""
    done < "$VARIANTS_FILE"

    if [[ ${#FAILED[@]} -gt 0 ]]; then
        echo "[ERROR] Failed variants: ${FAILED[*]}" >&2
        exit 1
    fi

    echo "[llvm-mca] All variants analyzed successfully."
    exit 0
else
    # Single file mode (backward compatible)
    SOURCE="$1"
    shift
    LEVEL="128"
    if [[ "${1:-}" =~ ^(128|192|256)$ ]]; then
        LEVEL="$1"
        shift
    fi
    EXTRA_FLAGS="${@:-}"

    VARIANT=$(basename "${SOURCE%.*}")
    compile_and_analyze "$SOURCE" "$VARIANT" "$LEVEL" "$EXTRA_FLAGS"
fi
