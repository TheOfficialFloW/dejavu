/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>

#include "ms.h"
#include "msif.h"
#include "main.h"

#define N_ADMA_DESCS 0x2000

static SceMsifAdmaDescriptor g_adma_descs[N_ADMA_DESCS];

static char __attribute__((aligned(64))) g_start_tmp_buf[64];
static char __attribute__((aligned(64))) g_end_tmp_buf[64];

static uint32_t g_start_size = 0, g_end_size = 0;
static void *g_start_addr = NULL, *g_end_addr = NULL;

static uint16_t ms_get_attr(uint32_t size) {
  uint16_t attr;

  if ((size & 0x3f) == 0) {
    attr = 0x7; // 64 bytes aligned
  } else if ((size & 0x1f) == 0) {
    attr = 0x5; // 32 bytes aligned
  } else if ((size & 0xf) == 0) {
    attr = 0x3; // 16 bytes aligned
  } else {
    attr = 0;
  }

  attr |= (0x4000 | 0x8000);

  return attr;
}

static int sceMsifPrepareDmaTable(void *buf, uint32_t size, int is_write) {
  uint32_t chunk_size = 0;
  int seg = 0;

  char *start = buf;
  char *end = buf + size;

  g_start_size = 0;
  g_end_size = 0;
  g_start_addr = NULL;
  g_end_addr = NULL;

  if (size == 0)
    return 0;
  if ((uintptr_t)buf & 3)
    return -1;

  if (size > 0) {
    if ((uintptr_t)start & 0x3f) {
      g_start_size = 0x40 - ((uintptr_t)start & 0x3f);
      g_start_addr = buf;
      if (g_start_size > 0) {
        if (g_start_size > size)
          g_start_size = size;
        if (is_write)
          memcpy(g_start_tmp_buf, g_start_addr, g_start_size);
        buf += g_start_size;
        size -= g_start_size;
      }
    }
  }

  if (size > 0) {
    if ((uintptr_t)end & 0x3f) {
      g_end_size = (uintptr_t)end & 0x3f;
      g_end_addr = (void *)((uintptr_t)end & ~0x3f);
      if (g_end_size > 0) {
        if (g_end_size > size)
          g_end_size = size;
        if (is_write)
          memcpy(g_end_tmp_buf, g_end_addr, g_end_size);
        size -= g_end_size;
      }
    }
  }

  if (g_start_size > 0 && seg < N_ADMA_DESCS) {
    if (seg != 0)
      g_adma_descs[seg - 1].next = &g_adma_descs[seg];

    g_adma_descs[seg].addr = g_start_tmp_buf;
    g_adma_descs[seg].next = NULL;
    g_adma_descs[seg].size = g_start_size >> 2;
    g_adma_descs[seg].attr = ms_get_attr(g_start_size);
    seg++;
  }

  while (size > 0 && seg < N_ADMA_DESCS) {
    chunk_size = MIN(size, 0x40000);

    if (seg != 0)
      g_adma_descs[seg - 1].next = &g_adma_descs[seg];

    g_adma_descs[seg].addr = buf;
    g_adma_descs[seg].next = NULL;
    g_adma_descs[seg].size = chunk_size >> 2;
    g_adma_descs[seg].attr = ms_get_attr(chunk_size);
    seg++;

    buf += chunk_size;
    size -= chunk_size;
  }

  if (g_end_size > 0 && seg < N_ADMA_DESCS) {
    if (seg != 0)
      g_adma_descs[seg - 1].next = &g_adma_descs[seg];

    g_adma_descs[seg].addr = g_end_tmp_buf;
    g_adma_descs[seg].next = NULL;
    g_adma_descs[seg].size = g_end_size >> 2;
    g_adma_descs[seg].attr = ms_get_attr(g_end_size);
    seg++;
  }

  if (seg >= N_ADMA_DESCS)
    return -1;

  if (seg != 0)
    g_adma_descs[seg - 1].attr &= ~0x4000;

  return seg;
}

int ms_read_sector(void *buf, int sector, int count) {
  int res;

  res = sceMsifPrepareDmaTable(buf, count * MS_SECTOR_SIZE, 0);
  if (res < 0)
    return res;

  res = msif_read_sector(sector, count, g_adma_descs);
  if (res < 0)
    return res;

  if (g_start_size > 0)
    memcpy(g_start_addr, g_start_tmp_buf, g_start_size);
  if (g_end_size > 0)
    memcpy(g_end_addr, g_end_tmp_buf, g_end_size);

  return res;
}

int ms_write_sector(const void *buf, int sector, int count) {
  int res;

  res = sceMsifPrepareDmaTable((void *)buf, count * MS_SECTOR_SIZE, 1);
  if (res < 0)
    return res;

  res = msif_write_sector(sector, count, g_adma_descs);
  if (res < 0)
    return res;

  return res;
}
