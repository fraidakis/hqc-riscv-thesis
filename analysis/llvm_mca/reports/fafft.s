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
	call	karatsuba_clmul
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
	.p2align	1                               # -- Begin function karatsuba_clmul
	.type	karatsuba_clmul,@function
karatsuba_clmul:                        # @karatsuba_clmul
# %bb.0:
	addi	sp, sp, -160
	sd	ra, 152(sp)                     # 8-byte Folded Spill
	sd	s0, 144(sp)                     # 8-byte Folded Spill
	sd	s1, 136(sp)                     # 8-byte Folded Spill
	sd	s2, 128(sp)                     # 8-byte Folded Spill
	sd	s3, 120(sp)                     # 8-byte Folded Spill
	sd	s4, 112(sp)                     # 8-byte Folded Spill
	sd	s5, 104(sp)                     # 8-byte Folded Spill
	sd	s6, 96(sp)                      # 8-byte Folded Spill
	sd	s7, 88(sp)                      # 8-byte Folded Spill
	sd	s8, 80(sp)                      # 8-byte Folded Spill
	sd	s9, 72(sp)                      # 8-byte Folded Spill
	sd	s10, 64(sp)                     # 8-byte Folded Spill
	sd	s11, 56(sp)                     # 8-byte Folded Spill
	mv	s5, a3
	mv	s9, a2
	mv	s7, a1
	li	a1, 16
	mv	s4, a0
	bltu	a1, a3, .LBB1_6
# %bb.1:
	slli	a2, s5, 4
	mv	a0, s4
	li	a1, 0
	call	memset
	beqz	s5, .LBB1_39
# %bb.2:
	li	a0, 0
	slli	a6, s5, 3
	mv	s1, s4
.LBB1_3:                                # =>This Loop Header: Depth=1
                                        #     Child Loop BB1_4 Depth 2
	slli	a2, a0, 3
	add	a3, s7, a2
	ld	a3, 0(a3)
	add	a2, a2, a6
	add	a4, s4, a2
	mv	a5, s9
	mv	a2, s1
.LBB1_4:                                #   Parent Loop BB1_3 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	ld	s0, 0(a5)
	#APP
	clmul	a1, a3, s0
	clmulh	s0, a3, s0

	#NO_APP
	vsetivli	zero, 2, e64, m1, ta, ma
	vle64.v	v8, (a2)
	vmv.v.x	v9, a1
	vslide1down.vx	v9, v9, s0
	vxor.vv	v8, v8, v9
	vse64.v	v8, (a2)
	addi	a2, a2, 8
	addi	a5, a5, 8
	bne	a2, a4, .LBB1_4
# %bb.5:                                #   in Loop: Header=BB1_3 Depth=1
	addi	a0, a0, 1
	addi	s1, s1, 8
	bne	a0, s5, .LBB1_3
	j	.LBB1_39
.LBB1_6:
	srli	s8, s5, 1
	slli	s3, s5, 4
	slli	s10, s5, 3
	slli	s1, s5, 6
	sub	s0, s5, s8
	add	s2, a4, s3
	add	a0, s2, s3
	sd	a0, 8(sp)                       # 8-byte Folded Spill
	add	a0, a0, s3
	sd	a0, 24(sp)                      # 8-byte Folded Spill
	add	a0, a0, s10
	sd	a0, 16(sp)                      # 8-byte Folded Spill
	sd	s1, 0(sp)                       # 8-byte Folded Spill
	add	s1, s1, a4
	sd	a4, 48(sp)                      # 8-byte Folded Spill
	mv	a0, a4
	mv	a1, s7
	mv	a2, s9
	mv	a3, s8
	mv	a4, s1
	call	karatsuba_clmul
	slli	a2, s8, 3
	add	a1, s7, a2
	sd	a2, 32(sp)                      # 8-byte Folded Spill
	add	a2, a2, s9
	mv	a0, s2
	mv	a3, s0
	mv	s6, s1
	mv	a4, s1
	call	karatsuba_clmul
	li	a0, 1
	csrr	a2, vlenb
	srli	s11, a2, 2
	li	a1, 8
	maxu	a1, s11, a1
	maxu	a6, s0, a0
	sd	a2, 40(sp)                      # 8-byte Folded Spill
	mv	t1, s0
	bgeu	s0, a1, .LBB1_8
# %bb.7:
	li	t4, 0
	mv	a4, s6
	ld	a0, 8(sp)                       # 8-byte Folded Reload
	ld	a3, 32(sp)                      # 8-byte Folded Reload
	ld	a1, 24(sp)                      # 8-byte Folded Reload
	ld	a2, 16(sp)                      # 8-byte Folded Reload
	mv	t5, s10
	mv	s10, a3
	j	.LBB1_16
.LBB1_8:
	slli	a7, a2, 1
	mv	a4, s6
	ld	a0, 8(sp)                       # 8-byte Folded Reload
	mv	t5, s10
	bgeu	s10, a7, .LBB1_10
# %bb.9:
	li	t4, 0
	ld	s10, 32(sp)                     # 8-byte Folded Reload
	ld	a1, 24(sp)                      # 8-byte Folded Reload
	ld	a2, 16(sp)                      # 8-byte Folded Reload
	j	.LBB1_16
.LBB1_10:
	neg	a1, s11
	vsetvli	a2, zero, e64, m2, ta, ma
	vid.v	v10
	and	t4, a6, a1
	vmv.v.x	v12, s8
	vmv.v.i	v14, 0
	mv	a3, s7
	mv	t0, s8
	mv	t2, s9
	ld	a1, 24(sp)                      # 8-byte Folded Reload
	mv	t3, a1
	ld	a2, 16(sp)                      # 8-byte Folded Reload
	mv	s1, a2
	mv	s0, t4
	ld	s10, 32(sp)                     # 8-byte Folded Reload
.LBB1_11:                               # =>This Inner Loop Header: Depth=1
	vsetvli	zero, zero, e64, m2, ta, ma
	vmsltu.vx	v8, v10, s8
	vmsleu.vv	v0, v12, v10
	add	a5, a3, s10
	vmv2r.v	v16, v14
	vle64.v	v18, (a5), v0.t
	vmv1r.v	v0, v8
	vle64.v	v20, (a3), v0.t
	vle64.v	v22, (a5), v0.t
	slli	a5, t0, 3
	vadd.vx	v10, v10, s11
	sub	s0, s0, s11
	add	a5, a5, s9
	vsetvli	zero, zero, e64, m2, ta, mu
	vle64.v	v16, (t2), v0.t
	vxor.vv	v18, v22, v20, v0.t
	vl2re64.v	v8, (a5)
	add	t0, t0, s11
	add	t2, t2, a7
	vs2r.v	v18, (t3)
	vxor.vv	v8, v8, v16
	add	t3, t3, a7
	vs2r.v	v8, (s1)
	add	s1, s1, a7
	add	a3, a3, a7
	bnez	s0, .LBB1_11
# %bb.12:
	bne	a6, t4, .LBB1_16
.LBB1_13:
	mv	s6, t1
	mv	a3, t1
	call	karatsuba_clmul
	mv	a0, s4
	li	a1, 0
	mv	a2, s3
	call	memset
	andi	a0, s5, -2
	ld	a6, 48(sp)                      # 8-byte Folded Reload
	ld	s0, 40(sp)                      # 8-byte Folded Reload
	beqz	a0, .LBB1_26
# %bb.14:
	bgeu	a0, s11, .LBB1_21
# %bb.15:
	li	a1, 0
	j	.LBB1_24
.LBB1_16:
	li	s1, 0
	ld	a5, 0(sp)                       # 8-byte Folded Reload
	sub	a7, a5, s3
	slli	a3, t4, 3
	sub	s0, a5, t5
	add	a5, t4, s8
	add	t2, a7, a3
	add	s0, s0, a3
	slli	a5, a5, 3
	add	a7, s7, a3
	add	t0, s9, a3
	add	s9, s9, a5
	add	s7, s7, a5
	ld	a5, 48(sp)                      # 8-byte Folded Reload
	add	t2, t2, a5
	add	t3, a5, s0
	j	.LBB1_19
.LBB1_17:                               #   in Loop: Header=BB1_19 Depth=1
	add	a5, a7, s1
	ld	a3, 0(a3)
	ld	s0, 0(a5)
	add	a5, t0, s1
	ld	t5, 0(a5)
	xor	s0, s0, a3
.LBB1_18:                               #   in Loop: Header=BB1_19 Depth=1
	add	a5, t2, s1
	add	a3, s9, s1
	ld	a3, 0(a3)
	sd	s0, 0(a5)
	add	a5, t3, s1
	addi	t4, t4, 1
	xor	a3, a3, t5
	sd	a3, 0(a5)
	addi	s1, s1, 8
	beq	a6, t4, .LBB1_13
.LBB1_19:                               # =>This Inner Loop Header: Depth=1
	add	a3, s7, s1
	bltu	t4, s8, .LBB1_17
# %bb.20:                               #   in Loop: Header=BB1_19 Depth=1
	ld	s0, 0(a3)
	li	t5, 0
	j	.LBB1_18
.LBB1_21:
	neg	a1, s11
	and	a1, s5, a1
	sub	a2, a1, s11
	divu	a3, a2, s11
	srli	a4, s0, 3
	slli	a2, a4, 4
	slli	a3, a3, 4
	addi	a3, a3, 16
	mul	a3, a3, a4
	add	a3, a3, s4
	mv	a4, a6
	mv	a5, s4
	vsetvli	s1, zero, e64, m2, ta, ma
.LBB1_22:                               # =>This Inner Loop Header: Depth=1
	vl2re64.v	v8, (a4)
	vl2re64.v	v10, (a5)
	vxor.vv	v8, v10, v8
	vs2r.v	v8, (a5)
	add	a5, a5, a2
	add	a4, a4, a2
	bne	a5, a3, .LBB1_22
# %bb.23:
	beq	a0, a1, .LBB1_26
.LBB1_24:
	slli	a2, a1, 3
	slli	a3, s8, 4
	add	a1, s4, a2
	add	a2, a2, a6
	add	a3, a3, s4
.LBB1_25:                               # =>This Inner Loop Header: Depth=1
	ld	a4, 0(a2)
	ld	a5, 0(a1)
	xor	a4, a4, a5
	sd	a4, 0(a1)
	addi	a1, a1, 8
	addi	a2, a2, 8
	bne	a1, a3, .LBB1_25
.LBB1_26:
	slli	s6, s6, 1
	beqz	s6, .LBB1_39
# %bb.27:
	srli	a2, s0, 3
	slli	s5, s5, 5
	bgeu	s6, s11, .LBB1_29
# %bb.28:
	li	a1, 0
	j	.LBB1_32
.LBB1_29:
	slli	a3, a0, 3
	neg	a1, s11
	add	a3, a3, s4
	and	a1, s6, a1
	slli	a4, a2, 4
	mv	a5, a1
	vsetvli	s1, zero, e64, m2, ta, ma
.LBB1_30:                               # =>This Inner Loop Header: Depth=1
	vl2re64.v	v8, (s2)
	vl2re64.v	v10, (a3)
	sub	a5, a5, s11
	vxor.vv	v8, v10, v8
	vs2r.v	v8, (a3)
	add	a3, a3, a4
	add	s2, s2, a4
	bnez	a5, .LBB1_30
# %bb.31:
	beq	s6, a1, .LBB1_34
.LBB1_32:
	slli	a1, a1, 3
	slli	s8, s8, 4
	add	a3, s3, a1
	add	a4, s8, a1
	sub	a5, s5, s8
	add	a1, a6, a3
	add	a3, s4, a4
	add	a4, a6, a5
.LBB1_33:                               # =>This Inner Loop Header: Depth=1
	ld	a5, 0(a1)
	ld	s1, 0(a3)
	addi	a1, a1, 8
	xor	a5, a5, s1
	sd	a5, 0(a3)
	addi	a3, a3, 8
	bne	a1, a4, .LBB1_33
.LBB1_34:
	bgeu	s6, s11, .LBB1_36
# %bb.35:
	li	a1, 0
	j	.LBB1_40
.LBB1_36:
	neg	a1, s11
	and	a1, s6, a1
	sub	a3, a1, s11
	divu	a5, a3, s11
	add	a3, s4, s10
	vsetvli	a4, zero, e64, m2, ta, ma
	vid.v	v8
	slli	s0, a2, 4
	slli	a5, a5, 4
	addi	a5, a5, 16
	mul	a2, a5, a2
	add	a2, a2, a6
	vmv.v.i	v10, 0
	mv	a4, a6
.LBB1_37:                               # =>This Inner Loop Header: Depth=1
	vsetvli	zero, zero, e64, m2, ta, mu
	vmsltu.vx	v0, v8, a0
	vmv2r.v	v12, v10
	add	s1, a4, s5
	vle64.v	v12, (a4), v0.t
	vl2re64.v	v14, (s1)
	add	a5, a4, s3
	vl2re64.v	v16, (a5)
	vxor.vv	v12, v14, v12
	vl2re64.v	v14, (a3)
	vxor.vv	v12, v12, v16
	vadd.vx	v8, v8, s11
	add	a4, a4, s0
	vxor.vv	v12, v12, v14
	vs2r.v	v12, (a3)
	add	a3, a3, s0
	bne	a4, a2, .LBB1_37
# %bb.38:
	bne	s6, a1, .LBB1_40
.LBB1_39:
	ld	ra, 152(sp)                     # 8-byte Folded Reload
	ld	s0, 144(sp)                     # 8-byte Folded Reload
	ld	s1, 136(sp)                     # 8-byte Folded Reload
	ld	s2, 128(sp)                     # 8-byte Folded Reload
	ld	s3, 120(sp)                     # 8-byte Folded Reload
	ld	s4, 112(sp)                     # 8-byte Folded Reload
	ld	s5, 104(sp)                     # 8-byte Folded Reload
	ld	s6, 96(sp)                      # 8-byte Folded Reload
	ld	s7, 88(sp)                      # 8-byte Folded Reload
	ld	s8, 80(sp)                      # 8-byte Folded Reload
	ld	s9, 72(sp)                      # 8-byte Folded Reload
	ld	s10, 64(sp)                     # 8-byte Folded Reload
	ld	s11, 56(sp)                     # 8-byte Folded Reload
	addi	sp, sp, 160
	ret
.LBB1_40:
	slli	a2, a1, 3
	add	a3, s3, a2
	add	a4, s5, a2
	add	s10, s10, a2
	add	a2, a2, a6
	add	a3, a3, a6
	add	a4, a4, a6
	add	s4, s4, s10
	j	.LBB1_43
.LBB1_41:                               #   in Loop: Header=BB1_43 Depth=1
	ld	a5, 0(a2)
.LBB1_42:                               #   in Loop: Header=BB1_43 Depth=1
	ld	s1, 0(a4)
	ld	s0, 0(a3)
	xor	a5, a5, s1
	ld	s1, 0(s4)
	addi	a1, a1, 1
	addi	a3, a3, 8
	addi	a4, a4, 8
	addi	a2, a2, 8
	xor	s0, s0, s1
	xor	a5, a5, s0
	sd	a5, 0(s4)
	addi	s4, s4, 8
	beq	s6, a1, .LBB1_39
.LBB1_43:                               # =>This Inner Loop Header: Depth=1
	bltu	a1, a0, .LBB1_41
# %bb.44:                               #   in Loop: Header=BB1_43 Depth=1
	li	a5, 0
	j	.LBB1_42
.Lfunc_end1:
	.size	karatsuba_clmul, .Lfunc_end1-karatsuba_clmul
                                        # -- End function
	.ident	"clang version 21.1.1 (https://github.com/llvm/llvm-project.git 5a86dc996c26299de63effc927075dcbfb924167)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
