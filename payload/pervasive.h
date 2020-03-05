/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __PERVASIVE_H__
#define __PERVASIVE_H__

uint32_t pervasive_read_misc(uint32_t offset);

void pervasive_clock_enable_gpio(void);
void pervasive_reset_exit_gpio(void);

void pervasive_clock_enable_spi(int bus);
void pervasive_clock_disable_spi(int bus);
void pervasive_reset_exit_spi(int bus);

void pervasive_clock_enable_msif(void);
void pervasive_clock_disable_msif(void);
void pervasive_reset_exit_msif(void);
void pervasive_reset_enter_msif(void);

int pervasive_msif_get_card_insert_state(void);
uint32_t pervasive_msif_unk(void);
void pervasive_msif_set_clock(uint32_t clock);

#endif
