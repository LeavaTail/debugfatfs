# Test Specification

This document focus on the test specification.

## Environment

The test uses the following directory structure.

```bash
$ tree
.
|-- 00
|   |-- FILE1.TXT    (FATCHAIN)
    |-- FILE2.TXT
|   `-- DIR
|       `-- SUBFILE.TXT
|-- 01
|  |-- ABCDEFGHIJKLMNOPQRSTUVWXYZ!
|  |-- ¼
|  |-- Ō
|  |-- あいうえお
|  `-- U10000
|      `-- 𠮷
`-- 02
    |-- DELETED.TXT  (Deleted)
    `-- FILE3.TXT
```

The test environment was created by Windows 10, and the following command.

```powershell
New-Item 00 -itemType Directory
New-Item 01 -itemType Directory
New-Item 02 -itemType Directory
cd 00
fsutil file createnew FILE1.TXT $clu
fsutil file createnew FILE2.TXT ($clu * 2)
echo "" >> FILE1.TXT
New-Item DIR -itemType Directory
cd DIR
New-Item SUBFILE.TXT
cd \01
New-Item ABCDEFGHIJKLMNOPQRSTUVWXYZ
New-Item ¼
New-Item Ō
New-Item あいうえお
cd \02
New-Item DELETED.TXT
New-Item FILE3.TXT
Remove-item DELETED.TXT
```

## Test Item

The following tests shall be performed.

- DO NOT abort in each option and filesystem image(FAT12/FAT16/FAT32/exFAT)
- DO NOT abort in shell mode command and filesystem image(FAT12/FAT16/FAT32/exFAT)
- DO NOT abort in the wrong usage

