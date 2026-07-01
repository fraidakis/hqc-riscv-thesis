#!/usr/bin/env bash
# scripts/bench/run_vlen_sweep.sh
# VLEN scaling sweep for HQC-RISC-V vector variants on Spike ISS.
#
# Builds one ELF per (variant, level, VLEN) combination — each compiled with
# -mrvv-vector-bits=VLEN so the compiler can constant-fold vsetvl results —
# then runs each ELF on Spike with the matching zvl<VLEN>b ISA string and
# collects instruction-retired counts.
#
# Output CSVs mirror the existing format in
#   results/simulation/benchmarks/vlen_scaling/
# one file per (level, variant):
#   sim_vlen_l<LEVEL>-<slug>_<timestamp>.csv   (archive copy)
#   sim_vlen_l<LEVEL>-<slug>_latest.csv        (tracked latest)
#
# Only vector variants participate (vector_rvv, vector_zvk). Scalar variants
# are rejected with a clear error.
#
# Examples:
#   run_vlen_sweep.sh --variant vector_zvk
#   run_vlen_sweep.sh --variant vector_rvv --level 128
#   run_vlen_sweep.sh --variant all --level all
#   run_vlen_sweep.sh --variant vector_zvk --vlens "256 512 1024"

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
  run_vlen_sweep.sh --variant VARIANT [OPTIONS]

Required:
  --variant VARIANT   Vector variant or "all" (vector_rvv, vector_zvk, or all)

Options:
  --level LEVEL       Security level: 128, 192, 256, or all (default: all)
  --vlens "V1 V2…"   Space-separated VLEN values to sweep (default: "256 512 1024 2048")
  --keccak IMPL       Keccak override (ref | opt); auto-mapped from variant if omitted
  --decode IMPL       Decode override (ref | rvv | zvbc); auto-mapped from variant if omitted
  --timeout SECS      Per-VLEN Spike timeout in seconds (default: 3, env: SPIKE_TIMEOUT)
  -h, --help          Show this help and exit

Vector variants:
  vector_rvv   RVV hybrid Karatsuba (Keccak=ref, Decode=rvv by default)
  vector_zvk   Zvbc/Zvbb champion   (Keccak=ref, Decode=zvbc by default)
  all          Run both vector_rvv and vector_zvk

Notes:
  - Scalar variants (scalar_ref, scalar_zb) are not supported.
  - Each (variant, VLEN) pair gets its own build directory so ELFs compiled
    at different vector widths never collide:
      build/spike/bench_k<keccak>_d<decode>_vlen<VLEN>
  - Each VLEN build is configured with -DHQC_VECTOR_BITS=<VLEN> and
    -DVLEN_LIST=<VLEN> so the _vlen<VLEN> CMake targets are generated.
  - Spike is invoked with --isa=...zvl<VLEN>b so the emulated register
    width matches the compiled assumption.
  - Spike's Zvbc (vclmul/vclmulh) simulation has super-linear cost with
    VLEN, causing timeouts at VLEN≥1024 for vector_zvk and ≥2048 for
    vector_rvv.  These are reported as TIMEOUT and the sweep continues.
    Override with --timeout or SPIKE_TIMEOUT env variable.
  - Results are written to:
      results/simulation/benchmarks/vlen_scaling/

Examples:
  run_vlen_sweep.sh --variant vector_zvk
  run_vlen_sweep.sh --variant vector_rvv --level 128
  run_vlen_sweep.sh --variant all --level all
  run_vlen_sweep.sh --variant vector_zvk --vlens "256 512 1024"
EOF
}

# ── Defaults ─────────────────────────────────────────────────────────────────
VARIANT=""
LEVEL="all"
VLENS="256 512 1024 2048"
KECCAK_IMPL_ARG=""
DECODE_IMPL_ARG=""

# ── Argument Parsing ─────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
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
        --vlens)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --vlens requires a quoted list of values" >&2
                usage >&2
                exit 1
            fi
            VLENS="$2"
            shift 2
            ;;
        --keccak)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --keccak requires a value" >&2
                usage >&2
                exit 1
            fi
            KECCAK_IMPL_ARG="$2"
            shift 2
            ;;
        --decode)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --decode requires a value" >&2
                usage >&2
                exit 1
            fi
            DECODE_IMPL_ARG="$2"
            shift 2
            ;;
        --timeout)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --timeout requires a value (seconds)" >&2
                usage >&2
                exit 1
            fi
            SPIKE_TIMEOUT="$2"
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

if [[ -z "$VARIANT" ]]; then
    echo "ERROR: --variant is required" >&2
    usage >&2
    exit 1
fi

if [[ "$LEVEL" != "128" && "$LEVEL" != "192" && "$LEVEL" != "256" && "$LEVEL" != "all" ]]; then
    echo "ERROR: --level must be one of: 128, 192, 256, all; got: '$LEVEL'" >&2
    exit 1
fi

# Vector-only: reject scalar variants early with a clear message.
VECTOR_VARIANTS=(vector_rvv vector_zvk)
if [[ "$VARIANT" != "all" ]]; then
    _is_vector=0
    for _v in "${VECTOR_VARIANTS[@]}"; do
        [[ "$_v" == "$VARIANT" ]] && _is_vector=1 && break
    done
    if [[ "$_is_vector" -eq 0 ]]; then
        echo "ERROR: '$VARIANT' is not a vector variant." >&2
        echo "       VLEN sweep only applies to: vector_rvv, vector_zvk, or all" >&2
        exit 1
    fi
    unset _is_vector _v
fi

# Validate each requested VLEN: must be a power-of-two integer >= 128.
for _vlen in $VLENS; do
    if [[ ! "$_vlen" =~ ^[0-9]+$ ]] || (( _vlen < 128 )) || (( (_vlen & (_vlen - 1)) != 0 )); then
        echo "ERROR: Invalid VLEN '$_vlen'. Must be a power-of-two integer >= 128." >&2
        exit 1
    fi
done
unset _vlen

# ── Environment ──────────────────────────────────────────────────────────────
RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
SPIKE="${RISCV}/bin/spike"
TOOLCHAIN_FILE="${HQC_TOOLCHAIN_FILE:-toolchain/riscv-gcc.cmake}"
SCALAR_MARCH="${BENCH_EXPECT_HQC_SCALAR_MARCH:-rv64gc_zbc_zbb_zbs}"
BENCH_TIME="$(date +'%Y-%m-%d_%H-%M')"
GIT_HASH="$(git rev-parse --short HEAD 2>/dev/null || echo 'unknown')"

# Per-invocation timeout (seconds).  Spike's Zvbc (vclmul/vclmulh) simulation
# has O(VLEN) per-element cost that becomes prohibitive at large VLEN (≥1024
# for vector_zvk, ≥2048 for vector_rvv).  A generous timeout prevents the
# sweep from hanging while still allowing legitimately slow runs to complete.
# Calibrated at ~5× the slowest successful run (HQC-256 vector_zvk ≈ 0.5s).
SPIKE_TIMEOUT="${SPIKE_TIMEOUT:-3}"

if ! bench_require_executable "$SPIKE" "Spike not found"; then
    echo "       Source toolchain/toolchain.env or set RISCV" >&2
    exit 2
fi

# ISA base string — zvl<VLEN>b is appended per VLEN at run time.
# Include the full set of extensions so both variants run correctly.
ISA_BASE="rv64gcv_zbc_zbb_zba_zvbc_zvbb"

# ── Expand variant and level ─────────────────────────────────────────────────
if [[ "$VARIANT" == "all" ]]; then
    VARIANTS=("${VECTOR_VARIANTS[@]}")
else
    VARIANTS=("$VARIANT")
fi

if [[ "$LEVEL" == "all" ]]; then
    LEVELS="128 192 256"
else
    LEVELS="$LEVEL"
fi

# Compute the last VLEN in the list once (used for progress prefix).
LAST_VLEN="$(echo "$VLENS" | awk '{print $NF}')"

# ── Print configuration ───────────────────────────────────────────────────────
BOX_W=61

_box_line() {
    local content="$1"
    printf "║  %-$(( BOX_W - 4 ))s  ║\n" "$content"
}

echo "╔$(printf '═%.0s' $(seq 1 $BOX_W))╗"
_box_line "run_vlen_sweep.sh"
_box_line "$(printf 'Variant: %-12s  Level: %-5s' "$VARIANT" "$LEVEL")"
_box_line "$(printf 'VLENs: %s' "$VLENS")"
_box_line "$(printf 'Spike timeout: %ss per invocation' "$SPIKE_TIMEOUT")"
[[ -n "$KECCAK_IMPL_ARG" ]] && _box_line "Keccak override: ${KECCAK_IMPL_ARG}"
[[ -n "$DECODE_IMPL_ARG" ]] && _box_line "Decode override: ${DECODE_IMPL_ARG}"
echo "╚$(printf '═%.0s' $(seq 1 $BOX_W))╝"
echo ""

# ── Output directory ─────────────────────────────────────────────────────────
OUT_DIR="${REPO_ROOT}/results/simulation/benchmarks/vlen_scaling"
mkdir -p "${OUT_DIR}/archive"

# ── vlen_configure_and_build ─────────────────────────────────────────────────
# Configure and build a per-VLEN bench dir. Extends bench_auto_configure_and_build
# by also passing -DVLEN_LIST=<VLEN> so CMake generates the _vlen<VLEN> targets.
#
# The function sets HQC_VECTOR_BITS in the environment before delegating so
# bench_auto_configure_and_build picks it up via ${HQC_VECTOR_BITS:-256}.
# If the cache already exists with the correct settings the cmake step is
# skipped by bench_auto_configure_and_build's cache-comparison logic.
#
# For the VLEN_LIST case we must also pass it explicitly on the cmake command
# line, because bench_auto_configure_and_build does not know about it.
# We therefore do the configure step ourselves when VLEN_LIST needs setting.
vlen_configure_and_build() {
    local build_dir="$1"
    local target="$2"
    local vlen="$3"
    # KECCAK_IMPL / DECODE_IMPL are read from the caller's exported globals.

    # CMake only accepts HQC_KECCAK_IMPL=ref. The opt/ref distinction is
    # handled via SCALAR_MARCH (with/without Zbb for idiom recognition).
    # --keccak opt → keep _zbb in march so rori/andn idiom recognition fires.
    # --keccak ref → strip _zbb from march (baseline codegen).
    local keccak="ref"
    local decode="${DECODE_IMPL:-ref}"
    local lock_fd=""

    mkdir -p "$build_dir"
    if command -v flock >/dev/null 2>&1; then
        exec {lock_fd}>"${build_dir}.lock"
        flock "$lock_fd"
    fi

    local need_config=0
    local config_reason=""

    if [[ ! -f "${build_dir}/CMakeCache.txt" ]]; then
        need_config=1
        config_reason="missing build dir"
    else
        local cache_file="${build_dir}/CMakeCache.txt"
        local cached_keccak cached_decode cached_scalar cached_vbits cached_vlen_list
        local cached_toolchain_base want_toolchain_base
        cached_keccak="$(awk    -F'=' '/^HQC_KECCAK_IMPL:STRING=/{print $2; exit}'  "$cache_file")"
        cached_decode="$(awk    -F'=' '/^HQC_DECODE_IMPL:STRING=/{print $2; exit}'  "$cache_file")"
        cached_scalar="$(awk    -F'=' '/^HQC_SCALAR_MARCH:STRING=/{print $2; exit}' "$cache_file")"
        cached_vbits="$( awk    -F'=' '/^HQC_VECTOR_BITS:STRING=/{print $2; exit}'  "$cache_file")"
        cached_vlen_list="$(awk -F'=' '/^VLEN_LIST:STRING=/{print $2; exit}'         "$cache_file")"
        cached_vbits="${cached_vbits:-256}"
        cached_toolchain_base="$(awk -F'=' \
            '/^CMAKE_TOOLCHAIN_FILE:(FILEPATH|UNINITIALIZED)=/{print $2; exit}' \
            "$cache_file" | xargs basename 2>/dev/null || true)"
        want_toolchain_base="$(basename "$TOOLCHAIN_FILE")"

        if [[ -z "$cached_keccak" || -z "$cached_decode" || -z "$cached_scalar" ]]; then
            need_config=1
            config_reason="incomplete cache"
        elif [[ "$cached_keccak"        != "$keccak"                   || \
                "$cached_decode"        != "$decode"                   || \
                "$cached_scalar"        != "$SCALAR_MARCH"             || \
                "$cached_vbits"         != "$vlen"                     || \
                "$cached_vlen_list"     != "$vlen"                     || \
                "$cached_toolchain_base" != "$want_toolchain_base" ]]; then
            need_config=1
            config_reason="cache mismatch"
        fi
    fi

    # Also reconfigure if the requested target is missing from the Makefile
    # (e.g. after a variant rename or VLEN_LIST change).
    if [[ "$need_config" -eq 0 && -f "${build_dir}/Makefile" ]]; then
        if ! grep -q "^${target}:" "${build_dir}/Makefile"; then
            need_config=1
            config_reason="stale targets (${target} missing)"
        fi
    fi

    if [[ "$need_config" -eq 1 ]]; then
        echo " ├── ⚙️  Auto-configuring ${config_reason}: $(basename "$build_dir")..."
        if ! cmake \
                -S "$REPO_ROOT" \
                -B "$build_dir" \
                -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
                -DCMAKE_BUILD_TYPE=Release \
                -DHQC_SIM=spike \
                -DHQC_KECCAK_IMPL="${keccak}" \
                -DHQC_DECODE_IMPL="${decode}" \
                -DHQC_SCALAR_MARCH="${SCALAR_MARCH}" \
                -DHQC_VECTOR_BITS="${vlen}" \
                -DVLEN_LIST="${vlen}" \
                >/dev/null 2>&1; then
            [[ -n "$lock_fd" ]] && eval "exec ${lock_fd}>&-"
            echo "[vlen-sweep] AUTO-CONFIG FAIL ${build_dir}" >&2
            return 1
        fi
    fi

    if ! cmake --build "$build_dir" --target "$target" -j"$(nproc)" >/dev/null 2>&1; then
        [[ -n "$lock_fd" ]] && eval "exec ${lock_fd}>&-"
        echo "[vlen-sweep] AUTO-BUILD FAIL ${target}" >&2
        return 1
    fi

    [[ -n "$lock_fd" ]] && eval "exec ${lock_fd}>&-"
    return 0
}

# ── Per-variant sweep runner ─────────────────────────────────────────────────
run_vlen_sweep_single_variant() {
    local variant="$1"
    local slug
    slug="$(bench_variant_slug "$variant")"

    # Resolve Keccak/Decode impls for this variant (auto-map if not overridden).
    local track_args=("--variant" "$variant")
    [[ -n "$KECCAK_IMPL_ARG" ]] && track_args+=("--keccak" "$KECCAK_IMPL_ARG")
    [[ -n "$DECODE_IMPL_ARG" ]] && track_args+=("--decode" "$DECODE_IMPL_ARG")
    bench_parse_track_args "${track_args[@]}"
    # KECCAK_IMPL / DECODE_IMPL are now exported globals.

    echo "━━━ ${variant} (keccak=${KECCAK_IMPL}, decode=${DECODE_IMPL}) ━━━"
    echo ""

    for level in $LEVELS; do
        echo "📦 ${variant} — HQC-${level}"

        # ── Per-level CSV ─────────────────────────────────────────────────
        # Filename convention matches existing files:
        #   sim_vlen_l<LEVEL>-<slug>_<timestamp>.csv
        local csv="${OUT_DIR}/sim_vlen_l${level}-${slug}_${BENCH_TIME}.csv"
        local csv_latest="${OUT_DIR}/sim_vlen_l${level}-${slug}_latest.csv"

        # Write CSV header.
        {
            echo "# date: $(date +'%Y-%m-%d %H:%M')"
            echo "# commit: ${GIT_HASH}"
            echo "# variant: ${variant}"
            echo "# level: ${level}"
            echo "op,vlen,instructions"
        } > "$csv"

        # Collect per-op instruction counts across all VLENs, then emit rows
        # in the canonical order: keypair, encaps, decaps, total.
        declare -A _kp _enc _dec

        for vlen in $VLENS; do
            # One build dir per (keccak, decode, VLEN) triple — matches the
            # existing naming convention used in the build tree:
            #   build/spike/bench_k<keccak>_d<decode>_vlen<VLEN>
            local build_dir="${REPO_ROOT}/build/spike/bench_k${KECCAK_IMPL}_d${DECODE_IMPL}_vlen${vlen}"
            local target="bench_hqc${level}_${variant}"
            local elf="${build_dir}/${target}.elf"

            vlen_configure_and_build "$build_dir" "$target" "$vlen" || {
                echo " ├── ❌ BUILD FAIL  ${target}" >&2
                _kp[$vlen]=""; _enc[$vlen]=""; _dec[$vlen]=""
                continue
            }

            if [[ ! -f "$elf" ]]; then
                echo " ├── ❌ SKIP  ${target}: ELF not found after build" >&2
                _kp[$vlen]=""; _enc[$vlen]=""; _dec[$vlen]=""
                continue
            fi

            # Run Spike with the matching zvl<VLEN>b ISA string.
            # A timeout guard is essential: Spike's Zvbc (vclmul/vclmulh)
            # simulation cost scales super-linearly with VLEN, causing
            # multi-minute hangs at VLEN≥1024 for vector_zvk and ≥2048 for
            # vector_rvv.  The timeout lets the sweep skip infeasible VLENs
            # gracefully instead of blocking indefinitely.
            local isa="${ISA_BASE}_zvl${vlen}b"
            local tmp_log
            tmp_log="$(mktemp)"
            local spike_rc=0
            timeout "$SPIKE_TIMEOUT" "$SPIKE" --isa="$isa" "$elf" > "$tmp_log" 2>/dev/null || spike_rc=$?
            if [[ "$spike_rc" -ne 0 ]]; then
                if [[ "$spike_rc" -eq 124 ]]; then
                    echo " ├── ⏱️  TIMEOUT  VLEN=${vlen}: Spike exceeded ${SPIKE_TIMEOUT}s (simulator limitation)" >&2
                else
                    echo " ├── ❌ SKIP  VLEN=${vlen}: Spike exited with error (rc=${spike_rc})" >&2
                fi
                rm -f "$tmp_log"
                _kp[$vlen]=""; _enc[$vlen]=""; _dec[$vlen]=""
                continue
            fi

            # Extract KEM instruction counts from the bench harness output.
            local kp enc dec
            kp="$(awk  -F',' '/^keypair,/ {print $7; exit}' "$tmp_log")"
            enc="$(awk -F',' '/^encaps,/  {print $7; exit}' "$tmp_log")"
            dec="$(awk -F',' '/^decaps,/  {print $7; exit}' "$tmp_log")"
            kp="${kp:-0}"; enc="${enc:-0}"; dec="${dec:-0}"

            _kp[$vlen]="$kp"
            _enc[$vlen]="$enc"
            _dec[$vlen]="$dec"

            rm -f "$tmp_log"

            # Progress line.
            local prefix=" ├──"
            [[ "$vlen" == "$LAST_VLEN" ]] && prefix=" └──"
            printf '%s VLEN=%-4s ⋯ Kp: %s | Enc: %s | Dec: %s\n' \
                "$prefix" "$vlen" "$kp" "$enc" "$dec"
        done

        # ── Emit CSV rows in canonical order ──────────────────────────────
        for vlen in $VLENS; do
            [[ -n "${_kp[$vlen]:-}"  ]] && echo "keypair,${vlen},${_kp[$vlen]}"  >> "$csv"
        done
        for vlen in $VLENS; do
            [[ -n "${_enc[$vlen]:-}" ]] && echo "encaps,${vlen},${_enc[$vlen]}"   >> "$csv"
        done
        for vlen in $VLENS; do
            [[ -n "${_dec[$vlen]:-}" ]] && echo "decaps,${vlen},${_dec[$vlen]}"   >> "$csv"
        done
        # total row: sum of all three ops per VLEN.
        for vlen in $VLENS; do
            if [[ -n "${_kp[$vlen]:-}" || -n "${_enc[$vlen]:-}" || -n "${_dec[$vlen]:-}" ]]; then
                local total=$(( ${_kp[$vlen]:-0} + ${_enc[$vlen]:-0} + ${_dec[$vlen]:-0} ))
                echo "total,${vlen},${total}" >> "$csv"
            fi
        done

        unset _kp _enc _dec

        # Archive management: same pattern as bench_refresh_tracked_csv.
        bench_refresh_tracked_csv "$csv" "$csv_latest" || true
        echo ""
    done
}

# ── Main loop ────────────────────────────────────────────────────────────────
for v in "${VARIANTS[@]}"; do
    run_vlen_sweep_single_variant "$v"
done
