/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>

#include "pervasive.h"
#include "utils.h"

#define PERVASIVE_RESET_BASE_ADDR   0xe3101000
#define PERVASIVE_GATE_BASE_ADDR    0xe3102000
#define PERVASIVE_BASECLK_BASE_ADDR 0xe3103000
#define PERVASIVE_MISC_BASE_ADDR    0xe3100000
#define PERVASIVE2_BASE_ADDR        0xe3110000

#define PERVASIVE_BASECLK_MSIF ((void *)(PERVASIVE_BASECLK_BASE_ADDR + 0xb0))

static inline void pervasive_mask_or(uint32_t addr, uint32_t val) {
  volatile unsigned long tmp;

  __asm__ volatile(
    "ldr %0, [%1]\n\t"
    "orr %0, %2\n\t"
    "str %0, [%1]\n\t"
    "dmb\n\t"
    "ldr %0, [%1]\n\t"
    "dsb\n\t"
    : "=&r"(tmp)
    : "r"(addr), "r"(val)
  );
}

static inline void pervasive_mask_and_not(uint32_t addr, uint32_t val) {
  volatile unsigned long tmp;

  __asm__ volatile(
    "ldr %0, [%1]\n\t"
    "bic %0, %2\n\t"
    "str %0, [%1]\n\t"
    "dmb\n\t"
    "ldr %0, [%1]\n\t"
    "dsb\n\t"
    : "=&r"(tmp)
    : "r"(addr), "r"(val)
  );
}

uint32_t pervasive_read_misc(uint32_t offset) {
  return *(uint32_t *)(PERVASIVE_MISC_BASE_ADDR + offset);
}

void pervasive_clock_enable_gpio(void) {
  pervasive_mask_or(PERVASIVE_GATE_BASE_ADDR + 0x100, 1);
}

void pervasive_reset_exit_gpio(void) {
  pervasive_mask_and_not(PERVASIVE_RESET_BASE_ADDR + 0x100, 1);
}

void pervasive_clock_enable_spi(int bus) {
  pervasive_mask_or(PERVASIVE_GATE_BASE_ADDR + 0x104 + 4 * bus, 1);
}

void pervasive_clock_disable_spi(int bus) {
  pervasive_mask_and_not(PERVASIVE_GATE_BASE_ADDR + 0x104 + 4 * bus, 1);
}

void pervasive_reset_exit_spi(int bus) {
  pervasive_mask_and_not(PERVASIVE_RESET_BASE_ADDR + 0x104 + 4 * bus, 1);
}

void pervasive_clock_enable_msif(void) {
  pervasive_mask_or(PERVASIVE_GATE_BASE_ADDR + 0xb0, 1);
}

void pervasive_clock_disable_msif(void) {
  pervasive_mask_and_not(PERVASIVE_GATE_BASE_ADDR + 0xb0, 1);
}

void pervasive_reset_exit_msif(void) {
  pervasive_mask_and_not(PERVASIVE_RESET_BASE_ADDR + 0xb0, 1);
}

void pervasive_reset_enter_msif(void) {
  pervasive_mask_or(PERVASIVE_RESET_BASE_ADDR + 0xb0, 1);
}

int pervasive_msif_get_card_insert_state(void) {
  return *(volatile uint32_t *)(PERVASIVE2_BASE_ADDR + 0xf40) & 1;
}

uint32_t pervasive_msif_unk(void) {
  uint32_t val;
  volatile uint32_t *pervasive2_regs = (void *)PERVASIVE2_BASE_ADDR;

  val = pervasive2_regs[0x3d1];
  pervasive2_regs[0x3d1] = val;
  pervasive2_regs[0x3d1];
  dsb();

  return val;
}

void pervasive_msif_set_clock(uint32_t clock) {
  uint32_t val;
  volatile uint32_t *baseclk_msif_regs = PERVASIVE_BASECLK_MSIF;

  if ((clock & ~(1 << 2)) > 2)
    return;

  if (clock & (1 << 2))
    val = 0x10000;
  else
    val = 0;

  *baseclk_msif_regs = (clock & 0b11) | val;
  dmb();
}
