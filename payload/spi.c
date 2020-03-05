/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>

#include "spi.h"
#include "pervasive.h"
#include "utils.h"

#define SPI_BASE_ADDR 0xe0a00000
#define SPI_REGS(i) ((void *)(SPI_BASE_ADDR + (i) * 0x10000))

int spi_init(int bus) {
  volatile uint32_t *spi_regs = SPI_REGS(bus);

  pervasive_clock_enable_spi(bus);
  pervasive_reset_exit_spi(bus);

  if (bus == 2) {
    spi_regs[2] = 0x30001;
    spi_regs[5] = 0xf;
    spi_regs[3] = 3;
  }

  spi_regs[8] = 0;
  spi_regs[8];

  dsb();

  return 0;
}

void spi_write_start(int bus) {
  volatile uint32_t *spi_regs = SPI_REGS(bus);

  // Flush pending data to be read from the FIFO
  while (spi_regs[0xa])
    spi_regs[0];

  spi_regs[0xb];
  spi_regs[9] = 0x600;
}

void spi_write_end(int bus) {
  volatile uint32_t *spi_regs = SPI_REGS(bus);
  spi_regs[2] = 0;
  spi_regs[4] = 1;
  spi_regs[4];
  dsb();
}

void spi_write(int bus, uint32_t data) {
  volatile uint32_t *spi_regs = SPI_REGS(bus);
  spi_regs[1] = data;
}

int spi_read_available(int bus) {
  volatile uint32_t *spi_regs = SPI_REGS(bus);
  return spi_regs[0xa];
}

int spi_read(int bus) {
  volatile uint32_t *spi_regs = SPI_REGS(bus);
  return spi_regs[0];
}

void spi_read_end(int bus) {
  volatile uint32_t *spi_regs = SPI_REGS(bus);
  spi_regs[4] = 0;
  spi_regs[4];
  dsb();
}
