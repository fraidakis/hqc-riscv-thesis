#!/usr/bin/env bash
# scripts/bench/run_bench.sh
# Unified benchmark runner for HQC-RISC-V.
#
# Supports both simulation (Spike ISS) and hardware (BPI-F3) platforms
# via the --platform flag.
#
# Examples:
#   run_bench.sh --platform sim --mode mul --variant scalar_zb --level 128
#   run_bench.sh --platform hw --mode kem --variant vector_rvv --quality publish
#   run_bench.sh --platform hw --remote bpi-f3 --mode mul --variant all --level all
#   run_bench.sh --platform sim --mode keccak --keccak opt

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# ── Usage ────────────────────────────────────────────────────────────────────
usage() {
    cat <<'EOF'
Usage:
  run_bench.sh --platform sim|hw [OPTIONS]

Required:
  --platform sim|hw       Execution platform (sim=Spike ISS, hw=BPI-F3 hardware)

Options:
  --mode MODE             Benchmark mode: kem, mul, keccak, decode (default: kem)
  --variant VARIANT       Variant name or "all" (default: scalar_zb)
  --level LEVEL           Security level: 128, 192, 256, or all (default: 128)
  --quality QUALITY       Iteration quality: dev or publish (hw only, default: dev)
  --remote HOST           Deploy and run on remote HOST via SSH (hw only)
  --local                 Run directly on this machine (hw only)
  --keccak IMPL           Keccak implementation override (e.g., ref, opt)
  --decode IMPL           Decode implementation override (e.g., ref, rvv, zvbc)
  -h, --help              Show this help and exit

Platform-specific notes:
  sim: Uses Spike ISS with deterministic instret counting.
  hw:  Uses perf_event_open cycle counting on the target hardware.
       --quality publish uses N=10000 iterations with nice -n -20.
       --quality dev uses N=100 iterations (default).
       Either --remote HOST or --local must be specified (auto-detected on riscv64).

Examples:
  run_bench.sh --platform sim --mode mul --variant scalar_zb
  run_bench.sh --platform hw --mode kem --variant vector_rvv --quality publish
  run_bench.sh --platform sim --mode keccak --keccak opt
  run_bench.sh --platform hw --remote bpi-f3 --mode mul --variant all --level all
EOF
}

# ── Defaults ─────────────────────────────────────────────────────────────────
PLATFORM=""
MODE="kem"
VARIANT="scalar_zb"
LEVEL="128"
QUALITY="dev"
REMOTE=""
LOCAL_MODE=0
KECCAK_IMPL=""
DECODE_IMPL=""

# ── Argument Parsing ─────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --platform)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --platform requires a value (sim or hw)" >&2
                usage >&2
                exit 1
            fi
            PLATFORM="$2"
            shift 2
            ;;
        --mode)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --mode requires a value" >&2
                usage >&2
                exit 1
            fi
            MODE="$2"
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
        --level)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --level requires a value" >&2
                usage >&2
                exit 1
            fi
            LEVEL="$2"
            shift 2
            ;;
        --quality)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --quality requires a value" >&2
                usage >&2
                exit 1
            fi
            QUALITY="$2"
            shift 2
            ;;
        --remote)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --remote requires a host" >&2
                usage >&2
                exit 1
            fi
            REMOTE="$2"
            LOCAL_MODE=0
            shift 2
            ;;
        --local)
            LOCAL_MODE=1
            shift
            ;;
        --keccak)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --keccak requires a value" >&2
                usage >&2
                exit 1
            fi
            KECCAK_IMPL="$2"
            shift 2
            ;;
        --decode)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --decode requires a value" >&2
                usage >&2
                exit 1
            fi
            DECODE_IMPL="$2"
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

# ── Legacy variant name resolution ───────────────────────────────────────────
# Map old variant names to their canonical equivalents (backward compatibility).
case "$VARIANT" in
    ref_scalar)
        echo "DEPRECATED: variant 'ref_scalar' is now 'scalar_ref'" >&2
        VARIANT="scalar_ref"
        ;;
    karatsuba_zbc)
        echo "DEPRECATED: variant 'karatsuba_zbc' is now 'scalar_zb'" >&2
        VARIANT="scalar_zb"
        ;;
    ref_vector)
        echo "DEPRECATED: variant 'ref_vector' is now 'vector_ref'" >&2
        VARIANT="vector_ref"
        ;;
    karatsuba_rvv)
        echo "DEPRECATED: variant 'karatsuba_rvv' is now 'vector_rvv'" >&2
        VARIANT="vector_rvv"
        ;;
    karatsuba_zvbc)
        echo "DEPRECATED: variant 'karatsuba_zvbc' is now 'vector_zvk'" >&2
        VARIANT="vector_zvk"
        ;;
esac

# ── Validation ───────────────────────────────────────────────────────────────

# --platform is required
if [[ -z "$PLATFORM" ]]; then
    echo "ERROR: --platform is required (sim or hw)" >&2
    usage >&2
    exit 1
fi

# Validate platform value
if [[ "$PLATFORM" != "sim" && "$PLATFORM" != "hw" ]]; then
    echo "ERROR: --platform must be 'sim' or 'hw', got: '$PLATFORM'" >&2
    usage >&2
    exit 1
fi

# Validate mode
if [[ "$MODE" != "kem" && "$MODE" != "mul" && "$MODE" != "keccak" && "$MODE" != "decode" ]]; then
    echo "ERROR: --mode must be one of: kem, mul, keccak, decode; got: '$MODE'" >&2
    exit 1
fi

# Validate level
if [[ "$LEVEL" != "128" && "$LEVEL" != "192" && "$LEVEL" != "256" && "$LEVEL" != "all" ]]; then
    echo "ERROR: --level must be one of: 128, 192, 256, all; got: '$LEVEL'" >&2
    exit 1
fi

# Validate quality (hw only)
if [[ "$QUALITY" != "dev" && "$QUALITY" != "publish" ]]; then
    echo "ERROR: --quality must be 'dev' or 'publish', got: '$QUALITY'" >&2
    exit 1
fi

# ── Source shared library ────────────────────────────────────────────────────
# shellcheck source=lib_bench.sh
source "${SCRIPT_DIR}/lib_bench.sh"

# ── Expand level ─────────────────────────────────────────────────────────────
if [[ "$LEVEL" == "all" ]]; then
    LEVELS="128 192 256"
else
    LEVELS="$LEVEL"
fi

# ── Validate variant ─────────────────────────────────────────────────────────
# Skip validation when VARIANT is "all" (expansion handled later per-platform)
if [[ "$VARIANT" != "all" ]]; then
    VARIANTS_FILE="${REPO_ROOT}/scripts/implemented_variants.txt"
    if [[ ! -f "$VARIANTS_FILE" ]]; then
        echo "ERROR: Cannot find implemented_variants.txt at: $VARIANTS_FILE" >&2
        exit 1
    fi
    # Read valid variants (skip blank lines)
    mapfile -t VALID_VARIANTS < <(grep -v '^\s*$' "$VARIANTS_FILE")
    _variant_found=0
    for _v in "${VALID_VARIANTS[@]}"; do
        if [[ "$_v" == "$VARIANT" ]]; then
            _variant_found=1
            break
        fi
    done
    if [[ "$_variant_found" -eq 0 ]]; then
        echo "ERROR: Invalid variant '$VARIANT'" >&2
        echo "Valid variants:" >&2
        for _v in "${VALID_VARIANTS[@]}"; do
            echo "  - $_v" >&2
        done
        exit 1
    fi
    unset _variant_found _v VALID_VARIANTS
fi

# ── Print configuration ──────────────────────────────────────────────────────
echo "╔══════════════════════════════════════════════════════════════╗"
printf "║  run_bench.sh                                                ║\n"
printf "║  Platform: %-5s  Mode: %-7s  Variant: %-15s ║\n" "$PLATFORM" "$MODE" "$VARIANT"
printf "║  Level: %-7s" "$LEVEL"
if [[ "$PLATFORM" == "hw" ]]; then
    printf "  Quality: %-7s" "$QUALITY"
    if [[ -n "$REMOTE" ]]; then
        printf "  Remote: %-10s" "$REMOTE"
    elif [[ "$LOCAL_MODE" -eq 1 ]]; then
        printf "  Local: yes       "
    fi
fi
printf "║\n"
if [[ -n "$KECCAK_IMPL" ]]; then
    printf "║  Keccak override: %-40s ║\n" "$KECCAK_IMPL"
fi
if [[ -n "$DECODE_IMPL" ]]; then
    printf "║  Decode override: %-40s ║\n" "$DECODE_IMPL"
fi
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# ── Platform dispatch ────────────────────────────────────────────────────────
case "$PLATFORM" in
    sim)
        # ── Simulation execution path ────────────────────────────────────
        RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
        SPIKE="${RISCV}/bin/spike"
        SPIKE_RUNNER="${REPO_ROOT}/sim/spike/run.sh"
        ISA="rv64gcv_zbc_zbb_zba_zvbc_zvbb_zvl256b_zicntr_zihpm"
        BENCH_EXPECT_HQC_SCALAR_MARCH="${BENCH_EXPECT_HQC_SCALAR_MARCH:-rv64gc_zbc_zbb}"
        export BENCH_EXPECT_HQC_SCALAR_MARCH
        BENCH_TIME="$(date +'%Y-%m-%d_%H-%M')"
        GIT_HASH="$(git rev-parse --short HEAD 2>/dev/null || echo 'unknown')"

        if ! bench_require_executable "$SPIKE" "Spike not found"; then
            echo "       Source toolchain/toolchain.env or set RISCV" >&2
            exit 2
        fi
        if ! bench_require_executable "$SPIKE_RUNNER" "Spike runner not found"; then
            exit 2
        fi

        # Load implemented variants for --variant all expansion
        bench_load_implemented_variants "$REPO_ROOT"

        # Expand --variant all
        if [[ "$VARIANT" == "all" ]]; then
            VARIANTS=("${IMPLEMENTED_VARIANTS[@]}")
        else
            VARIANTS=("$VARIANT")
        fi

        # ── Helper: run simulation for a single variant + level set ───────
        sim_run_single_variant() {
            local variant="$1"
            local slug
            slug="$(bench_variant_slug "$variant")"

            # Set up track args for keccak/decode auto-mapping
            local track_args=("--mul" "$variant")
            if [[ -n "$KECCAK_IMPL" ]]; then
                track_args+=("--keccak" "$KECCAK_IMPL")
            fi
            if [[ -n "$DECODE_IMPL" ]]; then
                track_args+=("--decode" "$DECODE_IMPL")
            fi
            bench_parse_track_args "${track_args[@]}"

            local build_dir="${REPO_ROOT}/build/spike/bench_k${KECCAK_IMPL}_d${DECODE_IMPL}"

            # Determine output directory and operation based on MODE
            local out_dir op
            case "$MODE" in
                mul)
                    out_dir="${REPO_ROOT}/results/simulation/benchmarks/components/poly_mul"
                    op="mul"
                    ;;
                kem)
                    out_dir="${REPO_ROOT}/results/simulation/benchmarks/kem"
                    op="kem"
                    ;;
                keccak)
                    out_dir="${REPO_ROOT}/results/simulation/benchmarks/components/keccak"
                    op="keccak"
                    ;;
                decode)
                    out_dir="${REPO_ROOT}/results/simulation/benchmarks/components/decode"
                    op="decode"
                    ;;
            esac
            mkdir -p "$out_dir"

            # Determine the variant slug used in filenames
            local file_slug variant_header
            variant_header="# Multiplication variant: ${variant}"
            if [[ "$MODE" == "keccak" ]]; then
                local impl_flag="ref"
                case "$KECCAK_IMPL" in
                    opt|zvbb_*) impl_flag="opt" ;;
                esac
                file_slug="$(bench_variant_slug "$impl_flag")"
                variant_header="# Keccak variant: ${KECCAK_IMPL}"
            elif [[ "$MODE" == "decode" ]]; then
                local impl_flag="ref"
                if [[ "$DECODE_IMPL" == "rvv" ]]; then
                    impl_flag="rvv"
                elif [[ "$DECODE_IMPL" == "zvbc" || "$DECODE_IMPL" == "rs_zvbc_exp" ]]; then
                    impl_flag="zvbc"
                fi
                file_slug="$(bench_variant_slug "$impl_flag")"
                variant_header="# Decode variant: ${DECODE_IMPL}"
            else
                file_slug="$slug"
            fi

            # Build filenames using the unified naming convention
            local csv csv_latest
            csv="${out_dir}/$(bench_build_filename "sim" "$op" "$file_slug" "$BENCH_TIME")"
            csv_latest="${out_dir}/$(bench_build_filename "sim" "$op" "$file_slug" "latest")"
            echo "# date: $(date +'%Y-%m-%d %H:%M')" > "$csv"
            echo "# commit: ${GIT_HASH}" >> "$csv"
            echo "$variant_header" >> "$csv"
            echo "level,op,instructions,codesize" >> "$csv"

            if [[ "$MODE" == "keccak" ]]; then
                echo -e "\n📦 keccak_${KECCAK_IMPL} (isolated)"
            elif [[ "$MODE" == "decode" ]]; then
                echo -e "\n📦 decode_${DECODE_IMPL} (isolated)"
            else
                echo -e "\n📦 ${variant} (${MODE})"
            fi

            # Loop through security levels
            for level in $LEVELS; do
                local elf target
                case "$MODE" in
                    kem)
                        elf="${build_dir}/bench_hqc${level}_${variant}.elf"
                        target="bench_hqc${level}_${variant}"
                        ;;
                    mul)
                        elf="${build_dir}/bench_polymul_hqc${level}_${variant}.elf"
                        target="bench_polymul_hqc${level}_${variant}"
                        ;;
                    keccak)
                        elf="${build_dir}/bench_keccak_hqc${level}.elf"
                        target="bench_keccak_hqc${level}"
                        ;;
                    decode)
                        elf="${build_dir}/bench_decode_hqc${level}.elf"
                        target="bench_decode_hqc${level}"
                        ;;
                esac

                if ! bench_auto_configure_and_build "$build_dir" "$REPO_ROOT" "$target" "spike" "0" "bench"; then
                    continue
                fi

                if [[ ! -f "$elf" ]]; then
                    echo "[bench] SKIP  ${target}: ELF build failed" >&2
                    continue
                fi

                # Run Spike, capture raw output
                local tmp_log
                tmp_log=$(mktemp)
                "$SPIKE_RUNNER" --clean "$elf" > "$tmp_log" 2>/dev/null

                # Get code size
                local size_tool text_bytes
                size_tool="$RISCV/bin/llvm-size"
                if [[ ! -x "$size_tool" ]]; then
                    size_tool="$RISCV/bin/riscv64-unknown-elf-size"
                fi
                text_bytes="$($size_tool "$elf" 2>/dev/null | awk 'NR==2{print $1}')"
                text_bytes="${text_bytes:-0}"

                # Parse raw Spike output into CSV rows
                awk -F',' -v level="$level" -v codesize="$text_bytes" '
                    /^(vect_mul|keccak|decode|keypair|encaps|decaps),/ {
                        op = $1
                        instructions = $7
                        printf "%s,%s,%s,%s\n", level, op, instructions, codesize
                    }
                ' "$tmp_log" >> "$csv"

                # If KEM mode, also add a "total" row
                if [[ "$MODE" == "kem" ]]; then
                    local total
                    total=$(awk -F',' '
                        /^(keypair|encaps|decaps),/ { sum += $7 }
                        END { print sum+0 }
                    ' "$tmp_log")
                    echo "${level},total,${total},${text_bytes}" >> "$csv"
                fi

                # Print progress
                local ins_str=""
                if [[ "$MODE" == "kem" ]]; then
                    local kp_raw enc_raw dec_raw total_raw
                    kp_raw="$(awk -F',' '/^keypair,/ {print $7; exit}' "$tmp_log")"
                    enc_raw="$(awk -F',' '/^encaps,/ {print $7; exit}' "$tmp_log")"
                    dec_raw="$(awk -F',' '/^decaps,/ {print $7; exit}' "$tmp_log")"
                    kp_raw="${kp_raw:-0}"; enc_raw="${enc_raw:-0}"; dec_raw="${dec_raw:-0}"
                    total_raw=$((kp_raw + enc_raw + dec_raw))
                    ins_str="$(printf 'Kp: %s | Enc: %s | Dec: %s | Total: %s' "$kp_raw" "$enc_raw" "$dec_raw" "$total_raw")"
                else
                    local metric_raw
                    case "$MODE" in
                        mul)    metric_raw="$(awk -F',' '/^vect_mul,/ {print $7; exit}' "$tmp_log")" ;;
                        keccak) metric_raw="$(awk -F',' '/^keccak,/   {print $7; exit}' "$tmp_log")" ;;
                        decode) metric_raw="$(awk -F',' '/^decode,/   {print $7; exit}' "$tmp_log")" ;;
                    esac
                    metric_raw="${metric_raw:-0}"
                    ins_str="${metric_raw} instructions"
                fi

                local prefix=" ├──"
                if [[ "$level" == "256" && "$LEVELS" == "128 192 256" ]]; then prefix=" └──"; fi
                # Also use └── if this is the last level in a non-all expansion
                if [[ "$LEVELS" != "128 192 256" ]]; then prefix=" └──"; fi
                echo "${prefix} L${level} ⋯ ${ins_str}"
                rm -f "$tmp_log"
            done

            echo ""
            bench_refresh_tracked_csv "$csv" "$csv_latest" || true
        }

        # ── Execute for each variant ─────────────────────────────────────
        for v in "${VARIANTS[@]}"; do
            sim_run_single_variant "$v"
        done
        ;;
    hw)
        # TODO(task 7.3): Implement hardware execution path
        #   - Build via Makefile.hw (local) or deploy via SSH (remote)
        #   - Execute with taskset isolation
        #   - Store results via bench_hw_store_result
        #   - Handle --quality publish (N=10000, nice -n -20)
        echo "[run_bench] Hardware path — not yet implemented (see task 7.3)"
        ;;
esac
