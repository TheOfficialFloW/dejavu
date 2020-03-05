/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __FS_H__
#define __FS_H__

typedef struct {
  void *fnode;
  int type;
  uint64_t Position;
  void *alignedBuf;
  void *buf;
  int Cluster;
  int ClusterIndex; // index
  int unk_20;
  int unk_24;
  uint64_t CacheIndex;
  int CachedCluster;
  int unk_34;
} FileCache;

typedef struct {
  int unk_00;
  int unk_04;
  char path[0x208];
  char path2[0x208];
  int unk_418;
  int isRoot;
} SearchFile;

void set_ignore_writedevice(void);
void clear_ignore_writedevice(void);

void update_filesystem(void);

void get_iofilemgr_offsets(void);
void patch_exfatfs(void);

#endif
