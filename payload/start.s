@ dejavu savestate plugin
@
@ Copyright (C) 2020 TheFloW
@
@ This software may be modified and distributed under the terms
@ of the MIT license.  See the LICENSE file for details.
@

#include "../shared.h"

  .syntax unified
  .cpu cortex-a9
  .text
  .align 4

  .global _start
_start:
  cpsid if
  clrex

  mov r0, #0
  bic r0, #0x40000000
  bic r0, #0x10000000
  bic r0, #0x20000000
  bic r0, #0x2000000
  bic r0, #0x2000
  bic r0, #0x1000
  bic r0, #0x800
  bic r0, #0x400
  bic r0, #2
  bic r0, #4
  bic r0, #1
  mcr p15, 0, r0, c1, c0, 0  @ [>] SCTLR (System Control Register)

  mov r0, #0
  mcr p15, 0, r0, c7, c1, 0  @ [>] ICIALLUIS (Instruction Cache Invalidate All to PoU, Inner Shareable)
  dsb
  mcr p15, 0, r0, c7, c1, 6  @ [>] BPIALLIS (Branch Predictor Invalidate All, Inner Shareable)
  isb
  mcr p15, 0, r0, c7, c5, 0  @ [>] ICIALLU (Instruction Cache Invalidate All to PoU)
  dsb

1:
  mov r1, #0
2:
  orr r2, r1, r0
  mcr p15, 0, r2, c7, c6, 2  @ [>] DCISW (Data Cache line Invalidate by Set/Way)
  add r1, #0x40000000
  cmp r1, #0
  bne 2b
  add r0, #0x20
  cmp r0, #0x2000
  bne 1b

  dsb
  mcr p15, 0, r0, c7, c5, 6  @ [>] BPIALL (Branch Predictor Invalidate All)
  isb
  mcr p15, 0, r0, c8, c3, 0  @ [>] TLBIALLIS (TLB Invalidate All, Inner Shareable)
  mov r0, #0
  mcr p15, 0, r0, c7, c5, 0  @ [>] ICIALLU (Instruction Cache Invalidate All to PoU)
  mov r0, #0
  mcr p15, 0, r0, c8, c7, 0  @ [>] TLBIALL (TLB Invalidate All)
  mcr p15, 0, r0, c2, c0, 2  @ [>] TTBCR (Translation Table Base Control Register)
  mov r0, #0
  mcr p15, 0, r0, c7, c5, 6  @ [>] BPIALL (Branch Predictor Invalidate All)
  isb

  mrc p15, 0, r0, c0, c0, 5
  and r0, #0xf

  cpsid aif, #0x1f
  ldr sp, =STACK_PADDR

  cmp r0, #0
  bne 1f
  bl main

1:
  ldr r1, =g_sync_point
  bl cpus_sync

  ldr lr, =SCE_RESUME_PADDR
  bx lr

  .global cpus_sync
cpus_sync:
  mrc p15, 0, r0, c0, c0, 5  @ [<] MPIDR (Multiprocessor Affinity Register)
  ands r0, #0xf
  cmp r0, #0
  it eq
  streq r0, [r1]

1:
  ldrb r2, [r1]
  cmp r0, r2
  itt ne
  wfene
  bne 1b
  ldrh r2, [r1]
  adds r2, #1
  adds r2, #0x100
  strh r2, [r1]
  dsb
  sev

1:
  ldrb r2, [r1, #1]
  cmp r2, #4
  itt ne
  wfene
  bne 1b

  bx lr
