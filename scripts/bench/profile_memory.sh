#!/usr/bin/env bash
# P-21: QEMU memory traffic profiling (load/store + estimated cache-line traffic).
#
# NOTE: qemu-system-riscv64 trace logs do not expose true cache hierarchy events.
# This script estimates cache-line traffic from executed load/store instruction mix.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

VARIANT="${1:-karatsuba_ref}"
LEVEL="${2:-128}"
ITER="${3:-1}"

if ! [[ "$LEVEL" =~ ^(128|192|256)$ ]]; then
    echo "[profile_memory] ERROR: level must be 128, 192, or 256" >&2
    exit 2
fi

RISCV="${RISCV:-$HOME/Thesis/toolchains/installed-tools}"
QEMU="${RISCV}/bin/qemu-system-riscv64"

if [[ ! -x "$QEMU" ]]; then
    echo "[profile_memory] ERROR: qemu-system-riscv64 not found at $QEMU" >&2
    exit 2
fi

BUILD_DIR="$REPO_ROOT/build/qemu_memprof_hqc${LEVEL}_${VARIANT}"
TARGET="bench_polymul_hqc${LEVEL}_${VARIANT}"
ELF="$BUILD_DIR/${TARGET}.elf"

cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
    -DHQC_SIM=qemu \
    -DBENCH_N_ITER="$ITER" >/dev/null

cmake --build "$BUILD_DIR" --target "$TARGET" -j"$(nproc)" >/dev/null

STAMP="$(date +%Y%m%d_%H%M%S)"
OUT_DIR="$REPO_ROOT/analysis/benchmarks/memory_profile"
mkdir -p "$OUT_DIR"
QEMU_LOG="$OUT_DIR/qemu_trace_hqc${LEVEL}_${VARIANT}_${STAMP}.log"
UART_LOG="$OUT_DIR/qemu_stdout_hqc${LEVEL}_${VARIANT}_${STAMP}.txt"
CSV_OUT="$OUT_DIR/memory_profile_hqc${LEVEL}_${VARIANT}_${STAMP}.csv"

"$QEMU" \
    -machine virt \
    -cpu rv64,v=true,vlen=256,elen=64,vext_spec=v1.0,zbkb=true,zbc=true,zvbc=true \
    -m 256M \
    -nographic \
    -bios none \
    -kernel "$ELF" \
    -serial mon:stdio \
    -no-reboot \
    -d in_asm,nochain \
    -D "$QEMU_LOG" > "$UART_LOG" 2>&1

# Count scalar + vector load/store instructions from disassembly lines.
read -r load_bytes store_bytes load_ins store_ins <<<"$(awk '
function add_load(bytes){lb+=bytes;li++}
function add_store(bytes){sb+=bytes;si++}
{
    line=tolower($0)
    if (line ~ /\<vle8\./) { add_load(8); next }
    if (line ~ /\<vle16\./) { add_load(16); next }
    if (line ~ /\<vle32\./) { add_load(32); next }
    if (line ~ /\<vle64\./) { add_load(64); next }
    if (line ~ /\<vse8\./) { add_store(8); next }
    if (line ~ /\<vse16\./) { add_store(16); next }
    if (line ~ /\<vse32\./) { add_store(32); next }
    if (line ~ /\<vse64\./) { add_store(64); next }

    if (line ~ /\<lb\>/ || line ~ /\<lbu\>/) { add_load(1); next }
    if (line ~ /\<lh\>/ || line ~ /\<lhu\>/) { add_load(2); next }
    if (line ~ /\<lw\>/ || line ~ /\<lwu\>/ || line ~ /\<flw\>/) { add_load(4); next }
    if (line ~ /\<ld\>/ || line ~ /\<fld\>/) { add_load(8); next }

    if (line ~ /\<sb\>/) { add_store(1); next }
    if (line ~ /\<sh\>/) { add_store(2); next }
    if (line ~ /\<sw\>/ || line ~ /\<fsw\>/) { add_store(4); next }
    if (line ~ /\<sd\>/ || line ~ /\<fsd\>/) { add_store(8); next }
}
END{printf "%d %d %d %d", lb, sb, li, si}
' "$QEMU_LOG")"

load_lines=$(( (load_bytes + 63) / 64 ))
store_lines=$(( (store_bytes + 63) / 64 ))

echo "level,variant,iter,load_instructions,store_instructions,load_bytes,store_bytes,est_cache_lines_load,est_cache_lines_store,qemu_log,uart_log" > "$CSV_OUT"
echo "${LEVEL},${VARIANT},${ITER},${load_ins},${store_ins},${load_bytes},${store_bytes},${load_lines},${store_lines},${QEMU_LOG},${UART_LOG}" >> "$CSV_OUT"

echo "[profile_memory] variant=${VARIANT} level=${LEVEL} iter=${ITER}"
echo "[profile_memory] load_instructions=${load_ins} store_instructions=${store_ins}"
echo "[profile_memory] load_bytes=${load_bytes} store_bytes=${store_bytes}"
echo "[profile_memory] est_cache_lines_load=${load_lines} est_cache_lines_store=${store_lines}"
echo "[profile_memory] csv=${CSV_OUT}"
