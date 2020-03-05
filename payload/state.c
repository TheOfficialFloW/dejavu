/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "msif/ms.h"
#include "ff.h"

#define N_SEGMENTS 0x4000

static Segment segments[N_SEGMENTS];
static int n_segments = 0;

static char __attribute__((aligned(64))) mediaid[0x200];

static int compare_segment(const void *a, const void *b) {
  Segment *_a = (Segment *)a;
  Segment *_b = (Segment *)b;
  if (_a->addr < _b->addr) return -1;
  if (_a->addr > _b->addr) return 1;
  return 0;
}

static int is_segment_in_vram_dram(void *addr, uint32_t size) {
  if (addr >= (void *)0x20000000 && (addr + size) <= (void *)0x60000000) {
    return 1;
  }
  return 0;
}

static void add_segment(void *addr, uint32_t size) {
  if (n_segments < N_SEGMENTS) {
    segments[n_segments].addr = addr;
    segments[n_segments].size = size;
    n_segments++;
  }
}

static void l2_page_table(uint32_t *tbl) {
  for (int i = 0; i < 0x100; i++) {
    uint32_t entry = tbl[i];
    if ((entry & 0b11) == 0b00) {
      // Fault
      continue;
    } else {
      void *addr;
      uint32_t size;
      if ((entry & 0b11) == 0b01) {
        // Large page
        addr = (void *)(entry & 0xffff0000);
        size = 0x10000;
        i += 0xf;
      } else if ((entry & 0b10) == 0b10) {
        // Small page
        addr = (void *)(entry & 0xfffff000);
        size = 0x1000;
      }

      if (is_segment_in_vram_dram(addr, size))
        add_segment(addr, size);
    }
  }
}

static void l1_page_table(uint32_t *tbl) {
  for (int i = 0; i < 0x1000; i++) {
    uint32_t entry = tbl[i];
    if ((entry & 0b11) == 0b00) {
      // Fault
      continue;
    } else if ((entry & 0b11) == 0b01) {
      // Page table
      l2_page_table((uint32_t *)(entry & 0xfffffc00));
    } else if ((entry & 0b11) == 0b10) {
      void *addr;
      uint32_t size;
      if (entry & 0x40000) {
        // Supersection
        addr = (void *)(entry & 0xff000000);
        size = 0x1000000;
        i += 0xf;
      } else {
        // Section
        addr = (void *)(entry & 0xfff00000);
        size = 0x100000;
      }

      if (is_segment_in_vram_dram(addr, size))
        add_segment(addr, size);
    }
  }
}

static void coalesce_segments(void) {
  void *s, *e;
  int j = 0;

  qsort(segments, n_segments, sizeof(Segment), compare_segment);

  s = segments[0].addr;
  e = s + segments[0].size;

  for (int i = 1; i < n_segments; i++) {
    void *start = segments[i].addr;
    void *end = start + segments[i].size;

    if (start >= s && start <= e) {
      // if overlap or adjacent, choose max end
      e = end > e ? end : e;
    } else {
      if (j < N_SEGMENTS) {
        segments[j].addr = s;
        segments[j].size = e - s;
        j++;
      }
      s = start;
      e = end;
    }
  }

  if (j < N_SEGMENTS) {
    segments[j].addr = s;
    segments[j].size = e - s;
    j++;
  }

  n_segments = j;
}

static void get_dram_vram_segments(void) {
  for (int i = 0; i < pargs->process_num + 1; i++) {
    l1_page_table((uint32_t *)pargs->ttbs[i]);
  }

  coalesce_segments();
}

static int write_file_info(FIL *fpdst, FILINFO *fno, const char *path,
                           UINT path_len, int is_root) {
  SaveStateFile file;
  FRESULT res;
  UINT bw;
  UINT zero = 0;

  file.is_root  = is_root;
  file.fattrib  = fno->fattrib;
  file.ftime    = fno->ftime;
  file.fdate    = fno->fdate;
  file.data_len = fno->fsize;
  file.path_len = ALIGN(path_len, 4);

  res = f_write(fpdst, &file, sizeof(SaveStateFile), &bw);
  if (res != FR_OK)
    return res;

  res = f_write(fpdst, path, path_len, &bw);
  if (res != FR_OK)
    return res;

  res = f_write(fpdst, &zero, file.path_len - path_len, &bw);
  if (res != FR_OK)
    return res;

  return 0;
}

static int copy_file(FIL *fpdst, FIL *fpsrc, UINT size) {
  static BYTE __attribute__((aligned(64))) buf[0x4000];
  FRESULT res;
  UINT br, bw;

  while (size > 0) {
    res = f_read(fpsrc, buf, MIN(size, sizeof(buf)), &br);
    if (res != FR_OK)
      return res;

    res = f_write(fpdst, buf, br, &bw);
    if (res != FR_OK)
      return res;

    size -= br;
  }

  return 0;
}

static int backup_file(FIL *fpdst, const char *src) {
  FRESULT res;
  FIL fpsrc;
  UINT bw;
  UINT zero = 0;

  res = f_open(&fpsrc, src, FA_READ);
  if (res != FR_OK)
    return res;

  res = copy_file(fpdst, &fpsrc, f_size(&fpsrc));
  if (res != FR_OK)
    goto err;

  res = f_write(fpdst, &zero, ALIGN(f_tell(fpdst), 4) - f_tell(fpdst), &bw);
  if (res != FR_OK)
    goto err;

err:
  f_close(&fpsrc);
  return res;
}

static int backup_dir(FIL *fp, const char *path, int level, int *count) {
  static FILINFO fno;
  char new_path[512];
  FRESULT res;
  DIR dp;

  int len = strlen(path);

  res = f_stat(path, &fno);
  if (res != FR_OK)
    return res;

  res = f_opendir(&dp, path);
  if (res != FR_OK)
    return res;

  res = write_file_info(fp, &fno, path, len + 1, level == 0);
  if (res != FR_OK)
    goto err;

  strcpy(new_path, path);
  strcpy(new_path + len, "/");

  do {
    res = f_readdir(&dp, &fno);
    if (res != FR_OK || fno.fname[0] == 0)
      break;

    strcpy(new_path + len + 1, fno.fname);

    if (fno.fattrib & AM_DIR) {
      res = backup_dir(fp, new_path, level + 1, count);
    } else {
      res = write_file_info(fp, &fno, new_path, strlen(new_path) + 1, 0);
      if (res != FR_OK)
        goto err;

      res = backup_file(fp, new_path);
      if (res == FR_OK)
        (*count)++;
    }
  } while (res == FR_OK);

  if (res == FR_OK)
    (*count)++;

err:
  f_closedir(&dp);
  return res;
}

static int remove_dir(const char *path) {
  static FILINFO fno;
  char new_path[512];
  FRESULT res;
  DIR dp;

  int len = strlen(path);

  res = f_opendir(&dp, path);
  if (res != FR_OK)
    return res;

  strcpy(new_path, path);
  strcpy(new_path + len, "/");

  do {
    res = f_readdir(&dp, &fno);
    if (res != FR_OK || fno.fname[0] == 0)
      break;

    strcpy(new_path + len + 1, fno.fname);

    if (fno.fattrib & AM_DIR) {
      res = remove_dir(new_path);
    } else {
      res = f_unlink(new_path);
    }
  } while (res == FR_OK);

  if (res == FR_OK)
    res = f_unlink(path);

  f_closedir(&dp);
  return res;
}

static int restore_dir(FIL *fpsrc, int file_num) {
  static FILINFO fno;
  SaveStateFile file;
  char path[512];
  FRESULT res;
  FIL fpdst;
  UINT br;

  for (int i = 0; i < file_num; i++) {
    res = f_read(fpsrc, &file, sizeof(SaveStateFile), &br);
    if (res != FR_OK)
      return res;

    res = f_read(fpsrc, path, file.path_len, &br);
    if (res != FR_OK)
      return res;

    if (file.is_root) {
      res = remove_dir(path);
      if (res != FR_OK && res != FR_NO_PATH)
        return res;
    }

    if (file.fattrib & AM_DIR) {
      res = f_mkdir(path);
      if (res != FR_OK && res != FR_EXIST)
        return res;
    } else {
      res = f_open(&fpdst, path, FA_CREATE_ALWAYS | FA_WRITE);
      if (res != FR_OK)
        return res;

      res = copy_file(&fpdst, fpsrc, file.data_len);
      f_close(&fpdst);
      if (res != FR_OK)
        return res;

      res = f_lseek(fpsrc, ALIGN(f_tell(fpsrc), 4));
      if (res != FR_OK)
        return res;
    }

    fno.ftime = file.ftime;
    fno.fdate = file.fdate;
    f_utime(path, &fno);
    f_chmod(path, file.fattrib, 0xff);
  }

  return 0;
}

int save_state(const char *path) {
  SaveStateHeader header;
  FRESULT res;
  FIL fp;
  UINT bw;
  int sector;

  memset(&header, 0, sizeof(SaveStateHeader));
  header.magic   = SAVESTATE_MAGIC;
  header.version = SAVESTATE_VERSION;

  res = f_mkdir("ux0:savestate");
  if (res != FR_OK && res != FR_EXIST)
    return res;

  res = f_open(&fp, path, FA_CREATE_ALWAYS | FA_WRITE);
  if (res != FR_OK)
    goto err;

  res = f_lseek(&fp, sizeof(SaveStateHeader));
  if (res != FR_OK)
    goto err;

  for (int i = 0; i < pargs->process_num; i++) {
    if (pargs->titleid[i][0] != '\0' &&
        strncmp(pargs->titleid[i], "main", 4) != 0 &&
        strncmp(pargs->titleid[i], "NPXS", 4) != 0) {
      static char savedata_path[256];
      strcpy(savedata_path, "ux0:user/00/savedata/");
      strcpy(savedata_path + 21, pargs->titleid[i]);
      backup_dir(&fp, savedata_path, 0, (int *)&header.file_num);
    }
  }

  get_dram_vram_segments();

  add_segment((void *)MEDIAID_ADDR_MAGIC, 0x200);
  add_segment((void *)CONTEXT_PADDR, sizeof(SceSysconResumeContext));
  add_segment((void *)SPRAM_PADDR, 80 * 1024);

  res = f_write(&fp, segments, n_segments * sizeof(Segment), &bw);
  if (res != FR_OK)
    goto err;

  res = f_lseek(&fp, ALIGN(f_tell(&fp), 0x200));
  if (res != FR_OK)
    goto err;

  for (int i = 0; i < n_segments; i++) {
    void *addr = segments[i].addr;
    uint32_t size = segments[i].size;

    if (addr == MEDIAID_ADDR_MAGIC) {
      sector = get_mediaid_sector();
      ms_read_sector(mediaid, sector, 1);
      addr = mediaid;
    }

    res = f_write(&fp, addr, size, &bw);
    if (res != FR_OK)
      goto err;
  }

  header.segs_num = n_segments;

  // Write header
  res = f_rewind(&fp);
  if (res != FR_OK)
    goto err;

  res = f_write(&fp, &header, sizeof(SaveStateHeader), &bw);
  if (res != FR_OK)
    goto err;

err:
  f_close(&fp);
  return res;
}

int load_state(const char *path) {
  SaveStateHeader header;
  FRESULT res;
  FIL fp;
  UINT br;
  int sector;

  res = f_open(&fp, path, FA_READ);
  if (res != FR_OK)
    goto err;

  res = f_read(&fp, &header, sizeof(SaveStateHeader), &br);
  if (res != FR_OK)
    goto err;

  res = restore_dir(&fp, header.file_num);
  if (res != FR_OK)
    goto err;

  res = f_read(&fp, segments, header.segs_num * sizeof(Segment), &br);
  if (res != FR_OK)
    goto err;

  res = f_lseek(&fp, ALIGN(f_tell(&fp), 0x200));
  if (res != FR_OK)
    goto err;

  int is_mediaid = 0;
  for (int i = 0; i < header.segs_num; i++) {
    void *addr = segments[i].addr;
    uint32_t size = segments[i].size;

    if (addr == MEDIAID_ADDR_MAGIC) {
      is_mediaid = 1;
      addr = mediaid;
    }

    res = f_read(&fp, addr, size, &br);
    if (res != FR_OK)
      goto err;

    if (is_mediaid) {
      sector = get_mediaid_sector();
      ms_write_sector(addr, sector, 1);
      is_mediaid = 0;
    }
  }

err:
  f_close(&fp);
  return res;
}
