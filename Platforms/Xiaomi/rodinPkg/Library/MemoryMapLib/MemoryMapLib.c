#include <Library/MemoryMapLib.h>

//
// [rodin 2026-11-14] CoreInitializeMemoryServices (Gcd.c) requires the resource
// descriptor HOB that covers the PHIT free range to carry EXACTLY
// PRESENT|INITIALIZED|TESTED after masking. SYS_MEM_TESTED additionally sets
// UNCACHEABLE/WC/WT/WB/EXECUTION_PROTECTED, so the strict equality failed and DXE
// asserted at Gcd.c(2448) with Found=0. Cacheability and XN keep coming from the
// last two table columns, which drive the MMU setup and are unaffected.
//
#define SYS_MEM_TESTED (PRESENT | INITIALIZED | TESTED)

//
// [rodin 2026-11-14] First-boot map, built from two sources:
//
//   1. The firmware-owned low extents are the structure rothko validated on the
//      same MTK generation during its own UniXNU boot (BootShim Source = where
//      LK loads the boot image, FD/Stack/Heap as below). Rodin's stock-boot
//      /proc/iomem (2026-11-14, docs/device-proc-iomem.txt) confirms the same
//      shape: System RAM from 0x40000000, LK DTB at 0x47C80000 (identical to
//      rothko's handoff address), and small runtime reserves that we hold back
//      out of caution.
//
//   2. Every free/reserved extent above 0x48080000 is transcribed from that
//      live /proc/iomem dump (System RAM spans minus their claimed children),
//      so the numbers are kernel-verified physical RAM, not guesses. The stock
//      kernel's own runtime claims (Kernel code/data) do not apply to a UniXNU
//      boot and are replaced by the firmware structures.
//
//   Everything above the 0xC0000000 ceiling rothko proved reachable stays
//   unmapped, as does every top-level reserved span (TEE/dmabuf pools between
//   the extents). Unlisted gaps cannot be corrupted by the firmware.
//
//   Handoff structures, live-confirmed on rodin:
//     - LK DTB 0x47C80000: stock iomem reserves 0x47C80000-0x47D02FFF, the same
//       base rothko's snapshot used.
//     - Incoming Initrd 0x66F00000: stock /chosen linux,initrd-start is exactly
//       0x66F00000. 0x5D00000 (93 MiB) fits between it and the next stock
//       reserve at 0x6CC00000; the UniXNU payload envelope (~60 MiB) fits.
//     - Display Reserved 0xFCC20000: stock /chosen atag,videolfb-fb_base_l/h =
//       0xFCC20000, vramSize 0x33D0000 (1220x2712 32bpp, panel
//       o10_42_02_0a_dsc_vdo_lcm_drv), ending just below 4 GiB.
//     - Pstore 0x48080000: stock iomem reserves 0x48080000-0x4809FFFF there.
//
STATIC EFI_MEMORY_REGION_DESCRIPTOR gMemoryDescriptor[] = {
  // Low free extent [0x40000000, 0x47C80000), split for firmware and handoff.
  {"BootShim Source", 0x40000000, 0x00201000, AddMem, SYS_MEM, SYS_MEM_TESTED, BsData, WRITE_BACK},
  {"Free Low A",      0x40201000, 0x001FF000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv,   WRITE_BACK_XN},
  {"UEFI FD",         0x40400000, 0x00200000, AddMem, SYS_MEM, SYS_MEM_TESTED, BsData, WRITE_BACK},
  {"UEFI Stack",      0x40600000, 0x00040000, AddMem, SYS_MEM, SYS_MEM_TESTED, BsData, WRITE_BACK_XN},
  {"Free Low B",      0x40640000, 0x00040000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv,   WRITE_BACK_XN},
  {"DXE Heap",        0x40680000, 0x01000000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv,   WRITE_BACK_XN},
  {"Free Low C1",     0x41680000, 0x00690000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv,   WRITE_BACK_XN},
  // Stock-boot runtime reserves inside the low extent (iomem "reserved"
  // children at 0x41d10000-0x4211ffff and 0x423c0000-0x4254ffff). Their
  // occupants (MCUPM/DRAMC-side buffers) may persist into a UniXNU boot, so
  // the firmware holds them instead of allocating over them.
  {"Low Reserve 1",   0x41D10000, 0x00410000, AddMem, MEM_RES, SYS_MEM_TESTED, Reserv, WRITE_BACK_XN},
  {"Free Low C2",     0x42120000, 0x002A0000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv,   WRITE_BACK_XN},
  {"Low Reserve 2",   0x423C0000, 0x00190000, AddMem, MEM_RES, SYS_MEM_TESTED, Reserv, WRITE_BACK_XN},
  {"Free Low C3",     0x425A0000, 0x056E0000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv,   WRITE_BACK_XN},
  //
  // LK hands the live device tree over in x0 and RodinDisplayDxe reads it
  // (panel geometry, atag,videolfb), so GCD must not hand this range out.
  // Reserved, like rothko's map.
  //
  {"LK DTB", 0x47C80000, 0x00400000, AddMem, MEM_RES, SYS_MEM_TESTED, Reserv, WRITE_BACK_XN},
  //
  // Rodin's pstore carve (stock iomem 0x48080000-0x4809FFFF, 128 KiB; rothko's
  // was 0x48090000/0xE0000). Uncached so the buffer survives a warm watchdog
  // reset for post-mortem reading from Android.
  //
  {"Pstore", 0x48080000, 0x00020000, AddMem, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},
  //
  // Remaining free extents, transcribed from the live iomem dump. The initrd
  // envelope (Incoming Initrd below) is carved out of the 0x48A00000 extent;
  // the stock top-level reserved spans in between (0x6CC00000-0x6F00FFFF,
  // 0x78000000-0x7992FFFF, 0x7AE00000-0x7DFFFFFF, 0x7FEF0000-0x800FFFFF,
  // 0x87E00000-0x89F1FFFF, 0x8A000000-0x8C55FFFF, 0x8ED60000-0x8F83FFFF,
  // 0x9F060000-0x9FFEffff, 0xB8...) stay unmapped.
  //
  {"Free 01", 0x480A0000, 0x00370000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv, WRITE_BACK_XN},
  {"Free 02", 0x48420000, 0x003E0000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv, WRITE_BACK_XN},
  {"Free 03", 0x48A00000, 0x1E500000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv, WRITE_BACK_XN},
  //
  // Kernel hand-off carrier. LK loads the boot image's ramdisk at
  // 0x66F00000 (rodin stock: /chosen linux,initrd-start is exactly that), and
  // this build puts the UniXNU payload volume in the ramdisk slot, so the
  // range is reserved up to the next stock reservation at 0x6CC00000.
  // Reserved, not BsData: UniXNURamDiskDxe reads the FAT volume from here and
  // the firmware must not hand the pages out and zero them.
  //
  {"Incoming Initrd",0x66F00000, 0x05D00000, AddMem, MEM_RES, SYS_MEM_TESTED, Reserv, WRITE_BACK_XN},
  {"Free 04", 0x6F010000, 0x08FF0000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv, WRITE_BACK_XN},
  {"Free 05", 0x79930000, 0x014D0000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv, WRITE_BACK_XN},
  {"Free 06", 0x7E800000, 0x01080000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv, WRITE_BACK_XN},
  {"Free 07", 0x7FE00000, 0x000F0000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv, WRITE_BACK_XN},
  {"Free 08", 0x80100000, 0x07D00000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv, WRITE_BACK_XN},
  {"Free 09", 0x89F20000, 0x000E0000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv, WRITE_BACK_XN},
  {"Free 10", 0x8C560000, 0x01AA0000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv, WRITE_BACK_XN},
  {"Free 11", 0x8E140000, 0x00C20000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv, WRITE_BACK_XN},
  {"Free 12", 0x8F840000, 0x000C0000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv, WRITE_BACK_XN},
  {"Free 13", 0x98000000, 0x07060000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv, WRITE_BACK_XN},
  {"Free 14", 0x9FFF0000, 0x18010000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv, WRITE_BACK_XN},
  {"Free 15", 0xB8A00000, 0x06600000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv, WRITE_BACK_XN},
  {"Free 16", 0xBFD00000, 0x000F0000, AddMem, SYS_MEM, SYS_MEM_TESTED, Conv, WRITE_BACK_XN},
  //
  // [rodin 2026-11-14] Above 0xC0000000 held back entirely, like rothko's
  // "top-region experiment": their DXE translation tables only covered the low
  // range and mapping higher faulted. Rodin's 12 GiB single-bank DRAM extends
  // to 0x33FEFFFFF (iomem System RAM at 0x100000000-0x1BFFEffff and
  // 0x1C0000000-0x3257fffff); RodinHighMemDxe handles the reach when it is
  // brought over, not the memory map.
  //
  //
  // Display Reserved is the framebuffer carveout LK hands over in
  // atag,videolfb: base 0xFCC20000, size 0x33D0000 (1220x2712, 32bpp, panel
  // o10_42_02_0a_dsc_vdo_lcm_drv), live-confirmed 2026-11-14 from
  // /proc/device-tree/chosen on the running device. In DSC+VDO mode the panel
  // keeps scanning out of this buffer, so the boot console (which draws into
  // it via FrameBufferSerialPortLib) is live with no display register writes.
  //
  {"Display Reserved", 0xFCC20000, 0x33D0000, AddMem, MEM_RES, SYS_MEM_TESTED, Reserv, WRITE_THROUGH},

  // FDT-described GICv3 register windows (stock DT interrupt-controller@c400000,
  // live iomem GICD 0c400000-0c43ffff / GICR 0c440000-0c63ffff).
  {"GIC Distributor",   0x0C400000, 0x00040000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},
  {"GIC Redistributors",0x0C440000, 0x00200000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},
  //
  // MTK watchdog registers, used to force a watchdog reset and to encode boot
  // stages (RothkoStage writes WDT_LENGTH at +4). Stock DT watchdog@1c00b000,
  // compatible "mediatek,mt6899-wdt" - same base as rothko.
  //
  {"Watchdog",        0x1C00B000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},
  //
  // MT6899 pinctrl / GPIO block (stock DT /soc/gpio@10005000, reg 0x10005000
  // + 0x1000). Volume Up on rodin is GPIO 196 (0xC4, active low) per the stock
  // DTBO fragment /volup-key, and GpioDxe finds this window by the name
  // "Pinctrl".
  //
  {"Pinctrl", 0x10005000, 0x1000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},
  //
  // SPMI PMIF controller (stock DT spmi@1cc04000). Register 0x070 bit0 is the
  // power-key event flag; reading it is enough to detect the key without an
  // SPMI transaction.
  //
  {"PMIF", 0x1CC04000, 0x1000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},
  //
  // Display plumbing for RodinDisplayDxe, verified against the MT6899 stock DT
  // (syscon@14000000, disp1-mutex0@14201000, syscon@14200000, disp1-dsc-wrap0
  // @14206000, dsi0@1420a000, syscon@14400000, syscon@14600000,
  // mipi-tx-config0@11e90000). Read-only probing in DSC+VDO mode; the driver
  // never writes display registers on this board.
  //
  {"Display MMSYS", 0x14000000, 0x1000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},
  {"MUTEX DISPSYS0", 0x14001000, 0x1000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},
  {"Display DISPSYS1", 0x14200000, 0x1000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},
  {"MUTEX DISPSYS1", 0x14201000, 0x1000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},
  {"DSC0", 0x14206000, 0x1000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},
  {"DSI0", 0x1420A000, 0x1000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},
  {"Display OVLSYS0", 0x14400000, 0x10000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},
  {"Display OVLSYS1", 0x14600000, 0x10000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},
  {"MIPI TX0", 0x11E90000, 0x1000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},
  //
  // UFS host controller windows, live-confirmed on 2026-11-14
  // (ufshci@112b0000 window 0x2a00 and ufsphy@112a0000 in the stock DT,
  // identical addresses to rothko). MtkUfsDxe needs all of these: the PHY and
  // the HCI for the transport, plus TOPCKGEN and INFRACFG_AO because the
  // driver gates the UFS clocks itself.
  //
  // Without these the very first register write (INFRA_AO2_CG_CLR at
  // 0x100010a8) takes a translation fault and the driver dies with
  // "Data abort: Translation fault, third level".
  //
  {"TOPCKGEN", 0x10000000, 0x1000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},
  {"INFRACFG AO", 0x10001000, 0x1000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},
  {"UFS PHY", 0x112A0000, 0x10000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},
  // 0x2a00 is the DT window size but is not page aligned, so the MMU
  // refuses it ("RegionStart ... are not page aligned"). Map 0x3000 and keep
  // the driver-side bound at 0x2a00.
  {"UFS HCI", 0x112B0000, 0x3000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},
  {"UFS AO", 0x112B8000, 0x1000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},
  {"UFS PDN", 0x112BB000, 0x1000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},
};

VOID
GetMemoryMap (
  OUT EFI_MEMORY_REGION_DESCRIPTOR **MemoryDescriptor,
  OUT UINT8                         *MemoryDescriptorCount
  )
{
  *MemoryDescriptor      = gMemoryDescriptor;
  *MemoryDescriptorCount = (UINT8)ARRAY_SIZE (gMemoryDescriptor);
}
