/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __MSIF_H__
#define __MSIF_H__

#define MSIF_REG 0xe0900000

#define MSIF_REG_FIFO   REG32(MSIF_REG + 0x34)
#define MSIF_REG_STATUS REG16(MSIF_REG + 0x38)
#define MSIF_REG_SYSTEM REG16(MSIF_REG + 0x3c)

#define MS_SYS_PAR4   0x00
#define MS_SYS_PAR8   0x40
#define MS_SYS_SERIAL 0x80

#define MS_TPC_READ_LONG_DATA   0x2000
#define MS_TPC_READ_SHORT_DATA  0x3000
#define MS_TPC_READ_REG         0x4000
#define MS_TPC_GET_INT          0x7000
#define MS_TPC_SET_RW_REG_ADRS  0x8000
#define MS_TPC_EX_SET_CMD       0x9000
#define MS_TPC_WRITE_REG        0xb000
#define MS_TPC_WRITE_SHORT_DATA 0xc000
#define MS_TPC_WRITE_LONG_DATA  0xd000
#define MS_TPC_SET_CMD          0xe000

#define MSIF_STATUS_TIMEOUT   0x0100
#define MSIF_STATUS_CRC_ERROR 0x0200
#define MSIF_STATUS_READY     0x1000
#define MSIF_STATUS_UNK1      0x2000
#define MSIF_STATUS_FIFO_RW   0x4000

#define MS_PARAM_REG_SYS_PARAM 0x10
#define MS_PARAM_REG_TPC_PARAM 0x17

#define MSPRO_CMD_READ_DATA  0x20
#define MSPRO_CMD_WRITE_DATA 0x21

#define EIO       3
#define ETIMEDOUT 4

typedef struct SceMsifAdmaDescriptor {
  void *addr;
  struct SceMsifAdmaDescriptor *next;
  uint16_t size;
  uint16_t attr;
} SceMsifAdmaDescriptor;

int SceMsifSmshc(void);

int msif_read_sector(int sector, int count, SceMsifAdmaDescriptor *desc);
int msif_write_sector(int sector, int count, SceMsifAdmaDescriptor *desc);

int msif_send_request(uint32_t cmd, uint32_t size, void *buf, int timeout);
int msif_recv_response(uint32_t cmd, uint32_t size, void *buf, int timeout);

int msproal_start(void);
int msproal_mount(void);

#endif
