/** @file
  [rodin 2026-11-14] Publish the DRAM above 4 GiB to DXE.

  The phone has 12 GB (single bank 0x40000000-0x33FFFFFFF, live-confirmed);
  the PEI memory map stops at the 0xC0000000 ceiling and the remaining ~8 GiB
  was unreachable, so DarwinBoot could never satisfy the window its ADT asks
  for (largest low run is Free 03, 484 MiB at 0x48A00000).

  Adding the ranges to MemoryMapLib made the firmware fault on rothko instead:
  MemoryPeim() builds the translation tables once, in PEI, from a fixed stack
  array.  Here the ranges are added after DXE is running, through
  gDS->AddMemorySpace() (GCD plus UEFI memory map) and the CPU architectural
  protocol's SetMemoryAttributes(), which grows the translation tables on
  demand.  ARM_MMU maps UC first, then the range is switched to write-back.

  Ranges come from /proc/iomem (System RAM) on the running device
  (docs/device-proc-iomem.txt, 2026-11-14):
    - 0x100100000-0x1BFFEFFFF (~3.00 GiB; 0x100000000-0x100FFFFFF is a stock
      16 MiB reserve, the framebuffer carve below 4 GiB stays in MemoryMapLib)
    - 0x1C0000000-0x314507FFF (~5.32 GiB; the top 0x314508000-0x3257FFFFF is
      stock TEE/dmabuf reserve, 0x325800000-0x33FFEFFFF is fully reserved)

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>

#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/Cpu.h>

typedef struct {
  UINT64  Base;
  UINT64  Size;
} ROTHKO_HIGH_RANGE;

STATIC CONST ROTHKO_HIGH_RANGE  mRanges[] = {
  { 0x0000000100100000ULL, 0x00000000BFEF0000ULL },   //  ~3.00 GiB  100100000-1bffeffff
  { 0x00000001C0000000ULL, 0x0000000154508000ULL }    //  ~5.32 GiB  1c0000000-314507fff
};

//
// [k70u 2026-11-04] Everything must stay inside the 40-bit physical address
// space the firmware actually reports (ArmGetPhysicalAddressBits() = 40).  The
// first version of this table had the last entry 16x too large, which put the
// range end at 0x90FFD0000; the moment that address was touched the firmware
// died with an address size fault at FAR 0x90FFAF000.
//
#define ROTHKO_PA_LIMIT  0x10000000000ULL
#define ROTHKO_BLOCK     0x1000ULL

#define ROTHKO_HIGH_COUNT  (sizeof (mRanges) / sizeof (mRanges[0]))

EFI_STATUS
EFIAPI
RodinHighMemEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS             Status;
  EFI_CPU_ARCH_PROTOCOL  *Cpu;
  UINTN                  Index;
  UINT64                 TotalAdded;

  (VOID)ImageHandle;
  (VOID)SystemTable;

  Status = gBS->LocateProtocol (&gEfiCpuArchProtocolGuid, NULL, (VOID **)&Cpu);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ROTHKO-HIMEM: no CPU arch protocol (%r), skipped\n", Status));
    return EFI_SUCCESS;
  }

  TotalAdded = 0;
  for (Index = 0; Index < ROTHKO_HIGH_COUNT; Index++) {
    UINT64  Base = mRanges[Index].Base;
    UINT64  Size = mRanges[Index].Size;

    if (((Base | Size) & (ROTHKO_BLOCK - 1)) != 0) {
      DEBUG ((DEBUG_ERROR, "ROTHKO-HIMEM: skip 0x%lx+0x%lx (not page aligned)\n", Base, Size));
      continue;
    }

    if (((Base + Size) < Base) || ((Base + Size) > ROTHKO_PA_LIMIT)) {
      DEBUG ((DEBUG_ERROR, "ROTHKO-HIMEM: skip 0x%lx+0x%lx (outside 40-bit PA, end 0x%lx)\n",
              Base, Size, Base + Size));
      continue;
    }

    //
    // Publish to the GCD and the UEFI memory map.  Accept every capability;
    // the mapping itself is set below.
    //
    Status = gDS->AddMemorySpace (
                    EfiGcdMemoryTypeSystemMemory,
                    Base,
                    Size,
                    EFI_MEMORY_UC | EFI_MEMORY_WC | EFI_MEMORY_WT |
                    EFI_MEMORY_WB | EFI_MEMORY_WP | EFI_MEMORY_XP
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "ROTHKO-HIMEM: AddMemorySpace 0x%lx+0x%lx failed %r\n", Base, Size, Status));
      continue;
    }

    //
    // Map it.  SetMemoryAttributes builds the translation table entries on
    // demand, which is what the PEI path could not do for a second batch.
    //
    Status = Cpu->SetMemoryAttributes (Cpu, Base, Size, EFI_MEMORY_WB);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "ROTHKO-HIMEM: SetMemoryAttributes 0x%lx+0x%lx failed %r\n", Base, Size, Status));
      gDS->RemoveMemorySpace (Base, Size);
      continue;
    }

    gDS->SetMemorySpaceAttributes (Base, Size, EFI_MEMORY_WB);

    TotalAdded += Size;
    DEBUG ((DEBUG_ERROR, "ROTHKO-HIMEM: added 0x%lx + 0x%lx (%lu MiB)\n",
            Base, Size, Size >> 20));
  }

  DEBUG ((DEBUG_ERROR, "ROTHKO-HIMEM: total added %lu MiB\n", TotalAdded >> 20));
  return EFI_SUCCESS;
}
