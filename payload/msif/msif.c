/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>

#include "main.h"
#include "utils.h"
#include "msif.h"
#include "msif_auth.h"
#include "msif_instr.h"

static uint64_t g_timer_start_time = 0, g_timer_wait_time = 0;
static uint32_t g_status = 0;

static void msif_timer_start(int ms) {
  g_timer_start_time = ksceKernelGetSystemTimeWide();
  g_timer_wait_time = ms * 1000;
}

static void msif_timer_end(void) {
  g_timer_start_time = 0;
  g_timer_wait_time = 0;
}

static int msif_timer_expired(void) {
  if (ksceKernelGetSystemTimeWide() >= (g_timer_start_time + g_timer_wait_time))
    return -104;
  return 0;
}

int SceMsifSmshc(void) {
  uint32_t status;

  if (REG16(MSIF_REG + 0x00) & 4) {
    REG16(MSIF_REG + 0x00) |= 8;
    g_status |= 0x20000000;
  } else {
    status = MSIF_REG_STATUS;
    MSIF_REG_STATUS = status; // interrupt acknowledge
    g_status |= status;
  }

  return -1;
}

static int msproal_user_poll_flg(uint32_t flags) {
  SceMsifSmshc();
  if (g_status & flags)
    return 0;
  return -1;
}

static int msproal_user_wait_flg(uint32_t flags, int ms) {
  uint64_t start_time = ksceKernelGetSystemTimeWide();
  while (ksceKernelGetSystemTimeWide() < (start_time + ms * 1000)) {
    if (!msproal_user_poll_flg(flags))
      return 0;
  }

  return -1;
}

static void msproal_user_clear_flg(uint32_t flags) {
  g_status &= flags;
}

static int msproal_user_get_flg(void) {
  return g_status;
}

static void msif_tpc(uint32_t cmd, uint32_t size) {
  REG16(MSIF_REG + 0x30) = cmd | (size & 0x7ff);
}

static int msif_fifo_read_64(uint32_t *buf) {
  while (!msif_timer_expired()) {
    if (MSIF_REG_STATUS & MSIF_STATUS_FIFO_RW) {
      buf[0] = MSIF_REG_FIFO;
      buf[1] = MSIF_REG_FIFO;
      return 0;
    }
    if (!msproal_user_poll_flg(MSIF_STATUS_TIMEOUT)) {
      msproal_user_clear_flg(~(MSIF_STATUS_READY | MSIF_STATUS_TIMEOUT));
      return -103;
    }
  }

  return -ETIMEDOUT;
}

static int msif_fifo_write_64(uint32_t *buf) {
  while (!msif_timer_expired()) {
    if (MSIF_REG_STATUS & MSIF_STATUS_FIFO_RW) {
      MSIF_REG_FIFO = buf[0];
      MSIF_REG_FIFO = buf[1];
      return 0;
    }
  }

  return -ETIMEDOUT;
}

static int msif_fifo_read(uint32_t size, void *buf) {
  uint32_t data[2];
  int res;

  while (size > 8) {
    res = msif_fifo_read_64(buf);
    if (res != 0)
      return res;
    buf += 8;
    size -= 8;
  }

  if (size > 0) {
    res = msif_fifo_read_64(data);
    if (res != 0)
      return res;
    memcpy(buf, data, size > 1 ? size : 1);
  }

  return 0;
}

static int msif_fifo_write(uint32_t size, void *buf) {
  uint32_t data[2];
  int res;

  while (size > 8) {
    res = msif_fifo_write_64(buf);
    if (res != 0)
      return res;
    buf += 8;
    size -= 8;
  }

  if (size > 0) {
    memset(data, 0, sizeof(data));
    memcpy(data, buf, size > 1 ? size : 1);
    res = msif_fifo_write_64(data);
    if (res != 0)
      return res;
  }

  return 0;
}

static int msif_tpc_read(uint32_t cmd, uint32_t size, void *buf, int timeout) {
  int res;

  for (int i = 4; i > 0; i--) {
    msproal_user_clear_flg(~0xffff);
    msif_tpc(cmd, size);
    msif_timer_start(timeout);
    res = msif_fifo_read(size, buf);
    msif_timer_end();
    if (res != 0) {
      if (res == -103)
        continue;
      return res;
    }
    if (msproal_user_wait_flg(MSIF_STATUS_READY | MSIF_STATUS_CRC_ERROR |
                              MSIF_STATUS_TIMEOUT, 1))
      return -ETIMEDOUT;
    msproal_user_clear_flg(~(MSIF_STATUS_FIFO_RW | MSIF_STATUS_READY |
                             MSIF_STATUS_CRC_ERROR | MSIF_STATUS_TIMEOUT));
    if (!(msproal_user_get_flg() & (MSIF_STATUS_CRC_ERROR |
                                    MSIF_STATUS_TIMEOUT)))
      return 0;
    if (msproal_user_get_flg() & MSIF_STATUS_TIMEOUT)
      return -103;
  }

  return -EIO;
}

static int msif_tpc_write(uint32_t cmd, uint32_t size, void *buf, int timeout) {
  int res;

  for (int i = 4; i > 0; i--) {
    msproal_user_clear_flg(~0xffff);
    msif_tpc(cmd, size);
    msif_timer_start(timeout);
    res = msif_fifo_write(size, buf);
    msif_timer_end();
    if (res != 0)
      return res;
    if (msproal_user_wait_flg(MSIF_STATUS_READY | MSIF_STATUS_CRC_ERROR |
                              MSIF_STATUS_TIMEOUT, 1))
      return -ETIMEDOUT;
    msproal_user_clear_flg(~(MSIF_STATUS_FIFO_RW | MSIF_STATUS_READY |
                             MSIF_STATUS_CRC_ERROR | MSIF_STATUS_TIMEOUT));
    if (!(msproal_user_get_flg() & (MSIF_STATUS_CRC_ERROR |
                                    MSIF_STATUS_TIMEOUT)))
      return 0;
    if (!(msproal_user_get_flg() & MSIF_STATUS_TIMEOUT))
      return -102;
  }

  return -EIO;
}

static int msif_set_rw_regs_adrs(uint8_t addr1, uint8_t size1,
                                 uint8_t addr2, uint8_t size2) {
  uint8_t buf[4];
  buf[0] = addr1;
  buf[1] = size1;
  buf[2] = addr2;
  buf[3] = size2;
  return msif_tpc_write(MS_TPC_SET_RW_REG_ADRS, sizeof(buf), buf, 1);
}

static int msif_set_cmd(uint8_t cmd) {
  return msif_tpc_write(MS_TPC_SET_CMD, sizeof(cmd), &cmd, 1);
}

static int msif_get_int(uint8_t *val) {
  return msif_tpc_read(MS_TPC_GET_INT, sizeof(*val), val, 1);
}

static int msif_read_reg(uint32_t size, void *buf) {
  return msif_tpc_read(MS_TPC_READ_REG, size, buf, 5);
}

static int msif_write_reg(uint32_t size, void *buf) {
  return msif_tpc_write(MS_TPC_WRITE_REG, size, buf, 10);
}

static int msif_read_short_data(uint32_t size, void *buf) {
  return msif_tpc_read(MS_TPC_READ_SHORT_DATA, size, buf, 1);
}

static int msif_write_short_data(uint32_t size, void *buf) {
  return msif_tpc_write(MS_TPC_WRITE_SHORT_DATA, size, buf, 1);
}

static int msif_read_addr(uint8_t addr, uint32_t size, void *buf) {
  int res;

  if (size > 0x100)
    return -1;

  res = msif_set_rw_regs_adrs(addr, size, 0x10, 0xf);
  if (res != 0)
    return res;

  return msif_read_reg(size, buf);
}

static int msif_write_addr(uint8_t addr, uint32_t size, void *buf) {
  int res;

  if (size > 0x100)
    return -1;

  res = msif_set_rw_regs_adrs(2, 1, addr, size);
  if (res != 0)
    return res;

  return msif_write_reg(size, buf);
}

static int mshc_instructions(uint16_t start, uint16_t end, uint16_t *instr) {
  if (end <= 0 || start < 0 || (start + end) > 0x100)
    return -1;

  REG16(MSIF_REG + 0x02) = start;

  for (int i = 0; i < end; i++)
    REG16(MSIF_REG + 0x20) = instr[i];

  return 0;
}

static int mshc_set_adma_desc(SceMsifAdmaDescriptor *desc, int port) {
  if (port == 1) {
    REG32(MSIF_REG + 0x80) = (uintptr_t)desc->addr;
    REG32(MSIF_REG + 0x84) = (uintptr_t)desc->next;
    REG32(MSIF_REG + 0x88) = (desc->attr << 16) | desc->size;
    return 0;
  } else if (port == 2) {
    REG32(MSIF_REG + 0x90) = (uintptr_t)desc->addr;
    REG32(MSIF_REG + 0x94) = (uintptr_t)desc->next;
    REG32(MSIF_REG + 0x98) = (desc->attr << 16) | desc->size;
    return 0;
  }

  return -1;
}

static void mshc_clear_adma_desc(void) {
  REG32(MSIF_REG + 0x88) = 0;
  REG32(MSIF_REG + 0x98) = 0;
}

static int mshc_wait(uint16_t unk) {
  uint16_t status;

  REG16(MSIF_REG + 0x02) = unk;
  REG16(MSIF_REG + 0x00) |= 1;
  msproal_user_clear_flg(~0x20000000);
  if (msproal_user_wait_flg(0x20000000, 20000))
    return -ETIMEDOUT;
  msproal_user_clear_flg(~0x20000000);

  status = REG16(MSIF_REG + 0x08);

  if (status & 0x40)
    return -(status & 0xf);

  if(!(status & 0x20)) {
    if (status & 0x80)
      return -EIO;
    return -ETIMEDOUT;
  }

  if ((REG16(MSIF_REG + 0x02) & 0xff) == 2)
    return -REG16(MSIF_REG + 0x16);

  if (((int)MSIF_REG_STATUS << 19) >= 0)
    return -ETIMEDOUT;

  return -EIO;
}

static int mshc_reset(void) {
  MSIF_REG_SYSTEM |= 0x8000;

  msif_timer_start(1);
  while (!msif_timer_expired()) {
    if (!(MSIF_REG_SYSTEM & 0x8000)) {
      msif_timer_end();
      MSIF_REG_SYSTEM = (MSIF_REG_SYSTEM & ~0x7) | 0x4005;
      return 0;
    }
  }

  msif_timer_end();
  return -ETIMEDOUT;
}

int msif_read_sector(int sector, int count, SceMsifAdmaDescriptor *desc) {
  int res;

  if (count == 0)
    return 0;
  if (count <= 0)
    return -1;

  res = mshc_instructions(0, sizeof(mshc_instr_read_sector) / sizeof(uint16_t),
                          mshc_instr_read_sector);
  if (res != 0)
    return res;

  REG16(MSIF_REG + 0x10) = count;
  REG16(MSIF_REG + 0x00) = 0xe0;
  REG32(MSIF_REG + 0x0c) &= ~0x300000;
  REG8(MSIF_REG + 0x24) = MSPRO_CMD_READ_DATA;
  REG8(MSIF_REG + 0x24) = 0;
  REG8(MSIF_REG + 0x24) = count;
  REG8(MSIF_REG + 0x24) = (sector >> 24) & 0xff;
  REG8(MSIF_REG + 0x24) = (sector >> 16) & 0xff;
  REG8(MSIF_REG + 0x24) = (sector >> 8) & 0xff;
  REG8(MSIF_REG + 0x24) = sector & 0xff;

  res = mshc_set_adma_desc(desc, 2);
  if (res != 0)
    return res;

  res = mshc_wait(0);
  mshc_clear_adma_desc();

  return res;
}

int msif_write_sector(int sector, int count, SceMsifAdmaDescriptor *desc) {
  int res;

  if (count == 0)
    return 0;
  if (count <= 0)
    return -1;

  res = mshc_instructions(0, sizeof(mshc_instr_write_sector) / sizeof(uint16_t),
                          mshc_instr_write_sector);
  if (res != 0)
    return res;

  REG16(MSIF_REG + 0x10) = count;
  REG16(MSIF_REG + 0x00) = 0x90;
  REG32(MSIF_REG + 0x0c) &= ~0x300000;
  REG8(MSIF_REG + 0x24) = MSPRO_CMD_WRITE_DATA;
  REG8(MSIF_REG + 0x24) = 0;
  REG8(MSIF_REG + 0x24) = count;
  REG8(MSIF_REG + 0x24) = (sector >> 24) & 0xff;
  REG8(MSIF_REG + 0x24) = (sector >> 16) & 0xff;
  REG8(MSIF_REG + 0x24) = (sector >> 8) & 0xff;
  REG8(MSIF_REG + 0x24) = sector & 0xff;

  res = mshc_set_adma_desc(desc, 2);
  if (res != 0)
    return res;

  res = mshc_wait(0);
  mshc_clear_adma_desc();

  return res;
}

static int msif_wait_unk1(int timeout, int mode) {
  if (msproal_user_wait_flg(MSIF_STATUS_UNK1, timeout)) {
    msproal_user_clear_flg(~MSIF_STATUS_UNK1);
    return -ETIMEDOUT;
  }
  return 0;
}

static int msif_wait_result(int timeout, uint8_t *status) {
  int res;

  res = msif_wait_unk1(timeout, 0);
  if (res != 0) {
    if (res == -ETIMEDOUT)
      return -EIO;
    return res;
  }

  *status = msproal_user_get_flg();

  if (*status & 0x41) {
    if (*status & 0x1)
      return -105;
    else
      return -106;
  } else if (*status & 0xa0) {
    return 0;
  } else {
    return -ETIMEDOUT;
  }
}

static int msif_execute_command(int cmd, int timeout) {
  int res;
  uint8_t status;

  res = msif_set_cmd(cmd);
  if (res != 0)
    return res;

  if (cmd == 0x3c)
    return 0;

  return msif_wait_result(timeout, &status);
}

int msif_set_short_data_size(uint32_t size) {
  uint8_t param;

  switch (size) {
    case 32:
      param = 0;
      break;
    case 64:
      param = 1;
      break;
    case 128:
      param = 2;
      break;
    case 256:
      param = 3;
      break;
    default:
      return -1;
  }

  return msif_write_addr(MS_PARAM_REG_TPC_PARAM, sizeof(param), &param);
}

int msif_send_request(uint32_t cmd, uint32_t size, void *buf, int timeout) {
  int res;
  uint8_t status;

  res = msif_set_short_data_size(size);
  if (res != 0)
    return res;

  res = msif_execute_command(cmd, timeout);
  if (res != 0)
    return res;

  res = msif_write_short_data(size, buf);
  if (res != 0)
    return res;

  return msif_wait_result(timeout, &status);
}

int msif_recv_response(uint32_t cmd, uint32_t size, void *buf, int timeout) {
  int res;
  uint8_t status;

  res = msif_set_short_data_size(size);
  if (res != 0)
    return res;

  res = msif_execute_command(cmd, timeout);
  if (res != 0)
    return res;

  res = msif_read_short_data(size, buf);
  if (res != 0)
    return res;

  return msif_wait_result(timeout, &status);
}

static int sub_81000A74(void) {
  uint32_t val;

  val = REG32(MSIF_REG + 0xa0);
  REG16(MSIF_REG + 0x04) |= 1;

  msif_timer_start(1);
  while (!msif_timer_expired()) {
    if (!(REG16(MSIF_REG + 0x04) & 1)) {
      msif_timer_end();
      REG32(MSIF_REG + 0xa0) = val;
      REG16(MSIF_REG + 0x04) |= 4;
      return 0;
    }
  }

  msif_timer_end();
  return -ETIMEDOUT;
}

static int sub_81000D0C(void) {
  int res;

  res = sub_81000A74();
  if (res != 0)
    return res;

  return mshc_reset();
}

static void msif_set_clock_for_bus_mode(int bus_mode) {
  int clock;

  if (bus_mode == 1)
    clock = 4;
  else if (bus_mode <= 4)
    clock = 5;
  else
    clock = 6;

  kscePervasiveMsifSetClock(clock);
}

static void msif_set_system_reg_and_bus_mode(int bus_mode) {
  uint16_t val;

  val = MSIF_REG_SYSTEM;

  if (bus_mode == 1)
    val = (val & ~0x40) | 0x80;
  else if (bus_mode == 5)
    val = (val & ~0x80) | 0x40;
  else
    val = val & ~(0x40 | 0x80);

  MSIF_REG_SYSTEM = val;

  msif_set_clock_for_bus_mode(bus_mode);
}

static int msif_set_bus_mode(int bus_mode) {
  int res;
  uint8_t sys_param;

  switch (bus_mode) {
    case 1:
      sys_param = MS_SYS_SERIAL;
      break;
    case 3:
      sys_param = MS_SYS_SERIAL | 8;
      break;
    case 4:
      sys_param = MS_SYS_PAR4;
      break;
    default:
      sys_param = MS_SYS_PAR8;
      break;
  }

  res = msif_set_rw_regs_adrs(2, 1, MS_PARAM_REG_SYS_PARAM, sizeof(sys_param));
  if (res != 0)
    return res;

  res = msif_write_reg(sizeof(sys_param), &sys_param);
  if (res != 0)
    return res;

  msif_set_system_reg_and_bus_mode(bus_mode);

  return 0;
}

static int msif_init(void) {
  msproal_user_clear_flg(0);
  msif_set_clock_for_bus_mode(1);
  return sub_81000D0C();
}

int msproal_start(void) {
  return msif_init();
}

int msproal_mount(void) {
  int res;

  MSIF_REG_SYSTEM &= ~0x2000;
  REG16(MSIF_REG + 0x00) = 0x60;

  res = mshc_instructions(0, sizeof(mshc_instr_mount) / sizeof(uint16_t),
                          mshc_instr_mount);
  if (res != 0)
    return res;

  res = mshc_wait(0);
  if (res != 0)
    return res;

  MSIF_REG_SYSTEM |= 0x2000;
  REG16(MSIF_REG + 0x28); // this read is required

  res = msif_set_bus_mode(4);
  if (res != 0)
    return res;

  res = msif_auth(NULL);
  if (res != 0)
    return res;

  return 0;
}
