import sys
import subprocess
from collections import defaultdict

counts = {}
with open('pc_counts_all.txt', 'r') as f:
    for line in f:
        parts = line.strip().split()
        if len(parts) == 2:
            counts[parts[1]] = int(parts[0])

pcs = list(counts.keys())
cmd = ['riscv64-unknown-elf-addr2line', '-e', 'build-spike/bench_polymul_hqc128_karatsuba_zvbc.elf', '-f', '-p'] + pcs

print("Running addr2line on", len(pcs), "addresses...")

result = subprocess.run(cmd, stdout=subprocess.PIPE, text=True)
output = result.stdout.strip().split('\n')

funcs = defaultdict(int)

for pc, line in zip(pcs, output):
    # line format: "function_name at file:line" or similar
    func_name = line.split(" at ")[0].strip()
    funcs[func_name] += counts[pc]

print("--- FUNCTION TOTALS ---")
for f, c in sorted(funcs.items(), key=lambda x: x[1], reverse=True):
    print(f"{f}: {c}")

