/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#include <stdio.h>
#include <string.h>

#include "gpio.h"
#include "utils.h"

#define GPIO0_BASE_ADDR 0xe20a0000
#define GPIO1_BASE_ADDR 0xe0100000

#define GPIO_REGS(i) ((void *)((i) == 0 ? GPIO0_BASE_ADDR : GPIO1_BASE_ADDR))

void gpio_set_port_mode(int bus, int port, int mode) {
  volatile uint32_t *gpio_regs = GPIO_REGS(bus);
  gpio_regs[0] = (gpio_regs[0] & ~(1 << port)) | (mode << port);
  dmb();
}

int gpio_port_read(int bus, int port) {
  volatile uint32_t *gpio_regs = GPIO_REGS(bus);
  return (gpio_regs[1] >> port) & 1;
}

void gpio_port_set(int bus, int port) {
  volatile uint32_t *gpio_regs = GPIO_REGS(bus);
  gpio_regs[2] |= 1 << port;
  gpio_regs[0xd];
  dsb();
}

void gpio_port_clear(int bus, int port) {
  volatile uint32_t *gpio_regs = GPIO_REGS(bus);
  gpio_regs[3] |= 1 << port;
  gpio_regs[0xd];
  dsb();
}

void gpio_set_intr_mode(int bus, int port, int mode) {
  volatile uint32_t *gpio_regs = GPIO_REGS(bus);
  uint32_t reg = 5 + port / 15;
  uint32_t off = 2 * (port % 15);

  gpio_regs[reg] |= (gpio_regs[reg] & ~(3 << off)) | (mode << off);
  dmb();
}

int gpio_query_intr(int bus, int port) {
  volatile uint32_t *gpio_regs = GPIO_REGS(bus);
  return (1 << port) & ((gpio_regs[0x0e] & ~gpio_regs[0x07]) |
                        (gpio_regs[0x0f] & ~gpio_regs[0x08]) |
                        (gpio_regs[0x10] & ~gpio_regs[0x09]) |
                        (gpio_regs[0x11] & ~gpio_regs[0x0a]) |
                        (gpio_regs[0x12] & ~gpio_regs[0x0b]));
}

int gpio_acquire_intr(int bus, int port) {
  uint32_t ret;
  uint32_t mask = 1 << port;
  volatile uint32_t *gpio_regs = GPIO_REGS(bus);

  ret = mask & ((gpio_regs[0x0e] & ~gpio_regs[0x07]) |
                (gpio_regs[0x0f] & ~gpio_regs[0x08]) |
                (gpio_regs[0x10] & ~gpio_regs[0x09]) |
                (gpio_regs[0x11] & ~gpio_regs[0x0a]) |
                (gpio_regs[0x12] & ~gpio_regs[0x0b]));

  gpio_regs[0x0e] = mask;
  gpio_regs[0x0f] = mask;
  gpio_regs[0x10] = mask;
  gpio_regs[0x11] = mask;
  gpio_regs[0x12] = mask;
  dsb();

  return ret;
}
