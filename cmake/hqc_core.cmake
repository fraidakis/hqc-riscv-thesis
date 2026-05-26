# cmake/hqc_core.cmake
# Defines hqc_add_core(LEVEL) — creates hqc_core_<LEVEL> OBJECT library.
#
# The OBJECT library contains only the invariant cryptographic skeleton:
# files that are NEVER replaced across any of the three thesis optimisation phases.
#
#   Phase 1 replaces: gf2x.c  (polymul strategy sources)
#   Phase 2 replaces: fips202.c / symmetric.c (keccak impl)
#   Phase 3 replaces: reed_muller.c / reed_solomon.c (decoder)
#
# Only kem.c (the FO transform) and the utility files below are truly invariant.
# Everything else is added as direct sources to each per-target add_executable().
#
# The OBJECT library is compiled ONCE per security level (3 compilations total)
# rather than once per (level × polymul) combination (up to 15 compilations).

function(hqc_add_core LEVEL)
    if(LEVEL EQUAL 128)
        set(VARIANT "hqc-1")
    elseif(LEVEL EQUAL 192)
        set(VARIANT "hqc-3")
    elseif(LEVEL EQUAL 256)
        set(VARIANT "hqc-5")
    else()
        message(FATAL_ERROR "hqc_add_core: unknown security level '${LEVEL}'")
    endif()

    set(TGT "hqc_core_${LEVEL}")
    if(TARGET ${TGT})
        return()   # already defined for this level in this configure run
    endif()

    set(HQC_REF "${CMAKE_SOURCE_DIR}/vendor/hqc-ref")

    add_library(${TGT} OBJECT
        # ── Truly invariant files — never touched by any thesis phase ─────────
        # KeyGen / Encaps / Decaps implementation with FO transform.
        # This file calls into hqc.c (PKE), kem.c is the outermost KEM wrapper.
        ${HQC_REF}/src/common/kem.c

        # Constant-time memset — security-critical, never modified.
        ${HQC_REF}/src/common/crypto_memset.c

        # Key/ciphertext serialization (parsing.c) — no algorithmic content.
        ${HQC_REF}/src/ref/parsing.c

        # NOTE: vector.c is intentionally NOT compiled here.
        # It is added as a per-target source in CMakeLists.txt so that
        # experiment variants (e.g. karatsuba_zbc_exp) can substitute an
        # optimised version without rebuilding hqc_core.
        # See the VECTOR_SOURCES variable in the target matrix loop.

        # Base field GF(2^8) arithmetic — Target 3 Track A promoted (2026-05-14).
        # Default (HQC_GF_IMPL=asm): src/common/gf.c — inline __asm__ volatile clmul.
        # Experiment (HQC_GF_IMPL=intrinsic): uses __riscv_clmul() from
        # <riscv_bitmanip.h> — see experiments/EXP-intrinsics-vs-asm/.
        $<IF:$<STREQUAL:${HQC_GF_IMPL},intrinsic>,
            ${CMAKE_SOURCE_DIR}/experiments/EXP-intrinsics-vs-asm/gf_intrinsic.c,
            ${CMAKE_SOURCE_DIR}/src/common/gf.c>

        # NOT included here — per-target source selection via HQC_*_IMPL flags:
        #   fips202.c / symmetric.c  ← HQC_KECCAK_IMPL  (Phase 2)
        #   gf2x.c (+ strategy file) ← polymul variant   (Phase 1)
        #   reed_muller.c            ← HQC_DECODE_IMPL   (Phase 3)
        #   reed_solomon.c           ← HQC_DECODE_IMPL   (Phase 3)
        #   code.c / fft.c           ← per-target always (call RM/RS by symbol)
        #   hqc.c                    ← per-target always (calls gf2x by symbol)
        #   vector.c                 ← per-target always (see VECTOR_SOURCES):
        #                               src/vector.c — promoted O(weight) scatter
        #                               (Target 2 Track A, 2026-05-14)
    )

    target_include_directories(${TGT} PUBLIC
        # Level-specific parameters.h MUST come first — it defines PARAM_N,
        # PARAM_OMEGA, etc.  All other headers are identical across levels.
        ${HQC_REF}/src/common/${VARIANT}
        ${HQC_REF}/src/common
        ${HQC_REF}/src/ref/${VARIANT}
        ${HQC_REF}/src/ref
        ${HQC_REF}/lib/fips202
        ${CMAKE_SOURCE_DIR}/include
    )

    target_compile_definitions(${TGT} PUBLIC
        SECURITY_LEVEL=${LEVEL}
        HQC_${LEVEL}
    )

    # Suppress upstream warnings that fire with -ffreestanding:
    #   -Wno-missing-prototypes : fips202.h not visible when compiling kem.c
    #   -Wno-pedantic           : upstream uses some non-standard extensions
    target_compile_options(${TGT} PRIVATE
        -Wno-missing-prototypes
        -Wno-pedantic
    )
endfunction()
