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

  .global resume_function
  .type resume_function, %function
resume_function:
  dsb

  ldr r1, =sync_point_1
  bl cpus_sync

  mrc p15, 0, r0, c0, c0, 5 @ [<] MPIDR (Multiprocessor Affinity Register)
  ands r0, #0xf
  bne 1f

  ldr r0, =ori_resume_func_vaddr
  ldr r1, =CONTEXT_PADDR
  ldr r0, [r0]
  str r0, [r1, #0xc]

  ldr r0, =PAYLOAD_ARGS_PADDR
  ldr r1, =pargs
  ldr r2, =pargs_size
  ldr r2, [r2]
  bl resume_memcpy

  ldr r0, =PAYLOAD_PADDR
  ldr r1, =_binary_payload_dejavu_payload_bin_start
  ldr r2, =_binary_payload_dejavu_payload_bin_size
  bl resume_memcpy

  dsb
  sev
  b 2f
1:
  wfe
2:
  ldr r1, =sync_point_2
  bl cpus_sync

  ldr lr, =PAYLOAD_PADDR
  bx lr

  .global resume_memcpy
resume_memcpy:
  ldmia r1!, {r3}
  stmia r0!, {r3}
  subs r2, #4
  bne resume_memcpy
  bx lr

  .global cpus_sync
cpus_sync:
  mrc p15, 0, r0, c0, c0, 5 @ [<] MPIDR (Multiprocessor Affinity Register)
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

  .data
sync_point_1: .word 0
sync_point_2: .word 0
