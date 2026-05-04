#!/usr/bin/env bash
# Automate Spike deterministic tracing and profiling

set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <elf-file>"
    exit 1
fi

ELF=$1
BASENAME=$(basename "$ELF" .elf)
OUTDIR="analysis/benchmarks/profile_${BASENAME}"

mkdir -p "$OUTDIR"

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
cat << 'PYEOF' > "$OUTDIR/map_pcs.py"
import sys
import subprocess
from collections import defaultdict

if len(sys.argv) != 3:
    print("Usage: map_pcs.py <elf_file> <out_dir>")
    sys.exit(1)

elf_file = sys.argv[1]
out_dir = sys.argv[2]

counts = {}
with open(f'{out_dir}/pc_counts_all.txt', 'r') as f:
    for line in f:
        parts = line.strip().split()
        if len(parts) == 2:
            counts[parts[1]] = int(parts[0])

pcs = list(counts.keys())
cmd = ['riscv64-unknown-elf-addr2line', '-e', elf_file, '-f', '-p'] + pcs

result = subprocess.run(cmd, stdout=subprocess.PIPE, text=True)
output = result.stdout.strip().split('\n')

funcs = defaultdict(int)

for pc, line in zip(pcs, output):
    func_name = line.split(" at ")[0].strip()
    funcs[func_name] += counts[pc]

with open(f'{out_dir}/final_function_counts.txt', 'w') as f:
    f.write("--- FUNCTION TOTALS ---\n")
    for func, c in sorted(funcs.items(), key=lambda x: x[1], reverse=True):
        f.write(f"{func}: {c}\n")
PYEOF

python3 "$OUTDIR/map_pcs.py" "$ELF" "$OUTDIR"

echo "=========================================="
echo "Done. Results saved to $OUTDIR/"
echo "Top 10 Functions by executed instructions:"
echo "=========================================="
cat "$OUTDIR/final_function_counts.txt" | head -n 11
