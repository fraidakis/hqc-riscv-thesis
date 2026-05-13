/* EXP-fafft-zvbc-v5/fafft_v5_crt_vclmul.c
 *
 * ZVBC CRT-bypass + vclmul RVV schoolbook — archived as loser from the FAFFT showdown.
 *
 * L128 polymul_instructions: 198,453  (showdown date: 2026-05-13)
 * Defeated by: EXP-fafft-zvbc-v4 vclmul schoolbook (193,177 instructions)
 *
 * Original location: src/poly_mul/vector/zvbc/fafft_zvbc/fafft_v5.c
 * Reason for retirement: The CRT split overhead and multi-block schoolbook
 * structure adds instructions vs the simpler unified vclmul.vv schoolbook
 * (v4).  v4 executes the full polynomial product without the CRT split,
 * saving ~5,276 instructions at L128.
 */
