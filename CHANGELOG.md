# Changelog

## [0.4.0] - 2023-06-01

### Added

- Support print FAT chain by "-a" option
- Support to output FAT entry by "-f" option
- Support to output the last part for file contents by "tail" command
- Support to output file metadata
- Support to remove empty directory

### Fixed

- Fixed a bug that PWD could be NULL in interactive mode
- Return 0 as success in "-u" option
- Allow duplicate "/" as pathname
- Prohibit to create duplicate filename in exFAT
- Fixed to update filesize if cluster size is changed
- Add "ValidDataLength" updates in exFAT
- create doesn't calculate the number of cluster correctly
- Return incorrect heap offset in FAT12/FAT16/FAT32
- Return incorrect last cluster in FAT12/FAT16/FAT32
- Fixed to allocate used cluster in FAT12/FAT16/FAT32
- Return incorrect FAT entry in FAT12/FAT16/FAT32
- Fixed a bug that cannot identify a file of size is 0
- Fixed a bug that doesn't work cluster chain combining correctly
- trim doesn't work correctly around boundary unit
- Incorrect timestamp in exFAT by GMT offset
- Prohibit to remove directory by "remove" command
- Keep allocated even the file/directory was removed

### Removed

- Discontinue to support "update" command
- Discontinue to support "save"/"load" option
- Discontinue to support "force" option
- Discontinue to support "entry" option
- Discontinue to support "directory" option
- Discontinue command option in interactive mode

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
