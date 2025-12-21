	.section	__TEXT,__text,regular,pure_instructions
	.build_version macos, 12, 0	sdk_version 12, 3
	.globl	__Z3Barb                        ## -- Begin function _Z3Barb
	.p2align	4, 0x90
__Z3Barb:                               ## @_Z3Barb
Lfunc_begin0:
	.cfi_startproc
	.cfi_personality 155, ___gxx_personality_v0
	.cfi_lsda 16, Lexception0
## %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$64, %rsp
	movb	%dil, %al
	andb	$1, %al
	movb	%al, -1(%rbp)
	leaq	-8(%rbp), %rdi
	movl	$17, %esi
	callq	__ZN3Foo9SetLengthEi
	testb	$1, -1(%rbp)
	je	LBB0_7
## %bb.1:
	movl	$8, %edi
	callq	___cxa_allocate_exception
	movq	%rax, %rcx
	movq	%rcx, -48(%rbp)                 ## 8-byte Spill
	movq	%rax, -40(%rbp)                 ## 8-byte Spill
Ltmp0:
	movl	$16, %edi
	callq	__Znwm
Ltmp1:
	movq	%rax, -32(%rbp)                 ## 8-byte Spill
	jmp	LBB0_2
LBB0_2:
	movq	-32(%rbp), %rdi                 ## 8-byte Reload
	movq	%rdi, %rax
	movq	%rax, -56(%rbp)                 ## 8-byte Spill
Ltmp3:
	leaq	L_.str(%rip), %rsi
	callq	__ZN9ExceptionC1EPKc
Ltmp4:
	jmp	LBB0_3
LBB0_3:
	movq	-48(%rbp), %rdi                 ## 8-byte Reload
	movq	-40(%rbp), %rax                 ## 8-byte Reload
	movq	-56(%rbp), %rcx                 ## 8-byte Reload
	movq	%rcx, (%rax)
	movq	__ZTIP9Exception@GOTPCREL(%rip), %rsi
	xorl	%eax, %eax
	movl	%eax, %edx
	callq	___cxa_throw
LBB0_4:
Ltmp2:
	movq	%rax, %rcx
	movl	%edx, %eax
	movq	%rcx, -16(%rbp)
	movl	%eax, -20(%rbp)
	jmp	LBB0_6
LBB0_5:
Ltmp5:
	movq	-32(%rbp), %rdi                 ## 8-byte Reload
	movq	%rax, %rcx
	movl	%edx, %eax
	movq	%rcx, -16(%rbp)
	movl	%eax, -20(%rbp)
	callq	__ZdlPv
LBB0_6:
	movq	-48(%rbp), %rdi                 ## 8-byte Reload
	callq	___cxa_free_exception
	jmp	LBB0_8
LBB0_7:
	leaq	-8(%rbp), %rdi
	movl	$24, %esi
	callq	__ZN3Foo9SetLengthEi
	leaq	-8(%rbp), %rdi
	callq	__ZNK3Foo9GetLengthEv
	addq	$64, %rsp
	popq	%rbp
	retq
LBB0_8:
	movq	-16(%rbp), %rdi
	callq	__Unwind_Resume
Lfunc_end0:
	.cfi_endproc
	.section	__TEXT,__gcc_except_tab
	.p2align	2
GCC_except_table0:
Lexception0:
	.byte	255                             ## @LPStart Encoding = omit
	.byte	255                             ## @TType Encoding = omit
	.byte	1                               ## Call site Encoding = uleb128
	.uleb128 Lcst_end0-Lcst_begin0
Lcst_begin0:
	.uleb128 Lfunc_begin0-Lfunc_begin0      ## >> Call Site 1 <<
	.uleb128 Ltmp0-Lfunc_begin0             ##   Call between Lfunc_begin0 and Ltmp0
	.byte	0                               ##     has no landing pad
	.byte	0                               ##   On action: cleanup
	.uleb128 Ltmp0-Lfunc_begin0             ## >> Call Site 2 <<
	.uleb128 Ltmp1-Ltmp0                    ##   Call between Ltmp0 and Ltmp1
	.uleb128 Ltmp2-Lfunc_begin0             ##     jumps to Ltmp2
	.byte	0                               ##   On action: cleanup
	.uleb128 Ltmp3-Lfunc_begin0             ## >> Call Site 3 <<
	.uleb128 Ltmp4-Ltmp3                    ##   Call between Ltmp3 and Ltmp4
	.uleb128 Ltmp5-Lfunc_begin0             ##     jumps to Ltmp5
	.byte	0                               ##   On action: cleanup
	.uleb128 Ltmp4-Lfunc_begin0             ## >> Call Site 4 <<
	.uleb128 Lfunc_end0-Ltmp4               ##   Call between Ltmp4 and Lfunc_end0
	.byte	0                               ##     has no landing pad
	.byte	0                               ##   On action: cleanup
Lcst_end0:
	.p2align	2
                                        ## -- End function
	.section	__TEXT,__text,regular,pure_instructions
	.globl	__ZN3Foo9SetLengthEi            ## -- Begin function _ZN3Foo9SetLengthEi
	.weak_definition	__ZN3Foo9SetLengthEi
	.p2align	4, 0x90
__ZN3Foo9SetLengthEi:                   ## @_ZN3Foo9SetLengthEi
	.cfi_startproc
## %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	%rdi, -8(%rbp)
	movl	%esi, -12(%rbp)
	movq	-8(%rbp), %rax
	movl	-12(%rbp), %ecx
	movl	%ecx, (%rax)
	popq	%rbp
	retq
	.cfi_endproc
                                        ## -- End function
	.globl	__ZN9ExceptionC1EPKc            ## -- Begin function _ZN9ExceptionC1EPKc
	.weak_def_can_be_hidden	__ZN9ExceptionC1EPKc
	.p2align	4, 0x90
__ZN9ExceptionC1EPKc:                   ## @_ZN9ExceptionC1EPKc
	.cfi_startproc
## %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movq	-8(%rbp), %rdi
	movq	-16(%rbp), %rsi
	callq	__ZN9ExceptionC2EPKc
	addq	$16, %rsp
	popq	%rbp
	retq
	.cfi_endproc
                                        ## -- End function
	.globl	__ZNK3Foo9GetLengthEv           ## -- Begin function _ZNK3Foo9GetLengthEv
	.weak_definition	__ZNK3Foo9GetLengthEv
	.p2align	4, 0x90
__ZNK3Foo9GetLengthEv:                  ## @_ZNK3Foo9GetLengthEv
	.cfi_startproc
## %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	%rdi, -8(%rbp)
	movq	-8(%rbp), %rax
	movl	(%rax), %eax
	popq	%rbp
	retq
	.cfi_endproc
                                        ## -- End function
	.globl	_main                           ## -- Begin function main
	.p2align	4, 0x90
_main:                                  ## @main
Lfunc_begin1:
	.cfi_startproc
	.cfi_personality 155, ___gxx_personality_v0
	.cfi_lsda 16, Lexception1
## %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$80, %rsp
	movl	$0, -4(%rbp)
	movl	%edi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movl	-8(%rbp), %eax
	subl	$2, %eax
	setge	-21(%rbp)
	movzbl	-21(%rbp), %edi
Ltmp6:
	andl	$1, %edi
	callq	__Z3Barb
Ltmp7:
	movl	%eax, -60(%rbp)                 ## 4-byte Spill
	jmp	LBB4_1
LBB4_1:
	movl	-60(%rbp), %eax                 ## 4-byte Reload
	movl	%eax, -28(%rbp)
	movl	$0, -20(%rbp)
	jmp	LBB4_7
LBB4_2:
Ltmp8:
	movq	%rax, %rcx
	movl	%edx, %eax
	movq	%rcx, -40(%rbp)
	movl	%eax, -44(%rbp)
## %bb.3:
	movl	-44(%rbp), %eax
	movl	$2, %ecx
	cmpl	%ecx, %eax
	jne	LBB4_8
## %bb.4:
	movq	-40(%rbp), %rdi
	callq	___cxa_begin_catch
	movq	%rax, -56(%rbp)
	movq	-56(%rbp), %rdi
Ltmp15:
	callq	__ZNK9Exception7GetTextEv
Ltmp16:
	movq	%rax, -72(%rbp)                 ## 8-byte Spill
	jmp	LBB4_5
LBB4_5:
Ltmp17:
	movq	-72(%rbp), %rsi                 ## 8-byte Reload
	leaq	L_.str.2(%rip), %rdi
	xorl	%eax, %eax
                                        ## kill: def $al killed $al killed $eax
	callq	_printf
Ltmp18:
	jmp	LBB4_6
LBB4_6:
	movl	$1, -20(%rbp)
	callq	___cxa_end_catch
LBB4_7:
	movl	-20(%rbp), %eax
	addq	$80, %rsp
	popq	%rbp
	retq
LBB4_8:
	movq	-40(%rbp), %rdi
	callq	___cxa_begin_catch
Ltmp9:
	leaq	L_.str.1(%rip), %rdi
	callq	_puts
Ltmp10:
	jmp	LBB4_9
LBB4_9:
	movl	$1, -20(%rbp)
	callq	___cxa_end_catch
	jmp	LBB4_7
LBB4_10:
Ltmp11:
	movq	%rax, %rcx
	movl	%edx, %eax
	movq	%rcx, -40(%rbp)
	movl	%eax, -44(%rbp)
Ltmp12:
	callq	___cxa_end_catch
Ltmp13:
	jmp	LBB4_11
LBB4_11:
	jmp	LBB4_13
LBB4_12:
Ltmp19:
	movq	%rax, %rcx
	movl	%edx, %eax
	movq	%rcx, -40(%rbp)
	movl	%eax, -44(%rbp)
	callq	___cxa_end_catch
LBB4_13:
	movq	-40(%rbp), %rdi
	callq	__Unwind_Resume
LBB4_14:
Ltmp14:
	movq	%rax, %rdi
	callq	___clang_call_terminate
Lfunc_end1:
	.cfi_endproc
	.section	__TEXT,__gcc_except_tab
	.p2align	2
GCC_except_table4:
Lexception1:
	.byte	255                             ## @LPStart Encoding = omit
	.byte	155                             ## @TType Encoding = indirect pcrel sdata4
	.uleb128 Lttbase0-Lttbaseref0
Lttbaseref0:
	.byte	1                               ## Call site Encoding = uleb128
	.uleb128 Lcst_end1-Lcst_begin1
Lcst_begin1:
	.uleb128 Ltmp6-Lfunc_begin1             ## >> Call Site 1 <<
	.uleb128 Ltmp7-Ltmp6                    ##   Call between Ltmp6 and Ltmp7
	.uleb128 Ltmp8-Lfunc_begin1             ##     jumps to Ltmp8
	.byte	3                               ##   On action: 2
	.uleb128 Ltmp7-Lfunc_begin1             ## >> Call Site 2 <<
	.uleb128 Ltmp15-Ltmp7                   ##   Call between Ltmp7 and Ltmp15
	.byte	0                               ##     has no landing pad
	.byte	0                               ##   On action: cleanup
	.uleb128 Ltmp15-Lfunc_begin1            ## >> Call Site 3 <<
	.uleb128 Ltmp18-Ltmp15                  ##   Call between Ltmp15 and Ltmp18
	.uleb128 Ltmp19-Lfunc_begin1            ##     jumps to Ltmp19
	.byte	0                               ##   On action: cleanup
	.uleb128 Ltmp18-Lfunc_begin1            ## >> Call Site 4 <<
	.uleb128 Ltmp9-Ltmp18                   ##   Call between Ltmp18 and Ltmp9
	.byte	0                               ##     has no landing pad
	.byte	0                               ##   On action: cleanup
	.uleb128 Ltmp9-Lfunc_begin1             ## >> Call Site 5 <<
	.uleb128 Ltmp10-Ltmp9                   ##   Call between Ltmp9 and Ltmp10
	.uleb128 Ltmp11-Lfunc_begin1            ##     jumps to Ltmp11
	.byte	0                               ##   On action: cleanup
	.uleb128 Ltmp10-Lfunc_begin1            ## >> Call Site 6 <<
	.uleb128 Ltmp12-Ltmp10                  ##   Call between Ltmp10 and Ltmp12
	.byte	0                               ##     has no landing pad
	.byte	0                               ##   On action: cleanup
	.uleb128 Ltmp12-Lfunc_begin1            ## >> Call Site 7 <<
	.uleb128 Ltmp13-Ltmp12                  ##   Call between Ltmp12 and Ltmp13
	.uleb128 Ltmp14-Lfunc_begin1            ##     jumps to Ltmp14
	.byte	1                               ##   On action: 1
	.uleb128 Ltmp13-Lfunc_begin1            ## >> Call Site 8 <<
	.uleb128 Lfunc_end1-Ltmp13              ##   Call between Ltmp13 and Lfunc_end1
	.byte	0                               ##     has no landing pad
	.byte	0                               ##   On action: cleanup
Lcst_end1:
	.byte	1                               ## >> Action Record 1 <<
                                        ##   Catch TypeInfo 1
	.byte	0                               ##   No further actions
	.byte	2                               ## >> Action Record 2 <<
                                        ##   Catch TypeInfo 2
	.byte	125                             ##   Continue to action 1
	.p2align	2
                                        ## >> Catch TypeInfos <<
	.long	__ZTIP9Exception@GOTPCREL+4     ## TypeInfo 2
	.long	0                               ## TypeInfo 1
Lttbase0:
	.p2align	2
                                        ## -- End function
	.section	__TEXT,__text,regular,pure_instructions
	.private_extern	___clang_call_terminate ## -- Begin function __clang_call_terminate
	.globl	___clang_call_terminate
	.weak_definition	___clang_call_terminate
	.p2align	4, 0x90
___clang_call_terminate:                ## @__clang_call_terminate
## %bb.0:
	pushq	%rax
	callq	___cxa_begin_catch
	callq	__ZSt9terminatev
                                        ## -- End function
	.globl	__ZNK9Exception7GetTextEv       ## -- Begin function _ZNK9Exception7GetTextEv
	.weak_definition	__ZNK9Exception7GetTextEv
	.p2align	4, 0x90
__ZNK9Exception7GetTextEv:              ## @_ZNK9Exception7GetTextEv
	.cfi_startproc
## %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	%rdi, -8(%rbp)
	movq	-8(%rbp), %rax
	movq	8(%rax), %rax
	popq	%rbp
	retq
	.cfi_endproc
                                        ## -- End function
	.globl	__ZN9ExceptionC2EPKc            ## -- Begin function _ZN9ExceptionC2EPKc
	.weak_def_can_be_hidden	__ZN9ExceptionC2EPKc
	.p2align	4, 0x90
__ZN9ExceptionC2EPKc:                   ## @_ZN9ExceptionC2EPKc
	.cfi_startproc
## %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movq	-8(%rbp), %rdi
	movq	%rdi, -24(%rbp)                 ## 8-byte Spill
	callq	__ZN6ObjectC2Ev
	movq	-24(%rbp), %rax                 ## 8-byte Reload
	movq	__ZTV9Exception@GOTPCREL(%rip), %rcx
	addq	$16, %rcx
	movq	%rcx, (%rax)
	movq	-16(%rbp), %rcx
	movq	%rcx, 8(%rax)
	addq	$32, %rsp
	popq	%rbp
	retq
	.cfi_endproc
                                        ## -- End function
	.globl	__ZN6ObjectC2Ev                 ## -- Begin function _ZN6ObjectC2Ev
	.weak_def_can_be_hidden	__ZN6ObjectC2Ev
	.p2align	4, 0x90
__ZN6ObjectC2Ev:                        ## @_ZN6ObjectC2Ev
	.cfi_startproc
## %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	%rdi, -8(%rbp)
	movq	-8(%rbp), %rax
	movq	__ZTV6Object@GOTPCREL(%rip), %rcx
	addq	$16, %rcx
	movq	%rcx, (%rax)
	popq	%rbp
	retq
	.cfi_endproc
                                        ## -- End function
	.globl	__ZN9ExceptionD1Ev              ## -- Begin function _ZN9ExceptionD1Ev
	.weak_def_can_be_hidden	__ZN9ExceptionD1Ev
	.p2align	4, 0x90
__ZN9ExceptionD1Ev:                     ## @_ZN9ExceptionD1Ev
	.cfi_startproc
## %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movq	%rdi, -8(%rbp)
	movq	-8(%rbp), %rdi
	callq	__ZN9ExceptionD2Ev
	addq	$16, %rsp
	popq	%rbp
	retq
	.cfi_endproc
                                        ## -- End function
	.globl	__ZN9ExceptionD0Ev              ## -- Begin function _ZN9ExceptionD0Ev
	.weak_def_can_be_hidden	__ZN9ExceptionD0Ev
	.p2align	4, 0x90
__ZN9ExceptionD0Ev:                     ## @_ZN9ExceptionD0Ev
Lfunc_begin2:
	.cfi_startproc
	.cfi_personality 155, ___gxx_personality_v0
	.cfi_lsda 16, Lexception2
## %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
	movq	%rdi, -8(%rbp)
	movq	-8(%rbp), %rdi
	movq	%rdi, -32(%rbp)                 ## 8-byte Spill
Ltmp20:
	callq	__ZN9ExceptionD1Ev
Ltmp21:
	jmp	LBB10_1
LBB10_1:
	movq	-32(%rbp), %rdi                 ## 8-byte Reload
	callq	__ZdlPv
	addq	$32, %rsp
	popq	%rbp
	retq
LBB10_2:
Ltmp22:
	movq	-32(%rbp), %rdi                 ## 8-byte Reload
	movq	%rax, %rcx
	movl	%edx, %eax
	movq	%rcx, -16(%rbp)
	movl	%eax, -20(%rbp)
	callq	__ZdlPv
## %bb.3:
	movq	-16(%rbp), %rdi
	callq	__Unwind_Resume
Lfunc_end2:
	.cfi_endproc
	.section	__TEXT,__gcc_except_tab
	.p2align	2
GCC_except_table10:
Lexception2:
	.byte	255                             ## @LPStart Encoding = omit
	.byte	255                             ## @TType Encoding = omit
	.byte	1                               ## Call site Encoding = uleb128
	.uleb128 Lcst_end2-Lcst_begin2
Lcst_begin2:
	.uleb128 Ltmp20-Lfunc_begin2            ## >> Call Site 1 <<
	.uleb128 Ltmp21-Ltmp20                  ##   Call between Ltmp20 and Ltmp21
	.uleb128 Ltmp22-Lfunc_begin2            ##     jumps to Ltmp22
	.byte	0                               ##   On action: cleanup
	.uleb128 Ltmp21-Lfunc_begin2            ## >> Call Site 2 <<
	.uleb128 Lfunc_end2-Ltmp21              ##   Call between Ltmp21 and Lfunc_end2
	.byte	0                               ##     has no landing pad
	.byte	0                               ##   On action: cleanup
Lcst_end2:
	.p2align	2
                                        ## -- End function
	.section	__TEXT,__text,regular,pure_instructions
	.globl	__ZN6ObjectD1Ev                 ## -- Begin function _ZN6ObjectD1Ev
	.weak_def_can_be_hidden	__ZN6ObjectD1Ev
	.p2align	4, 0x90
__ZN6ObjectD1Ev:                        ## @_ZN6ObjectD1Ev
	.cfi_startproc
## %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movq	%rdi, -8(%rbp)
	movq	-8(%rbp), %rdi
	callq	__ZN6ObjectD2Ev
	addq	$16, %rsp
	popq	%rbp
	retq
	.cfi_endproc
                                        ## -- End function
	.globl	__ZN6ObjectD0Ev                 ## -- Begin function _ZN6ObjectD0Ev
	.weak_def_can_be_hidden	__ZN6ObjectD0Ev
	.p2align	4, 0x90
__ZN6ObjectD0Ev:                        ## @_ZN6ObjectD0Ev
Lfunc_begin3:
	.cfi_startproc
	.cfi_personality 155, ___gxx_personality_v0
	.cfi_lsda 16, Lexception3
## %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
	movq	%rdi, -8(%rbp)
	movq	-8(%rbp), %rdi
	movq	%rdi, -32(%rbp)                 ## 8-byte Spill
Ltmp23:
	callq	__ZN6ObjectD1Ev
Ltmp24:
	jmp	LBB12_1
LBB12_1:
	movq	-32(%rbp), %rdi                 ## 8-byte Reload
	callq	__ZdlPv
	addq	$32, %rsp
	popq	%rbp
	retq
LBB12_2:
Ltmp25:
	movq	-32(%rbp), %rdi                 ## 8-byte Reload
	movq	%rax, %rcx
	movl	%edx, %eax
	movq	%rcx, -16(%rbp)
	movl	%eax, -20(%rbp)
	callq	__ZdlPv
## %bb.3:
	movq	-16(%rbp), %rdi
	callq	__Unwind_Resume
Lfunc_end3:
	.cfi_endproc
	.section	__TEXT,__gcc_except_tab
	.p2align	2
GCC_except_table12:
Lexception3:
	.byte	255                             ## @LPStart Encoding = omit
	.byte	255                             ## @TType Encoding = omit
	.byte	1                               ## Call site Encoding = uleb128
	.uleb128 Lcst_end3-Lcst_begin3
Lcst_begin3:
	.uleb128 Ltmp23-Lfunc_begin3            ## >> Call Site 1 <<
	.uleb128 Ltmp24-Ltmp23                  ##   Call between Ltmp23 and Ltmp24
	.uleb128 Ltmp25-Lfunc_begin3            ##     jumps to Ltmp25
	.byte	0                               ##   On action: cleanup
	.uleb128 Ltmp24-Lfunc_begin3            ## >> Call Site 2 <<
	.uleb128 Lfunc_end3-Ltmp24              ##   Call between Ltmp24 and Lfunc_end3
	.byte	0                               ##     has no landing pad
	.byte	0                               ##   On action: cleanup
Lcst_end3:
	.p2align	2
                                        ## -- End function
	.section	__TEXT,__text,regular,pure_instructions
	.globl	__ZN6ObjectD2Ev                 ## -- Begin function _ZN6ObjectD2Ev
	.weak_def_can_be_hidden	__ZN6ObjectD2Ev
	.p2align	4, 0x90
__ZN6ObjectD2Ev:                        ## @_ZN6ObjectD2Ev
	.cfi_startproc
## %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	%rdi, -8(%rbp)
	popq	%rbp
	retq
	.cfi_endproc
                                        ## -- End function
	.globl	__ZN9ExceptionD2Ev              ## -- Begin function _ZN9ExceptionD2Ev
	.weak_def_can_be_hidden	__ZN9ExceptionD2Ev
	.p2align	4, 0x90
__ZN9ExceptionD2Ev:                     ## @_ZN9ExceptionD2Ev
	.cfi_startproc
## %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movq	%rdi, -8(%rbp)
	movq	-8(%rbp), %rdi
	callq	__ZN6ObjectD2Ev
	addq	$16, %rsp
	popq	%rbp
	retq
	.cfi_endproc
                                        ## -- End function
	.section	__TEXT,__cstring,cstring_literals
L_.str:                                 ## @.str
	.asciz	"Exception requested by caller"

	.section	__TEXT,__const
	.globl	__ZTSP9Exception                ## @_ZTSP9Exception
	.weak_definition	__ZTSP9Exception
__ZTSP9Exception:
	.asciz	"P9Exception"

	.globl	__ZTS9Exception                 ## @_ZTS9Exception
	.weak_definition	__ZTS9Exception
__ZTS9Exception:
	.asciz	"9Exception"

	.globl	__ZTS6Object                    ## @_ZTS6Object
	.weak_definition	__ZTS6Object
__ZTS6Object:
	.asciz	"6Object"

	.section	__DATA,__const
	.globl	__ZTI6Object                    ## @_ZTI6Object
	.weak_definition	__ZTI6Object
	.p2align	3
__ZTI6Object:
	.quad	__ZTVN10__cxxabiv117__class_type_infoE+16
	.quad	__ZTS6Object

	.globl	__ZTI9Exception                 ## @_ZTI9Exception
	.weak_definition	__ZTI9Exception
	.p2align	3
__ZTI9Exception:
	.quad	__ZTVN10__cxxabiv120__si_class_type_infoE+16
	.quad	__ZTS9Exception
	.quad	__ZTI6Object

	.globl	__ZTIP9Exception                ## @_ZTIP9Exception
	.weak_definition	__ZTIP9Exception
	.p2align	3
__ZTIP9Exception:
	.quad	__ZTVN10__cxxabiv119__pointer_type_infoE+16
	.quad	__ZTSP9Exception
	.long	0                               ## 0x0
	.space	4
	.quad	__ZTI9Exception

	.section	__TEXT,__cstring,cstring_literals
L_.str.1:                               ## @.str.1
	.asciz	"Internal error: Unhandled exception detected"

L_.str.2:                               ## @.str.2
	.asciz	"Error: %s\n"

	.section	__DATA,__const
	.globl	__ZTV9Exception                 ## @_ZTV9Exception
	.weak_def_can_be_hidden	__ZTV9Exception
	.p2align	3
__ZTV9Exception:
	.quad	0
	.quad	__ZTI9Exception
	.quad	__ZN9ExceptionD1Ev
	.quad	__ZN9ExceptionD0Ev

	.globl	__ZTV6Object                    ## @_ZTV6Object
	.weak_def_can_be_hidden	__ZTV6Object
	.p2align	3
__ZTV6Object:
	.quad	0
	.quad	__ZTI6Object
	.quad	__ZN6ObjectD1Ev
	.quad	__ZN6ObjectD0Ev

.subsections_via_symbols
