/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __DEJAVU_H__
#define __DEJAVU_H__

#include "shared.h"

#define ALIGN(x, align) (((x) + ((align) - 1)) & ~((align) - 1))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

#define SCE_RTC_CTX_SIZE 0x1c0

typedef struct SceSysconResumeContext {
  uint32_t size;
  uint32_t unk;
  uint32_t buff_vaddr;
  uint32_t resume_func_vaddr;
  uint32_t SCTLR;
  uint32_t ACTLR;
  uint32_t CPACR;
  uint32_t TTBR0;
  uint32_t TTBR1;
  uint32_t TTBCR;
  uint32_t DACR;
  uint32_t PRRR;
  uint32_t NMRR;
  uint32_t VBAR;
  uint32_t CONTEXTIDR;
  uint32_t TPIDRURW;
  uint32_t TPIDRURO;
  uint32_t TPIDRPRW;
  uint64_t unk2;
  uint64_t current_time;
  uint64_t alarm_tick;
  uint64_t system_time;
} SceSysconResumeContext;

typedef struct {
  uint32_t mode;
  uint32_t fattime;
  uint32_t process_num;
  uint32_t ttbs[32];
  char titleid[32][32];
  char rtc_ctx[SCE_RTC_CTX_SIZE];
  char path[256];
} PayloadArguments;

enum DejavuModes {
  DEJAVU_MODE_NONE,
  DEJAVU_MODE_LOAD,
  DEJAVU_MODE_SAVE,
};

typedef struct {
  void *addr;
  uint32_t size;
} Segment;

typedef struct {
  uint8_t  is_root;
  uint8_t  fattrib;
  uint16_t ftime;
  uint16_t fdate;
  uint32_t data_len;
  uint16_t path_len;
} SaveStateFile;

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t file_num;
  uint32_t segs_num;
} SaveStateHeader;

#define SAVESTATE_MAGIC 0x54535653
#define SAVESTATE_VERSION 1

#define MEDIAID_ADDR_MAGIC (void *)0x1337

#endif
