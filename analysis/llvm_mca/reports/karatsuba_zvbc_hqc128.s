	.attribute	4, 16
	.attribute	5, "rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0_v1p0_zicsr2p0_zifencei2p0_zmmul1p0_zaamo1p0_zalrsc1p0_zca1p0_zcd1p0_zbb1p0_zbc1p0_zbkb1p0_zvbc1p0_zve32f1p0_zve32x1p0_zve64d1p0_zve64f1p0_zve64x1p0_zvl128b1p0_zvl32b1p0_zvl64b1p0"
	.file	"gf2x_vclmul.c"
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
	lui	a3, 10
	addi	a3, a3, -1536
	sub	sp, sp, a3
	mv	s0, a0
	lui	a0, 9
	addi	a0, a0, -1400
	add	a0, a0, sp
	li	a3, 277
	addi	a4, sp, 8
	lui	a5, 9
	addi	a5, a5, -1400
	add	s1, sp, a5
	call	karatsuba_vclmul
	csrr	a1, vlenb
	srli	a2, a1, 3
	li	a0, 138
	bgeu	a0, a2, .LBB0_2
# %bb.1:
	li	a6, 0
	j	.LBB0_4
.LBB0_2:
	srli	a3, a1, 2
	negw	a0, a3
	andi	a6, a0, 276
	sub	a4, a6, a3
	divu	a3, a4, a3
	slli	a1, a1, 1
	slli	a3, a3, 4
	addi	a3, a3, 16
	mul	a2, a3, a2
	add	a2, a2, s0
	li	a3, 59
	mv	a4, s0
	vsetvli	a5, zero, e64, m2, ta, ma
.LBB0_3:                                # =>This Inner Loop Header: Depth=1
	addi	a5, s1, 2047
	addi	a0, a5, 161
	addi	a5, a5, 169
	vl2re64.v	v8, (a0)
	vl2re64.v	v10, (a5)
	vl2re64.v	v12, (s1)
	vsrl.vi	v8, v8, 5
	vsll.vx	v10, v10, a3
	vxor.vv	v8, v12, v8
	vxor.vv	v8, v8, v10
	vs2r.v	v8, (a4)
	add	a4, a4, a1
	add	s1, s1, a1
	bne	a4, a2, .LBB0_3
.LBB0_4:
	slli	a1, a6, 3
	lui	a0, 9
	addi	a0, a0, -1400
	add	a2, sp, a0
	addi	a0, s0, 2047
	add	s0, s0, a1
	add	a1, a1, a2
	addi	a2, a0, 169
.LBB0_5:                                # =>This Inner Loop Header: Depth=1
	addi	a3, a1, 2047
	ld	a4, 161(a3)
	ld	a3, 169(a3)
	ld	a5, 0(a1)
	srli	a4, a4, 5
	slli	a3, a3, 59
	xor	a4, a4, a5
	xor	a3, a3, a4
	sd	a3, 0(s0)
	addi	s0, s0, 8
	addi	a1, a1, 8
	bne	s0, a2, .LBB0_5
# %bb.6:
	ld	a1, 161(a0)
	andi	a1, a1, 31
	sd	a1, 161(a0)
	lui	a0, 10
	addi	a0, a0, -1536
	add	sp, sp, a0
	ld	ra, 488(sp)                     # 8-byte Folded Reload
	ld	s0, 480(sp)                     # 8-byte Folded Reload
	ld	s1, 472(sp)                     # 8-byte Folded Reload
	addi	sp, sp, 496
	ret
.Lfunc_end0:
	.size	vect_mul, .Lfunc_end0-vect_mul
                                        # -- End function
	.p2align	1                               # -- Begin function karatsuba_vclmul
	.type	karatsuba_vclmul,@function
karatsuba_vclmul:                       # @karatsuba_vclmul
# %bb.0:
	addi	sp, sp, -144
	sd	ra, 136(sp)                     # 8-byte Folded Spill
	sd	s0, 128(sp)                     # 8-byte Folded Spill
	sd	s1, 120(sp)                     # 8-byte Folded Spill
	sd	s2, 112(sp)                     # 8-byte Folded Spill
	sd	s3, 104(sp)                     # 8-byte Folded Spill
	sd	s4, 96(sp)                      # 8-byte Folded Spill
	sd	s5, 88(sp)                      # 8-byte Folded Spill
	sd	s6, 80(sp)                      # 8-byte Folded Spill
	sd	s7, 72(sp)                      # 8-byte Folded Spill
	sd	s8, 64(sp)                      # 8-byte Folded Spill
	sd	s9, 56(sp)                      # 8-byte Folded Spill
	sd	s10, 48(sp)                     # 8-byte Folded Spill
	sd	s11, 40(sp)                     # 8-byte Folded Spill
	mv	s8, a3
	mv	s7, a2
	mv	s3, a1
	li	a1, 16
	mv	s2, a0
	bltu	a1, a3, .LBB1_10
# %bb.1:
	slli	a2, s8, 4
	mv	a0, s2
	li	a1, 0
	call	memset
	li	a1, 4
	li	a0, 0
	bltu	s8, a1, .LBB1_6
# %bb.2:
	li	a6, 0
.LBB1_3:                                # =>This Loop Header: Depth=1
                                        #     Child Loop BB1_4 Depth 2
	li	a4, 0
	slli	a0, a6, 3
	add	a7, s3, a0
	addi	s0, a0, 8
	addi	s1, a0, 16
	addi	a1, a0, 24
	add	a3, s3, s0
	add	a5, s3, s1
	add	a2, s3, a1
	ld	a7, 0(a7)
	ld	t0, 0(a3)
	ld	t1, 0(a5)
	ld	t2, 0(a2)
	add	a0, a0, s2
	add	s0, s0, s2
	add	s1, s1, s2
	add	a2, s2, a1
.LBB1_4:                                #   Parent Loop BB1_3 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	sub	a1, s8, a4
	slli	a3, a4, 3
	vsetvli	a5, a1, e64, m4, ta, ma
	add	a1, s7, a3
	vle64.v	v8, (a1)
	add	a1, a0, a3
	vle64.v	v12, (a1)
	vmv.v.x	v16, a7
	vclmul.vv	v20, v16, v8
	vxor.vv	v12, v12, v20
	vse64.v	v12, (a1)
	addi	a1, a1, 8
	vle64.v	v12, (a1)
	vclmulh.vv	v16, v16, v8
	vxor.vv	v12, v12, v16
	vse64.v	v12, (a1)
	add	a1, s0, a3
	vle64.v	v12, (a1)
	vmv.v.x	v16, t0
	vclmul.vv	v20, v16, v8
	vxor.vv	v12, v12, v20
	vse64.v	v12, (a1)
	addi	a1, a1, 8
	vle64.v	v12, (a1)
	vclmulh.vv	v16, v16, v8
	vxor.vv	v12, v12, v16
	vse64.v	v12, (a1)
	add	a1, s1, a3
	vle64.v	v12, (a1)
	vmv.v.x	v16, t1
	vclmul.vv	v20, v16, v8
	vxor.vv	v12, v12, v20
	vse64.v	v12, (a1)
	addi	a1, a1, 8
	vle64.v	v12, (a1)
	add	a3, a3, a2
	vclmulh.vv	v16, v16, v8
	vxor.vv	v12, v12, v16
	vse64.v	v12, (a1)
	vle64.v	v12, (a3)
	vmv.v.x	v16, t2
	vclmul.vv	v20, v16, v8
	vxor.vv	v12, v12, v20
	addi	a1, a3, 8
	vse64.v	v12, (a3)
	vle64.v	v12, (a1)
	vclmulh.vv	v8, v16, v8
	vxor.vv	v8, v12, v8
	add	a4, a4, a5
	vse64.v	v8, (a1)
	bltu	a4, s8, .LBB1_4
# %bb.5:                                #   in Loop: Header=BB1_3 Depth=1
	addi	a0, a6, 4
	addi	a1, a6, 7
	mv	a6, a0
	bltu	a1, s8, .LBB1_3
.LBB1_6:
	bgeu	a0, s8, .LBB1_29
.LBB1_7:                                # =>This Loop Header: Depth=1
                                        #     Child Loop BB1_8 Depth 2
	li	a1, 0
	slli	a3, a0, 3
	add	a2, s3, a3
	ld	a2, 0(a2)
	add	a3, a3, s2
.LBB1_8:                                #   Parent Loop BB1_7 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	sub	a4, s8, a1
	slli	a5, a1, 3
	vsetvli	a4, a4, e64, m4, ta, ma
	add	s1, s7, a5
	add	a5, a5, a3
	vle64.v	v8, (s1)
	vle64.v	v12, (a5)
	vmv.v.x	v16, a2
	vclmul.vv	v20, v16, v8
	vxor.vv	v12, v12, v20
	addi	s1, a5, 8
	vse64.v	v12, (a5)
	vle64.v	v12, (s1)
	vclmulh.vv	v8, v16, v8
	vxor.vv	v8, v12, v8
	add	a1, a1, a4
	vse64.v	v8, (s1)
	bltu	a1, s8, .LBB1_8
# %bb.9:                                #   in Loop: Header=BB1_7 Depth=1
	addi	a0, a0, 1
	bne	a0, s8, .LBB1_7
	j	.LBB1_29
.LBB1_10:
	mv	s6, a4
	srli	s9, s8, 1
	slli	a0, s8, 4
	slli	s10, s8, 3
	slli	s1, s8, 6
	sub	s0, s8, s9
	add	s4, a4, a0
	add	s5, s4, a0
	sd	a0, 24(sp)                      # 8-byte Folded Spill
	add	a0, a0, s5
	sd	a0, 8(sp)                       # 8-byte Folded Spill
	add	a0, a0, s10
	sd	a0, 16(sp)                      # 8-byte Folded Spill
	sd	s1, 0(sp)                       # 8-byte Folded Spill
	add	s1, s1, a4
	mv	a0, a4
	mv	a1, s3
	mv	a2, s7
	mv	a3, s9
	mv	a4, s1
	call	karatsuba_vclmul
	slli	s11, s9, 3
	add	a1, s3, s11
	add	a2, s7, s11
	mv	a0, s4
	mv	a3, s0
	sd	s1, 32(sp)                      # 8-byte Folded Spill
	mv	a4, s1
	call	karatsuba_vclmul
	li	a1, 1
	csrr	a0, vlenb
	srli	a7, a0, 2
	li	a2, 8
	maxu	a2, a7, a2
	maxu	a6, s0, a1
	mv	t1, s0
	bgeu	s0, a2, .LBB1_12
# %bb.11:
	li	a5, 0
	ld	a1, 8(sp)                       # 8-byte Folded Reload
	ld	a0, 24(sp)                      # 8-byte Folded Reload
	ld	a2, 16(sp)                      # 8-byte Folded Reload
	mv	t4, s10
	mv	s10, a0
	j	.LBB1_30
.LBB1_12:
	slli	t0, a0, 1
	mv	t4, s10
	bgeu	s10, t0, .LBB1_14
# %bb.13:
	li	a5, 0
	ld	a1, 8(sp)                       # 8-byte Folded Reload
	ld	s10, 24(sp)                     # 8-byte Folded Reload
	ld	a2, 16(sp)                      # 8-byte Folded Reload
	j	.LBB1_30
.LBB1_14:
	neg	a1, a7
	vsetvli	a2, zero, e64, m2, ta, ma
	vid.v	v10
	and	a5, a6, a1
	vmv.v.x	v12, s9
	vmv.v.i	v14, 0
	mv	a4, s3
	mv	t2, s9
	mv	t3, s7
	ld	a1, 8(sp)                       # 8-byte Folded Reload
	mv	s0, a1
	ld	a2, 16(sp)                      # 8-byte Folded Reload
	mv	s1, a2
	mv	a3, a5
.LBB1_15:                               # =>This Inner Loop Header: Depth=1
	vsetvli	zero, zero, e64, m2, ta, ma
	vmsltu.vx	v8, v10, s9
	vmsleu.vv	v0, v12, v10
	add	a0, a4, s11
	vmv2r.v	v16, v14
	vle64.v	v18, (a0), v0.t
	vmv1r.v	v0, v8
	vle64.v	v20, (a4), v0.t
	vle64.v	v22, (a0), v0.t
	slli	a0, t2, 3
	vadd.vx	v10, v10, a7
	sub	a3, a3, a7
	add	a0, a0, s7
	vsetvli	zero, zero, e64, m2, ta, mu
	vle64.v	v16, (t3), v0.t
	vxor.vv	v18, v22, v20, v0.t
	vl2re64.v	v8, (a0)
	add	t2, t2, a7
	add	t3, t3, t0
	vs2r.v	v18, (s0)
	vxor.vv	v8, v8, v16
	add	s0, s0, t0
	vs2r.v	v8, (s1)
	add	s1, s1, t0
	add	a4, a4, t0
	bnez	a3, .LBB1_15
# %bb.16:
	ld	s10, 24(sp)                     # 8-byte Folded Reload
	bne	a6, a5, .LBB1_30
.LBB1_17:
	mv	a0, s5
	mv	s7, t1
	mv	a3, t1
	ld	a4, 32(sp)                      # 8-byte Folded Reload
	call	karatsuba_vclmul
	mv	a0, s2
	li	a1, 0
	mv	a2, s10
	call	memset
	andi	s1, s8, -2
	beqz	s1, .LBB1_20
# %bb.18:
	li	a0, 0
.LBB1_19:                               # =>This Inner Loop Header: Depth=1
	sub	a1, s1, a0
	slli	a2, a0, 3
	vsetvli	a1, a1, e64, m4, ta, ma
	add	a3, s2, a2
	add	a2, a2, s6
	vle64.v	v8, (a3)
	vle64.v	v12, (a2)
	vxor.vv	v8, v8, v12
	add	a0, a0, a1
	vse64.v	v8, (a3)
	bltu	a0, s1, .LBB1_19
.LBB1_20:
	slli	s7, s7, 1
	beqz	s7, .LBB1_23
# %bb.21:
	li	a0, 0
	slli	a1, s1, 3
	add	a1, a1, s2
.LBB1_22:                               # =>This Inner Loop Header: Depth=1
	sub	a2, s7, a0
	slli	a3, a0, 3
	vsetvli	a2, a2, e64, m4, ta, ma
	add	a4, a1, a3
	add	a3, a3, s4
	vle64.v	v8, (a4)
	vle64.v	v12, (a3)
	vxor.vv	v8, v8, v12
	add	a0, a0, a2
	vse64.v	v8, (a4)
	bltu	a0, s7, .LBB1_22
.LBB1_23:
	beqz	s1, .LBB1_26
# %bb.24:
	li	a0, 0
	add	a1, s2, s11
.LBB1_25:                               # =>This Inner Loop Header: Depth=1
	sub	a2, s1, a0
	slli	a3, a0, 3
	vsetvli	a2, a2, e64, m4, ta, ma
	add	a4, s5, a3
	vle64.v	v8, (a4)
	add	a4, s6, a3
	vle64.v	v12, (a4)
	add	a4, a1, a3
	vle64.v	v16, (a4)
	add	a3, a3, s4
	vle64.v	v20, (a3)
	vxor.vv	v8, v16, v8
	vxor.vv	v8, v8, v12
	vxor.vv	v8, v8, v20
	add	a0, a0, a2
	vse64.v	v8, (a4)
	bltu	a0, s1, .LBB1_25
.LBB1_26:
	bgeu	s1, s7, .LBB1_29
# %bb.27:
	add	s2, s2, s11
.LBB1_28:                               # =>This Inner Loop Header: Depth=1
	sub	a0, s7, s1
	slli	a1, s1, 3
	vsetvli	a0, a0, e64, m4, ta, ma
	add	a2, s5, a1
	vle64.v	v8, (a2)
	add	a2, s4, a1
	add	a1, a1, s2
	vle64.v	v12, (a1)
	vle64.v	v16, (a2)
	vxor.vv	v8, v12, v8
	vxor.vv	v8, v8, v16
	add	s1, s1, a0
	vse64.v	v8, (a1)
	bltu	s1, s7, .LBB1_28
.LBB1_29:
	ld	ra, 136(sp)                     # 8-byte Folded Reload
	ld	s0, 128(sp)                     # 8-byte Folded Reload
	ld	s1, 120(sp)                     # 8-byte Folded Reload
	ld	s2, 112(sp)                     # 8-byte Folded Reload
	ld	s3, 104(sp)                     # 8-byte Folded Reload
	ld	s4, 96(sp)                      # 8-byte Folded Reload
	ld	s5, 88(sp)                      # 8-byte Folded Reload
	ld	s6, 80(sp)                      # 8-byte Folded Reload
	ld	s7, 72(sp)                      # 8-byte Folded Reload
	ld	s8, 64(sp)                      # 8-byte Folded Reload
	ld	s9, 56(sp)                      # 8-byte Folded Reload
	ld	s10, 48(sp)                     # 8-byte Folded Reload
	ld	s11, 40(sp)                     # 8-byte Folded Reload
	addi	sp, sp, 144
	ret
.LBB1_30:
	li	s1, 0
	ld	a4, 0(sp)                       # 8-byte Folded Reload
	sub	a0, a4, s10
	slli	a3, a5, 3
	sub	a4, a4, t4
	add	s0, a5, s9
	add	a0, a0, a3
	add	a4, a4, a3
	slli	s0, s0, 3
	add	a7, s3, a3
	add	t0, s7, a3
	add	s7, s7, s0
	add	s3, s3, s0
	add	t2, s6, a0
	add	t3, s6, a4
	j	.LBB1_33
.LBB1_31:                               #   in Loop: Header=BB1_33 Depth=1
	add	s0, a7, s1
	ld	a3, 0(a0)
	ld	s0, 0(s0)
	add	a0, t0, s1
	ld	a0, 0(a0)
	xor	s0, s0, a3
.LBB1_32:                               #   in Loop: Header=BB1_33 Depth=1
	add	a4, t2, s1
	add	a3, s7, s1
	ld	a3, 0(a3)
	sd	s0, 0(a4)
	add	a4, t3, s1
	addi	a5, a5, 1
	xor	a3, a3, a0
	sd	a3, 0(a4)
	addi	s1, s1, 8
	beq	a6, a5, .LBB1_17
.LBB1_33:                               # =>This Inner Loop Header: Depth=1
	add	a0, s3, s1
	bltu	a5, s9, .LBB1_31
# %bb.34:                               #   in Loop: Header=BB1_33 Depth=1
	ld	s0, 0(a0)
	li	a0, 0
	j	.LBB1_32
.Lfunc_end1:
	.size	karatsuba_vclmul, .Lfunc_end1-karatsuba_vclmul
                                        # -- End function
	.ident	"clang version 21.1.1 (https://github.com/llvm/llvm-project.git 5a86dc996c26299de63effc927075dcbfb924167)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
