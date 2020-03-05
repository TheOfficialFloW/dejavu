/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#define REG8(addr)  *(volatile uint8_t *)(addr)
#define REG16(addr) *(volatile uint16_t *)(addr)
#define REG32(addr) *(volatile uint32_t *)(addr)

#define dmb() __asm__ volatile("dmb\n\t")
#define dsb() __asm__ volatile("dsb\n\t")

#endif
