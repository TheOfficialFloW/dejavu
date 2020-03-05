/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __MSIF_AUTH_H__
#define __MSIF_AUTH_H__

struct msif_auth_dmac5_41_req1 {
  uint8_t f00d_1C_key[0x10];
  uint8_t card_info[0x8];
  uint8_t challenge[0x8];
  uint8_t session_id[8];
} __attribute__((packed));

struct msif_auth_dmac5_41_req2 {
  uint8_t session_id[0x8];
  uint8_t challenge[0x8];
} __attribute__((packed));

struct msif_auth_tpc_cmd48_req {
  uint8_t session_id[0x8];
  uint8_t f00d_cmd1_data;
  uint8_t reserved[0x17];
} __attribute__((packed));

struct msif_auth_tpc_cmd49_resp {
  uint8_t f00d_1C_key[0x10];
  uint8_t card_info[0x8];
  uint8_t challenge[0x8];
  uint8_t iv[0x08];
  uint8_t reserved[0x18];
} __attribute__((packed));

struct msif_auth_tpc_cmd4A_req {
  uint8_t iv[0x8];
  uint8_t reserved[0x18];
} __attribute__((packed));

int msif_auth(void *mc_1C_key);

#endif
