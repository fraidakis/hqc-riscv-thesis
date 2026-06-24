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
  --level LEVEL           Security level: 128, 192, 256, or all (default: all)
  --quality QUALITY       Iteration quality: dev or publish (hw only, default: dev)
  --remote HOST           Deploy and run on remote HOST via SSH (hw only, default: root@192.168.1.50)
  --local                 Run directly on this machine (hw only)
  --keccak IMPL           Keccak implementation override (e.g., ref, opt)
  --decode IMPL           Decode implementation override (e.g., ref, rvv, zvbc)
  --pgo                   Enable PGO (profile-guided optimization) for hw builds (kem/mul only)
  -h, --help              Show this help and exit

Platform-specific notes:
  sim: Uses Spike ISS with deterministic instret counting.
  hw:  Uses perf_event_open cycle counting on the target hardware.
       --quality publish uses N=10000 iterations with nice -n -20.
       --quality dev uses N=100 iterations (default).
       Either --remote HOST or --local must be specified (auto-detected on riscv64).
       Default remote: root@192.168.1.50 (from credentials file).

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
LEVEL="all"
QUALITY="dev"
REMOTE=""
REMOTE_EXPLICIT=0
LOCAL_MODE=0
KECCAK_IMPL=""
DECODE_IMPL=""
PGO=0

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
            VARIANT="$(bench_resolve_variant "$2")"
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
            REMOTE_EXPLICIT=1
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
        --pgo)
            PGO=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        -*)
            echo "ERROR: Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
        *)
            # Positional argument: treat as impl name for decode/keccak modes
            if [[ "$MODE" == "decode" && -z "$DECODE_IMPL" ]]; then
                DECODE_IMPL="$1"
            elif [[ "$MODE" == "keccak" && -z "$KECCAK_IMPL" ]]; then
                KECCAK_IMPL="$1"
            else
                echo "ERROR: Unexpected positional argument: $1" >&2
                usage >&2
                exit 1
            fi
            shift
            ;;
    esac
done

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
BOX_W=61  # inner width between ║ chars

_box_line() {
    local content="$1"
    printf "║  %-$(( BOX_W - 4 ))s  ║\n" "$content"
}

echo "╔$(printf '═%.0s' $(seq 1 $BOX_W))╗"
_box_line "run_bench.sh"
_box_line "$(printf 'Platform: %-5s  Mode: %-7s  Variant: %s' "$PLATFORM" "$MODE" "$VARIANT")"

# Second detail line
_detail="$(printf 'Level: %-5s' "$LEVEL")"
if [[ "$PLATFORM" == "hw" ]]; then
    _detail+="$(printf '  Quality: %-7s' "$QUALITY")"
    if [[ -n "$REMOTE" ]]; then
        _detail+="  Remote: ${REMOTE}"
    elif [[ "$LOCAL_MODE" -eq 1 ]]; then
        _detail+="  Local: yes"
    fi
fi
_box_line "$_detail"

if [[ -n "$KECCAK_IMPL" ]]; then
    _box_line "Keccak override: ${KECCAK_IMPL}"
fi
if [[ -n "$DECODE_IMPL" ]]; then
    _box_line "Decode override: ${DECODE_IMPL}"
fi
echo "╚$(printf '═%.0s' $(seq 1 $BOX_W))╝"
echo ""

# ── Platform dispatch ────────────────────────────────────────────────────────
case "$PLATFORM" in
    sim)
        # ── Simulation execution path ────────────────────────────────────
        RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
        SPIKE="${RISCV}/bin/spike"
        SPIKE_RUNNER="${REPO_ROOT}/sim/spike/run.sh"
        ISA="rv64gcv_zbc_zbb_zba_zvbc_zvbb_zvl256b"
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
            local track_args=("--variant" "$variant")
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
        # ── Hardware execution path ──────────────────────────────────────

        # ── Load credentials ─────────────────────────────────────────────
        CREDS_FILE="${REPO_ROOT}/docs/hardware/BPI-F3/bpi-f3-credentials.env"
        BPI_PASS=""
        if [[ -f "$CREDS_FILE" ]]; then
            # shellcheck source=/dev/null
            source "$CREDS_FILE"
            BPI_PASS="${BPI_PASS:-}"
        fi

        # ── Auto-detect local mode on riscv64 ────────────────────────────
        if [[ "$(uname -m)" == "riscv64" && "$REMOTE_EXPLICIT" -eq 0 ]]; then
            LOCAL_MODE=1
        fi

        # ── Default remote when not explicitly specified ─────────────────
        if [[ "$REMOTE_EXPLICIT" -eq 0 && "$LOCAL_MODE" -eq 0 ]]; then
            REMOTE="${BPI_USER:-root}@${BPI_HOST:-192.168.1.50}"
        fi

        # ── Validate execution target ───────────────────────────────────
        if [[ "$LOCAL_MODE" -eq 0 && -z "$REMOTE" ]]; then
            echo "ERROR: Not on riscv64 — specify --remote HOST or --local" >&2
            echo "       The hw platform requires a RISC-V target." >&2
            exit 1
        fi

        # ── SSH transport selection ──────────────────────────────────────
        # Prefer key-based auth (BatchMode). Fall back to sshpass if keys
        # are not configured and a password is available in the credentials
        # file.  Run `ssh-copy-id` once to avoid the sshpass fallback.
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

        # ── SSH/rsync wrappers ───────────────────────────────────────────
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

        # ── Remote infrastructure ────────────────────────────────────────
        REMOTE_DIR="~/hqc-riscv"
        BENCH_DIR="${REPO_ROOT}/tests/bench"

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

        # ── Variant-to-POLYMUL mapping ───────────────────────────────────
        # Makefile.hw now uses the same canonical names as implemented_variants.txt.
        hw_variant_to_polymul() {
            echo "$1"
        }

        # ── Variant expansion ────────────────────────────────────────
        # Hardware (BPI-F3) supports: scalar_ref, scalar_zb, vector_rvv.
        # vector_zvk requires Zvbc/Zvbb extensions not available on this SoC.
        HW_SUPPORTED_VARIANTS=(scalar_ref scalar_zb vector_rvv)

        bench_load_implemented_variants "$REPO_ROOT"

        if [[ "$VARIANT" == "all" ]]; then
            VARIANTS=("${HW_SUPPORTED_VARIANTS[@]}")
        else
            VARIANTS=("$VARIANT")
        fi

        # ── Build on target ──────────────────────────────────────────
        hw_build() {
            local level="$1" polymul="$2"
            local dir="$REPO_ROOT"
            if [[ "$LOCAL_MODE" -eq 0 ]]; then dir="${REMOTE_DIR}"; fi

            local target
            case "$MODE" in
                kem)     target="kem" ;;
                mul)     target="mul" ;;
                keccak)  target="keccak" ;;
                decode)  target="decode" ;;
            esac

            # Use PGO target if requested (only for kem/mul — keccak/decode not PGO-worthy)
            if [[ "$PGO" -eq 1 && ("$target" == "kem" || "$target" == "mul") ]]; then
                target="pgo_${target}"
            fi

            # Clean then build — show build output on failure for debugging
            local build_log extra_args=""
            case "$MODE" in
                kem)
                    extra_args="KECCAK=$KECCAK_IMPL DECODE=$DECODE_IMPL"
                    ;;
                decode)
                    if [[ -n "$DECODE_IMPL" ]]; then
                        extra_args="DECODE=$DECODE_IMPL"
                    fi
                    ;;
                mul|keccak)
                    # No extra component args needed
                    ;;
            esac
            if ! build_log=$(run_cmd "cd $dir && make -f Makefile.hw SUITE=bench LEVEL=$level POLYMUL=$polymul MODE=$QUALITY EXTENDED=0 $extra_args clean >/dev/null 2>&1; make -f Makefile.hw SUITE=bench LEVEL=$level POLYMUL=$polymul MODE=$QUALITY EXTENDED=0 $extra_args $target 2>&1"); then
                echo " ├── ❌ BUILD FAIL (level=$level)" >&2
                echo "$build_log" | tail -5 >&2
                return 1
            fi
        }

        # ── Execute benchmark binary ────────────────────────────────
        hw_execute() {
            local level="$1" polymul="$2"
            local dir="$REPO_ROOT"
            if [[ "$LOCAL_MODE" -eq 0 ]]; then dir="${REMOTE_DIR}"; fi

            local bin
            local decode_suffix="${DECODE_IMPL:-ref}"
            case "$MODE" in
                kem)     bin="build-hw/bench_hw_kem_hqc${level}_${polymul}" ;;
                mul)     bin="build-hw/bench_hw_mul_hqc${level}_${polymul}" ;;
                keccak)  bin="build-hw/bench_hw_keccak_hqc${level}" ;;
                decode)  bin="build-hw/bench_hw_decode_hqc${level}_${decode_suffix}" ;;
            esac

            local exec_prefix="taskset -c 7"
            if [[ "$QUALITY" == "publish" ]]; then
                exec_prefix="sudo nice -n -20 taskset -c 7"
            fi

            run_cmd "cd $dir && $exec_prefix ./$bin"
        }

        # ── Store CSV result ─────────────────────────────────────────
        hw_store_csv() {
            local output="$1" variant="$2" op="$3" slug="$4" timestamp="$5"

            local n_iter n_warmup
            if [[ "$QUALITY" == "publish" ]]; then
                n_iter=10000; n_warmup=100
            else
                n_iter=100; n_warmup=10
            fi

            local header
            header="$(bench_hw_csv_header "$variant" "$n_iter" "$n_warmup" "$QUALITY")"

            local csv_content="${header}
${output}"

            bench_hw_store_result "$csv_content" "$variant" "$op" "$timestamp" "$slug"
        }

        # ── Print progress line ──────────────────────────────────────
        hw_print_progress() {
            local output="$1" level="$2"

            local prefix=" ├──"
            # Use └── for the final level
            if [[ "$LEVELS" == "128 192 256" && "$level" == "256" ]]; then
                prefix=" └──"
            elif [[ "$LEVELS" != "128 192 256" ]]; then
                prefix=" └──"
            fi

            local ins_str=""
            if [[ "$MODE" == "kem" ]]; then
                local kp enc dec total
                kp="$(echo "$output" | awk -F',' '/^keypair,/ {print $4; exit}')"
                enc="$(echo "$output" | awk -F',' '/^encaps,/ {print $4; exit}')"
                dec="$(echo "$output" | awk -F',' '/^decaps,/ {print $4; exit}')"
                kp="${kp:-0}"; enc="${enc:-0}"; dec="${dec:-0}"
                total=$(awk "BEGIN {print $kp + $enc + $dec}")
                ins_str="$(printf 'Kp: %s | Enc: %s | Dec: %s | Total: %s' "$kp" "$enc" "$dec" "$total")"
            else
                local metric_field
                case "$MODE" in
                    mul)     metric_field="vect_mul" ;;
                    keccak)  metric_field="keccak" ;;
                    decode)  metric_field="decode" ;;
                esac
                local median
                median="$(echo "$output" | awk -F',' -v op="$metric_field" '$1==op {print $4; exit}')"
                median="${median:-0}"
                ins_str="${median} cycles (median)"
            fi

            echo "${prefix} L${level} ⋯ ${ins_str}"
        }

        # ── Per-variant runner ───────────────────────────────────────
        hw_run_single_variant() {
            local variant="$1"
            local polymul slug timestamp

            polymul="$(hw_variant_to_polymul "$variant")"
            slug="$(bench_variant_slug "$variant")"
            timestamp="$(date +'%Y-%m-%d_%H-%M')"

            # Auto-map keccak/decode implementations from variant
            local track_args=("--variant" "$variant")
            if [[ -n "$KECCAK_IMPL" ]]; then
                track_args+=("--keccak" "$KECCAK_IMPL")
            fi
            if [[ -n "$DECODE_IMPL" ]]; then
                track_args+=("--decode" "$DECODE_IMPL")
            fi
            bench_parse_track_args "${track_args[@]}"

            # Determine operation and output directory based on MODE
            local op out_dir
            case "$MODE" in
                kem)     op="kem";     out_dir="${REPO_ROOT}/results/hardware/benchmarks/kem" ;;
                mul)     op="mul";     out_dir="${REPO_ROOT}/results/hardware/benchmarks/components/poly_mul" ;;
                keccak)  op="keccak";  out_dir="${REPO_ROOT}/results/hardware/benchmarks/components/keccak" ;;
                decode)  op="decode";  out_dir="${REPO_ROOT}/results/hardware/benchmarks/components/decode" ;;
            esac

            # For keccak/decode modes, derive filename slug from the impl, not the mul variant.
            # Resolve effective impl name (default when not explicitly overridden).
            local file_slug="$slug"
            local variant_label="${variant}"
            if [[ "$MODE" == "keccak" ]]; then
                local keccak_effective="${KECCAK_IMPL:-ref}"
                local impl_flag="ref"
                case "$keccak_effective" in
                    opt|zvbb_*) impl_flag="opt" ;;
                esac
                file_slug="$(bench_variant_slug "$impl_flag")"
                variant_label="keccak_${keccak_effective}"
            elif [[ "$MODE" == "decode" ]]; then
                local decode_effective="${DECODE_IMPL:-ref}"
                local impl_flag="ref"
                if [[ "$decode_effective" == "rvv" ]]; then
                    impl_flag="rvv"
                elif [[ "$decode_effective" == "zvbc" || "$decode_effective" == "rs_zvbc_exp" ]]; then
                    impl_flag="zvbc"
                fi
                file_slug="$(bench_variant_slug "$impl_flag")"
                variant_label="decode_${decode_effective}"
            fi

            # Print variant header
            echo -e "\n📦 ${variant_label} (${MODE})"

            # Accumulate transformed rows across all levels.
            # We prepend a "level" column to match the sim CSV format:
            #   level,op,iterations,min_cycles,...
            local csv_col_header=""
            local accumulated_rows=""

            for level in $LEVELS; do
                # Build
                hw_build "$level" "$polymul" || continue

                # Execute
                local output
                output="$(hw_execute "$level" "$polymul")" || continue

                # Progress
                hw_print_progress "$output" "$level"

                # Extract column header from first level (strip comment lines)
                if [[ -z "$csv_col_header" ]]; then
                    csv_col_header="level,$(echo "$output" | grep -m1 '^op,')"
                fi

                # Extract data rows (skip comments and the header line) and prepend level
                local level_rows
                level_rows="$(echo "$output" | grep -v '^#' | grep -v '^op,' | awk -v lvl="$level" -F',' 'NF>1 {print lvl "," $0}')"

                if [[ -n "$level_rows" ]]; then
                    if [[ -n "$accumulated_rows" ]]; then
                        accumulated_rows="${accumulated_rows}
${level_rows}"
                    else
                        accumulated_rows="$level_rows"
                    fi
                fi
            done

            # Store the combined result for all levels (single write)
            if [[ -n "$accumulated_rows" && -n "$csv_col_header" ]]; then
                local combined_csv="${csv_col_header}
${accumulated_rows}"
                hw_store_csv "$combined_csv" "$variant" "$op" "$file_slug" "$timestamp"
            fi
        }

        # ── Main execution ───────────────────────────────────────────
        deploy
        set_governor

        for v in "${VARIANTS[@]}"; do
            hw_run_single_variant "$v"
        done

        # ── Sync results to remote (keep remote in sync with local) ────
        # Results are written locally by bench_hw_store_result. Push them
        # back to the remote so subsequent deploys don't carry stale CSVs.
        if [[ "$LOCAL_MODE" -eq 0 ]]; then
            echo ""
            echo "[sync] Pushing results to ${REMOTE}..."
            if ! _rsync -az "${REPO_ROOT}/results/hardware/" "${REMOTE}:${REMOTE_DIR}/results/hardware/"; then
                echo "WARNING: Failed to sync results to remote. Local results are correct." >&2
            fi
        fi
        ;;
esac
