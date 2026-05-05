	.attribute	4, 16
	.attribute	5, "rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0_v1p0_zicsr2p0_zifencei2p0_zmmul1p0_zaamo1p0_zalrsc1p0_zca1p0_zcd1p0_zbb1p0_zbc1p0_zbkb1p0_zvbc1p0_zve32f1p0_zve32x1p0_zve64d1p0_zve64f1p0_zve64x1p0_zvl128b1p0_zvl32b1p0_zvl64b1p0"
	.file	"gf2x.c"
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
	call	karatsuba_mul
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
	.p2align	1                               # -- Begin function karatsuba_mul
	.type	karatsuba_mul,@function
karatsuba_mul:                          # @karatsuba_mul
# %bb.0:
	addi	sp, sp, -192
	sd	ra, 184(sp)                     # 8-byte Folded Spill
	sd	s0, 176(sp)                     # 8-byte Folded Spill
	sd	s1, 168(sp)                     # 8-byte Folded Spill
	sd	s2, 160(sp)                     # 8-byte Folded Spill
	sd	s3, 152(sp)                     # 8-byte Folded Spill
	sd	s4, 144(sp)                     # 8-byte Folded Spill
	sd	s5, 136(sp)                     # 8-byte Folded Spill
	sd	s6, 128(sp)                     # 8-byte Folded Spill
	sd	s7, 120(sp)                     # 8-byte Folded Spill
	sd	s8, 112(sp)                     # 8-byte Folded Spill
	sd	s9, 104(sp)                     # 8-byte Folded Spill
	sd	s10, 96(sp)                     # 8-byte Folded Spill
	sd	s11, 88(sp)                     # 8-byte Folded Spill
	mv	s3, a3
	mv	s11, a2
	li	a3, 16
	slli	a2, s3, 4
	sd	a0, 80(sp)                      # 8-byte Folded Spill
	sd	a1, 72(sp)                      # 8-byte Folded Spill
	bltu	a3, s3, .LBB1_16
# %bb.1:
	li	a1, 0
	call	memset
	beqz	s3, .LBB1_69
# %bb.2:
	li	t4, 0
	csrr	ra, vlenb
	srli	a0, ra, 2
	neg	a1, a0
	and	t5, s3, a1
	add	a1, a1, t5
	divu	a1, a1, a0
	slli	t3, s3, 3
	ld	t6, 80(sp)                      # 8-byte Folded Reload
	addi	s2, t6, 8
	li	a2, 4
	add	a6, s11, t3
	add	a7, t6, t3
	addi	t0, t3, 8
	slli	ra, ra, 1
	maxu	a0, a0, a2
	sltu	t1, s3, a0
	mul	t2, ra, a1
	li	s5, 64
	j	.LBB1_4
.LBB1_3:                                #   in Loop: Header=BB1_4 Depth=1
	addi	t4, t4, 1
	addi	s2, s2, 8
	addi	t6, t6, 8
	beq	t4, s3, .LBB1_69
.LBB1_4:                                # =>This Loop Header: Depth=1
                                        #     Child Loop BB1_6 Depth 2
                                        #       Child Loop BB1_8 Depth 3
                                        #       Child Loop BB1_12 Depth 3
                                        #       Child Loop BB1_15 Depth 3
	li	s1, 0
	slli	a0, t4, 3
	ld	a4, 80(sp)                      # 8-byte Folded Reload
	add	s10, a4, t2
	add	s0, t3, a0
	add	a1, ra, a0
	add	s8, a4, a0
	add	a2, a7, a0
	add	s10, s10, a1
	sltu	a1, s8, a6
	sltu	a2, s11, a2
	and	a1, a1, a2
	add	a3, t0, a0
	ld	a2, 72(sp)                      # 8-byte Folded Reload
	add	a0, a0, a2
	add	s7, a4, s0
	ld	s9, 0(a0)
	add	a3, a3, a4
	or	a0, t1, a1
	andi	s4, a0, 1
	j	.LBB1_6
.LBB1_5:                                #   in Loop: Header=BB1_6 Depth=2
	addi	s1, s1, 1
	beq	s1, s5, .LBB1_3
.LBB1_6:                                #   Parent Loop BB1_4 Depth=1
                                        # =>  This Loop Header: Depth=2
                                        #       Child Loop BB1_8 Depth 3
                                        #       Child Loop BB1_12 Depth 3
                                        #       Child Loop BB1_15 Depth 3
	srl	a0, s9, s1
	slli	a0, a0, 63
	srai	a4, a0, 63
	beqz	s1, .LBB1_9
# %bb.7:                                #   in Loop: Header=BB1_6 Depth=2
	ld	a5, 0(s8)
	sub	a0, s5, s1
	mv	a2, s11
	mv	a1, s2
.LBB1_8:                                #   Parent Loop BB1_4 Depth=1
                                        #     Parent Loop BB1_6 Depth=2
                                        # =>    This Inner Loop Header: Depth=3
	ld	s6, 0(a2)
	sll	s0, s6, s1
	and	s0, s0, a4
	xor	a5, a5, s0
	sd	a5, -8(a1)
	ld	a5, 0(a2)
	ld	s0, 0(a1)
	srl	a5, a5, a0
	and	a5, a5, a4
	xor	a5, a5, s0
	sd	a5, 0(a1)
	addi	a1, a1, 8
	addi	a2, a2, 8
	bne	a1, a3, .LBB1_8
	j	.LBB1_5
.LBB1_9:                                #   in Loop: Header=BB1_6 Depth=2
	beqz	s4, .LBB1_11
# %bb.10:                               #   in Loop: Header=BB1_6 Depth=2
	li	a0, 0
	j	.LBB1_14
.LBB1_11:                               #   in Loop: Header=BB1_6 Depth=2
	mv	a0, s11
	mv	a1, t6
	vsetvli	a2, zero, e64, m2, ta, ma
.LBB1_12:                               #   Parent Loop BB1_4 Depth=1
                                        #     Parent Loop BB1_6 Depth=2
                                        # =>    This Inner Loop Header: Depth=3
	vl2re64.v	v8, (a0)
	vl2re64.v	v10, (a1)
	vand.vx	v8, v8, a4
	vxor.vv	v8, v10, v8
	vs2r.v	v8, (a1)
	add	a1, a1, ra
	add	a0, a0, ra
	bne	a1, s10, .LBB1_12
# %bb.13:                               #   in Loop: Header=BB1_6 Depth=2
	mv	a0, t5
	beq	s3, t5, .LBB1_5
.LBB1_14:                               #   in Loop: Header=BB1_6 Depth=2
	slli	a1, a0, 3
	add	a0, t6, a1
	add	a1, a1, s11
.LBB1_15:                               #   Parent Loop BB1_4 Depth=1
                                        #     Parent Loop BB1_6 Depth=2
                                        # =>    This Inner Loop Header: Depth=3
	ld	a2, 0(a1)
	ld	a5, 0(a0)
	and	a2, a2, a4
	xor	a2, a2, a5
	sd	a2, 0(a0)
	addi	a0, a0, 8
	addi	a1, a1, 8
	bne	a0, s7, .LBB1_15
	j	.LBB1_5
.LBB1_16:
	srli	s9, s3, 1
	add	s1, a4, a2
	slli	s8, s3, 3
	slli	a3, s3, 6
	sub	s7, s3, s9
	add	a0, s1, a2
	sd	a0, 48(sp)                      # 8-byte Folded Spill
	sd	a2, 56(sp)                      # 8-byte Folded Spill
	add	a0, a0, a2
	sd	a0, 32(sp)                      # 8-byte Folded Spill
	add	a0, a0, s8
	sd	a0, 24(sp)                      # 8-byte Folded Spill
	sd	a3, 64(sp)                      # 8-byte Folded Spill
	add	s0, a4, a3
	sd	a4, 40(sp)                      # 8-byte Folded Spill
	mv	a0, a4
	mv	a2, s11
	mv	a3, s9
	mv	a4, s0
	mv	s5, a1
	call	karatsuba_mul
	slli	s4, s9, 3
	add	s5, s5, s4
	add	s6, s11, s4
	mv	a0, s1
	mv	a1, s5
	mv	a2, s6
	mv	a3, s7
	mv	s2, s0
	mv	a4, s0
	call	karatsuba_mul
	li	a0, 1
	csrr	s0, vlenb
	maxu	a6, s7, a0
	srli	s10, s0, 2
	li	a0, 48
	maxu	a0, s10, a0
	srli	a1, s0, 3
	sd	a1, 16(sp)                      # 8-byte Folded Spill
	mv	a3, s7
	sd	s1, 8(sp)                       # 8-byte Folded Spill
	bgeu	s7, a0, .LBB1_23
# %bb.17:
	li	a5, 0
	ld	s5, 80(sp)                      # 8-byte Folded Reload
	ld	t0, 72(sp)                      # 8-byte Folded Reload
	ld	a0, 40(sp)                      # 8-byte Folded Reload
	ld	s7, 56(sp)                      # 8-byte Folded Reload
	mv	a4, s2
	mv	s2, a0
	ld	a1, 32(sp)                      # 8-byte Folded Reload
	ld	t3, 24(sp)                      # 8-byte Folded Reload
	mv	t2, s8
	mv	s8, a3
.LBB1_18:
	li	a2, 0
	ld	a3, 64(sp)                      # 8-byte Folded Reload
	sub	s1, a3, t2
	slli	a0, a5, 3
	sub	a3, a3, s7
	add	s0, a5, s9
	add	t2, s1, a0
	add	s1, a3, a0
	slli	s0, s0, 3
	add	a7, t0, a0
	add	t1, s11, a0
	add	s11, s11, s0
	add	s6, t0, s0
	add	t0, s2, t2
	add	s1, s1, s2
	j	.LBB1_21
.LBB1_19:                               #   in Loop: Header=BB1_21 Depth=1
	add	a0, a7, a2
	add	a3, t1, a2
	ld	a0, 0(a0)
	ld	s0, 0(a3)
.LBB1_20:                               #   in Loop: Header=BB1_21 Depth=1
	add	a3, s6, a2
	ld	a3, 0(a3)
	xor	a0, a0, a3
	add	a3, s1, a2
	sd	a0, 0(a3)
	add	a0, s11, a2
	ld	a0, 0(a0)
	add	a3, t0, a2
	addi	a5, a5, 1
	xor	a0, a0, s0
	sd	a0, 0(a3)
	addi	a2, a2, 8
	beq	a6, a5, .LBB1_35
.LBB1_21:                               # =>This Inner Loop Header: Depth=1
	bltu	a5, s9, .LBB1_19
# %bb.22:                               #   in Loop: Header=BB1_21 Depth=1
	li	a0, 0
	li	s0, 0
	j	.LBB1_20
.LBB1_23:
	slli	t1, s0, 1
	li	a5, 0
	mv	a4, s2
	ld	a1, 32(sp)                      # 8-byte Folded Reload
	ld	t3, 24(sp)                      # 8-byte Folded Reload
	mv	t2, s8
	bltu	s8, t1, .LBB1_71
# %bb.24:
	ld	a0, 64(sp)                      # 8-byte Folded Reload
	ld	a2, 56(sp)                      # 8-byte Folded Reload
	sub	a2, a0, a2
	ld	s2, 40(sp)                      # 8-byte Folded Reload
	add	a2, a2, s2
	ld	t0, 72(sp)                      # 8-byte Folded Reload
	sub	a0, a2, t0
	bltu	a0, t1, .LBB1_72
# %bb.25:
	sub	a0, a2, s11
	mv	s8, a3
	ld	s7, 56(sp)                      # 8-byte Folded Reload
	bltu	a0, t1, .LBB1_70
# %bb.26:
	sub	a0, a2, s5
	bltu	a0, t1, .LBB1_70
# %bb.27:
	sub	a0, s6, a2
	bltu	a0, t1, .LBB1_70
# %bb.28:
	ld	a0, 64(sp)                      # 8-byte Folded Reload
	sub	a2, a0, t2
	add	a3, a2, s2
	sub	a0, a3, t0
	bltu	a0, t1, .LBB1_70
# %bb.29:
	sub	a0, a3, s11
	bltu	a0, t1, .LBB1_70
# %bb.30:
	sub	a0, a3, s5
	bltu	a0, t1, .LBB1_70
# %bb.31:
	sub	a3, a3, s6
	ld	s5, 80(sp)                      # 8-byte Folded Reload
	bltu	a3, t1, .LBB1_18
# %bb.32:
	neg	a0, s10
	and	a5, a6, a0
	sub	a0, a5, s10
	divu	a0, a0, s10
	vsetvli	a3, zero, e64, m2, ta, ma
	vid.v	v8
	add	a2, a2, s2
	slli	a0, a0, 4
	addi	a0, a0, 16
	ld	a3, 16(sp)                      # 8-byte Folded Reload
	mul	a3, a0, a3
	add	a7, a2, a3
	vmv.v.i	v10, 0
	mv	a0, t0
	mv	a3, s11
	mv	a2, a1
	mv	s1, t3
.LBB1_33:                               # =>This Inner Loop Header: Depth=1
	vsetvli	zero, zero, e64, m2, ta, mu
	vmsltu.vx	v0, v8, s9
	vmv2r.v	v12, v10
	add	s0, a0, s4
	vle64.v	v12, (a0), v0.t
	vl2re64.v	v14, (s0)
	vmv2r.v	v16, v10
	vle64.v	v16, (a3), v0.t
	add	s0, a3, s4
	vxor.vv	v12, v14, v12
	vs2r.v	v12, (a2)
	vl2re64.v	v12, (s0)
	vadd.vx	v8, v8, s10
	add	a2, a2, t1
	vxor.vv	v12, v12, v16
	vs2r.v	v12, (s1)
	add	s1, s1, t1
	add	a3, a3, t1
	add	a0, a0, t1
	bne	s1, a7, .LBB1_33
# %bb.34:
	bne	a6, a5, .LBB1_18
.LBB1_35:
	ld	s6, 48(sp)                      # 8-byte Folded Reload
	mv	a0, s6
	mv	a2, t3
	mv	a3, s8
	call	karatsuba_mul
	mv	a0, s5
	li	a1, 0
	mv	a2, s7
	call	memset
	andi	a6, s3, -2
	slli	s9, s9, 4
	beqz	a6, .LBB1_42
# %bb.36:
	li	a1, 12
	maxu	a1, s10, a1
	bltu	a6, a1, .LBB1_39
# %bb.37:
	add	a1, s2, s9
	bgeu	s5, a1, .LBB1_48
# %bb.38:
	add	a1, s5, s9
	bgeu	s2, a1, .LBB1_48
.LBB1_39:
	li	a1, 0
.LBB1_40:
	slli	a2, a1, 3
	add	a1, s5, a2
	add	a2, a2, s2
	add	a3, s5, s9
.LBB1_41:                               # =>This Inner Loop Header: Depth=1
	ld	a4, 0(a2)
	ld	a5, 0(a1)
	xor	a4, a4, a5
	sd	a4, 0(a1)
	addi	a1, a1, 8
	addi	a2, a2, 8
	bne	a1, a3, .LBB1_41
.LBB1_42:
	slli	s8, s8, 1
	beqz	s8, .LBB1_69
# %bb.43:
	li	a1, 16
	maxu	a1, s10, a1
	slli	s1, s3, 5
	bgeu	s8, a1, .LBB1_45
# %bb.44:
	li	a2, 0
	ld	a0, 80(sp)                      # 8-byte Folded Reload
	ld	a7, 8(sp)                       # 8-byte Folded Reload
	j	.LBB1_54
.LBB1_45:
	slli	a1, a6, 3
	sub	a2, s1, s9
	ld	a0, 80(sp)                      # 8-byte Folded Reload
	add	a1, a1, a0
	add	a2, a2, s2
	ld	a7, 8(sp)                       # 8-byte Folded Reload
	bgeu	a1, a2, .LBB1_51
# %bb.46:
	add	a2, a0, s7
	bgeu	a7, a2, .LBB1_51
# %bb.47:
	li	a2, 0
	j	.LBB1_54
.LBB1_48:
	neg	a1, s10
	and	a1, s3, a1
	sub	a2, a1, s10
	divu	a3, a2, s10
	ld	a0, 16(sp)                      # 8-byte Folded Reload
	slli	a2, a0, 4
	slli	a3, a3, 4
	addi	a3, a3, 16
	mul	a3, a3, a0
	add	a3, a3, s5
	mv	a4, s2
	mv	a5, s5
	vsetvli	s1, zero, e64, m2, ta, ma
.LBB1_49:                               # =>This Inner Loop Header: Depth=1
	vl2re64.v	v8, (a4)
	vl2re64.v	v10, (a5)
	vxor.vv	v8, v10, v8
	vs2r.v	v8, (a5)
	add	a5, a5, a2
	add	a4, a4, a2
	bne	a5, a3, .LBB1_49
# %bb.50:
	beq	a6, a1, .LBB1_42
	j	.LBB1_40
.LBB1_51:
	neg	a2, s10
	and	a2, s8, a2
	sub	a3, a2, s10
	divu	a4, a3, s10
	ld	a5, 16(sp)                      # 8-byte Folded Reload
	slli	a3, a5, 4
	slli	a4, a4, 4
	addi	a4, a4, 16
	mul	a4, a4, a5
	add	a5, s2, s7
	add	a4, a4, a5
	mv	a5, a7
	vsetvli	s0, zero, e64, m2, ta, ma
.LBB1_52:                               # =>This Inner Loop Header: Depth=1
	vl2re64.v	v8, (a5)
	vl2re64.v	v10, (a1)
	add	a5, a5, a3
	vxor.vv	v8, v10, v8
	vs2r.v	v8, (a1)
	add	a1, a1, a3
	bne	a5, a4, .LBB1_52
# %bb.53:
	beq	s8, a2, .LBB1_56
.LBB1_54:
	slli	a2, a2, 3
	sub	a3, s1, s9
	add	a1, s7, a2
	add	a2, a2, s9
	add	a1, a1, s2
	add	a2, a2, a0
	add	a3, a3, s2
.LBB1_55:                               # =>This Inner Loop Header: Depth=1
	ld	a4, 0(a1)
	ld	a5, 0(a2)
	addi	a1, a1, 8
	xor	a4, a4, a5
	sd	a4, 0(a2)
	addi	a2, a2, 8
	bne	a1, a3, .LBB1_55
.LBB1_56:
	li	a1, 24
	maxu	a1, s10, a1
	bgeu	s8, a1, .LBB1_63
# %bb.57:
	li	a1, 0
.LBB1_58:
	slli	a2, a1, 3
	add	a3, s1, a2
	add	a4, s7, a2
	add	s4, s4, a2
	add	a2, a2, s2
	add	a3, a3, s2
	add	a4, a4, s2
	ld	s7, 80(sp)                      # 8-byte Folded Reload
	add	s7, s7, s4
	j	.LBB1_61
.LBB1_59:                               #   in Loop: Header=BB1_61 Depth=1
	ld	a5, 0(a2)
.LBB1_60:                               #   in Loop: Header=BB1_61 Depth=1
	ld	s1, 0(a4)
	ld	s0, 0(a3)
	ld	a0, 0(s7)
	addi	a1, a1, 1
	addi	a3, a3, 8
	addi	a4, a4, 8
	addi	a2, a2, 8
	xor	a5, a5, s1
	xor	a0, a0, s0
	xor	a0, a0, a5
	sd	a0, 0(s7)
	addi	s7, s7, 8
	beq	s8, a1, .LBB1_69
.LBB1_61:                               # =>This Inner Loop Header: Depth=1
	bltu	a1, a6, .LBB1_59
# %bb.62:                               #   in Loop: Header=BB1_61 Depth=1
	li	a5, 0
	j	.LBB1_60
.LBB1_63:
	add	a2, a0, s4
	sub	a3, s7, s4
	ld	a1, 64(sp)                      # 8-byte Folded Reload
	sub	a1, a1, s7
	add	a3, a3, a0
	sub	a1, a1, s9
	add	a1, a1, s2
	sltu	a1, a2, a1
	sltu	a4, s6, a3
	and	a4, a4, a1
	li	a1, 0
	bnez	a4, .LBB1_58
# %bb.64:
	sub	a4, s1, s9
	add	a4, a4, s2
	sltu	a4, a2, a4
	sltu	a5, a7, a3
	and	a4, a4, a5
	bnez	a4, .LBB1_58
# %bb.65:
	sub	a4, s7, s9
	add	a4, a4, s2
	sltu	a4, a2, a4
	sltu	a3, s2, a3
	and	a3, a3, a4
	bnez	a3, .LBB1_58
# %bb.66:
	neg	a1, s10
	and	a1, s8, a1
	sub	a3, a1, s10
	divu	a4, a3, s10
	vsetvli	a3, zero, e64, m2, ta, ma
	vid.v	v8
	ld	a0, 16(sp)                      # 8-byte Folded Reload
	slli	s0, a0, 4
	slli	a4, a4, 4
	addi	a4, a4, 16
	mul	a4, a4, a0
	add	a4, a4, s2
	vmv.v.i	v10, 0
	mv	a3, s2
.LBB1_67:                               # =>This Inner Loop Header: Depth=1
	vsetvli	zero, zero, e64, m2, ta, mu
	vmsltu.vx	v0, v8, a6
	vmv2r.v	v12, v10
	add	a5, a3, s7
	vl2re64.v	v14, (a5)
	add	a5, a3, s1
	vl2re64.v	v16, (a2)
	vle64.v	v12, (a3), v0.t
	vl2re64.v	v18, (a5)
	vadd.vx	v8, v8, s10
	add	a3, a3, s0
	vxor.vv	v12, v14, v12
	vxor.vv	v12, v12, v18
	vxor.vv	v12, v12, v16
	vs2r.v	v12, (a2)
	add	a2, a2, s0
	bne	a3, a4, .LBB1_67
# %bb.68:
	bne	s8, a1, .LBB1_58
.LBB1_69:
	ld	ra, 184(sp)                     # 8-byte Folded Reload
	ld	s0, 176(sp)                     # 8-byte Folded Reload
	ld	s1, 168(sp)                     # 8-byte Folded Reload
	ld	s2, 160(sp)                     # 8-byte Folded Reload
	ld	s3, 152(sp)                     # 8-byte Folded Reload
	ld	s4, 144(sp)                     # 8-byte Folded Reload
	ld	s5, 136(sp)                     # 8-byte Folded Reload
	ld	s6, 128(sp)                     # 8-byte Folded Reload
	ld	s7, 120(sp)                     # 8-byte Folded Reload
	ld	s8, 112(sp)                     # 8-byte Folded Reload
	ld	s9, 104(sp)                     # 8-byte Folded Reload
	ld	s10, 96(sp)                     # 8-byte Folded Reload
	ld	s11, 88(sp)                     # 8-byte Folded Reload
	addi	sp, sp, 192
	ret
.LBB1_70:
	ld	s5, 80(sp)                      # 8-byte Folded Reload
	j	.LBB1_18
.LBB1_71:
	ld	s5, 80(sp)                      # 8-byte Folded Reload
	ld	t0, 72(sp)                      # 8-byte Folded Reload
	mv	s8, a3
	ld	s2, 40(sp)                      # 8-byte Folded Reload
	ld	s7, 56(sp)                      # 8-byte Folded Reload
	j	.LBB1_18
.LBB1_72:
	ld	s5, 80(sp)                      # 8-byte Folded Reload
	mv	s8, a3
	ld	s7, 56(sp)                      # 8-byte Folded Reload
	j	.LBB1_18
.Lfunc_end1:
	.size	karatsuba_mul, .Lfunc_end1-karatsuba_mul
                                        # -- End function
	.ident	"clang version 21.1.1 (https://github.com/llvm/llvm-project.git 5a86dc996c26299de63effc927075dcbfb924167)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
