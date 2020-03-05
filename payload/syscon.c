/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>

#include "syscon.h"
#include "pervasive.h"
#include "spi.h"
#include "gpio.h"
#include "utils.h"

#define SYSCON_TX_CMD_LO 0
#define SYSCON_TX_CMD_HI 1
#define SYSCON_TX_LENGTH 2
#define SYSCON_TX_DATA(i) (3 + (i))

#define SYSCON_RX_STATUS_LO 0
#define SYSCON_RX_STATUS_HI 1
#define SYSCON_RX_LENGTH 2
#define SYSCON_RX_RESULT 3

struct syscon_packet {
  uint8_t tx[32]; // tx[0..1] = cmd, tx[2] = length
  uint8_t rx[32]; // rx[0..1] = status, rx[2] = length, rx[3] = result
};

static void syscon_packet_start(struct syscon_packet *packet) {
  int i = 0;
  uint8_t cmd_size = packet->tx[2];
  uint8_t tx_total_size = cmd_size + 3;

  gpio_port_clear(0, GPIO_PORT_SYSCON_OUT);
  spi_write_start(0);

  do {
    spi_write(0, (packet->tx[i + 1] << 8) | packet->tx[i]);
    i += 2;
  } while (i < tx_total_size);

  spi_write_end(0);
  gpio_port_set(0, GPIO_PORT_SYSCON_OUT);
}

static uint8_t syscon_cmd_sync(struct syscon_packet *packet) {
  int i = 0;

  while (!gpio_query_intr(0, GPIO_PORT_SYSCON_IN));

  gpio_acquire_intr(0, GPIO_PORT_SYSCON_IN);

  while (spi_read_available(0)) {
    uint32_t data = spi_read(0);
    packet->rx[i] = data & 0xff;
    packet->rx[i + 1] = (data >> 8) & 0xff;
    i += 2;
  }

  spi_read_end(0);
  gpio_port_clear(0, GPIO_PORT_SYSCON_OUT);

  return packet->rx[SYSCON_RX_RESULT];
}

void syscon_common_read(uint32_t *buffer, uint16_t cmd) {
  struct syscon_packet packet;

  packet.tx[SYSCON_TX_CMD_LO] = cmd & 0xff;
  packet.tx[SYSCON_TX_CMD_HI] = (cmd >> 8) & 0xff;
  packet.tx[SYSCON_TX_LENGTH] = 1;

  memset(packet.rx, -1, sizeof(packet.rx));

  syscon_packet_start(&packet);
  syscon_cmd_sync(&packet);

  memcpy(buffer, &packet.rx[4], packet.rx[SYSCON_RX_LENGTH] - 2);
}

void syscon_common_write(uint32_t data, uint16_t cmd, uint32_t length) {
  int i;
  uint8_t hash, result;
  struct syscon_packet packet;

  packet.tx[SYSCON_TX_CMD_LO] = cmd & 0xff;
  packet.tx[SYSCON_TX_CMD_HI] = (cmd >> 8) & 0xff;
  packet.tx[SYSCON_TX_LENGTH] = length;

  packet.tx[SYSCON_TX_DATA(0)] = data & 0xff;
  packet.tx[SYSCON_TX_DATA(1)] = (data >> 8) & 0xff;
  packet.tx[SYSCON_TX_DATA(2)] = (data >> 16) & 0xff;
  packet.tx[SYSCON_TX_DATA(3)] = (data >> 24) & 0xff;

  // Calculate packet hash
  hash = 0;
  for (i = 0; i < length + 2; i++)
    hash += packet.tx[i];

  packet.tx[2 + length] = ~hash;
  memset(&packet.tx[3 + length], -1, sizeof(packet.rx) - (3 + length));

  do {
    memset(packet.rx, -1, sizeof(packet.rx));
    syscon_packet_start(&packet);

    result = syscon_cmd_sync(&packet);
  } while (result == 0x80 || result == 0x81);
}

int syscon_init(void) {
  uint32_t syscon_version;

  spi_init(0);

  gpio_set_port_mode(0, GPIO_PORT_SYSCON_OUT, GPIO_PORT_MODE_OUTPUT);
  gpio_set_port_mode(0, GPIO_PORT_SYSCON_IN, GPIO_PORT_MODE_INPUT);
  gpio_set_intr_mode(0, GPIO_PORT_SYSCON_IN, 3);

  syscon_common_read(&syscon_version, 1);

  if (syscon_version > 0x1000003)
    syscon_common_write(0x12, 0x80, 3);
  else if (syscon_version > 0x70501)
    syscon_common_write(2, 0x80, 3);

  return 0;
}

void syscon_msif_set_power(int enable) {
  syscon_common_write(enable, 0x89b, 2);
}
