/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>

#include "msif.h"
#include "msif_auth.h"

static const uint8_t msif_key[32] = {
  0xD4, 0x19, 0xA2, 0xEB, 0x9D, 0x61, 0xA5, 0x2F,
  0x4F, 0xA2, 0x8B, 0x27, 0xE3, 0x2F, 0xCD, 0xD7,
  0xE0, 0x04, 0x8D, 0x44, 0x3D, 0x63, 0xC9, 0x2C,
  0x0B, 0x27, 0x13, 0x55, 0x41, 0xD9, 0x2E, 0xC4
};

#define DMAC5_KEYSIZE 0x20

static void dmac5_set_key(int keyslot, const uint32_t *key) {
  uint32_t *keyring = (uint32_t *)(0xe04e0000 + keyslot * DMAC5_KEYSIZE);
  for (int i = 0; i < DMAC5_KEYSIZE/4; i++)
    keyring[i] = key[i];
}

static void dmac5_submit(uint32_t cmd, void *dst, const void *src,
                         int keyslot, void *iv, uint32_t len) {
  volatile uint32_t *device = (uint32_t *)0xe0410000;

  #define DMAC_COMMIT_WAIT \
    do { \
      device[10] = device[10]; \
      device[7] = 1; \
      while (device[9] & 1) \
        ; \
    } while (0)

  device[0] = (uintptr_t)src;
  device[1] = (uintptr_t)dst;
  device[2] = len;
  device[3] = cmd;
  device[4] = keyslot;
  device[5] = (uintptr_t)iv;
  // device[8] = 0;
  device[11] = 0xe070;
  device[12] = 0x700070;
  DMAC_COMMIT_WAIT;
  __asm__ volatile ("dmb sy");
  __asm__ volatile ("dsb sy");
}

static void dmac5_command(uint32_t cmd, void *dst, const void *src,
                          const void *key, void *iv, uint32_t len) {
  dmac5_set_key(0x1d, key);
  dmac5_submit(cmd, dst, src, 0x1d, iv, len);
}

static void aes_128_cbc_enc(void *dst, const void *src, const void *key,
                            void *iv, uint32_t size) {
  dmac5_command(0x109, dst, src, key, iv, size);
}

static void des3_cbc_cts_enc(void *dst, const void *src, const void *key,
                      void *iv, uint32_t size) {
  dmac5_command(0x241, dst, src, key, iv, size);
}

static void get_random_data(void *data, uint32_t size) {
  memset(data, 0, size);
}

static int call_get_key_master_gen_no(uint32_t *res) {
  *res = 0;
  return 0;
}

static int call_set_ind_key(const uint8_t seed[32], uint8_t out_key[32]) {
  uint8_t zero_iv[16];
  uint8_t seed_trunc[16];

  memcpy(seed_trunc, seed, 16);

  memset(zero_iv, 0, 16);
  aes_128_cbc_enc(&out_key[0], seed_trunc, &msif_key[0], zero_iv, 16);

  memset(zero_iv, 0, 16);
  aes_128_cbc_enc(&out_key[16], seed_trunc, &msif_key[16], zero_iv, 16);

  return 0;
}

uint64_t be_uint64_t_load(const void *addr) {
  return __builtin_bswap64(*(uint64_t *)addr);
}

void be_uint64_t_store(void *addr, uint64_t val) {
  *(uint64_t *)addr = __builtin_bswap64(val);
}

static int msif_auth_derive_iv_tweak(const uint8_t *tweak_seed,
                                     uint8_t *tweak_key0, uint8_t *tweak_key1) {
  uint64_t tmp;

  tmp = be_uint64_t_load(tweak_seed) * 2;
  be_uint64_t_store(tweak_key0, tmp);
  tweak_key0[7] = ((tweak_seed[0] & 0x80) > 0) ? (tweak_key0[7] ^ 0x1b) :
                                                  tweak_key0[7];

  tmp = be_uint64_t_load(tweak_key0) * 2;
  be_uint64_t_store(tweak_key1, tmp);
  tweak_key1[7] = ((tweak_key0[0] & 0x80) > 0) ? (tweak_key1[7] ^ 0x1b) :
                                                  tweak_key1[7];

  return 0;
}

static int msif_auth_derive_iv(void *result, const void *data,
                               const void *key, uint32_t size) {
  uint8_t tweak_seed_enc[8];
  uint8_t tweak_seed[8];
  uint8_t IV_zero[8];

  memset(IV_zero, 0, 8);

  memset(tweak_seed, 0, sizeof(tweak_seed));
  des3_cbc_cts_enc(tweak_seed_enc, tweak_seed, key, IV_zero, 8);

  uint32_t tweak_key0[2];
  uint32_t tweak_key1[2];
  msif_auth_derive_iv_tweak(tweak_seed_enc, (uint8_t *)tweak_key0,
                            (uint8_t *)tweak_key1);
  if (size <= 8)
    return -1;

  const uint32_t *current_ptr = data;
  uint32_t current_size = size;

  uint32_t IV[2];
  memset(IV, 0, sizeof(IV));

  while (current_size > 8) {
    int current_round[2];
    current_round[0] = current_ptr[0] ^ IV[0];
    current_round[1] = current_ptr[1] ^ IV[1];

    des3_cbc_cts_enc(IV, current_round, key, IV_zero, 8);

    current_ptr = current_ptr + 2;
    current_size = current_size - 8;
  }

  uint32_t IV_mod[2];
  uint32_t tail_data[2];

  if (current_size == 8) {
    tail_data[0] = current_ptr[0];
    tail_data[1] = current_ptr[1];
    IV_mod[0] = IV[0] ^ tweak_key0[0];
    IV_mod[1] = IV[1] ^ tweak_key0[1];
  } else {
    tail_data[0] = current_ptr[0];
    tail_data[1] = current_ptr[1];
    memset(((char *)tail_data) + current_size, 0, 8 - current_size);
    IV_mod[0] = IV[0] ^ tweak_key1[0];
    IV_mod[1] = IV[1] ^ tweak_key1[1];
  }

  uint32_t final_round[2];
  final_round[0] = tail_data[0] ^ IV_mod[0];
  final_round[1] = tail_data[1] ^ IV_mod[1];

  des3_cbc_cts_enc(result, final_round, key, IV_zero, 8);

  return 0;
}

int msif_auth(void *mc_1C_key) {
  int res;
  uint8_t key_1C[32];
  uint8_t session_id[8];
  uint32_t f00d_cmd1_res;

  res = call_get_key_master_gen_no(&f00d_cmd1_res);
  if (res < 0)
    return res;

  get_random_data(session_id, sizeof(session_id));

  // Send request - establish session with memory card
  struct msif_auth_tpc_cmd48_req cmd48_req;
  memcpy(cmd48_req.session_id, session_id, sizeof(session_id));
  cmd48_req.f00d_cmd1_data = f00d_cmd1_res;
  memset(cmd48_req.reserved, 0, 0x17);
  res = msif_send_request(0x48, sizeof(cmd48_req), &cmd48_req, 1000);
  if (res != 0) {
    if (res == -105)
      return -201;
    return res;
  }

  // Get response - result of establishing session with memory card
  struct msif_auth_tpc_cmd49_resp cmd49_resp;
  memset(&cmd49_resp, 0, sizeof(cmd49_resp));
  msif_recv_response(0x49, sizeof(cmd49_resp), &cmd49_resp, 1000);

  // Execute f00d rm_auth cmd 0x2, scrambles and sets the key into slot 0x1C
  res = call_set_ind_key(cmd49_resp.f00d_1C_key, key_1C);
  if (res < 0)
    return res;

  // Prepare 3des-cbc-cts (dmac5 cmd 0x41 request) data
  struct msif_auth_dmac5_41_req1 d5req1;
  memcpy(d5req1.f00d_1C_key, cmd49_resp.f00d_1C_key, 0x10);
  memcpy(d5req1.card_info, cmd49_resp.card_info, 0x8);
  memcpy(d5req1.challenge, cmd49_resp.challenge, 0x8);
  memcpy(d5req1.session_id, session_id, 0x8);

  // Encrypt prepared buffer with 3des-cbc-cts and obtain IV
  char des3_iv_1[0x8];
  memset(des3_iv_1, 0, sizeof(des3_iv_1));
  res = msif_auth_derive_iv(des3_iv_1, &d5req1, key_1C, 0x28);
  if (res != 0)
    return res;

  // Verify that IV matches to the one, given in TPC 0x49 response
  if (memcmp(des3_iv_1, cmd49_resp.iv, 8) != 0)
    return -202;

  // Prepare 3des-cbc-cts (dmac5 cmd 0x41 request) data
  struct msif_auth_dmac5_41_req2 d5req2;
  memcpy(d5req2.session_id, d5req1.session_id, 8);
  memcpy(d5req2.challenge, cmd49_resp.challenge, 8);

  // Encrypt prepared buffer with 3des-cbc-cts and obtain IV
  char des3_iv_2[0x8];
  memset(des3_iv_2, 0, sizeof(des3_iv_2));
  res = msif_auth_derive_iv(des3_iv_2, &d5req2, key_1C, 0x10);
  if (res != 0)
    return res;

  // Send request - complete auth
  struct msif_auth_tpc_cmd4A_req cmd4A_req;
  memcpy(cmd4A_req.iv, des3_iv_2, 8);
  memset(cmd4A_req.reserved, 0, 0x18);
  res = msif_send_request(0x4a, sizeof(cmd4A_req), &cmd4A_req, 1000);
  if (res != 0) {
    if (res == -106)
      return -202;
    return res;
  }

  if (mc_1C_key)
    memcpy(mc_1C_key, cmd49_resp.f00d_1C_key, 0x10);

  return 0;
}
