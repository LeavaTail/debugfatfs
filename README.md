# debugfatfs

[![Test](https://github.com/LeavaTail/debugfatfs/actions/workflows/test.yml/badge.svg)](https://github.com/LeavaTail/debugfatfs/actions/workflows/test.yml)
[![codecov](https://codecov.io/gh/LeavaTail/debugfatfs/branch/main/graph/badge.svg)](https://codecov.io/gh/LeavaTail/debugfatfs)

debugfatfs is a utilities to check/update FAT/exFAT Volume image without mounting file system.

## Introduction

FAT/exFAT is one of the filesystem developed by Microsoft, that have been used in removavle media such as USB flash drive.

A specification for these filesystem are published ([[1]], [[2]]), and anyone can know the data structure.  
Users and developers will need these data for performance improvement, feature development, and bug analysis.
(e.g. cluster size, root directory cluster index)

debugfatfs analyzes the FAT/exFAT volume image without mounting filesystem, output in a format that is readable to them.
It also allows for simple file/directory modification to prepare bug analysis.

[1]: https://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/fatgen103.doc
[2]: https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification

## Features

debugfatfs can ontain these inforamtion.

- Print Main Boot Sector Field
- Print Cluster/Sector raw data
- Print File contents
- Print File metadata
- Print Directory contents
- Update FAT entry
- Create/Remove file
- Cluster compaction

:warning: debugfatfs can write filesystem image.
If you don't want, Please add `-r`(read only) option.

## Example

### Normal usage

#### Case 1: Print Main Boot Sector Field

- User can print Main Boot Sector by default. (`-a` option outputs more information)

```
$ sudo debugfatfs /dev/sdc1
Sector size:            512
Cluster size:           32768
FAT offset:             2048
FAT size:               557056
FAT count:              1
Partition offset:       1048576
Volume size:            4294967296
Cluster offset:         2097152
Cluster count:          65472
First cluster:          4
Volume serial:          0xd6423d82
Filesystem revision:    1.00
Usage rate:             0
```

#### Case 2: Print Cluster/Sector raw data

- User can print cluster by `-c` option. (print sector by `-s` option)
- User can print cluster by `cluster` command in Interactive Mode.

```
$ sudo debugfatfs -c 4 /dev/sdc1
Cluster #4:
00000000:  83 0B 44 00 44 00 44 00 44 00 44 00 44 00 44 00  ..D.D.D.D.D.D.D.
00000010:  44 00 44 00 44 00 44 00 00 00 00 00 00 00 00 00  D.D.D.D.........
00000020:  81 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
00000030:  00 00 00 00 02 00 00 00 F8 3F 00 00 00 00 00 00  .........?......
00000040:  82 00 00 00 0D D3 19 E6 00 00 00 00 00 00 00 00  ................
00000050:  00 00 00 00 03 00 00 00 CC 16 00 00 00 00 00 00  ................
00000060:  85 03 8B 3A 16 00 00 00 09 AD BD 56 09 AD BD 56  ...:.......V...V
00000070:  09 AD BD 56 1E 1E A4 A4 A4 00 00 00 00 00 00 00  ...V............
00000080:  C0 03 00 19 B8 FF 00 00 00 80 00 00 00 00 00 00  ................
00000090:  00 00 00 00 05 00 00 00 00 80 00 00 00 00 00 00  ................
000000A0:  C1 00 53 00 79 00 73 00 74 00 65 00 6D 00 20 00  ..S.y.s.t.e.m. .
000000B0:  56 00 6F 00 6C 00 75 00 6D 00 65 00 20 00 49 00  V.o.l.u.m.e. .I.
000000C0:  C1 00 6E 00 66 00 6F 00 72 00 6D 00 61 00 74 00  ..n.f.o.r.m.a.t.
000000D0:  69 00 6F 00 6E 00 00 00 00 00 00 00 00 00 00 00  i.o.n...........
000000E0:  85 02 28 60 10 00 00 00 10 AD BD 56 10 AD BD 56  ..(`.......V...V
000000F0:  10 AD BD 56 59 59 A4 A4 A4 00 00 00 00 00 00 00  ...VYY..........
00000100:  C0 03 00 02 1E 00 00 00 00 80 00 00 00 00 00 00  ................
00000110:  00 00 00 00 08 00 00 00 00 80 00 00 00 00 00 00  ................
00000120:  C1 00 30 00 30 00 00 00 00 00 00 00 00 00 00 00  ..0.0...........
00000130:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
00000140:  85 02 48 82 10 00 00 00 10 AD BD 56 10 AD BD 56  ..H........V...V
00000150:  10 AD BD 56 5E 5E A4 A4 A4 00 00 00 00 00 00 00  ...V^^..........
00000160:  C0 03 00 02 1E 80 00 00 00 80 00 00 00 00 00 00  ................
00000170:  00 00 00 00 09 00 00 00 00 80 00 00 00 00 00 00  ................
00000180:  C1 00 30 00 31 00 00 00 00 00 00 00 00 00 00 00  ..0.1...........
00000190:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
000001A0:  85 02 28 64 10 00 00 00 10 AD BD 56 10 AD BD 56  ..(d.......V...V
000001B0:  10 AD BD 56 62 62 A4 A4 A4 00 00 00 00 00 00 00  ...Vbb..........
000001C0:  C0 03 00 02 1F 00 00 00 00 80 00 00 00 00 00 00  ................
000001D0:  00 00 00 00 0A 00 00 00 00 80 00 00 00 00 00 00  ................
000001E0:  C1 00 30 00 32 00 00 00 00 00 00 00 00 00 00 00  ..0.2...........
000001F0:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
00007FF0:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
```

#### Case 3: Print File metadata w/o mounting filesystem

- User can print cluster 2rd argument.
- User can print cluster by `stat` command in Interactive Mode.

```
$ sudo debugfatfs /dev/sdc1 /00/FILE1.TXT
Sector size:            512
Cluster size:           32768
FAT offset:             2048
FAT size:               557056
FAT count:              1
Partition offset:       1150287872
Volume size:            4294967296
Cluster offset:         2097152
Cluster count:          65472
First cluster:          4
Volume serial:          0x50b96952
Filesystem revision:    1.00
Usage rate:             0

File Name:   FILE1.TXT
File Size:   32772
Clusters:    2
First Clu:   11
File Attr:   ----A
File Flags:  FatChain/ AllocationPossible
Access Time: 2023-05-29 21:40:34
Modify Time: 2023-05-29 21:40:34
Create Time: 2023-05-29 21:40:33
```

#### Case 4: Print FAT entry

- User can print cluster by `-f` option.
- User can print cluster by `fat` command in Interactive Mode.

```
$ sudo debugfatfs -f 11 /dev/sdc1
Get: Cluster 11 is FAT entry 0000000e
```

### Advanced usage

These usages may corrupt the filesystem images.  
Please be careful while using that!

#### Case 1: Update FAT entry

- Developer can print and update FAT entry by `fat` command in Interactive Mode.

```
$ sudo debugfatfs -i /dev/sdc2
Welcome to debugfatfs 0.4.0 (Interactive Mode)

/> fat 100
Get: Cluster 100 is FAT entry 00000065
/> fat 100 8
Set: Cluster 100 is FAT entry 00000008
/> fat 100
Get: Cluster 100 is FAT entry 00000008
```

#### Case 2: Create/Remove file

- Developer can create file by `create` command in Interactive Mode.
- Developer can remove file by `remove` command in Interactive Mode.

```
$ sudo debugfatfs -i /dev/sdc2
Welcome to debugfatfs 0.4.0 (Interactive Mode)

/> cd 03_DELETE
/03_DELETE/> ls
----A        0 2020-11-21 08:01:46 NEW_FILE
----A        0 2020-11-21 08:02:10 FILE

/03_DELETE/> create FILE2
/03_DELETE/> ls
----A        0 2020-11-21 08:01:46 NEW_FILE
----A        0 2020-11-21 08:02:10 FILE
----A        0 2020-11-21 17:03:45 FILE2

/03_DELETE/> remove NEW_FILE
/03_DELETE/> ls
----A        0 2020-11-21 08:02:10 FILE
----A        0 2020-11-21 17:03:45 FILE2
```

#### Case 3: Cluster compaction

- Developer can compact deleted entry by `trim` command in Interactive Mode.

```
$ sudo debugfatfs -i /dev/sdc2
Welcome to debugfatfs 0.4.0 (Interactive Mode)

/> cd 03_DELETE
/03_DELETE/> ls
----A        0 2020-11-21 08:01:46 NEW_FILE
----A        0 2020-11-21 08:02:10 FILE

/03_DELETE> cluster 12
Cluster #12:
00000000:  05 02 8E DC 20 00 00 00 37 40 75 51 37 40 75 51  .... ...7@uQ7@uQ
00000010:  37 40 75 51 00 00 00 A4 A4 00 00 00 00 00 00 00  7@uQ............
00000020:  40 01 00 08 53 11 00 00 00 00 00 00 00 00 00 00  @...S...........
00000030:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
00000040:  41 00 4E 00 45 00 57 00 5F 00 46 00 49 00 4C 00  A.N.E.W._.F.I.L.
00000050:  45 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  E...............
00000060:  85 02 22 10 20 00 00 00 45 40 75 51 45 40 75 51  ..". ...E@uQE@uQ
00000070:  45 40 75 51 00 00 00 A4 A4 00 00 00 00 00 00 00  E@uQ............
00000080:  C0 01 00 04 2E D4 00 00 00 00 00 00 00 00 00 00  ................
00000090:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
000000A0:  C1 00 46 00 49 00 4C 00 45 00 00 00 5F 00 5F 00  ..F.I.L.E..._._.
000000B0:  5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00  _._._._._._._._.
000000C0:  41 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00  A._._._._._._._.
000000D0:  5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00  _._._._._._._._.
000000E0:  41 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00  A._._._._._._._.
000000F0:  5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00  _._._._._._._._.
00000100:  41 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00  A._._._._._._._.
00000110:  5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00  _._._._._._._._.
00000120:  41 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00  A._._._._._._._.
00000130:  5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00  _._._._._._._._.
00000140:  41 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00  A._._._._._._._.
00000150:  5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00  _._._._._._._._.
00000160:  41 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00  A._._._._._._._.
00000170:  5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 2E 00 74 00  _._._._._._...t.
00000180:  41 00 78 00 74 00 00 00 00 00 00 00 00 00 00 00  A.x.t...........
00000190:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
000001A0:  05 02 EE 38 20 00 00 00 C2 7B 2D 51 3D AC 2C 51  ...8 ....{-Q=.,Q
000001B0:  C2 7B 2D 51 32 00 A4 A4 A4 00 00 00 00 00 00 00  .{-Q2...........
000001C0:  40 01 00 09 ED 22 00 00 00 00 00 00 00 00 00 00  @...."..........
000001D0:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
000001E0:  41 00 46 00 49 00 4C 00 45 00 36 00 2E 00 74 00  A.F.I.L.E.6...t.
000001F0:  78 00 74 00 00 00 00 00 00 00 00 00 00 00 00 00  x.t.............
00000200:  85 02 93 DB 20 00 00 00 76 40 75 51 76 40 75 51  .... ...v@uQv@uQ
00000210:  76 40 75 51 64 64 A4 A4 A4 00 00 00 00 00 00 00  v@uQdd..........
00000220:  C0 03 00 05 24 B5 00 00 00 00 00 00 00 00 00 00  ....$...........
00000230:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
00000240:  C1 00 46 00 49 00 4C 00 45 00 32 00 00 00 00 00  ..F.I.L.E.2.....
00000250:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
00007FF0:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................

/03_DELETE/> trim
/03_DELETE/> ls
----A        0 2020-11-21 08:02:10 FILE
----A        0 2020-11-21 17:03:45 FILE2

/03_DELETE/> cluster 12
Cluster #12:
00000000:  85 02 22 10 20 00 00 00 45 40 75 51 45 40 75 51  ..". ...E@uQE@uQ
00000010:  45 40 75 51 00 00 00 A4 A4 00 00 00 00 00 00 00  E@uQ............
00000020:  C0 01 00 04 2E D4 00 00 00 00 00 00 00 00 00 00  ................
00000030:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
00000040:  C1 00 46 00 49 00 4C 00 45 00 00 00 5F 00 5F 00  ..F.I.L.E..._._.
00000050:  5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00 5F 00  _._._._._._._._.
00000060:  85 02 93 DB 20 00 00 00 76 40 75 51 76 40 75 51  .... ...v@uQv@uQ
00000070:  76 40 75 51 64 64 A4 A4 A4 00 00 00 00 00 00 00  v@uQdd..........
00000080:  C0 03 00 05 24 B5 00 00 00 00 00 00 00 00 00 00  ....$...........
00000090:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
000000A0:  C1 00 46 00 49 00 4C 00 45 00 32 00 00 00 00 00  ..F.I.L.E.2.....
000000B0:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
00007FF0:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
```

## Usage

debugfatfs support support these optoin. (Please look at man-page)

- **-a**, **--all** --- Trverse all directories
- **-b**, **--byte**=*offset* --- dump the any byte after dump filesystem information
- **-c**, **--cluster**=*index* --- dump the cluster after dump filesystem information
- **-f**, **--fat=index** --- read FAT entry value for index
- **-i**, **--interactive** --- prompt the user operate filesystem
- **-o**, **--output**=*file* --- send output to file rather than stdout
- **-q**, **--quiet** --- Suppress message about Main boot Sector
- **-r**, **--ro** --- read only mode
- **-u**, **--upper** --- convert into uppercase latter by up-case Table
- **-v**, **--verbose** --- Version mode

And, debugfatfs with interactive mode support these command.

- **ls** --- list current directory contents
- **cd** *path* --- change directory
- **cluster** *cluster* --- print cluster raw-data
- **alloc** *cluster* --- allocate cluster
- **release** *cluster* --- release cluster
- **fat** *index* *[entry]* --- change File Allocation Table entry
- **create** *file* --- create directory entry for file
- **mkdir** *directory* --- create directory entry for directory
- **remove** *file* --- remove directory entry for file
- **rmdir** *directory* --- remove directory entry for directory
- **trim** --- trim deleted dentry
- **fill** *[entry]* --- fill in directory
- **tail** *[file]* --- output the last part of files
- **help** --- display this help
- **exit** --- exit interactive mode

## Requirements

- UTF-8 locale

## Install

These packages are needed to build debugfatfs.

- [autoconf](http://www.gnu.org/software/autoconf/)
- [automake](https://www.gnu.org/software/automake/)
- [libtool](https://www.gnu.org/software/libtool/)
- [make](https://www.gnu.org/software/make/)

## Install

```bash
$ ./script/bootstrap.sh
$ ./configure
$ make
$ sudo make install
```

## Authors

[LeavaTail](https://github.com/LeavaTail)
