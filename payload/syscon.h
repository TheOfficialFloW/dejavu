/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __SYSCON_H__
#define __SYSCON_H__

void syscon_common_write(uint32_t data, uint16_t cmd, uint32_t length);
int syscon_init(void);
void syscon_msif_set_power(int enable);

#endif
