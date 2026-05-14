#!/usr/bin/env bash
# scripts/test/run_kat.sh
#
# Run KAT ELFs for a polymul variant across one or more HQC security levels.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

usage() {
    cat <<'EOF'
Usage:
    run_kat.sh --variant <name> [--level <128|192|256|all|csv>] [--spike]

Flags:
    --variant <name>   Polymul variant name (required)
    --level <value>    Security level(s) to run.
                       Accepts: 128, 192, 256, all, or comma-separated values
                       (example: 128,192,256). Default: all.
    --spike            Use Spike instead of QEMU (default: QEMU).
EOF
}

VARIANT=""
LEVEL_INPUT="all"
USE_SPIKE=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --variant)
            if [[ $# -lt 2 || "$2" == --* ]]; then
                echo "ERROR: --variant requires a value" >&2
                usage >&2
                exit 1
            fi
            VARIANT="$2"
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

if [[ -z "$VARIANT" ]]; then
    echo "ERROR: --variant is required" >&2
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

find_elf_for_target() {
    local target="$1"
    local direct_path="${REPO_ROOT}/build/bench/${target}.elf"
    if [[ -f "$direct_path" ]]; then
        echo "$direct_path"
        return 0
    fi

    local first_match
    first_match="$(find "${REPO_ROOT}/build" -type f -name "${target}.elf" 2>/dev/null | sort | head -n 1 || true)"
    if [[ -n "$first_match" ]]; then
        echo "$first_match"
        return 0
    fi

    return 1
}

overall_status=0

for level in "${LEVELS[@]}"; do
    target="hqc${level}_${VARIANT}"
    rsp="${REPO_ROOT}/tests/kat/PQCkemKAT_${level}.rsp"

    if [[ ! -f "$rsp" ]]; then
        echo "FAIL [HQC-${level}]: reference RSP file '$rsp' not found" >&2
        overall_status=1
        continue
    fi

    if ! elf="$(find_elf_for_target "$target")"; then
        echo "FAIL [HQC-${level}]: ELF not found for target '$target' under build/" >&2
        overall_status=1
        continue
    fi

    log="$(mktemp /tmp/hqc_kat_${level}_${VARIANT}_XXXXXX.log)"

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
