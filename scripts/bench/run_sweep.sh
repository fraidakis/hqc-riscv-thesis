#!/usr/bin/env bash
# scripts/bench/run_sweep.sh
# Hardware parameter sweep runner for HQC-RISC-V.
#
# Dedicated script for one-off tuning experiments (threshold sweeps,
# LMUL sweeps) that don't belong in the main benchmark runner.
#
# This script is ONLY valid for hardware platform usage (BPI-F3).
#
# Examples:
#   run_sweep.sh --type threshold --variant vector_rvv --values "8 16 32"
#   run_sweep.sh --type lmul --variant vector_rvv --values "m1 m2 m4"
#   run_sweep.sh --type threshold --variant vector_rvv --level 192 --values "8 16 32 64"

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# ── Source shared library ────────────────────────────────────────────────────
# shellcheck source=lib_bench.sh
source "${SCRIPT_DIR}/lib_bench.sh"

# ── Usage ────────────────────────────────────────────────────────────────────
usage() {
    cat <<'EOF'
Usage:
  run_sweep.sh --type threshold|lmul --variant VARIANT --values "V1 V2 V3" [OPTIONS]

Required:
  --type TYPE         Sweep type: threshold or lmul
  --variant VARIANT   Variant to sweep (e.g. vector_rvv)
  --values "V1 V2…"  Space-separated values to sweep over

Options:
  --level LEVEL       Security level: 128, 192, 256 (default: 128)
  --local             Run directly on this machine (auto-detected on riscv64)
  --remote HOST       Deploy and run via SSH on HOST
  --core N            CPU core to pin to (default: 7)
  -h, --help          Show this help and exit

Sweep types:
  threshold   Vary the Karatsuba recursive threshold parameter.
              Values are integer thresholds (e.g. "8 16 32 64").
              Calls gen_karatsuba_specialized.py --threshold T for each value,
              rebuilds, and runs the benchmark.

  lmul        Vary the LMUL (vector register grouping) setting.
              Values are LMUL identifiers (e.g. "m1 m2 m4 m8").
              Patches the code generator to use the specified LMUL,
              rebuilds, and runs the benchmark.

Notes:
  This script is hardware-only. It must run on a RISC-V board (--local)
  or deploy to one (--remote HOST).

Examples:
  run_sweep.sh --type threshold --variant vector_rvv --values "8 16 32"
  run_sweep.sh --type lmul --variant vector_rvv --values "m1 m2 m4 m8"
  run_sweep.sh --type threshold --variant vector_rvv --level 256 --values "16 32 64"
EOF
}

# ── Defaults ─────────────────────────────────────────────────────────────────
SWEEP_TYPE=""
VARIANT=""
VALUES=""
LEVEL="128"
REMOTE_HOST=""
LOCAL_MODE=0
CORE=7

BENCH_DIR="${REPO_ROOT}/tests/bench"
GEN_SCRIPT="${REPO_ROOT}/scripts/gen/gen_karatsuba_specialized.py"
OUT_DIR="${REPO_ROOT}/results/hardware/sweeps"

# ── Auto-detect local mode ───────────────────────────────────────────────────
if [[ "$(uname -m)" == "riscv64" ]]; then
    LOCAL_MODE=1
fi

# ── Argument Parsing ─────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --type)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --type requires a value (threshold or lmul)" >&2
                usage >&2
                exit 1
            fi
            SWEEP_TYPE="$2"
            shift 2
            ;;
        --variant)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --variant requires a value" >&2
                usage >&2
                exit 1
            fi
            VARIANT="$2"
            shift 2
            ;;
        --values)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --values requires a quoted list of values" >&2
                usage >&2
                exit 1
            fi
            VALUES="$2"
            shift 2
            ;;
        --level)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --level requires a value" >&2
                usage >&2
                exit 1
            fi
            LEVEL="$2"
            shift 2
            ;;
        --remote)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --remote requires a host" >&2
                usage >&2
                exit 1
            fi
            REMOTE_HOST="$2"
            LOCAL_MODE=0
            shift 2
            ;;
        --local)
            LOCAL_MODE=1
            shift
            ;;
        --core)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --core requires a value" >&2
                usage >&2
                exit 1
            fi
            CORE="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "ERROR: Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

# ── Validation ───────────────────────────────────────────────────────────────

# --type is required
if [[ -z "$SWEEP_TYPE" ]]; then
    echo "ERROR: --type is required (threshold or lmul)" >&2
    usage >&2
    exit 1
fi

# Validate sweep type
if [[ "$SWEEP_TYPE" != "threshold" && "$SWEEP_TYPE" != "lmul" ]]; then
    echo "ERROR: --type must be 'threshold' or 'lmul', got: '$SWEEP_TYPE'" >&2
    usage >&2
    exit 1
fi

# --variant is required
if [[ -z "$VARIANT" ]]; then
    echo "ERROR: --variant is required" >&2
    usage >&2
    exit 1
fi

# --values is required
if [[ -z "$VALUES" ]]; then
    echo "ERROR: --values is required (e.g. --values \"8 16 32\")" >&2
    usage >&2
    exit 1
fi

# Validate level
if [[ "$LEVEL" != "128" && "$LEVEL" != "192" && "$LEVEL" != "256" ]]; then
    echo "ERROR: --level must be one of: 128, 192, 256; got: '$LEVEL'" >&2
    exit 1
fi

# Hardware-only: must have a way to execute
if [[ "$LOCAL_MODE" -eq 0 && -z "$REMOTE_HOST" ]]; then
    echo "ERROR: Not on riscv64 — specify --local or --remote HOST" >&2
    echo "       This script is hardware-only." >&2
    exit 1
fi

# ── Helpers ──────────────────────────────────────────────────────────────────
REMOTE_DIR="~/hqc-riscv"

run_cmd() {
    # Run a command locally or remotely.
    if [[ "$LOCAL_MODE" -eq 1 ]]; then
        eval "$1"
    else
        ssh "$REMOTE_HOST" "$1"
    fi
}

deploy() {
    if [[ "$LOCAL_MODE" -eq 1 ]]; then return; fi
    echo "[deploy] Syncing to ${REMOTE_HOST}:${REMOTE_DIR} ..."
    rsync -az --delete \
        --exclude='.venv' --exclude='build*' --exclude='.git' \
        --exclude='__pycache__' --exclude='.hypothesis' \
        "${REPO_ROOT}/" "${REMOTE_HOST}:${REMOTE_DIR}/"
}

set_governor() {
    run_cmd "echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null 2>&1 || true"
}

hw_build_sweep() {
    local level="$1" variant="$2"
    local dir="$BENCH_DIR"
    if [[ "$LOCAL_MODE" -eq 0 ]]; then dir="${REMOTE_DIR}/tests/bench"; fi
    run_cmd "cd $dir && make -f Makefile.hw LEVEL=$level POLYMUL=$variant MODE=dev clean >/dev/null 2>&1; make -f Makefile.hw LEVEL=$level POLYMUL=$variant MODE=dev mul 2>&1 | tail -2"
}

hw_run_sweep() {
    local level="$1" variant="$2"
    local dir="$BENCH_DIR"
    if [[ "$LOCAL_MODE" -eq 0 ]]; then dir="${REMOTE_DIR}/tests/bench"; fi
    local bin="bench_hw_mul_hqc${level}_${variant}"
    run_cmd "cd $dir && taskset -c $CORE ./$bin"
}

extract_median_cycles() { echo "$1" | awk -F',' '/^vect_mul,/ {print $4}'; }
extract_median_instret() { echo "$1" | awk -F',' '/^vect_mul,/ {print $9}'; }

print_sweep_result() {
    local label="$1" output="$2"
    local cyc ins
    cyc="$(extract_median_cycles "$output")"
    ins="$(extract_median_instret "$output")"
    if [[ -n "$cyc" && -n "$ins" ]]; then
        local ipc
        ipc=$(awk "BEGIN {printf \"%.3f\", $ins / $cyc}")
        printf "  %-24s median_cycles=%-8s instret=%-8s IPC=%s\n" "$label" "$cyc" "$ins" "$ipc"
    else
        printf "  %-24s (no data)\n" "$label"
    fi
}

# ── SSH connection validation ────────────────────────────────────────────────
if [[ "$LOCAL_MODE" -eq 0 && -n "$REMOTE_HOST" ]]; then
    if ! ssh -o ConnectTimeout=5 -o BatchMode=yes "$REMOTE_HOST" true 2>/dev/null; then
        echo "ERROR: Cannot connect to ${REMOTE_HOST} via SSH." >&2
        echo "       Check that the host is reachable and SSH keys are configured." >&2
        exit 2
    fi
fi

# ── Print configuration ─────────────────────────────────────────────────────
echo "╔══════════════════════════════════════════════════════════════╗"
printf "║  run_sweep.sh                                                ║\n"
printf "║  Type: %-11s  Variant: %-15s Level: %-3s  ║\n" "$SWEEP_TYPE" "$VARIANT" "$LEVEL"
printf "║  Values: %-49s ║\n" "$VALUES"
if [[ -n "$REMOTE_HOST" ]]; then
    printf "║  Remote: %-49s ║\n" "$REMOTE_HOST"
elif [[ "$LOCAL_MODE" -eq 1 ]]; then
    printf "║  Execution: local (core %s)                                  ║\n" "$CORE"
fi
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# ── Create sweep output directory ───────────────────────────────────────────
TIMESTAMP="$(date +'%Y-%m-%d_%H-%M')"
SWEEP_OUT_DIR="${OUT_DIR}/${SWEEP_TYPE}/${TIMESTAMP}"
mkdir -p "$SWEEP_OUT_DIR"

# ── Sweep dispatch ───────────────────────────────────────────────────────────
case "$SWEEP_TYPE" in
    threshold)
        # ── Threshold sweep ──────────────────────────────────────────────
        # Vary the Karatsuba recursive threshold parameter.
        # For each threshold value:
        #   1. Regenerate specialized code via gen_karatsuba_specialized.py
        #   2. Rebuild the benchmark binary
        #   3. Run and collect results
        echo "━━━ Threshold Sweep: ${VARIANT}, HQC-${LEVEL} ━━━"
        echo "  Values: ${VALUES}"
        echo ""

        deploy
        set_governor

        # CSV output for sweep results
        SWEEP_CSV="${SWEEP_OUT_DIR}/sweep_threshold_$(bench_variant_slug "$VARIANT")_hqc${LEVEL}.csv"
        echo "# date: $(date +'%Y-%m-%d %H:%M')" > "$SWEEP_CSV"
        echo "# variant: ${VARIANT}" >> "$SWEEP_CSV"
        echo "# level: ${LEVEL}" >> "$SWEEP_CSV"
        echo "# sweep_type: threshold" >> "$SWEEP_CSV"
        echo "threshold,median_cycles,median_instret,ipc" >> "$SWEEP_CSV"

        for T in $VALUES; do
            echo "━━━ T=${T} ━━━"

            if [[ "$LOCAL_MODE" -eq 0 ]]; then
                # Remote: generate + build + run on the board
                run_cmd "cd ${REMOTE_DIR} && python3 scripts/gen/gen_karatsuba_specialized.py --mode rvv_opt --threshold $T"
                hw_build_sweep "$LEVEL" "$VARIANT"
                output="$(hw_run_sweep "$LEVEL" "$VARIANT")"
            else
                # Local: generate + build + run directly
                python3 "$GEN_SCRIPT" --mode rvv_opt --threshold "$T"
                hw_build_sweep "$LEVEL" "$VARIANT"
                output="$(hw_run_sweep "$LEVEL" "$VARIANT")"
            fi

            print_sweep_result "T=${T}" "$output"

            # Record to CSV
            cyc="$(extract_median_cycles "$output")"
            ins="$(extract_median_instret "$output")"
            if [[ -n "$cyc" && -n "$ins" ]]; then
                ipc=$(awk "BEGIN {printf \"%.3f\", $ins / $cyc}")
            else
                cyc="${cyc:-0}"; ins="${ins:-0}"; ipc="0.000"
            fi
            echo "${T},${cyc},${ins},${ipc}" >> "$SWEEP_CSV"
            echo ""
        done

        # Restore default threshold (T=16)
        if [[ "$LOCAL_MODE" -eq 0 ]]; then
            run_cmd "cd ${REMOTE_DIR} && python3 scripts/gen/gen_karatsuba_specialized.py --mode rvv_opt --threshold 16"
        else
            python3 "$GEN_SCRIPT" --mode rvv_opt --threshold 16
        fi
        echo "[sweep] Restored default T=16."
        echo ""
        echo "Results saved to: ${SWEEP_CSV}"
        ;;

    lmul)
        # ── LMUL sweep ───────────────────────────────────────────────────
        # Vary the LMUL (vector register grouping) setting.
        # For each LMUL value:
        #   1. Patch the code generator to use the specified LMUL
        #   2. Rebuild the benchmark binary
        #   3. Run and collect results
        #   4. Restore original files
        echo "━━━ LMUL Sweep: ${VARIANT}, HQC-${LEVEL} ━━━"
        echo "  Values: ${VALUES}"
        echo ""

        # LMUL sweep must run locally (requires patching source files)
        if [[ "$LOCAL_MODE" -eq 0 ]]; then
            echo "ERROR: --type lmul must run on the board (--local)." >&2
            echo "       It patches source files and requires native compilation." >&2
            echo "       Deploy the repo and run locally:" >&2
            echo "         run_sweep.sh --local --type lmul --variant $VARIANT --values \"${VALUES}\"" >&2
            exit 1
        fi

        set_governor

        # Backup originals
        gen_bak="${GEN_SCRIPT}.lmul_bak"
        src_file="${REPO_ROOT}/src/mult/karatsuba_rvv/gf2x_clmul.c"
        src_bak="${src_file}.lmul_bak"
        cp "$GEN_SCRIPT" "$gen_bak"
        if [[ -f "$src_file" ]]; then
            cp "$src_file" "$src_bak"
        fi

        cleanup_lmul() {
            cp "$gen_bak" "$GEN_SCRIPT"
            if [[ -f "$src_bak" ]]; then
                cp "$src_bak" "$src_file"
            fi
            rm -f "$gen_bak" "$src_bak"
            python3 "$GEN_SCRIPT" --mode rvv_opt --threshold 16
            echo "[cleanup] Restored original files."
        }
        trap cleanup_lmul EXIT

        # CSV output for sweep results
        SWEEP_CSV="${SWEEP_OUT_DIR}/sweep_lmul_$(bench_variant_slug "$VARIANT")_hqc${LEVEL}.csv"
        echo "# date: $(date +'%Y-%m-%d %H:%M')" > "$SWEEP_CSV"
        echo "# variant: ${VARIANT}" >> "$SWEEP_CSV"
        echo "# level: ${LEVEL}" >> "$SWEEP_CSV"
        echo "# sweep_type: lmul" >> "$SWEEP_CSV"
        echo "lmul,median_cycles,median_instret,ipc" >> "$SWEEP_CSV"

        for LMUL in $VALUES; do
            echo "━━━ LMUL=${LMUL} ━━━"

            # Patch: replace default LMUL (m2) with the sweep value in the generator
            cp "$gen_bak" "$GEN_SCRIPT"
            if [[ "$LMUL" != "m2" ]]; then
                sed -i "s/__riscv_vsetvl_e64m2/__riscv_vsetvl_e64${LMUL}/g" "$GEN_SCRIPT"
                sed -i "s/vuint64m2_t/vuint64${LMUL}_t/g" "$GEN_SCRIPT"
                sed -i "s/_v_u64m2/_v_u64${LMUL}/g" "$GEN_SCRIPT"
                sed -i "s/_vv_u64m2/_vv_u64${LMUL}/g" "$GEN_SCRIPT"
            fi

            # Regenerate + build + run
            python3 "$GEN_SCRIPT" --mode rvv_opt --threshold 16
            hw_build_sweep "$LEVEL" "$VARIANT"
            output="$(hw_run_sweep "$LEVEL" "$VARIANT")"

            print_sweep_result "LMUL=${LMUL}" "$output"

            # Record to CSV
            cyc="$(extract_median_cycles "$output")"
            ins="$(extract_median_instret "$output")"
            if [[ -n "$cyc" && -n "$ins" ]]; then
                ipc=$(awk "BEGIN {printf \"%.3f\", $ins / $cyc}")
            else
                cyc="${cyc:-0}"; ins="${ins:-0}"; ipc="0.000"
            fi
            echo "${LMUL},${cyc},${ins},${ipc}" >> "$SWEEP_CSV"
            echo ""
        done

        # Cleanup is handled by the trap
        trap - EXIT
        cleanup_lmul

        echo ""
        echo "Results saved to: ${SWEEP_CSV}"
        ;;
esac
