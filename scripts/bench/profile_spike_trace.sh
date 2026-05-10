#!/usr/bin/env bash
# Automate Spike deterministic tracing and profiling

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <elf-file>"
    exit 1
fi

ELF=$1
BASENAME=$(basename "$ELF" .elf)
OUTDIR="analysis/benchmarks/hotspot_profile/${BASENAME}"
MIX_DIR="analysis/benchmarks/instruction_mix"

MIX_CSV="${MIX_DIR}/mix_${BASENAME}.csv"
if [[ "$BASENAME" =~ hqc(128|192|256)_([a-zA-Z0-9_]+)$ ]]; then
    LEVEL="${BASH_REMATCH[1]}"
    VARIANT="${BASH_REMATCH[2]}"
    MIX_CSV="${MIX_DIR}/mix_hqc${LEVEL}_${VARIANT}.csv"
fi

mkdir -p "$OUTDIR" "$MIX_DIR"

# Ensure RISCV environment is set
if [ -z "${RISCV:-}" ]; then
    echo "Please source toolchain/toolchain.env first"
    exit 1
fi

echo "=========================================="
echo "1. Generating trace for $BASENAME..."
echo "=========================================="
$RISCV/bin/spike -l --isa=rv64gcv_zbc_zbb_zbkb_zvbc "$ELF" 2> "$OUTDIR/spike_trace.log"

echo "=========================================="
echo "2. Extracting PCs..."
echo "=========================================="
grep "core   0:" "$OUTDIR/spike_trace.log" | awk '{print $3}' | sed 's/^(//' | sed 's/)//' | sort | uniq -c > "$OUTDIR/pc_counts_all.txt"

echo "=========================================="
echo "3. Mapping PCs to C functions..."
echo "=========================================="
python3 "$REPO_ROOT/scripts/bench/map_pcs.py" "$ELF" "$OUTDIR"
rm -f "$OUTDIR/pc_counts_all.txt"

echo "=========================================="
echo "4. Building instruction mix CSV..."
echo "=========================================="
python3 "$REPO_ROOT/scripts/report/generate_instruction_mix.py" \
    "$OUTDIR/spike_trace.log" \
    --label "$BASENAME" \
    --output "$MIX_CSV"

echo "=========================================="
echo "Done. Results saved to $OUTDIR/"
echo "Top 10 Functions by executed instructions:"
echo "=========================================="
cat "$OUTDIR/final_function_counts.txt" | head -n 11
echo "Instruction mix CSV: $MIX_CSV"
rm -f "$OUTDIR/spike_trace.log"
