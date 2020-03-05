/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __MS_H__
#define __MS_H__

#define MS_SECTOR_SIZE 512

int ms_read_sector(void *buf, int sector, int count);
int ms_write_sector(const void *buf, int sector, int count);

#endif
