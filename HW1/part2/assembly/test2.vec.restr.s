test2:
  xorl %r8d, %r8d
  jmp .LBB0_1
.LBB0_7:
  addl $1, %r8d
  cmpl $20000000, %r8d
  je .LBB0_8
.LBB0_1:
  xorl %ecx, %ecx
  jmp .LBB0_2
.LBB0_6:
  addq $2, %rcx
  cmpq $1024, %rcx
  je .LBB0_7
.LBB0_2:
  movl (%rdi,%rcx,4), %eax # rdi -> a , rcx -> j
  movl %eax, (%rdx,%rcx,4) # rdx -> c
  movss (%rsi,%rcx,4), %xmm0 # xmm0 -> b
  movd %eax, %xmm1 # xmm1 -> a
  ucomiss %xmm1, %xmm0 # compare a and b
  jbe .LBB0_4 # a <= b
  movss %xmm0, (%rdx,%rcx,4) # let c = b
.LBB0_4:
  movl 4(%rdi,%rcx,4), %eax
  movl %eax, 4(%rdx,%rcx,4)
  movss 4(%rsi,%rcx,4), %xmm0
  movd %eax, %xmm1
  ucomiss %xmm1, %xmm0
  jbe .LBB0_6
  movss %xmm0, 4(%rdx,%rcx,4)
  jmp .LBB0_6
.LBB0_8:
  retq