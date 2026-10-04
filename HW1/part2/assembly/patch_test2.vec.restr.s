test2:
  xorl %eax, %eax
.LBB0_1:
  xorl %ecx, %ecx
.LBB0_2:
  movaps (%rsi,%rcx,4), %xmm0 # xmm0 -> b[0:3] (4 elements)
  movaps 16(%rsi,%rcx,4), %xmm1 # xmm1 -> b[4:7] (4 elements)
  maxps (%rdi,%rcx,4), %xmm0 # xmm0 = max(a[0:3], b[0:3])
  maxps 16(%rdi,%rcx,4), %xmm1 # xmm1 = max(a[4:7], b[4:7])
  movaps %xmm0, (%rdx,%rcx,4) # c[0:3] = xmm0
  movaps %xmm1, 16(%rdx,%rcx,4) # c[4:7] = xmm1
  movaps 32(%rsi,%rcx,4), %xmm0
  movaps 48(%rsi,%rcx,4), %xmm1
  maxps 32(%rdi,%rcx,4), %xmm0
  maxps 48(%rdi,%rcx,4), %xmm1
  movaps %xmm0, 32(%rdx,%rcx,4)
  movaps %xmm1, 48(%rdx,%rcx,4)
  addq $16, %rcx
  cmpq $1024, %rcx
  jne .LBB0_2
  addl $1, %eax
  cmpl $20000000, %eax
  jne .LBB0_1
  retq