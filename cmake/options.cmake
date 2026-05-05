# cmake/options.cmake
# Three orthogonal CMake cache variables that independently control which
# implementation is compiled for each of the three optimizable HQC components.
#
# Usage:
#   cmake -B build -DHQC_KECCAK_IMPL=rvv -DHQC_DECODE_IMPL=ref  # Phase 2 config
#   cmake -B build -DHQC_KECCAK_IMPL=rvv -DHQC_DECODE_IMPL=rvv  # Full-stack
#
# All five POLYMUL_VARIANTS are always built regardless of these flags.
# The flags control which Keccak and Decoder sources are linked into every target.

# ── Phase 2 flag: Keccak / SHAKE implementation ───────────────────────────────
# ref : vendor scalar fips202 (default — no dependency on RVV Keccak)
# rvv : Zhang et al. RVV Keccak-f[1600] + HQC-specific SHAKE-256 wrapper
#       with 4-way multi-instance interleaving for seed_expand()
set(HQC_KECCAK_IMPL "ref" CACHE STRING
    "Keccak/SHAKE backend: ref (vendor fips202.c) | rvv (RVV Keccak, Phase 2)")
set_property(CACHE HQC_KECCAK_IMPL PROPERTY STRINGS "ref" "rvv")

# ── Phase 3 flag: RM/RS concatenated decoder ─────────────────────────────────
# ref : vendor scalar reed_muller.c + reed_solomon.c (default)
# rvv : RVV-vectorised 128-pt FHT + vluxei8.v RS syndrome computation (Phase 3)
set(HQC_DECODE_IMPL "ref" CACHE STRING
    "RM/RS decoder backend: ref (vendor scalar) | rvv (RVV vectorised, Phase 3)")
set_property(CACHE HQC_DECODE_IMPL PROPERTY STRINGS "ref" "rvv")

# ── Diagnostic messages ───────────────────────────────────────────────────────
message(STATUS "HQC configuration:")
message(STATUS "  POLYMUL  : all five variants (karatsuba_ref / karatsuba_zbc / fafft_zbc / hybrid_zbc / fafft_zvbc)")
message(STATUS "  KECCAK   : ${HQC_KECCAK_IMPL}")
message(STATUS "  DECODE   : ${HQC_DECODE_IMPL}")
if(HQC_KECCAK_IMPL STREQUAL "ref" AND HQC_DECODE_IMPL STREQUAL "ref")
    message(STATUS "  → Phase 1 build: polynomial multiplication head-to-head comparison")
elseif(HQC_KECCAK_IMPL STREQUAL "rvv" AND HQC_DECODE_IMPL STREQUAL "ref")
    message(STATUS "  → Phase 2 build: + RVV Keccak")
elseif(HQC_KECCAK_IMPL STREQUAL "rvv" AND HQC_DECODE_IMPL STREQUAL "rvv")
    message(STATUS "  → Full-stack build: poly-mul + RVV Keccak + RVV decoder")
endif()

# ── Multi-VLEN build matrix (P-02) ───────────────────────────────────────────
# VLEN_LIST: semicolon-separated VLEN values for which vector-variant bench
# targets are generated.  Each entry produces a distinct ELF compiled with
# -mvscale-min=N -mvscale-max=N (N = VLEN/64) so the compiler can use the
# guaranteed register width for constant-folding and vectorization decisions.
# Non-vector variants are always built once without a VLEN suffix.
#
# Example:  cmake -B build -DVLEN_LIST="128;256;512"
set(VLEN_LIST "256" CACHE STRING
    "VLEN values for vector-variant build matrix (semicolon-separated, e.g. 128;256;512)")
message(STATUS "  VLEN matrix: ${VLEN_LIST}")

# Variants that participate in the per-VLEN matrix.  These are the polymul
# strategies that contain RVV code whose performance is VLEN-sensitive.
set(VECTOR_POLYMUL_VARIANTS fafft_zbc fafft_zvbc karatsuba_zvbc)

# ── Validation ────────────────────────────────────────────────────────────────
if(NOT HQC_KECCAK_IMPL STREQUAL "ref" AND NOT HQC_KECCAK_IMPL STREQUAL "rvv")
    message(FATAL_ERROR "HQC_KECCAK_IMPL must be 'ref' or 'rvv', got '${HQC_KECCAK_IMPL}'")
endif()
if(NOT HQC_DECODE_IMPL STREQUAL "ref" AND NOT HQC_DECODE_IMPL STREQUAL "rvv")
    message(FATAL_ERROR "HQC_DECODE_IMPL must be 'ref' or 'rvv', got '${HQC_DECODE_IMPL}'")
endif()
