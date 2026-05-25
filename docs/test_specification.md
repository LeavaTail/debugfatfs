# Test Specification

This document describes the sample images and the behavior covered by the test suite.

## Sample Images

The integration tests use FAT12, FAT16, FAT32, and exFAT images stored as compressed archives under `tests/sample/`.

```text
tests/sample/fat12.tar.bz2
tests/sample/fat16.tar.bz2
tests/sample/fat32.tar.bz2
tests/sample/exfat.tar.bz2
```

`tests/common.sh` extracts these archives into `*.img` files when the image is missing or when its checksum does not match the expected value.

## Filesystem Layout

The images contain the following test layout.

```text
.
|-- 00
|   |-- FILE1.TXT    (FAT chain)
|   |-- FILE2.TXT
|   `-- DIR
|       `-- SUBFILE.TXT
|-- 01
|   |-- ABCDEFGHIJKLMNOPQRSTUVWXYZ!
|   |-- ¼
|   |-- Ō
|   |-- あいうえお
|   `-- 𠮷
`-- 02
    |-- DELETED.TXT  (deleted directory entry)
    `-- FILE3.TXT
```

The images were originally prepared on Windows 10 with commands equivalent to the following.

```powershell
New-Item 00 -ItemType Directory
New-Item 01 -ItemType Directory
New-Item 02 -ItemType Directory

cd 00
fsutil file createnew FILE1.TXT $clu
fsutil file createnew FILE2.TXT ($clu * 2)
echo "" >> FILE1.TXT
New-Item DIR -ItemType Directory
cd DIR
New-Item SUBFILE.TXT

cd \01
New-Item ABCDEFGHIJKLMNOPQRSTUVWXYZ!
New-Item ¼
New-Item Ō
New-Item あいうえお
New-Item 𠮷

cd \02
New-Item DELETED.TXT
New-Item FILE3.TXT
Remove-Item DELETED.TXT
```

## Covered Behavior

The test suite covers the following behavior.

- Command-line options do not abort on supported filesystem images
- Interactive shell commands do not abort on supported filesystem images
- Invalid command-line and shell usage is rejected without aborting
- Invalid filesystem images are rejected
- File metadata can be printed from FAT and exFAT images
- File contents can be printed from FAT and exFAT images
- FAT12 root directory entries can be created and removed
- FAT12 FAT entries can be read and updated beyond the first FAT sector
- FAT long-file-name entries can be created with lower-case and mixed-case names
- FAT directory creation allocates a real cluster instead of cluster 0
- FAT and exFAT directory entries can be displayed and corrupted by field name or raw offset
- exFAT boot, file metadata, and up-case table behavior work in smoke tests
- Cluster allocation and release helpers work across supported images

## Running Tests

Run the integration tests:

```bash
$ make check
```

Run the unit tests:

```bash
$ make -C tests/unit_test
$ ./tests/unit_test/test
```

Useful test dependencies are listed in `README.md`.
