# Changelog

## [0.3.0] - 2021-01-30

### Added

- Support to fill temporary directory entry in fill

### Fixed

- Invalid update bitmap in alloc_cluster
- Invalid update filesize in root directory
- Invalid division calculation (round up)
- Invalid namehash in lower-case character

## [0.2.0] - 2020-11-22

### Added

- Quite mode suppress message by statfs()
- Consider NoFatChain Field in exFAT filesystem
- Update directory entry API
- Allow to no-operand timezone
- Update filesize in alloc/free cluster
- Check Upcase-Table checksum
- Support new cluster in create/remove/trim

### Fixed

- Unexpected update Allocation Bitmap in memory
- Failed to check bit in ActiveFAT
- Invalid check whether directory was loaded in FAT
- Flush only fisrt cluster chain
- DIR_FileSize must always be 0 in Direcotry in FAT
- Invalid Detemination of FAT12 entry for a cluster
- FAT entry 0x1 is invalid in FAT

## [0.1.1] - 2020-11-07

### Fixed

- Unexpected update Allocation Bitmap in memory
- Not Flush Allocation Bitmap to Disk

## [0.1.0] - 2020-10-24

### Added

- Print main Boot Sector field
- Print any cluster
- Print Directory list
- Backup or restore FAT volume
- Convert into update latter
- Create file
- Remove file
- Change any FAT entry
- Change any allocation bitmap
- Trim deleted directory entry

## Initial Version
