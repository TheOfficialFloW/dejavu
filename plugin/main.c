/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/suspend.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/io/fcntl.h>
#include <psp2kern/io/dirent.h>
#include <psp2kern/io/stat.h>
#include <psp2kern/ctrl.h>
#include <psp2kern/power.h>
#include <psp2kern/syscon.h>
#include <taihen.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "main.h"
#include "fs.h"

extern int resume_function(void);

extern uint8_t _binary_payload_dejavu_payload_bin_start;
extern uint8_t _binary_payload_dejavu_payload_bin_size;

int (* _ksceKernelFindClassByName)(const char *name, SceClass **cls);
int (* _ksceGUIDGetUIDVectorByClass)(SceClass *cls, int vis_level,
                                     SceUID *vector, int num, int *ret_num);

static tai_hook_ref_t ksceSysconSendCommandRef;

static uint32_t *g_rtc_ctx;

uint32_t ori_resume_func_vaddr;

PayloadArguments pargs;
uint32_t pargs_size = sizeof(PayloadArguments);

static void *pa2va(uint32_t pa) {
  uint32_t va = 0;
  uint32_t vaddr;
  uint32_t paddr;
  uint32_t i;

  for (i = 0; i < 0x100000; i++) {
    vaddr = i << 12;
    __asm__ volatile ("mcr p15, 0, %1, c7, c8, 0\n\t"
                      "mrc p15, 0, %0, c7, c4, 0\n\t"
                      : "=r" (paddr) : "r" (vaddr));
    if ((pa & 0xfffff000) == (paddr & 0xfffff000)) {
      va = vaddr + (pa & 0xfff);
      break;
    }
  }

  return (void *)va;
}

static void map_memory(void) {
  uint32_t *ttb;
  uint32_t ttbcr;
  uint32_t ttbr0;
  uint32_t ttbr_mask[2];

  __asm__ volatile ("mrc p15, 0, %0, c2, c0, 2\n\t"
                    "mrc p15, 0, %1, c2, c0, 0\n\t"
                    : "=r"(ttbcr), "=r"(ttbr0));

  ttbr_mask[0] = 0xffffffff << (14 - (ttbcr & 0x7));
  ttbr_mask[1] = 0xffffffff << 14;

  ttb = (uint32_t *)pa2va(ttbr0 & ttbr_mask[0]);

  ttb[0x1c0] = 0x1c000000 | 0x91402;
  ttb[0x1f0] = 0x1f000000 | 0x91402;

  __asm__ volatile ("dmb sy");
  __asm__ volatile ("mcr p15, 0, %0, c7, c14, 1"
                    :: "r" (&ttb[0x1c0]) : "memory");
  __asm__ volatile ("mcr p15, 0, %0, c7, c14, 1"
                    :: "r" (&ttb[0x1f0]) : "memory");
  __asm__ volatile ("dsb sy\n\t"
                    "mcr p15, 0, r0, c8, c7, 0\n\t"
                    "dsb sy\n\t"
                    "isb sy" ::: "memory");
}

static int get_ttbs_and_titleids(void) {
  uint32_t ttbcr;
  uint32_t ttbr0;
  uint32_t ttbr_mask[2];
  SceUID vector[32];
  SceClass *cls;
  int i;
  int res;

  res = _ksceKernelFindClassByName("SceUIDProcessClass", &cls);
  if (res < 0)
    return res;

  res = _ksceGUIDGetUIDVectorByClass(cls, 8, vector,
                                     sizeof(vector) / sizeof(SceUID),
                                     (int *)&pargs.process_num);
  if (res < 0)
    return res;

  __asm__ volatile ("mrc p15, 0, %0, c2, c0, 2\n\t"
                    "mrc p15, 0, %1, c2, c0, 0\n\t"
                    : "=r"(ttbcr), "=r"(ttbr0));

  ttbr_mask[0] = 0xffffffff << (14 - (ttbcr & 0x7));
  ttbr_mask[1] = 0xffffffff << 14;

  for (i = 0; i < pargs.process_num; i++) {
    SceKernelProcessContext *ctx;
    ksceKernelGetPidContext(vector[i], &ctx);
    pargs.ttbs[i] = ctx->TTBR1 & ttbr_mask[1];

    ksceKernelGetProcessTitleId(vector[i], pargs.titleid[i], 32);
  }

  pargs.ttbs[i] = ttbr0 & ttbr_mask[0];

  return 0;
}

static void get_fattime(void) {
  SceDateTime time;
  ksceRtcGetCurrentClock(&time);
  pargs.fattime = ((time.year - 1980) << 25) |
                   (time.month  << 21) |
                   (time.day    << 16) |
                   (time.hour   << 11) |
                   (time.minute << 5)  |
                   (time.second << 0);
}

static int ksceSysconSendCommandPatched(int cmd, void *buffer, uint32_t size) {
  SceSysconResumeContext *resume_ctx;

  if (pargs.mode != DEJAVU_MODE_NONE) {
    if (cmd == SCE_SYSCON_CMD_RESET_DEVICE && size == sizeof(uintptr_t)) {
      if (*(uintptr_t *)buffer != 0) {
        resume_ctx = (SceSysconResumeContext *)pa2va(*(uintptr_t *)buffer);
        ori_resume_func_vaddr = resume_ctx->resume_func_vaddr;
        resume_ctx->resume_func_vaddr = (uint32_t)&resume_function;
      }
    }
  }

  return TAI_CONTINUE(int, ksceSysconSendCommandRef, cmd, buffer, size);
}

static int savestate_system_event(int resume, int eventid,
                                  void *args, void *opt) {
  if (pargs.mode != DEJAVU_MODE_NONE) {
    if (!resume) {
      if (eventid == 0x4000) {
        memcpy(pargs.rtc_ctx, g_rtc_ctx, SCE_RTC_CTX_SIZE);

        map_memory();
        get_fattime();

        if (pargs.mode == DEJAVU_MODE_SAVE) {
          get_ttbs_and_titleids();
        } else if (pargs.mode == DEJAVU_MODE_LOAD) {
          set_ignore_writedevice();
        }
      }
    } else {
      if (eventid == 0x10000) {
        memcpy(&pargs, (void *)PAYLOAD_ARGS_PADDR, sizeof(PayloadArguments));
        memcpy(g_rtc_ctx, pargs.rtc_ctx, SCE_RTC_CTX_SIZE);
      } else if (eventid == 0x100000) {
        if (pargs.mode == DEJAVU_MODE_LOAD) {
          update_filesystem();
          clear_ignore_writedevice();
        }

        pargs.mode = DEJAVU_MODE_NONE;
      }
    }
  }

  return 0;
}

static char *get_button_string(uint32_t buttons) {
  if (buttons & SCE_CTRL_SELECT)
    return "select";
  else if (buttons & SCE_CTRL_START)
    return "start";
  else if (buttons & SCE_CTRL_TRIANGLE)
    return "triangle";
  else if (buttons & SCE_CTRL_CIRCLE)
    return "circle";
  else if (buttons & SCE_CTRL_CROSS)
    return "cross";
  else if (buttons & SCE_CTRL_SQUARE)
    return "square";
  return NULL;
}

int (* prev_ctrlp_callback)(int port, SceCtrlData *pad_data, int unk);
static int ctrlp_callback(int port, SceCtrlData *pad_data, int unk) {
  if (pad_data->buttons & SCE_CTRL_PSBUTTON) {
    if (pad_data->buttons & (SCE_CTRL_L1 | SCE_CTRL_R1)) {
      char *button = get_button_string(pad_data->buttons);
      if (button) {
        snprintf(pargs.path, sizeof(pargs.path), "ux0:savestate/state_%s.bin",
                 button);
        if (pad_data->buttons & SCE_CTRL_L1) {
          pargs.mode = DEJAVU_MODE_LOAD;
        } else if (pad_data->buttons & SCE_CTRL_R1) {
          pargs.mode = DEJAVU_MODE_SAVE;
        }
        kscePowerRequestSoftReset();
      }

      pad_data->buttons &= ~SCE_CTRL_PSBUTTON;
    }
  }

  if (prev_ctrlp_callback)
    prev_ctrlp_callback(port, pad_data, unk);

  return 0;
}

static void get_sysmem_nids(void) {
  int res;

  res = module_get_export_func(KERNEL_PID, "SceSysmem",
                               0x63A519E5,
                               0x62989905,
                               (uintptr_t *)&_ksceKernelFindClassByName);
  if (res < 0)
    res = module_get_export_func(KERNEL_PID, "SceSysmem",
                                 0x02451F0F,
                                 0x7D87F706,
                                 (uintptr_t *)&_ksceKernelFindClassByName);

  res = module_get_export_func(KERNEL_PID, "SceSysmem",
                               0x63A519E5,
                               0xEC7D36EF,
                               (uintptr_t *)&_ksceGUIDGetUIDVectorByClass);
  if (res < 0)
    res = module_get_export_func(KERNEL_PID, "SceSysmem",
                                 0x02451F0F,
                                 0x52137FA3,
                                 (uintptr_t *)&_ksceGUIDGetUIDVectorByClass);
}

static void get_rtc_offsets(void) {
  tai_module_info_t info;

  memset(&info, 0, sizeof(tai_module_info_t));
  info.size = sizeof(tai_module_info_t);
  taiGetModuleInfoForKernel(KERNEL_PID, "SceRtc", &info);

  module_get_offset(KERNEL_PID, info.modid, 1, 0, (uintptr_t *)&g_rtc_ctx);
}

static void patch_ctrl(void) {
  uintptr_t *g_ctrlp_callback;
  tai_module_info_t info;

  memset(&info, 0, sizeof(tai_module_info_t));
  info.size = sizeof(tai_module_info_t);
  taiGetModuleInfoForKernel(KERNEL_PID, "SceCtrl", &info);

  module_get_offset(KERNEL_PID, info.modid, 1,
                    0xabc, (uintptr_t *)&g_ctrlp_callback);

  prev_ctrlp_callback = (void *)*g_ctrlp_callback;
  *g_ctrlp_callback = (uintptr_t)ctrlp_callback;
}

static void patch_syscon(void) {
  taiHookFunctionExportForKernel(KERNEL_PID,
                                 &ksceSysconSendCommandRef,
                                 "SceSyscon",
                                 TAI_ANY_LIBRARY,
                                 0xE26488B9,
                                 ksceSysconSendCommandPatched);
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize args, void *argp) {
  memset(&pargs, 0, sizeof(PayloadArguments));

  get_sysmem_nids();
  get_rtc_offsets();
  get_iofilemgr_offsets();
  patch_exfatfs();
  patch_syscon();
  patch_ctrl();

  ksceKernelRegisterSysEventHandler("savestate_system_event",
                                    savestate_system_event, NULL);

  return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize args, void *argp) {
  return SCE_KERNEL_STOP_CANCEL;
}