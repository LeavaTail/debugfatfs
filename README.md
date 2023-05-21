# debugfatfs

[![Test](https://github.com/LeavaTail/debugfatfs/actions/workflows/test.yml/badge.svg)](https://github.com/LeavaTail/debugfatfs/actions/workflows/test.yml)
[![codecov](https://codecov.io/gh/LeavaTail/debugfatfs/branch/main/graph/badge.svg)](https://codecov.io/gh/LeavaTail/debugfatfs)

debugfatfs is a utilities to check/update FAT/exFAT Volume image without mounting file system.

## Introduction

FAT/exFAT has filesystem information in first sector.
(e.g. cluster size, root directory cluster index, ...)  
Some users want to obtain these information to confirm filesystem status.  
In addition, Some developer want to create any FAT/exFAT filesystem image.

## Features

debugfatfs can ontain these inforamtion.

- Print Main Boot Sector Field
- Print Cluster/Sector raw data
- Print Directory entry
- Print File contents
- Backup/Restore FAT region
- Update FAT entry
- Create/Remove file
- Cluster compaction
- Create Directory entry

:warning: debugfatfs can write filesystem image.
If you don't want, Please add `-r`(read only) option.

## Example

### Normal usage

#### Case 1: Print Main Boot Sector Field

- User can print Main Boot Sector by default. (`-q` option restrict messages)

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
$ sudo debugfatfs -q -c 4 /dev/sdc1
Cluster #4:
00000000:  83 05 DC 30 EA 30 E5 30 FC 30 E0 30 00 00 00 00  ...0.0.0.0.0....
00000010:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
00000020:  81 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
00000030:  00 00 00 00 02 00 00 00 F8 3F 00 00 00 00 00 00  .........?......
00000040:  82 00 00 00 0D D3 19 E6 00 00 00 00 00 00 00 00  ................
00000050:  00 00 00 00 03 00 00 00 CC 16 00 00 00 00 00 00  ................
00000060:  85 03 5E EE 16 00 00 00 B3 7B 2D 51 B3 7B 2D 51  ..^......{-Q.{-Q
00000070:  B3 7B 2D 51 BF BF A4 A4 A4 00 00 00 00 00 00 00  .{-Q............
00000080:  C0 03 00 19 B8 FF 00 00 00 80 00 00 00 00 00 00  ................
00000090:  00 00 00 00 05 00 00 00 00 80 00 00 00 00 00 00  ................
000000A0:  C1 00 53 00 79 00 73 00 74 00 65 00 6D 00 20 00  ..S.y.s.t.e.m. .
000000B0:  56 00 6F 00 6C 00 75 00 6D 00 65 00 20 00 49 00  V.o.l.u.m.e. .I.
000000C0:  C1 00 6E 00 66 00 6F 00 72 00 6D 00 61 00 74 00  ..n.f.o.r.m.a.t.
000000D0:  69 00 6F 00 6E 00 00 00 00 00 00 00 00 00 00 00  i.o.n...........
000000E0:  85 02 DE 55 30 00 00 00 C2 7B 2D 51 2A 7A 2D 51  ...U0....{-Q*z-Q
000000F0:  C2 7B 2D 51 24 00 A4 A4 A4 00 00 00 00 00 00 00  .{-Q$...........
00000100:  C0 03 00 0B B6 14 00 00 00 80 00 00 00 00 00 00  ................
00000110:  00 00 00 00 08 00 00 00 00 80 00 00 00 00 00 00  ................
00000120:  C1 00 30 00 31 00 5F 00 4C 00 4F 00 4E 00 47 00  ..0.1._.L.O.N.G.
00000130:  4E 00 41 00 4D 00 45 00 00 00 00 00 00 00 00 00  N.A.M.E.........
00000140:  85 02 05 AC 30 00 00 00 C2 7B 2D 51 2A 7A 2D 51  ....0....-Q*z-Q
00000150:  C2 7B 2D 51 2A 00 A4 A4 A4 00 00 00 00 00 00 00  .{{-Q*...........
00000160:  C0 03 00 0A F3 2F 00 00 00 80 00 00 00 00 00 00  ...../..........
00000170:  00 00 00 00 0B 00 00 00 00 80 00 00 00 00 00 00  ................
00000180:  C1 00 30 00 32 00 5F 00 55 00 4E 00 49 00 43 00  ..0.2._.U.N.I.C.
00000190:  4F 00 44 00 45 00 00 00 00 00 00 00 00 00 00 00  O.D.E...........
000001A0:  85 02 28 C0 30 00 00 00 C2 7B 2D 51 2A 7A 2D 51  ..(.0....{-Q*z-Q
000001B0:  C2 7B 2D 51 2E 00 A4 A4 A4 00 00 00 00 00 00 00  .{-Q............
000001C0:  C0 03 00 09 A5 EE 00 00 00 80 00 00 00 00 00 00  ................
000001D0:  00 00 00 00 0C 00 00 00 00 80 00 00 00 00 00 00  ................
000001E0:  C1 00 30 00 33 00 5F 00 44 00 45 00 4C 00 45 00  ..0.3._.D.E.L.E.
000001F0:  54 00 45 00 00 00 00 00 00 00 00 00 00 00 00 00  T.E.............
00000200:  85 02 3E 71 30 00 00 00 C2 7B 2D 51 2A 7A 2D 51  ..>q0....{-Q*z-Q
00000210:  C2 7B 2D 51 33 00 A4 A4 A4 00 00 00 00 00 00 00  .{-Q3...........
00000220:  C0 03 00 09 7F 4C 00 00 00 80 00 00 00 00 00 00  .....L..........
00000230:  00 00 00 00 0D 00 00 00 00 80 00 00 00 00 00 00  ................
00000240:  C1 00 30 00 30 00 5F 00 53 00 49 00 4D 00 50 00  ..0.0._.S.I.M.P.
00000250:  4C 00 45 00 00 00 00 00 00 00 00 00 00 00 00 00  L.E.............
00000260:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
00007FF0:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
```

#### Case 3: Print Directory entry

- User can print Directory entry by `-e` option.
- User can print Directory entry by `entry` command in Interactive Mode.

```
$ sudo debugfatfs -q -e 4 /dev/sdc1
EntryType                       : c0
GeneralSecondaryFlags           : 03
Reserved1                       : 00
NameLength                      : 19
NameHash                        : ffb8
Reserved2                       : 0000
ValidDataLength                 : 0000000000008000
Reserved3                       : 00000000
FirstCluster                    : 00000005
DataLength                      : 0000000000008000
```

### Advanced usage

These usages may corrupt the filesystem images.  
Please be careful while using that!

#### Case 1: Backup/Restore FAT region

- Developer can backup boot and FAT region by `-s` option. (restore by `-l` option)

```
$ sudo debugfatfs -q -b 0x100000 /dev/sdc1
Sector #1048576:
00000000:  F8 FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
00000010:  FF FF FF FF 00 00 00 00 00 00 00 00 00 00 00 00  ................
00000020:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
00000030:  00 00 00 00 11 00 00 00 00 00 00 00 00 00 00 00  ................
00000040:  00 00 00 00 12 00 00 00 13 00 00 00 14 00 00 00  ................
00000050:  15 00 00 00 16 00 00 00 17 00 00 00 18 00 00 00  ................
00000060:  19 00 00 00 1A 00 00 00 1B 00 00 00 1C 00 00 00  ................
00000070:  1D 00 00 00 1E 00 00 00 1F 00 00 00 20 00 00 00  ............ ...
00000080:  21 00 00 00 22 00 00 00 23 00 00 00 24 00 00 00  !..."...#...$...
00000090:  25 00 00 00 26 00 00 00 27 00 00 00 28 00 00 00  %...&...'...(...
000000A0:  29 00 00 00 2A 00 00 00 2B 00 00 00 2C 00 00 00  )...*...+...,...
000000B0:  2D 00 00 00 2E 00 00 00 2F 00 00 00 30 00 00 00  -......./...0...
000000C0:  31 00 00 00 32 00 00 00 33 00 00 00 34 00 00 00  1...2...3...4...
000000D0:  35 00 00 00 36 00 00 00 37 00 00 00 38 00 00 00  5...6...7...8...
000000E0:  39 00 00 00 3A 00 00 00 3B 00 00 00 3C 00 00 00  9...:...;...<...
000000F0:  3D 00 00 00 3E 00 00 00 3F 00 00 00 40 00 00 00  =...>...?...@...
00000100:  41 00 00 00 42 00 00 00 43 00 00 00 44 00 00 00  A...B...C...D...
00000110:  45 00 00 00 46 00 00 00 47 00 00 00 48 00 00 00  E...F...G...H...
00000120:  49 00 00 00 4A 00 00 00 4B 00 00 00 4C 00 00 00  I...J...K...L...
00000130:  4D 00 00 00 4E 00 00 00 4F 00 00 00 50 00 00 00  M...N...O...P...
00000140:  51 00 00 00 52 00 00 00 53 00 00 00 54 00 00 00  Q...R...S...T...
00000150:  55 00 00 00 56 00 00 00 57 00 00 00 58 00 00 00  U...V...W...X...
00000160:  59 00 00 00 5A 00 00 00 5B 00 00 00 5C 00 00 00  Y...Z...[...\...
00000170:  5D 00 00 00 5E 00 00 00 5F 00 00 00 60 00 00 00  ]...^..._...`...
00000180:  61 00 00 00 62 00 00 00 63 00 00 00 64 00 00 00  a...b...c...d...
00000190:  65 00 00 00 66 00 00 00 67 00 00 00 68 00 00 00  e...f...g...h...
000001A0:  69 00 00 00 6A 00 00 00 6B 00 00 00 6C 00 00 00  i...j...k...l...
000001B0:  6D 00 00 00 6E 00 00 00 6F 00 00 00 70 00 00 00  m...n...o...p...
000001C0:  71 00 00 00 72 00 00 00 73 00 00 00 74 00 00 00  q...r...s...t...
000001D0:  75 00 00 00 76 00 00 00 77 00 00 00 78 00 00 00  u...v...w...x...
000001E0:  79 00 00 00 7A 00 00 00 7B 00 00 00 7C 00 00 00  y...z...{...|...
000001F0:  7D 00 00 00 7E 00 00 00 7F 00 00 00 80 00 00 00  }...~...........

$ sudo debugfatfs -s boot_fat.old /dev/sdc1

$ sudo debugfatfs -l boot_fat.new /dev/sdc1

$ sudo debugfatfs -q -b 0x100000 /dev/sdc1
Sector #1048576:
00000000:  F8 FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
00000010:  FF FF FF FF 00 00 00 00 00 00 00 00 00 00 00 00  ................
00000020:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
000001F0:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................

$ ls -l boot_fat.old
-rw-rw-r-- 1 leava leava 2097152 Nov 21 16:41 boot_fat.old
```

#### Case 2: Update FAT entry

- Developer can print and update FAT entry by `fat` command in Interactive Mode.

```
$ sudo debugfatfs -i /dev/sdc1
Welcome to debugfatfs 0.3.0 (Interactive Mode)

/> fat 100
Get: Cluster 100 is FAT entry 00000065
/> fat 100 8
Set: Cluster 100 is FAT entry 00000008
/> fat 100
Get: Cluster 100 is FAT entry 00000008
```

#### Case 3: Create/Remove file

- Developer can create file by `create` command in Interactive Mode.
- Developer can remove file by `remove` command in Interactive Mode.

```
$ sudo debugfatfs -i /dev/sdc1
Welcome to debugfatfs 0.3.0 (Interactive Mode)

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

#### Case 4: Cluster compaction

- Developer can compact deleted entry by `trim` command in Interactive Mode.

```
$ sudo debugfatfs -i /dev/sdc1
Welcome to debugfatfs 0.3.0 (Interactive Mode)

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

#### Case 5: Create Directory entry

- Developer can update directory entry by `update` command in Interactive Mode.

```
$ sudo debugfatfs -i /dev/sdc1
Welcome to debugfatfs 0.3.0 (Interactive Mode)

/> cd 00_SIMPLE
/00_SIMPLE/> cluster 13
Cluster #13:
00000000:  85 02 44 C9 20 00 00 00 C2 7B 2D 51 D4 A0 2C 51  ..D. ....{-Q..,Q
00000010:  C2 7B 2D 51 35 00 A4 A4 A4 00 00 00 00 00 00 00  .{-Q5...........
00000020:  C0 03 00 08 0C 2B 00 00 13 00 00 00 00 00 00 00  .....+..........
00000030:  00 00 00 00 0E 00 00 00 13 00 00 00 00 00 00 00  ................
00000040:  C1 00 46 00 49 00 4C 00 45 00 2E 00 54 00 58 00  ..F.I.L.E...T.X.
00000050:  54 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  T...............
00000060:  85 02 69 D1 30 00 00 00 C2 7B 2D 51 2A 7A 2D 51  ..i.0....{-Q*z-Q
00000070:  C2 7B 2D 51 36 00 A4 A4 A4 00 00 00 00 00 00 00  .{-Q6...........
00000080:  C0 03 00 03 34 40 00 00 00 80 00 00 00 00 00 00  ....4@..........
00000090:  00 00 00 00 0F 00 00 00 00 80 00 00 00 00 00 00  ................
000000A0:  C1 00 44 00 49 00 52 00 00 00 00 00 00 00 00 00  ..D.I.R.........
000000B0:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
00007FF0:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
/00_SIMPLE/> update 6
Please select a Entry type.
1) File
2) Stream
3) File Name
4) Allocation Bitmap
5) Up-case Table
6) Volume Label
7) Other
#? 2

Please select a GeneralSecondaryFlags
   AllocationPossible [N/y]
   NoFatChain [N/y]
Please input a Name Length.
#? 20
Please input a Name Hash.
#? 0
Please input a Valid Data Length.
#? 10
 Please input a First Cluster.
#? 100
Please input a Data Length.
#? 30
c0 3 0 20 0 0 0 0 10 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 30 0 0 0 0 0 0 0
/00_SIMPLE/> cluster 13
Cluster #13:
00000000:  85 02 44 C9 20 00 00 00 C2 7B 2D 51 D4 A0 2C 51  ..D. ....{-Q..,Q
00000010:  C2 7B 2D 51 35 00 A4 A4 A4 00 00 00 00 00 00 00  .{-Q5...........
00000020:  C0 03 00 08 0C 2B 00 00 13 00 00 00 00 00 00 00  .....+..........
00000030:  00 00 00 00 0E 00 00 00 13 00 00 00 00 00 00 00  ................
00000040:  C1 00 46 00 49 00 4C 00 45 00 2E 00 54 00 58 00  ..F.I.L.E...T.X.
00000050:  54 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  T...............
00000060:  85 02 69 D1 30 00 00 00 C2 7B 2D 51 2A 7A 2D 51  ..i.0....{-Q*z-Q
00000070:  C2 7B 2D 51 36 00 A4 A4 A4 00 00 00 00 00 00 00  .{-Q6...........
00000080:  C0 03 00 03 34 40 00 00 00 80 00 00 00 00 00 00  ....4@..........
00000090:  00 00 00 00 0F 00 00 00 00 80 00 00 00 00 00 00  ................
000000A0:  C1 00 44 00 49 00 52 00 00 00 00 00 00 00 00 00  ..D.I.R.........
000000B0:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
000000C0:  C0 03 00 20 00 00 00 00 10 00 00 00 00 00 00 00  ... ............
000000D0:  00 00 00 00 00 01 00 00 30 00 00 00 00 00 00 00  ........0.......
000000E0:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
00007FF0:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
```

#### Case 6: Fill with temproray directory entry

- Developer can fill directory by `fill` command in Interactive Mode.

```
$ sudo debugfatfs -i /dev/sdc1
Welcome to debugfatfs 0.3.0 (Interactive Mode)

/> entry 1023
EntryType                       : 00
/> fill
/> entry 1023
EntryType                       : c1
GeneralSecondaryFlags           : 00
FileName                        : 480050004e0058005600420044004c00540044005500350035004a003200
```

## Usage

debugfatfs support support these optoin. (Please look at man-page)

- **-a**, **--all** --- Trverse all directories
- **-b**, **--byte**=*offset* --- dump the any byte after dump filesystem information
- **-c**, **--cluster**=*index* --- dump the cluster after dump filesystem information
- **-d**, **--direcotry**=*path* --- read directory entry from path
- **-e**, **--entry=index** --- read raw directory entry in current directory
- **-f**, **--fource** --- write foucibly even if filesystem image has already mounted
- **-i**, **--interactive** --- prompt the user operate filesystem
- **-l**, **--load**=*file* --- Load Main boot region and FAT region from file
- **-o**, **--output**=*file* --- send output to file rather than stdout
- **-q**, **--quiet** --- Suppress message about Main boot Sector
- **-r**, **--ro** --- read only mode
- **-s**, **--save**=*file* --- Save Main boot region and FAT region in file
- **-u**, **--upper** --- convert into uppercase latter by up-case Table
- **-v**, **--verbose** --- Version mode

And, debugfatfs with interactive mode support these command.

- **ls** --- list current directory contents
- **cd** *path* --- change directory
- **cluster** *cluster* --- print cluster raw-data
- **entry** *index* --- print directory entry
- **alloc** *cluster* --- allocate cluster
- **release** *cluster* --- release cluster
- **fat** *index* *[entry]* --- change File Allocation Table entry
- **create** *file* --- create directory entry
- **remove** *file* --- remove directory entry
- **update** *index* --- update directory entry
- **trim** --- trim deleted dentry
- **fill** *[entry]* --- fill in directory
- **tail** *[file]* --- output the last part of files
- **help** --- display this help
- **exit** --- exit interactive mode

## Requirements

- [autoconf](http://www.gnu.org/software/autoconf/)
- [automake](https://www.gnu.org/software/automake/)
- [libtool](https://www.gnu.org/software/libtool/)
- [make](https://www.gnu.org/software/make/)

## Install

```
$ ./script/bootstrap.sh
$ ./configure
$ make
# make install
```

## Authors

[LeavaTail](https://github.com/LeavaTail)
