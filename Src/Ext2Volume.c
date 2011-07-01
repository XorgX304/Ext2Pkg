#include "Ext2.h"

EFI_STATUS EFIAPI
Ext2OpenVolume (
  IN EFI_SIMPLE_FILE_SYSTEM_PROTOCOL * This,
  OUT EFI_FILE_PROTOCOL ** Root
)
{
  return EFI_SUCCESS;
}

/**
  Checks if the volume contains a valid ext2 partition

  @param  Private[in]       A pointer to the volume to check.              

  @retval EFI_SUCCESS       This volume contains a valid ext2 partition.
  @retval other             This volume does not contain a valid ext2 partition.

**/
EFI_STATUS
Ext2CheckSB (
  IN OUT EXT2_DEV *Private
)
{
  EFI_STATUS      Status;

  Status =
    DiskIo->ReadDisk (Private->DiskIo, Private->BlockIo->Media->MediaId, 1024, sizeof (struct ext2fs), &Private->fs->e2fs);

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_INFO, "Ext2CheckSB: error reading ext2 superblock\n"));
    return Status;
  }

  if (Private->fs->e2fs.e2fs_magic != E2FS_MAGIC) {
    DEBUG ((EFI_D_INFO, "Ext2CheckSB: error not ext2 partition\n"));
    return EFI_UNSUPPORTED;
  }

  DEBUG ((EFI_D_INFO, "Ext2CheckSB: Success\n"));
  return EFI_SUCCESS;
}
