#
# Include from the Rodin platform DSC.
#
# [rodin 2026-11-14] Current UniXNUPkg (UniXNU main) no longer exports the
# RamDisk offset/size PCDs this file used to set: UniXNURamDiskDxe locates the
# payload volume at run time by scanning for the FAT32 VBR and taking the
# length from that volume's own BPB.  The old PcdUniXNURamDiskOffset/Size
# entries would now fail the DSC build with F001 ("Pcd ... not declared in DEC
# files referenced in INF files in FDF"), so they are gone.
#
# The package's remaining PCDs (gUniXNUPkgTokenSpaceGuid.PcdUniXNUDebugLevel,
# PcdUniXNUConfigPath, PcdUniXNUApplicationFile, PcdUniXNUDeviceModel) ship
# their own defaults in UniXNUPkg.dec; override them here only if this board
# needs something different.
#
[PcdsFixedAtBuild]
