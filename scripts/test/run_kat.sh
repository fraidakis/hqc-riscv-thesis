#!/usr/bin/env bash
# scripts/test/run_kat.sh
#
# Run KAT ELFs for a polymul variant across one or more HQC security levels.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${SCRIPT_DIR}/../bench/lib_bench.sh"

usage() {
    cat <<'EOF'
Usage:
    run_kat.sh --mul <name> [--keccak <impl>] [--decode <impl>]
               [--level <128|192|256|all|csv>] [--qemu]

Flags:
    --mul <name>       Polymul variant name (required)
    --keccak <impl>    Keccak implementation (auto-mapped from --mul suffix if omitted)
    --decode <impl>    Decode implementation (default: ref)
    --level <value>    Security level(s) to run.
                       Accepts: 128, 192, 256, all, or comma-separated values
                       (example: 128,192,256). Default: all.
    --qemu             Use QEMU instead of Spike (default: Spike).
    --spike            Alias for default Spike mode (kept for backward compatibility).
EOF
}

# Populate MUL_VARIANT, KECCAK_IMPL, DECODE_IMPL via shared auto-mapping.
bench_parse_track_args "$@"

LEVEL_INPUT="all"
USE_SPIKE=1

while [[ $# -gt 0 ]]; do
    case "$1" in
        --mul|--keccak|--decode)
            shift 2
            ;;
        --level)
            if [[ $# -lt 2 || "$2" == --* ]]; then
                echo "ERROR: --level requires a value" >&2
                usage >&2
                exit 1
            fi
            LEVEL_INPUT="$2"
            shift 2
            ;;
        --spike)
            USE_SPIKE=1
            shift 1
            ;;
        --qemu)
            USE_SPIKE=0
            shift 1
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

if [[ -z "$MUL_VARIANT" ]]; then
    echo "ERROR: --mul is required" >&2
    usage >&2
    exit 1
fi

declare -a LEVELS=()
if [[ "$LEVEL_INPUT" == "all" ]]; then
    LEVELS=(128 192 256)
else
    # Accept comma-separated and/or whitespace-separated values.
    LEVEL_INPUT="${LEVEL_INPUT//,/ }"
    read -r -a LEVELS <<< "$LEVEL_INPUT"
fi

if [[ ${#LEVELS[@]} -eq 0 ]]; then
    echo "ERROR: --level produced no security levels" >&2
    usage >&2
    exit 1
fi

for level in "${LEVELS[@]}"; do
    if [[ "$level" != "128" && "$level" != "192" && "$level" != "256" ]]; then
        echo "ERROR: Invalid level '$level' (allowed: 128, 192, 256, all)" >&2
        usage >&2
        exit 1
    fi
done

SPIKE_RUNNER="${REPO_ROOT}/sim/spike/run.sh"
QEMU_RUNNER="${REPO_ROOT}/sim/qemu/run.sh"
if [[ "$USE_SPIKE" -eq 1 ]]; then
    if [[ ! -x "$SPIKE_RUNNER" ]]; then
        echo "ERROR: sim/spike/run.sh not found or not executable" >&2
        exit 1
    fi
else
    if [[ ! -x "$QEMU_RUNNER" ]]; then
        echo "ERROR: sim/qemu/run.sh not found or not executable" >&2
        exit 1
    fi
fi

# ── Determine simulator type and QEMU-safe Keccak impl ────────────────────────
# zvbb_gather requires Zvbb which is not enabled in the QEMU virt CPU flags.
# For QEMU KAT tests, down-map to rvv_only (RVV 1.0 only; always supported
# by our QEMU CPU spec).  The KAT vectors are Keccak-implementation-agnostic:
# any correct Keccak implementation produces identical output for the same input.
if [[ "$USE_SPIKE" -eq 1 ]]; then
    sim_type="spike"
    eff_keccak="${KECCAK_IMPL:-ref}"
else
    sim_type="qemu"
    if [[ "${KECCAK_IMPL:-ref}" == "zvbb_gather" ]]; then
        eff_keccak="rvv_only"
    else
        eff_keccak="${KECCAK_IMPL:-ref}"
    fi
fi
# Override so bench_auto_configure_and_build picks up the adjusted value.
KECCAK_IMPL="$eff_keccak"
export KECCAK_IMPL

# Dedicated KAT build dir — owned by run_kat.sh and always configured with
# the correct HQC_SIM and Keccak impl, so cmake auto-build can never produce a
# simulator-incompatible ELF here.
# Hierarchy: build/<sim>/kat/   (dynamic Keccak variant is stored in the cmake
# cache; bench_auto_configure_and_build reconfigures on mismatch automatically).
KAT_BUILD_DIR="${REPO_ROOT}/build/${sim_type}/kat"

overall_status=0

for level in "${LEVELS[@]}"; do
    target="hqc${level}_${MUL_VARIANT}"
    rsp="${REPO_ROOT}/tests/kat/PQCkemKAT_${level}.rsp"

    if [[ ! -f "$rsp" ]]; then
        echo "FAIL [HQC-${level}]: reference RSP file '$rsp' not found" >&2
        overall_status=1
        continue
    fi

    # Ensure the KAT ELF is built in the owned build dir.
    if ! bench_auto_configure_and_build "$KAT_BUILD_DIR" "$REPO_ROOT" "$target" "$sim_type" 0 "kat"; then
        echo "FAIL [HQC-${level}]: failed to build $target in $KAT_BUILD_DIR" >&2
        overall_status=1
        continue
    fi

    elf="${KAT_BUILD_DIR}/${target}.elf"
    if [[ ! -f "$elf" ]]; then
        echo "FAIL [HQC-${level}]: ELF not found after build: $elf" >&2
        overall_status=1
        continue
    fi

    log="$(mktemp /tmp/hqc_kat_${level}_${MUL_VARIANT}_XXXXXX.log)"

    echo "[kat] Running $target"
    if [[ "$USE_SPIKE" -eq 1 ]]; then
        "${REPO_ROOT}/sim/spike/run.sh" --clean "$elf" > "$log" 2>&1 || true
    else
        "${REPO_ROOT}/sim/qemu/run.sh" "$elf" > "$log" 2>&1 || true
    fi

    if [[ ! -s "$log" ]]; then
        echo "FAIL [HQC-${level}]: Simulator produced no output for $(basename "$elf")" >&2
        rm -f "$log"
        overall_status=1
        continue
    fi

    if ! python3 "${REPO_ROOT}/tests/kat/kat_check.py" \
        --actual "$log" \
        --rsp "$rsp" \
        --level "$level"; then
        overall_status=1
    fi

    rm -f "$log"
done

exit "$overall_status"
