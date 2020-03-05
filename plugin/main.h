/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __MAIN_H__
#define __MAIN_H__

#include <psp2kern/kernel/sysmem.h>
#include "../dejavu.h"

int ksceRtcGetCurrentClock(SceDateTime *time);

int module_get_export_func(SceUID pid, const char *modname, uint32_t libnid,
                           uint32_t funcnid, uintptr_t *func);
int module_get_offset(SceUID pid, SceUID modid, int segidx,
                      size_t offset, uintptr_t *addr);

extern int (* _ksceKernelFindClassByName)(const char *name, SceClass **cls);
extern int (* _ksceGUIDGetUIDVectorByClass)(SceClass *cls, int vis_level,
                                            SceUID *vector, int num, int *ret_num);

#endif
