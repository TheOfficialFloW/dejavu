/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __MAIN_H__
#define __MAIN_H__

#include "../dejavu.h"

typedef struct {
  uint32_t off;
  uint32_t sz;
  uint8_t code;
  uint8_t type;
  uint8_t active;
  uint32_t flags;
  uint16_t unk;
} __attribute__((packed)) partition_t;

typedef struct {
  char magic[0x20];
  uint32_t version;
  uint32_t device_size;
  char unk1[0x28];
  partition_t partitions[0x10];
  char unk2[0x5e];
  char unk3[0x10 * 4];
  uint16_t sig;
} __attribute__((packed)) master_block_t;

extern PayloadArguments *pargs;

void kscePervasiveMsifSetClock(int clock);
uint64_t ksceKernelGetSystemTimeWide(void);

int get_mediaid_sector(void);

#endif
