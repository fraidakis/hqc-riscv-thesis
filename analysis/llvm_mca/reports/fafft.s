	.attribute	4, 16
	.attribute	5, "rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0_v1p0_zicsr2p0_zifencei2p0_zmmul1p0_zaamo1p0_zalrsc1p0_zca1p0_zcd1p0_zbb1p0_zbc1p0_zbkb1p0_zvbc1p0_zve32f1p0_zve32x1p0_zve64d1p0_zve64f1p0_zve64x1p0_zvl128b1p0_zvl32b1p0_zvl64b1p0"
	.file	"fafft.c"
	.text
	.globl	vect_mul                        # -- Begin function vect_mul
	.p2align	1
	.type	vect_mul,@function
vect_mul:                               # @vect_mul
# %bb.0:
	addi	sp, sp, -496
	sd	ra, 488(sp)                     # 8-byte Folded Spill
	sd	s0, 480(sp)                     # 8-byte Folded Spill
	sd	s1, 472(sp)                     # 8-byte Folded Spill
	sd	s2, 464(sp)                     # 8-byte Folded Spill
	lui	a3, 2
	addi	a3, a3, -448
	sub	sp, sp, a3
	mv	s0, a2
	mv	s1, a1
	mv	s2, a0
	addi	a0, sp, 16
	lui	a2, 2
	li	a1, 0
	call	memset
	addi	a0, sp, 16
	mv	a1, s1
	mv	a2, s0
	call	bmul2_4096_to_8192
	addi	a1, sp, 16
	mv	a0, s2
	call	reduce
	lui	a0, 2
	addi	a0, a0, -448
	add	sp, sp, a0
	ld	ra, 488(sp)                     # 8-byte Folded Reload
	ld	s0, 480(sp)                     # 8-byte Folded Reload
	ld	s1, 472(sp)                     # 8-byte Folded Reload
	ld	s2, 464(sp)                     # 8-byte Folded Reload
	addi	sp, sp, 496
	ret
.Lfunc_end0:
	.size	vect_mul, .Lfunc_end0-vect_mul
                                        # -- End function
	.ident	"clang version 21.1.1 (https://github.com/llvm/llvm-project.git 5a86dc996c26299de63effc927075dcbfb924167)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
