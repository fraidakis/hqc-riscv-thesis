#!/usr/bin/env bash
# scripts/bench/run_compiler_compare.sh
# Re-run compiler comparison (Clang vs GCC, -O2/-O3/-Os) for one variant.
#
# Supports both simulation (Spike ISS) and hardware (BPI-F3) platforms
# via the --platform flag.
#
# Usage:
#   run_compiler_compare.sh --platform sim --variant vector_zvk [--level 128|192|256]
#   run_compiler_compare.sh --platform hw --variant scalar_zb --quality publish
#   run_compiler_compare.sh --platform hw --remote bpi-f3 --variant vector_rvv
#
# Output:
#   sim → results/simulation/benchmarks/compiler_compare/sim_compiler_compare_<slug>_latest.csv
#   hw  → results/hardware/benchmarks/compiler_compare/hw_compiler_compare_<slug>_latest.csv
#
# GCC is automatically SKIP-guarded for vector_zvk (Zvbb incompatible).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${SCRIPT_DIR}/lib_bench.sh"

# ── Usage ─────────────────────────────────────────────────────────────────────
usage() {
    cat <<'EOF'
Usage:
  run_compiler_compare.sh --platform sim|hw --variant <variant> [OPTIONS]

Required:
  --platform sim|hw       Execution platform (sim=Spike ISS, hw=BPI-F3 hardware)
  --variant  <variant>    Variant name (scalar_ref, scalar_zb, vector_ref, vector_rvv, vector_zvk)

Options:
  --level 128|192|256     Security level (default: all three)
  --quality dev|publish   Iteration quality (hw only, default: dev)
  --remote HOST           Deploy and run on remote HOST via SSH (hw only)
  --local                 Run directly on this machine (hw only)
  --keccak IMPL           Keccak implementation override
  --decode IMPL           Decode implementation override
  -h, --help              Show this help and exit

Platform-specific notes:
  sim: Uses Spike ISS with deterministic instret counting. Compares cross-compiled
       Clang vs GCC via CMake toolchain files.
  hw:  Uses perf_event_open cycle counting on BPI-F3 hardware. Compares native
       GCC vs Clang (CC= override in Makefile.hw).
       --quality publish uses N=10000 iterations.
       --quality dev uses N=100 iterations (default).
EOF
}

# ── Defaults ─────────────────────────────────────────────────────────────────
PLATFORM=""
MUL_VARIANT=""
LEVEL=""
QUALITY="dev"
REMOTE=""
REMOTE_EXPLICIT=0
LOCAL_MODE=0

# ── Argument parsing ─────────────────────────────────────────────────────────
bench_parse_track_args "$@"   # sets MUL_VARIANT KECCAK_IMPL DECODE_IMPL

while [[ $# -gt 0 ]]; do
    case "$1" in
        --platform)   PLATFORM="$2"; shift 2 ;;
        --variant|--mul) shift 2 ;;
        --keccak|--decode) shift 2 ;;
        --level)      LEVEL="$2"; shift 2 ;;
        --quality)    QUALITY="$2"; shift 2 ;;
        --remote)     REMOTE="$2"; REMOTE_EXPLICIT=1; LOCAL_MODE=0; shift 2 ;;
        --local)      LOCAL_MODE=1; shift ;;
        -h|--help)    usage; exit 0 ;;
        *)            shift ;;
    esac
done

# ── Validation ───────────────────────────────────────────────────────────────
# Default platform to sim for backward compatibility
if [[ -z "$PLATFORM" ]]; then
    PLATFORM="sim"
fi

if [[ "$PLATFORM" != "sim" && "$PLATFORM" != "hw" ]]; then
    echo "ERROR: --platform must be 'sim' or 'hw', got: '$PLATFORM'" >&2
    usage; exit 2
fi

if [[ -z "$MUL_VARIANT" ]]; then
    echo "ERROR: --variant is required" >&2
    usage; exit 2
fi

if [[ "$QUALITY" != "dev" && "$QUALITY" != "publish" ]]; then
    echo "ERROR: --quality must be 'dev' or 'publish', got: '$QUALITY'" >&2
    exit 2
fi

LEVELS=(128 192 256)
if [[ -n "$LEVEL" ]]; then
    if ! [[ "$LEVEL" =~ ^(128|192|256)$ ]]; then
        echo "ERROR: --level must be 128, 192, or 256" >&2; exit 2
    fi
    LEVELS=("$LEVEL")
fi

GIT_HASH="$(git -C "$REPO_ROOT" rev-parse --short HEAD 2>/dev/null || echo 'unknown')"
BENCH_TIME="$(date +'%Y-%m-%d %H:%M')"

# Canonical variant name embedded in CSV rows (matches old naming for compatibility)
variant_csv_name() {
    case "$1" in
        scalar_ref) echo "ref_scalar" ;;
        scalar_zb)  echo "karatsuba_zbc" ;;
        vector_ref) echo "ref_vector" ;;
        vector_rvv) echo "karatsuba_rvv" ;;
        vector_zvk) echo "karatsuba_zvbc" ;;
        *)          echo "$1" ;;
    esac
}
VARIANT_CSV_NAME="$(variant_csv_name "$MUL_VARIANT")"
VARIANT_SLUG="$(bench_variant_slug "$MUL_VARIANT")"

# ── Print configuration ──────────────────────────────────────────────────────
echo ""
echo "╔══════════════════════════════════════════════════════════╗"
printf "║  %-56s  ║\n" "run_compiler_compare.sh"
printf "║  %-56s  ║\n" "Platform: ${PLATFORM}  Variant: ${MUL_VARIANT}"
printf "║  %-56s  ║\n" "Keccak: ${KECCAK_IMPL}  Decode: ${DECODE_IMPL}"
printf "║  %-56s  ║\n" "Commit:  ${GIT_HASH}  Date: ${BENCH_TIME}"
if [[ "$PLATFORM" == "hw" ]]; then
    local_info=""
    if [[ "$LOCAL_MODE" -eq 1 ]]; then local_info="Local: yes"
    elif [[ -n "$REMOTE" ]]; then local_info="Remote: ${REMOTE}"
    fi
    printf "║  %-56s  ║\n" "Quality: ${QUALITY}  ${local_info}"
fi
echo "╚══════════════════════════════════════════════════════════╝"
echo ""

# ══════════════════════════════════════════════════════════════════════════════
# Platform: sim (Spike ISS)
# ══════════════════════════════════════════════════════════════════════════════
if [[ "$PLATFORM" == "sim" ]]; then

# ── Toolchain / environment ───────────────────────────────────────────────────
RISCV="${RISCV:-$HOME/Thesis/toolchains/installed-tools}"
SPIKE="${RISCV}/bin/spike"
ISA="rv64gcv_zbc_zbb_zba_zbs_zvbc_zvbb_zvl256b"
SPIKE_TIMEOUT_SEC="${SPIKE_TIMEOUT_SEC:-120}"

if [[ ! -x "$SPIKE" ]]; then
    echo "ERROR: Spike not found at $SPIKE" >&2
    echo "       Source toolchain/toolchain.env or set RISCV" >&2
    exit 2
fi

# ── Output file ───────────────────────────────────────────────────────────────
OUT_DIR="${REPO_ROOT}/results/simulation/benchmarks/compiler_compare"
mkdir -p "$OUT_DIR"
STAMP="$(date +'%Y-%m-%d_%H-%M')"
CSV_OUT="${OUT_DIR}/sim_compiler_compare_${VARIANT_SLUG}_${STAMP}.csv"
CSV_LATEST="${OUT_DIR}/sim_compiler_compare_${VARIANT_SLUG}_latest.csv"

{
    echo "# date: ${BENCH_TIME}"
    echo "# commit: ${GIT_HASH}"
    echo "compiler,opt,level,variant,status,instructions,text_bytes,elf,error"
} > "$CSV_OUT"

# ── Single build+run (simulation) ────────────────────────────────────────────
run_one_sim() {
    local compiler="$1" opt="$2" level="$3"
    local toolchain elf target run_out run_err instret text_bytes

    case "$compiler" in
        clang) toolchain="toolchain/riscv-clang.cmake" ;;
        gcc)   toolchain="toolchain/riscv-gcc.cmake" ;;
        *) echo "ERROR: unknown compiler $compiler" >&2; return 1 ;;
    esac

    # GCC is incompatible with the Zvbb x2 SoA Keccak backend.
    if [[ "$compiler" == "gcc" && "$KECCAK_IMPL" == "zvbb_x2_soa" ]]; then
        local build_placeholder="${REPO_ROOT}/build/spike/bench_k${KECCAK_IMPL}_d${DECODE_IMPL}_${compiler}_${opt}"
        echo "${compiler},-${opt},${level},${VARIANT_CSV_NAME},SKIP,,,${build_placeholder}/bench_hqc${level}_${VARIANT_CSV_NAME}.elf,gcc_zvbb_incompatible" >> "$CSV_OUT"
        echo "SKIP (gcc_zvbb_incompatible)"
        return 0
    fi

    local build_dir="${REPO_ROOT}/build/spike/bench_k${KECCAK_IMPL}_d${DECODE_IMPL}_${compiler}_${opt}"
    mkdir -p "$build_dir"

    # Configure (always fresh for opt-level isolation)
    if ! cmake -S "$REPO_ROOT" -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE="${REPO_ROOT}/${toolchain}" \
        -DRISCV="${RISCV}" \
        -DRISCV_OPT_LEVEL="-${opt}" \
        -DHQC_SIM=spike \
        -DHQC_KECCAK_IMPL="${KECCAK_IMPL}" \
        -DHQC_DECODE_IMPL="${DECODE_IMPL}" \
        -DBENCH_N_ITER=1 \
        >/dev/null 2>&1; then
        echo "${compiler},-${opt},${level},${VARIANT_CSV_NAME},FAIL,,,${build_dir}/bench_hqc${level}_${VARIANT_CSV_NAME}.elf,cmake_configure_failed" >> "$CSV_OUT"
        echo "FAIL (cmake configure)"
        return 1
    fi

    target="bench_hqc${level}_${MUL_VARIANT}"
    if ! cmake --build "$build_dir" --target "$target" -j"$(nproc)" >/dev/null 2>&1; then
        echo "${compiler},-${opt},${level},${VARIANT_CSV_NAME},FAIL,,,${build_dir}/${target}.elf,build_failed" >> "$CSV_OUT"
        echo "FAIL (build)"
        return 1
    fi

    elf="${build_dir}/${target}.elf"
    if [[ ! -f "$elf" ]]; then
        echo "${compiler},-${opt},${level},${VARIANT_CSV_NAME},FAIL,,,${elf},elf_missing" >> "$CSV_OUT"
        echo "FAIL (elf missing)"
        return 1
    fi

    run_out="$(mktemp)"
    run_err="$(mktemp)"

    local rc=0
    if command -v timeout >/dev/null 2>&1; then
        timeout "${SPIKE_TIMEOUT_SEC}s" "$SPIKE" --isa="$ISA" "$elf" > "$run_out" 2>"$run_err" || rc=$?
    else
        "$SPIKE" --isa="$ISA" "$elf" > "$run_out" 2>"$run_err" || rc=$?
    fi

    if [[ "$rc" -eq 124 ]]; then
        echo "${compiler},-${opt},${level},${VARIANT_CSV_NAME},FAIL,,,${elf},spike_run_timeout" >> "$CSV_OUT"
        rm -f "$run_out" "$run_err"
        echo "FAIL (spike timeout)"
        return 1
    elif [[ "$rc" -ne 0 ]]; then
        echo "${compiler},-${opt},${level},${VARIANT_CSV_NAME},FAIL,,,${elf},spike_run_failed" >> "$CSV_OUT"
        rm -f "$run_out" "$run_err"
        echo "FAIL (spike error rc=$rc)"
        return 1
    fi

    # Sum keypair + encaps + decaps instret (column 7 in Spike CSV output)
    instret="$(awk -F',' '($1=="keypair" || $1=="encaps" || $1=="decaps") {sum += $7} END {print sum + 0}' "$run_out")"

    local size_tool="${RISCV}/bin/llvm-size"
    if [[ ! -x "$size_tool" ]]; then
        size_tool="${RISCV}/bin/riscv64-unknown-elf-size"
    fi
    text_bytes="$("$size_tool" "$elf" 2>/dev/null | awk 'NR==2{print $1}')"
    text_bytes="${text_bytes:-0}"

    echo "${compiler},-${opt},${level},${VARIANT_CSV_NAME},OK,${instret},${text_bytes},${elf}," >> "$CSV_OUT"
    rm -f "$run_out" "$run_err"
    printf "%s instructions" "$instret"
}

# ── Main loop (sim) ──────────────────────────────────────────────────────────
for level in "${LEVELS[@]}"; do
    echo "── Level HQC-${level} ──────────────────────────────────────────────"
    for compiler in clang gcc; do
        for opt in O2 O3; do
            printf "   %-6s %-3s  ⋯  " "$compiler" "-$opt"
            run_one_sim "$compiler" "$opt" "$level" || true
            echo ""
        done
    done
done

# Atomically update the _latest symlink/copy
bench_refresh_tracked_csv "$CSV_OUT" "$CSV_LATEST" || true

echo ""
echo "✔  Written: ${CSV_LATEST}"
echo ""

# Regenerate the compiler analysis summary
echo "Regenerating compiler_analysis.md ..."
python3 "${REPO_ROOT}/scripts/report/generate_compiler_compare.py"


# ══════════════════════════════════════════════════════════════════════════════
# Platform: hw (BPI-F3 hardware)
# ══════════════════════════════════════════════════════════════════════════════
elif [[ "$PLATFORM" == "hw" ]]; then

# ── Load credentials ─────────────────────────────────────────────────────────
CREDS_FILE="${REPO_ROOT}/docs/hardware/BPI-F3/bpi-f3-credentials.env"
BPI_PASS=""
if [[ -f "$CREDS_FILE" ]]; then
    # shellcheck source=/dev/null
    source "$CREDS_FILE"
    BPI_PASS="${BPI_PASS:-}"
fi

# ── Auto-detect local mode on riscv64 ────────────────────────────────────────
if [[ "$(uname -m)" == "riscv64" && "$REMOTE_EXPLICIT" -eq 0 ]]; then
    LOCAL_MODE=1
fi

# ── Default remote when not explicitly specified ─────────────────────────────
if [[ "$REMOTE_EXPLICIT" -eq 0 && "$LOCAL_MODE" -eq 0 ]]; then
    REMOTE="${BPI_USER:-root}@${BPI_HOST:-192.168.1.50}"
fi

# ── Validate execution target ────────────────────────────────────────────────
if [[ "$LOCAL_MODE" -eq 0 && -z "$REMOTE" ]]; then
    echo "ERROR: Not on riscv64 — specify --remote HOST or --local" >&2
    echo "       The hw platform requires a RISC-V target." >&2
    exit 1
fi

# ── SSH transport selection ──────────────────────────────────────────────────
USE_SSHPASS=0
if [[ "$LOCAL_MODE" -eq 0 && -n "$REMOTE" ]]; then
    if ssh -o ConnectTimeout=5 -o BatchMode=yes "$REMOTE" true 2>/dev/null; then
        USE_SSHPASS=0
    elif [[ -n "$BPI_PASS" ]] && command -v sshpass &>/dev/null; then
        USE_SSHPASS=1
        echo "[ssh] Key auth unavailable — using sshpass fallback." >&2
        echo "      Run: ssh-copy-id ${REMOTE}  (one-time setup)" >&2
    else
        echo "ERROR: Cannot connect to ${REMOTE} via SSH." >&2
        echo "       Key auth failed and no password/sshpass available." >&2
        echo "       Set up keys:  ssh-copy-id ${REMOTE}" >&2
        exit 2
    fi
fi

# ── SSH/rsync wrappers ───────────────────────────────────────────────────────
_ssh() {
    if [[ "$USE_SSHPASS" -eq 1 ]]; then
        sshpass -p "$BPI_PASS" ssh -o StrictHostKeyChecking=no "$@"
    else
        ssh -o StrictHostKeyChecking=no "$@"
    fi
}

_rsync() {
    if [[ "$USE_SSHPASS" -eq 1 ]]; then
        sshpass -p "$BPI_PASS" rsync -e "ssh -o StrictHostKeyChecking=no" "$@"
    else
        rsync -e "ssh -o StrictHostKeyChecking=no" "$@"
    fi
}

# ── Remote infrastructure ────────────────────────────────────────────────────
REMOTE_DIR="~/hqc-riscv"

run_cmd() {
    if [[ "$LOCAL_MODE" -eq 1 ]]; then
        eval "$1"
    else
        _ssh "$REMOTE" "$1"
    fi
}

deploy() {
    if [[ "$LOCAL_MODE" -eq 1 ]]; then return; fi
    echo "[deploy] Syncing to ${REMOTE}..."
    _rsync -az --delete \
        --exclude='.venv' --exclude='build*' --exclude='.git' \
        --exclude='__pycache__' --exclude='.hypothesis' \
        "${REPO_ROOT}/" "${REMOTE}:${REMOTE_DIR}/"
}

set_governor() {
    run_cmd "echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null 2>&1 || true"
}

# ── Output file ───────────────────────────────────────────────────────────────
OUT_DIR="${REPO_ROOT}/results/hardware/benchmarks/compiler_compare"
mkdir -p "$OUT_DIR"
STAMP="$(date +'%Y-%m-%d_%H-%M')"
CSV_OUT="${OUT_DIR}/hw_compiler_compare_${VARIANT_SLUG}_${STAMP}.csv"
CSV_LATEST="${OUT_DIR}/hw_compiler_compare_${VARIANT_SLUG}_latest.csv"

{
    echo "# date: ${BENCH_TIME}"
    echo "# commit: ${GIT_HASH}"
    echo "# platform: bpi-f3"
    echo "# quality: ${QUALITY}"
    echo "# variant: ${MUL_VARIANT}"
    echo "compiler,opt,level,variant,status,cycles,text_bytes,binary,error"
} > "$CSV_OUT"

# ── Single build+run (hardware) ──────────────────────────────────────────────
# Makefile.hw supports OPT=<level> (defaults to O3). We pass OPT from the
# command line to override cleanly without touching EXTRA_CFLAGS.
run_one_hw() {
    local compiler="$1" opt="$2" level="$3"
    local dir="$REPO_ROOT"
    if [[ "$LOCAL_MODE" -eq 0 ]]; then dir="${REMOTE_DIR}"; fi

    # GCC is incompatible with the Zvbb x2 SoA Keccak backend on hardware too.
    if [[ "$compiler" == "gcc" && "$KECCAK_IMPL" == "zvbb_x2_soa" ]]; then
        echo "${compiler},-${opt},${level},${VARIANT_CSV_NAME},SKIP,,,build-hw/bench_hw_kem_hqc${level}_${MUL_VARIANT},gcc_zvbb_incompatible" >> "$CSV_OUT"
        echo "SKIP (gcc_zvbb_incompatible)"
        return 0
    fi

    local make_mode="${QUALITY}"
    local extra_args="KECCAK=${KECCAK_IMPL} DECODE=${DECODE_IMPL}"
    local build_log

    # ── LTO configuration ────────────────────────────────────────────────────────
    # Makefile.hw auto-detects: LTO=1 for scalar variants, LTO=0 for vector
    # variants (GCC 14 ICEs on RVV intrinsics + -flto). Don't override here —
    # let the Makefile's auto-detection handle it.
    # Clang LTO requires lld (installed as lld-20); Makefile.hw adds -fuse-ld=lld
    # automatically when CC=clang and LTO=1.
    local lto_args=""

    # Clean and rebuild with the specified compiler + opt level.
    # OPT= overrides the Makefile's default O3; EXTRA_CFLAGS stays untouched
    # so the Makefile retains full control of variant-specific flags.
    if ! build_log=$(run_cmd "cd $dir && make -f Makefile.hw SUITE=bench LEVEL=$level POLYMUL=$MUL_VARIANT MODE=$make_mode EXTENDED=1 ${lto_args} CC=$compiler OPT=${opt} $extra_args clean >/dev/null 2>&1; make -f Makefile.hw SUITE=bench LEVEL=$level POLYMUL=$MUL_VARIANT MODE=$make_mode EXTENDED=1 ${lto_args} CC=$compiler OPT=${opt} $extra_args kem 2>&1" 2>&1); then
        echo "${compiler},-${opt},${level},${VARIANT_CSV_NAME},FAIL,,,build-hw/bench_hw_kem_hqc${level}_${MUL_VARIANT},build_failed" >> "$CSV_OUT"
        echo "FAIL (build)"
        return 1
    fi

    local bin="build-hw/bench_hw_kem_hqc${level}_${MUL_VARIANT}"

    # Execute the benchmark
    local exec_prefix="taskset -c 7"
    if [[ "$QUALITY" == "publish" ]]; then
        exec_prefix="sudo nice -n -20 taskset -c 7"
    fi

    local output
    if ! output=$(run_cmd "cd $dir && $exec_prefix ./$bin" 2>&1); then
        echo "${compiler},-${opt},${level},${VARIANT_CSV_NAME},FAIL,,,${bin},run_failed" >> "$CSV_OUT"
        echo "FAIL (run)"
        return 1
    fi

    # Parse output: sum median cycles for keypair + encaps + decaps
    # HW bench output format: op,level,n_iter,median,mean,min,max,stdev,...
    local cycles
    cycles="$(echo "$output" | awk -F',' '
        ($1=="keypair" || $1=="encaps" || $1=="decaps") { sum += $4 }
        END { printf "%.0f", sum }
    ')"
    cycles="${cycles:-0}"

    # Get text size from the binary on the target
    local text_bytes
    text_bytes="$(run_cmd "size $dir/$bin 2>/dev/null | awk 'NR==2{print \$1}'")"
    text_bytes="${text_bytes:-0}"

    echo "${compiler},-${opt},${level},${VARIANT_CSV_NAME},OK,${cycles},${text_bytes},${bin}," >> "$CSV_OUT"
    printf "%s cycles" "$cycles"
}

# ── Main execution (hw) ──────────────────────────────────────────────────────
deploy
set_governor

for level in "${LEVELS[@]}"; do
    echo "── Level HQC-${level} ──────────────────────────────────────────────"
    for compiler in gcc clang; do
        for opt in O2 O3; do
            printf "   %-6s %-3s  ⋯  " "$compiler" "-$opt"
            run_one_hw "$compiler" "$opt" "$level" || true
            echo ""
        done
    done
done

# Atomically update the _latest symlink/copy
bench_refresh_tracked_csv "$CSV_OUT" "$CSV_LATEST" || true

echo ""
echo "✔  Written: ${CSV_LATEST}"
echo ""

# ── Sync results back from remote ────────────────────────────────────────────
if [[ "$LOCAL_MODE" -eq 0 ]]; then
    echo "[sync] Pushing results to ${REMOTE}..."
    if ! _rsync -az "${REPO_ROOT}/results/hardware/" "${REMOTE}:${REMOTE_DIR}/results/hardware/"; then
        echo "WARNING: Failed to sync results to remote. Local results are correct." >&2
    fi
fi

# Regenerate the compiler analysis summary
echo "Regenerating compiler_analysis.md ..."
python3 "${REPO_ROOT}/scripts/report/generate_compiler_compare.py"

fi
