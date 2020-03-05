/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/io/fcntl.h>
#include <taihen.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "main.h"
#include "fs.h"

int ksceVfsNodeWaitEventFlag(void *vp);
int ksceVfsNodeSetEventFlag(void *vp);
int ksceVopInactive(void *vp);
int ksceVfsFreeVnode(void *vp);

static int (* UVFAT_Mount)(void *sharedResource, const char *assignName,
                           int mode, uint32_t *args);
static int (* UVFAT_ReadManagementArea)(void *sharedResource, void *fileSystem,
                                        int flags);
static int (* UVFAT_SearchFile_common)(void *sharedResource, void *file,
                                       void *fileSystem, void *fnode,
                                       void *parent_fnode);
static void *(* UVFAT_GetParentFnode)(void *sharedResource, void *fileSystem,
                                      const char *path);
static int (* UVFAT_ReleaseFnode)(void *sharedResource, void *fnode);
static int (* sceExfatfsVopUmount)(void *drive, int unk);
static int (* cleanup_cluster_cache_area)(void *drive);
static int (* invalidate_cluster_cache_data)(void *fnode);
static int (* FreeFpCache)(void *filep);
static int (* sfree)(void *p, int size);

static int (* clear_ncache)(void *nc, int unk);
static int (* invalidate_buff_cache)(void *file, void *vp);

static int (* utf16_to_utf8)(char *dst, int len, short *src);

static uint32_t *g_num_vnodes, *g_vnode_size;
static uintptr_t *g_vnodes;
static uintptr_t *g_mnt_head;

static void *g_shared_resource;

static int g_ignore_writedevice = 0;

static tai_hook_ref_t UVFAT_WriteDevice_Ref;

static void invalidate_file_caches(void) {
  uint32_t num = *(uint32_t *)(g_shared_resource + 0x295d5c);
  void *files = *(uintptr_t **)(g_shared_resource + 0x5cd0);
  for (int i = 0; i < num; i++) {
    FileCache *filep = (FileCache *)(files + 0x38 * i);
    if (filep->fnode) {
      FreeFpCache(filep);
      filep->Cluster = -2;
      filep->ClusterIndex = -1;
      filep->CacheIndex = -1;
      filep->CachedCluster = -2;
      filep->unk_34 = -1;
    }
  }
}

static int update_fnode(void *fnode) {
  int res;

  void *fileSystem = *(uintptr_t **)(fnode + 0x20c);
  if (!fileSystem)
    return -1;

  void *drive = *(uintptr_t **)(fileSystem + 0xac);

  res = ksceKernelLockFastMutex(drive + 0x1e0);
  if (res < 0)
    return res;

  static SearchFile file;
  static char utf8_path[0x104];
  memcpy(file.path, fnode, 0x208);
  utf16_to_utf8(utf8_path, sizeof(utf8_path), (short *)file.path);

  file.isRoot = *(uint16_t *)(file.path + 0x00) == '/' &&
                *(uint16_t *)(file.path + 0x02) == '\0';

  void **pFileSystem = fileSystem;
  void *temp = UVFAT_GetParentFnode(g_shared_resource, fileSystem, file.path);
  res = UVFAT_SearchFile_common(g_shared_resource, &file, &pFileSystem, fnode,
                                temp);
  UVFAT_ReleaseFnode(g_shared_resource, temp);

  invalidate_cluster_cache_data(fnode);
  *(uint32_t *)(fnode + 0x280) = -2;
  *(uint32_t *)(fnode + 0x284) = -2;
  *(uint32_t *)(fnode + 0x288) = 0;
  *(uint32_t *)(fnode + 0x28c) = 0;

  ksceKernelUnlockFastMutex(drive + 0x1e0);

  return res;
}

static void update_fnodes(void) {
  int num = *(int *)(g_shared_resource + 0x295d54);
  for (int i = 0; i < num; i++) {
    void *fnode = (void *)(g_shared_resource + 0x5cd8 + i * 0x290);
    update_fnode(fnode);
  }
}

static void update_vnodes(void) {
  int num = *g_num_vnodes;
  for (int i = 0; i < num; i++) {
    void *vp = (void *)(*g_vnodes + *g_vnode_size * i);
    void *nodeData = *(uintptr_t **)(vp + 0x48);
    uint32_t refCount = *(uint32_t *)(vp + 0x58);
    void *fnode = *(uintptr_t **)(vp + 0x60);

    if (fnode && fnode == nodeData) {
      static char utf8_path[0x104];
      utf16_to_utf8(utf8_path, sizeof(utf8_path), (short *)fnode);

      int res = update_fnode(fnode);
      if (res < 0) {
        if (refCount == 0) {
          void *nc = *(uintptr_t **)(vp + 0x70);
          if (nc)
            clear_ncache(nc, 0);
          ksceVopInactive(vp);
          ksceVfsFreeVnode(vp);
        }
      } else {
        *(uint64_t *)(vp + 0x80) = *(uint64_t *)(fnode + 0x218);
      }
    }
  }
}

static void invalidate_buff_caches(void) {
  int num = *g_num_vnodes;
  for (int i = 0; i < num; i++) {
    void *vp = (void *)(*g_vnodes + *g_vnode_size * i);
    void *file = *(uintptr_t **)(vp + 0x5c);
    if (file) {
      invalidate_buff_cache(file, vp);
      *(uint32_t *)(file + 0xc) = 8;
    }
  }
}

static void remount_filesystems(void) {
  for (int i = 0; i < 14; i++) {
    void *fileSystem = (void *)(g_shared_resource + 0x430 * i);
    uint32_t flags = *(uint32_t *)(fileSystem + 0xb0);
    void *drive = *(uintptr_t **)(fileSystem + 0xac);
    void *trash_fnode = *(uintptr_t **)(fileSystem + 0x114);
    if (drive) {
      if (ksceKernelLockFastMutex(drive + 0x1e0) >= 0) {
        char *assignName = *(char **)(drive + 0xc0);

        if (trash_fnode) {
          UVFAT_ReleaseFnode(g_shared_resource, trash_fnode);
          *(uintptr_t **)(fileSystem + 0x114) = NULL;
        }

        cleanup_cluster_cache_area(drive);

        if (*(uintptr_t **)(fileSystem + 0x12c)) {
          sfree(*(uintptr_t **)(fileSystem + 0x12c),
                *(uint32_t *)(fileSystem + 0x420) + 0x200);
          *(uint32_t *)(fileSystem + 0x128) = 0;
          *(uintptr_t **)(fileSystem + 0x12c) = NULL;
        }

        uint32_t args[2];
        args[0] = (uint32_t)drive;
        args[1] = flags;
        UVFAT_Mount(g_shared_resource, assignName, 1, args);

        ksceKernelUnlockFastMutex(drive + 0x1e0);
      }
    }
  }
}

void update_filesystem(void) {
  invalidate_buff_caches();
  invalidate_file_caches();
  remount_filesystems();
  update_vnodes();
  update_fnodes();
}

int UVFAT_WriteDevice_Patched(void *fileSystem, int unused, uint64_t sector,
                              int size, void *buf) {
  if (g_ignore_writedevice) {
    return 0;
  }

  return TAI_CONTINUE(int, UVFAT_WriteDevice_Ref, fileSystem, unused,
                                                  sector, size, buf);
}

void set_ignore_writedevice(void) {
  g_ignore_writedevice = 1;
}

void clear_ignore_writedevice(void) {
  g_ignore_writedevice = 0;
}

void get_iofilemgr_offsets(void) {
  tai_module_info_t info;

  memset(&info, 0, sizeof(tai_module_info_t));
  info.size = sizeof(tai_module_info_t);
  taiGetModuleInfoForKernel(KERNEL_PID, "SceIofilemgr", &info);

  switch (info.module_nid) {
    case 0x9642948C: // 3.60 retail
      module_get_offset(KERNEL_PID, info.modid, 0,
                        0x5a39, (uintptr_t *)&clear_ncache);
      module_get_offset(KERNEL_PID, info.modid, 0,
                        0x126f9, (uintptr_t *)&invalidate_buff_cache);
      break;

    case 0xA96ACE9D: // 3.65 retail
    case 0x3347A95F: // 3.67 retail
    case 0x90DA33DE: // 3.68 retail
      module_get_offset(KERNEL_PID, info.modid, 0,
                        0x3a11, (uintptr_t *)&clear_ncache);
      module_get_offset(KERNEL_PID, info.modid, 0,
                        0x106d1, (uintptr_t *)&invalidate_buff_cache);
      break;

    case 0xF16E72C7: // 3.69 retail
    case 0x81A49C2B: // 3.70 retail
    case 0xF2D59083: // 3.71 retail
    case 0x9C16D40A: // 3.72 retail
    case 0xF7794A6C: // 3.73 retail
      module_get_offset(KERNEL_PID, info.modid, 0,
                        0x3e69, (uintptr_t *)&clear_ncache);
      module_get_offset(KERNEL_PID, info.modid, 0,
                        0x10b29, (uintptr_t *)&invalidate_buff_cache);
      break;
  }

  module_get_offset(KERNEL_PID, info.modid, 1,
                    0x24, (uintptr_t *)&g_num_vnodes);
  module_get_offset(KERNEL_PID, info.modid, 1,
                    0x19dc, (uintptr_t *)&g_vnodes);
  module_get_offset(KERNEL_PID, info.modid, 1,
                    0x19e0, (uintptr_t *)&g_vnode_size);
  module_get_offset(KERNEL_PID, info.modid, 1,
                    0x19fc, (uintptr_t *)&g_mnt_head);
}

void patch_exfatfs(void) {
  tai_module_info_t info;

  memset(&info, 0, sizeof(tai_module_info_t));
  info.size = sizeof(tai_module_info_t);
  taiGetModuleInfoForKernel(KERNEL_PID, "SceExfatfs", &info);

  module_get_offset(KERNEL_PID, info.modid, 0,
                    0x7169, (uintptr_t *)&UVFAT_Mount);
  module_get_offset(KERNEL_PID, info.modid, 0,
                    0x13f01, (uintptr_t *)&UVFAT_ReadManagementArea);
  module_get_offset(KERNEL_PID, info.modid, 0,
                    0x10e39, (uintptr_t *)&UVFAT_SearchFile_common);
  module_get_offset(KERNEL_PID, info.modid, 0,
                    0x7a79, (uintptr_t *)&UVFAT_GetParentFnode);
  module_get_offset(KERNEL_PID, info.modid, 0,
                    0x7ae9, (uintptr_t *)&UVFAT_ReleaseFnode);
  module_get_offset(KERNEL_PID, info.modid, 0,
                    0x5f95, (uintptr_t *)&sceExfatfsVopUmount);
  module_get_offset(KERNEL_PID, info.modid, 0,
                    0x2695, (uintptr_t *)&cleanup_cluster_cache_area);
  module_get_offset(KERNEL_PID, info.modid, 0,
                    0x2191, (uintptr_t *)&invalidate_cluster_cache_data);
  module_get_offset(KERNEL_PID, info.modid, 0,
                    0x7ebd, (uintptr_t *)&FreeFpCache);
  module_get_offset(KERNEL_PID, info.modid, 0,
                    0x68a5, (uintptr_t *)&sfree);
  module_get_offset(KERNEL_PID, info.modid, 0,
                    0x19e1, (uintptr_t *)&utf16_to_utf8);

  module_get_offset(KERNEL_PID, info.modid, 1,
                    0x80, (uintptr_t *)&g_shared_resource);

  taiHookFunctionOffsetForKernel(KERNEL_PID, &UVFAT_WriteDevice_Ref, info.modid,
                                 0, 0x1315c, 1,  UVFAT_WriteDevice_Patched);

  // Allow UVFAT_ReadManagementArea
  uint32_t nop_nop_opcode = 0xbf00bf00;
  taiInjectDataForKernel(KERNEL_PID, info.modid, 0, 0x140fe,
                         &nop_nop_opcode, 4);
}
