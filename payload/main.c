/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "state.h"
#include "pervasive.h"
#include "gpio.h"
#include "syscon.h"
#include "utils.h"
#include "msif/ms.h"
#include "msif/msif.h"

#include "ff.h"

uint32_t g_sync_point = 0;

PayloadArguments *pargs;

void kscePervasiveMsifSetClock(int clock) {
  pervasive_msif_set_clock(clock);
}

uint64_t ksceKernelGetSystemTimeWide(void) {
  return REG32(0xe20b6000);
}

DWORD get_fattime(void) {
  return pargs->fattime;
}

static int verify_mbr(master_block_t *mbr) {
  if (memcmp(mbr->magic, "Sony Computer Entertainment Inc.", 32) != 0)
    return 0x8001002f;
  if (mbr->sig != 0xaa55)
    return 0x8001002f;
  return 0;
}

int get_mediaid_sector(void) {
  static master_block_t __attribute__((aligned(64))) mbr;
  int sector;
  int res;

  ms_read_sector(&mbr, 0, 1);

  res = verify_mbr(&mbr);
  if (res < 0)
    return res;

  sector = 0;
  for (int i = 0; i < 0x10; i++) {
    if (mbr.partitions[i].code == 0xd)
      sector = mbr.partitions[i].off;
  }

  return sector;
}

static void cdram_enable(void) {
  register uint32_t r12 __asm__("r12") = 0x117;
  __asm__ volatile("smc #0\n\t" :: "r"(r12));
}

static void l2_flush(void) {
  register uint32_t r0 __asm__("r0") = 1;
  register uint32_t r12 __asm__("r12") = 0x16a;
  __asm__ volatile("smc #0\n\t" : "+r"(r0) : "r"(r12));
}

int main(void) {
  pargs = (PayloadArguments *)PAYLOAD_ARGS_PADDR;

  l2_flush();
  syscon_init();
  cdram_enable();

  // Turn on Game Card LED
  syscon_common_write(0x80 | 0x40, 0x891, 2);
  gpio_set_port_mode(0, GPIO_PORT_GAMECARD_LED, GPIO_PORT_MODE_OUTPUT);
  gpio_port_set(0, GPIO_PORT_GAMECARD_LED);

  syscon_msif_set_power(1);
  pervasive_reset_enter_msif();
  pervasive_msif_set_clock(4);
  pervasive_reset_exit_msif();
  pervasive_clock_enable_msif();

  msproal_start();
  msproal_mount();

  FATFS fs;
  f_mount(&fs, "ux0:", 1);

  if (pargs->mode == DEJAVU_MODE_LOAD) {
    load_state(pargs->path);
  } else if (pargs->mode == DEJAVU_MODE_SAVE) {
    save_state(pargs->path);
  }

  f_unmount("ux0:");

  pervasive_clock_disable_msif();
  syscon_msif_set_power(0);

  return 0;
}
