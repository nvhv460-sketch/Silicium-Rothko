# Rodin UEFI Platform

Device platform for the Xiaomi Redmi Turbo 4 (`rodin`, MT6899 / Dimensity
8400-Ultra).

`DeviceBuild.py` exposes this package to `stuart` and adds the sibling
`UniXNU/package` directory to the EDK II package search path. The platform
inherits reviewed MT6989/MT6899-generation paths from the rothko package; the
board values below were re-derived from live rodin hardware on 2026-11-14
(docs/device-proc-iomem.txt in UniXNU-rodin, /proc/device-tree, stock DTBO).

## Platform components

- `UniXNUComponents.dsc.inc` and `UniXNUComponents.fdf.inc` register the
  UniXNU application, registrar and initrd block driver.
- `rodin.dsc` and `rodin.fdf` build the 2 MiB firmware volume at
  `0x40400000`.
- `Drivers/RodinDisplayDxe` publishes the board display. Rodin's panel is
  `o10_42_02_0a_dsc_vdo_lcm_drv` (DSC + DSI video mode), so the GOP sits on the
  LK framebuffer (`0xFCC20000`, size `0x33D0000`) that the panel keeps
  scanning; the command-mode frame push validates itself at runtime and stays
  off on this board. No display register writes are performed.
- `Drivers/RodinHighMemDxe` publishes the 12 GiB high-memory ranges.
- `Drivers/RodinWatchdogDxe` handles the board watchdog (`0x1C00B000`).
- `Drivers/MtkUfsDxe` provides the bounded MT6989-generation UFS transport
  (`0x112b0000` HCI / `0x112a0000` PHY, identical to rothko's live values).
- `Library/MemoryMapLib` maps the live /proc/iomem free extents below
  `0xC0000000`, keeps LK's DTB (`0x47C80000`), pstore (`0x48080000`) and the
  LK initrd handoff (`0x66F00000`, 93 MiB) carved out, and excludes every
  stock top-level reserved span.
- `Library/KeypadDeviceLib` and `Library/GpioImplLib` provide the three-key
  physical input route (Volume Up via PMIC GPIO 196, Power/Down via MT6363
  PWRKEY/HOMEKEY).

## UFS policy

The current UFS interfaces have several write restrictions:

- the device refclk property is preserved;
- the device-reset and refclk SMC paths are disabled;
- PHY power-on programming is skipped before any MMIO write;
- the UFS Device Config protocol is not published to applications;
- the retained filter rejects FORMAT UNIT, WRITE(6/10/12/16), WRITE BUFFER,
  UNMAP and SECURITY PROTOCOL OUT; other opcodes are still forwarded;
- the published Block I/O interface is marked read-only.

The filter is not a complete write prohibition: WRITE SAME, for example, is
not rejected at this boundary. PreHce still sends RTFF SMC BIT9 off/on and
changes host HCI/options and link state despite the separate PHY/reset/refclk
switches. No same-run evidence establishes those paths caused BROM.

The unx18 FD build passed offline. That build does not qualify later source
edits. UFS command completion, DMA behavior, complete storage-command policy
and a post-BROM hardware run remain open.

## Device assumptions

The memory map and FD placement come from the OS3.0.303 FDT and live LK mblock
snapshot used during K70U bring-up. Re-capture and compare those inputs before
using this platform with another LK or Android release.

The v4 vendor-ramdisk prefix is measured as `0x02719743`. The initrd size PCD
must match the sector-aligned FAT image used by the matching device build.
Never expose the vendor ramdisk table or its envelope as a filesystem.

## Build entry point

```sh
python3 Platforms/Xiaomi/rodinPkg/DeviceBuild.py \
  TARGET=DEBUG \
  ENABLE_SECUREBOOT=0 \
  FD_BASE=0x40400000 \
  FD_SIZE=0x200000 \
  FD_BLOCKS=0x200 \
  DEVICE_MODEL=0 \
  DISABLE_DEBUG_MACRO_CHECK=TRUE
```

See the repository root README for submodule setup, the matching
`UniXNU-rodin` package layout and the current verification boundary.
