# cmake/options.cmake
# Three orthogonal CMake cache variables that independently control which
# implementation is compiled for each of the three optimizable HQC components.
#
# Usage:
#   cmake -B build -DHQC_KECCAK_IMPL=ref -DHQC_DECODE_IMPL=rvv  # Track B
#   cmake -B build -DHQC_KECCAK_IMPL=ref -DHQC_DECODE_IMPL=zvbc # Track C
#
# All five POLYMUL_VARIANTS are always built regardless of these flags.
# The flags control which Keccak and Decoder sources are linked into every target.

# ── Phase 2 flag: Keccak / SHAKE implementation ───────────────────────────────
# The vectorized Keccak backends (zvbb_x2_soa, rvv_soa) were experimentally
# evaluated and found slower than scalar+Zbb on VLEN=256 in-order cores.
# They are archived under experiments/dropped/.
#
# The only stable Keccak implementation is the vendor scalar fips202.
# The ref-vs-opt distinction is now handled purely via HQC_SCALAR_MARCH:
#   ref : compiled with whatever HQC_SCALAR_MARCH says (default includes Zbb)
#   opt : same as ref (Zbb idiom recognition is the "optimization")
# To ablate Zbb, set HQC_SCALAR_MARCH=rv64gc_zbc (strips Zbb/Zbs).
set(HQC_KECCAK_IMPL "ref" CACHE STRING
    "Keccak/SHAKE backend: ref (only stable option; vectorized paths archived)")
set_property(CACHE HQC_KECCAK_IMPL PROPERTY STRINGS "ref")

# ── Phase 3 flag: RM/RS concatenated decoder ─────────────────────────────────
# ref : vendor scalar reed_muller.c + reed_solomon.c (default)
# rvv : RVV-vectorised 128-pt FHT + RVV log/exp gather RS syndromes (reed_solomon_rvv.c)
# zvbc : RVV RM + promoted Zvbc RS path (reed_solomon_zvbc.c)
# rs_zvbc_exp : legacy alias of zvbc (kept for backward compatibility)
set(HQC_DECODE_IMPL "ref" CACHE STRING
    "RM/RS decoder backend: ref | rvv | zvbc | zvbc_BD | rm_rvv_E | rm_rvv_EH | rm_rvv_EHP")
set_property(CACHE HQC_DECODE_IMPL PROPERTY STRINGS "ref" "rvv" "zvbc" "zvbc_BD" "rm_rvv_E" "rm_rvv_EH" "rm_rvv_EHP")

# ── Scalar ISA override (ablation experiments) ───────────────────────────────
# Overrides the -march for scalar-only variants (scalar_ref, scalar_zb).
# Default is the fully optimized baseline including bit-manipulation extensions.
# Set to e.g. "rv64gc_zbc" to strip zbb and measure auto-lowering impact.
#
# Note: vector variants (vector_rvv, vector_zvk) have their own -march
# strings set below.  Zba (address-generation: sh1add/sh2add/sh3add) was promoted
# on 2026-05-25 after ablation showed −5.6% polymul improvement on vector_zvk
# and −2.6% on vector_rvv (see docs/thesis-helper/optimizations/zba_ablation.md).
set(HQC_SCALAR_MARCH "rv64gc_zbc_zbb_zbs" CACHE STRING
    "ISA march for scalar variants (ablation: set to rv64gc_zbc to strip zbb)")

# ── Vector variant ISA march (configurable for Zbb ablation) ─────────────────
# These default to the full optimized march.  Override to strip _zbb for Keccak
# ablation profiling (e.g. -DHQC_VECTOR_MARCH=rv64gcv_zbc_zba).
set(HQC_VECTOR_MARCH "rv64gcv_zbc_zbb_zba" CACHE STRING
    "ISA march for vector_rvv variants (strip _zbb for Keccak ablation)")
set(HQC_VECTOR_ZVK_MARCH "rv64gcv_zbc_zbb_zba_zvbc_zvbb" CACHE STRING
    "ISA march for vector_zvk variant (strip _zbb for Keccak ablation)")

# ── Keccak bench ISA override ────────────────────────────────────────────────
# Controls the -march for the keccak micro-benchmark target (bench_keccak_hqcN).
# Defaults to HQC_SCALAR_MARCH (includes Zbb → rori/andn idiom recognition).
# Set to "rv64gc_zbc" to strip Zbb for the keccak-ref baseline measurement.
#
# Usage from scripts:
#   --keccak opt → HQC_KECCAK_BENCH_MARCH = HQC_SCALAR_MARCH (with Zbb)
#   --keccak ref → HQC_KECCAK_BENCH_MARCH = rv64gc_zbc (without Zbb)
set(HQC_KECCAK_BENCH_MARCH "${HQC_SCALAR_MARCH}" CACHE STRING
    "ISA march for keccak bench target (ref: rv64gc_zbc, opt: inherit HQC_SCALAR_MARCH)")

# ── Decode bench ISA override ─────────────────────────────────────────────────
# Controls the -march for the decode micro-benchmark target (bench_decode_hqcN).
# Defaults to the full vector+crypto march (rv64gcv_zbc_zbb_zvbc_zvbb) which
# enables Zbc idiom recognition for gf_mul (clmul-based carry-less multiply).
# Set to "rv64gcv_zbb_zvbc_zvbb" to strip Zbc for the pure-software gf_mul
# baseline measurement (schoolbook gf_carryless_mul path).
#
# Usage from scripts:
#   --decode ref       → HQC_DECODE_BENCH_MARCH = rv64gcv_zbb_zvbc_zvbb (without Zbc)
#   --decode scalar_zb → HQC_DECODE_BENCH_MARCH = rv64gcv_zbc_zbb_zvbc_zvbb (with Zbc)
#   --decode rvv       → HQC_DECODE_IMPL=rvv (march unchanged, uses vector decode)
#   --decode zvbc      → HQC_DECODE_IMPL=zvbc (march unchanged, uses Zvbc decode)
set(HQC_DECODE_BENCH_MARCH "rv64gcv_zbc_zbb_zvbc_zvbb" CACHE STRING
    "ISA march for decode bench target (ref: strip _zbc, scalar_zb: keep _zbc)")

# ── Diagnostic messages ───────────────────────────────────────────────────────
message(STATUS "HQC configuration:")
message(STATUS "  POLYMUL  : matrix includes scalar_ref baseline and optimized variants")
message(STATUS "  KECCAK   : ${HQC_KECCAK_IMPL}")
message(STATUS "  DECODE   : ${HQC_DECODE_IMPL}")
if(HQC_KECCAK_IMPL STREQUAL "ref" AND HQC_DECODE_IMPL STREQUAL "ref")
    message(STATUS "  → Phase 1 build: polynomial multiplication head-to-head comparison")
elseif(HQC_KECCAK_IMPL STREQUAL "ref" AND HQC_DECODE_IMPL STREQUAL "rvv")
    message(STATUS "  → Track B build: RVV polymul/decoder with scalar reference Keccak")
elseif(HQC_KECCAK_IMPL STREQUAL "ref" AND (HQC_DECODE_IMPL STREQUAL "zvbc" OR HQC_DECODE_IMPL STREQUAL "rs_zvbc_exp"))
    message(STATUS "  → Track B+ build: RVV polymul with Zvbc RS decode backend")
endif()

# ── RVV vector-bits assumption ────────────────────────────────────────────────
# HQC_VECTOR_BITS controls the -mrvv-vector-bits=N Clang flag applied to all
# RVV-compiled sources (decode rvv/zvbc).
# Default is 256 (matches the reference hardware VLEN and the full-KEM build).
# The VLEN sweep sets this to the target VLEN per build dir so the same
# pairings (vector_rvv+ref+rvv, vector_zvk+ref+zvbc) produce correct
# results at every VLEN without sacrificing performance at VLEN=256.
set(HQC_VECTOR_BITS "256" CACHE STRING
    "RVV vector register width assumed by -mrvv-vector-bits= (default: 256)")
message(STATUS "  HQC_VECTOR_BITS: ${HQC_VECTOR_BITS}")

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
set(VECTOR_POLYMUL_VARIANTS vector_zvk vector_rvv)

# ── PGO for reference baselines (P-06) ────────────────────────────────────────
# These flags apply ONLY to scalar_ref targets (KAT + bench + polymul bench).
# They do NOT affect any other variant.
#
# HQC_REF_PGO    : off (default) | generate | use
#   generate : compile with -fprofile-generate / -fprofile-instr-generate to
#              emit raw profile data when the ELF is run on Spike or hardware.
#              Run the ELF, collect the profile, then rebuild with
#              HQC_REF_PGO=use.
#   use      : compile with -fprofile-use / -fprofile-instr-use pointing at
#              HQC_REF_PGO_FILE (path to the merged .profdata or profile dir).
#
# Note: LTO (-flto) was evaluated and found to provide negligible benefit
# (<0.001% instret improvement on Spike, +0.3% cycle regression on real HW).
# PGO alone provides ~4.8% cycle improvement on real hardware (BPI-F3,
# SpacemiT K1 X60) via improved branch prediction and code layout — without
# requiring LTO.  Accordingly, LTO is no longer applied.
#
# Example workflow (GCC on hardware):
#   cd tests/bench
#   make -f Makefile.hw LEVEL=128 POLYMUL=ref_scalar PGO=generate kem
#   sudo ./bench_hw_kem_hqc128_ref_scalar  # emits .gcda profile data
#   make -f Makefile.hw LEVEL=128 POLYMUL=ref_scalar PGO=use kem
#
# Example workflow (Clang on Spike):
#   cmake -B build/pgo -DHQC_REF_PGO=generate ...
#   cmake --build build/pgo --target bench_hqc128_scalar_ref
#   spike --isa=rv64gcv ... bench_hqc128_scalar_ref.elf   # emits default.profraw
#   llvm-profdata merge -output=hqc_ref.profdata default.profraw
#   cmake -B build/pgo -DHQC_REF_PGO=use -DHQC_REF_PGO_FILE=hqc_ref.profdata ...
#   cmake --build build/pgo --target bench_hqc128_scalar_ref

set(HQC_REF_PGO "off" CACHE STRING
    "PGO mode for scalar_ref: off | generate | use")
set_property(CACHE HQC_REF_PGO PROPERTY STRINGS "off" "generate" "use")

set(HQC_REF_PGO_FILE "" CACHE FILEPATH
    "Path to merged .profdata or profile directory (required when HQC_REF_PGO=use)")

# Validate PGO settings
if(HQC_REF_PGO STREQUAL "use" AND NOT HQC_REF_PGO_FILE)
    message(FATAL_ERROR
        "HQC_REF_PGO=use requires HQC_REF_PGO_FILE to point at a merged "
        ".profdata file.  Run with HQC_REF_PGO=generate first, then collect "
        "the profile and set HQC_REF_PGO_FILE accordingly.")
endif()
if(NOT HQC_REF_PGO STREQUAL "off" AND
   NOT HQC_REF_PGO STREQUAL "generate" AND
   NOT HQC_REF_PGO STREQUAL "use")
    message(FATAL_ERROR "HQC_REF_PGO must be 'off', 'generate', or 'use', got '${HQC_REF_PGO}'")
endif()

message(STATUS "  reference PGO : ${HQC_REF_PGO}")

# ── Validation ────────────────────────────────────────────────────────────────
if(NOT HQC_KECCAK_IMPL STREQUAL "ref")
    message(FATAL_ERROR
        "HQC_KECCAK_IMPL must be 'ref' (vectorized backends archived to experiments/dropped/). "
        "Got '${HQC_KECCAK_IMPL}'")
endif()

if(NOT HQC_DECODE_IMPL STREQUAL "ref"         AND
   NOT HQC_DECODE_IMPL STREQUAL "rvv"         AND
   NOT HQC_DECODE_IMPL STREQUAL "zvbc"        AND
   NOT HQC_DECODE_IMPL STREQUAL "zvbc_BD"     AND
   NOT HQC_DECODE_IMPL STREQUAL "rs_zvbc_exp" AND
   NOT HQC_DECODE_IMPL STREQUAL "rm_rvv_E"    AND
   NOT HQC_DECODE_IMPL STREQUAL "rm_rvv_EH"   AND
   NOT HQC_DECODE_IMPL STREQUAL "rm_rvv_EHP")
    message(FATAL_ERROR
    "HQC_DECODE_IMPL must be one of: ref | rvv | zvbc | rm_rvv_E | rm_rvv_EH | rm_rvv_EHP, "
        "got '${HQC_DECODE_IMPL}'")
endif()
