#ifndef RODIN_STAGE_H
#define RODIN_STAGE_H

#include <Base.h>

//
// [rodin 2026-11-14] Boot-stage beacon, self-contained on purpose.
//
// The rothko port kept this helper in its Mu_Basecore fork
// (MdePkg/Include/Library/RothkoStage.h); the UniXNU firmware build uses the
// stock mu_basecore checkout, so rodinPkg carries its own copy instead of
// depending on a fork-only header. The encoding is unchanged: write
// (8 + Stage) << 11 into WDT_LENGTH and then kick WDT_RESTART, so a bounded
// boot's reset timing reports how far the firmware got. Watchdog base
// 0x1C00B000 is live-confirmed on rodin (mediatek,mt6899-wdt).
//
STATIC inline VOID
RodinStage (
  UINT32  Stage
  )
{
  volatile UINT32  *Rgu = (volatile UINT32 *)(UINTN)0x1C00B000;

  Rgu[1] = ((8U + Stage) << 11) | 8U;
  __asm__ volatile ("dsb sy" ::: "memory");
  Rgu[2] = 0x1971;
  __asm__ volatile ("dsb sy" ::: "memory");
}

STATIC inline VOID
RodinStageFail (
  UINT32  Stage,
  UINT64  Status
  )
{
  UINT64  Beacon = 0x5354000000000000ULL | ((UINT64)Stage << 32) | (Status & 0xFFFFFFFFULL);

  __asm__ volatile ("msr daifset, #2\nmov x30, %0\n1: b 1b" :: "r"(Beacon) : "x30", "memory");
}

#endif
