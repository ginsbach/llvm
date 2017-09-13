; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc < %s -mtriple=i686-unknown-unknown | FileCheck %s --check-prefix=X86
; RUN: llc < %s -mtriple=x86_64-unknown-unknown | FileCheck %s --check-prefix=X64

@a = common global i32 0, align 4
@c = common local_unnamed_addr global i8 0, align 1
@b = common local_unnamed_addr global i32* null, align 8

define void @e() {
; X86-LABEL: e:
; X86:       # BB#0: # %entry
; X86-NEXT:    movl b, %eax
; X86-NEXT:    .p2align 4, 0x90
; X86-NEXT:  .LBB0_1: # %for.cond
; X86-NEXT:    # =>This Inner Loop Header: Depth=1
; X86-NEXT:    movzbl c, %ecx
; X86-NEXT:    leal a+2(%ecx), %ecx
; X86-NEXT:    movb $0, c
; X86-NEXT:    movl %ecx, (%eax)
; X86-NEXT:    jmp .LBB0_1
;
; X64-LABEL: e:
; X64:       # BB#0: # %entry
; X64-NEXT:    movq {{.*}}(%rip), %rax
; X64-NEXT:    movl $a, %esi
; X64-NEXT:    .p2align 4, 0x90
; X64-NEXT:  .LBB0_1: # %for.cond
; X64-NEXT:    # =>This Inner Loop Header: Depth=1
; X64-NEXT:    movzbl {{.*}}(%rip), %edx
; X64-NEXT:    addq %rsi, %rdx
; X64-NEXT:    setb %cl
; X64-NEXT:    addq $2, %rdx
; X64-NEXT:    adcb $0, %cl
; X64-NEXT:    movb %cl, {{.*}}(%rip)
; X64-NEXT:    movl %edx, (%rax)
; X64-NEXT:    jmp .LBB0_1
entry:
  %0 = load i32*, i32** @b, align 8
  br label %for.cond

for.cond:
  %1 = load i8, i8* @c, align 1
  %conv = zext i8 %1 to i128
  %add = add nuw nsw i128 %conv, add (i128 ptrtoint (i32* @a to i128), i128 2)
  %2 = lshr i128 %add, 64
  %conv1 = trunc i128 %2 to i8
  store i8 %conv1, i8* @c, align 1
  %conv2 = trunc i128 %add to i32
  store i32 %conv2, i32* %0, align 4
  br label %for.cond
}
