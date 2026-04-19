/**
 * include/immintrin.h  —  RISC-V cross-compilation stub
 *
 * The HQC reference implementation (src/ref/) includes <immintrin.h> in
 * several headers (hqc.h, vector.h, parsing.h) as a vestigial x86-only
 * include.  None of the functions in these headers actually use x86 SIMD
 * types, so no real declarations from immintrin.h are needed.
 *
 * When compiling for RISC-V (--target=riscv64-unknown-elf), Clang would
 * otherwise find the Clang-internal immintrin.h which immediately errors
 * with "#error This header is only meant to be used on x86 and x64".
 *
 * This empty stub is found first via the project's -I include path and
 * silently satisfies the include, allowing the ref code to compile as-is.
 */
#ifndef _IMMINTRIN_H_STUB
#define _IMMINTRIN_H_STUB
/* Intentionally empty — no x86 intrinsics on RISC-V targets. */
#endif /* _IMMINTRIN_H_STUB */
