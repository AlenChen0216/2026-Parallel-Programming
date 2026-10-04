	.file	"test1.c"
	.text
	.globl	test1                           # -- Begin function test1
	.p2align	4
	.type	test1,@function
test1:                                  # @test1
	.cfi_startproc
# %bb.0:
	movq	%rdx, %rax
	subq	%rdi, %rax
	movq	%rdx, %rcx
	subq	%rsi, %rcx
	cmpq	$128, %rax
	setb	%r8b
	cmpq	$128, %rcx
	setb	%al
	orb	%r8b, %al
	xorl	%ecx, %ecx
	jmp	.LBB0_1
	.p2align	4
.LBB0_3:                                #   in Loop: Header=BB0_1 Depth=1
	incl	%ecx
	cmpl	$20000000, %ecx                 # imm = 0x1312D00
	je	.LBB0_4
.LBB0_1:                                # =>This Loop Header: Depth=1
                                        #     Child Loop BB0_2 Depth 2
                                        #     Child Loop BB0_5 Depth 2
	xorl	%r8d, %r8d
	testb	%al, %al
	je	.LBB0_2
	.p2align	4
.LBB0_5:                                #   Parent Loop BB0_1 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	vmovss	(%rdi,%r8,4), %xmm0             # xmm0 = mem[0],zero,zero,zero
	vaddss	(%rsi,%r8,4), %xmm0, %xmm0
	vmovss	%xmm0, (%rdx,%r8,4)
	vmovss	4(%rdi,%r8,4), %xmm0            # xmm0 = mem[0],zero,zero,zero
	vaddss	4(%rsi,%r8,4), %xmm0, %xmm0
	vmovss	%xmm0, 4(%rdx,%r8,4)
	vmovss	8(%rdi,%r8,4), %xmm0            # xmm0 = mem[0],zero,zero,zero
	vaddss	8(%rsi,%r8,4), %xmm0, %xmm0
	vmovss	%xmm0, 8(%rdx,%r8,4)
	vmovss	12(%rdi,%r8,4), %xmm0           # xmm0 = mem[0],zero,zero,zero
	vaddss	12(%rsi,%r8,4), %xmm0, %xmm0
	vmovss	%xmm0, 12(%rdx,%r8,4)
	addq	$4, %r8
	cmpq	$1024, %r8                      # imm = 0x400
	jne	.LBB0_5
	jmp	.LBB0_3
	.p2align	4
.LBB0_2:                                #   Parent Loop BB0_1 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	vmovaps	(%rdi,%r8,4), %ymm0
	vmovaps	32(%rdi,%r8,4), %ymm1
	vmovaps	64(%rdi,%r8,4), %ymm2
	vmovaps	96(%rdi,%r8,4), %ymm3
	vaddps	(%rsi,%r8,4), %ymm0, %ymm0
	vaddps	32(%rsi,%r8,4), %ymm1, %ymm1
	vaddps	64(%rsi,%r8,4), %ymm2, %ymm2
	vaddps	96(%rsi,%r8,4), %ymm3, %ymm3
	vmovaps	%ymm0, (%rdx,%r8,4)
	vmovaps	%ymm1, 32(%rdx,%r8,4)
	vmovaps	%ymm2, 64(%rdx,%r8,4)
	vmovaps	%ymm3, 96(%rdx,%r8,4)
	addq	$32, %r8
	cmpq	$1024, %r8                      # imm = 0x400
	jne	.LBB0_2
	jmp	.LBB0_3
.LBB0_4:
	vzeroupper
	retq
.Lfunc_end0:
	.size	test1, .Lfunc_end0-test1
	.cfi_endproc
                                        # -- End function
	.ident	"clang version 22.1.8"
	.section	".note.GNU-stack","",@progbits
	.addrsig
