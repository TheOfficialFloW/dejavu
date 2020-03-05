/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __SPI_H__
#define __SPI_H__

int spi_init(int bus);
void spi_write_start(int bus);
void spi_write_end(int bus);
void spi_write(int bus, uint32_t data);
int spi_read_available(int bus);
int spi_read(int bus);
void spi_read_end(int bus);

#endif
