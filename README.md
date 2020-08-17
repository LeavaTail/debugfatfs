# dumpexfat
dump FAT/exFAT filesystem information.

## Table of Contents
- [Description](#Description)
- [Example](#Example)
- [Requirement](#Requirement)
- [Install](#Install)
- [Authors](#Authors)

## Description
FAT/exFAT has filesystem information(e.g. cluster size, root directory cluster index, ...) in first Sector.  
Some users want to obtain these information to confirm filesystem status.  
In addition, Some developer want to create any FAT/exFAT filesystem image. 

dumpexfat can ontain these inforamtion.

 * Main Boot Sector field
 * Cluster raw data
 * Sector raw data
 * Convert any character
 * Create any directory entry (Only interactive mode)
 * Change any FAT entry (Only interactive mode)
 * Change any allocation bitmap (Only interactive mode)

:warning: dumpexfat can write filesystem image. If you don't want, Please add `-r`(read only) option.

## Example
**Command line mode**
```sh
$ sudo dumpexfat /dev/sdc1
media-relative sector offset    :      800 (sector)
Offset of the First FAT         :      800 (sector)
Length of FAT table             :     1984 (sector)
Offset of the Cluster Heap      :     1000 (sector)
The number of clusters          :    50496 (cluster)
The first cluster of the root   :        4 (cluster)
Size of exFAT volumes           : 15818752 (sector)
Bytes per sector                :      512 (byte)
Bytes per cluster               :    32768 (byte)
The number of FATs              :        1
The percentage of clusters      :        0 (%)

Read "/" Directory (4 entries).
System Volume Information 新しいi DIRECTORY NEW_TEXT.TXT 
Cluster #4:
00000000:  83 07 45 00 53 00 44 00 2D 00 55 00 53 00 42 00  ..E.S.D.-.U.S.B.
00000010:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
00000020:  81 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
00000030:  00 00 00 00 02 00 00 00 A8 78 00 00 00 00 00 00  .........x......
00000040:  82 00 00 00 0D D3 19 E6 00 00 00 00 00 00 00 00  ................
00000050:  00 00 00 00 03 00 00 00 CC 16 00 00 00 00 00 00  ................
00000060:  85 03 D6 59 16 00 00 00 ED 94 11 51 ED 94 11 51  ...Y.......Q...Q
00000070:  ED 94 11 51 9E 9E A4 A4 A4 00 00 00 00 00 00 00  ...Q............
00000080:  C0 03 00 19 B8 FF 00 00 00 80 00 00 00 00 00 00  ................
00000090:  00 00 00 00 05 00 00 00 00 80 00 00 00 00 00 00  ................
000000A0:  C1 00 53 00 79 00 73 00 74 00 65 00 6D 00 20 00  ..S.y.s.t.e.m. .
000000B0:  56 00 6F 00 6C 00 75 00 6D 00 65 00 20 00 49 00  V.o.l.u.m.e. .I.
000000C0:  C1 00 6E 00 66 00 6F 00 72 00 6D 00 61 00 74 00  ..n.f.o.r.m.a.t.
000000D0:  69 00 6F 00 6E 00 00 00 00 00 00 00 00 00 00 00  i.o.n...........
000000E0:  85 02 66 54 10 00 00 00 23 95 11 51 23 95 11 51  ..fT....#..Q#..Q
```

**Interactive mode**
```sh
$ sudo dumpexfat -i /dev/sdc1
Welcome to dumpexfat 0.1 (Interactive Mode)

/> ls
-HSD-    32768 2020-08-17 02:39:27 System Volume Information 
---D-    32768 2020-08-17 02:41:06 新しいiY 
---D-    32768 2020-08-17 02:41:10 DIRECTORY 
----A        0 2020-08-17 02:42:18 NEW_TEXT.TXT 

/> cd DIRECTORY
/DIRECTORY> ls

/DIRECTORY> exit
Goodbye!
```

## Requirements
* [autoconf](http://www.gnu.org/software/autoconf/)
* [automake](https://www.gnu.org/software/automake/)
* [libtool](https://www.gnu.org/software/libtool/)
* [help2man](https://www.gnu.org/software/help2man/)
* [make](https://www.gnu.org/software/make/)

## Install
```sh
$ ./script/bootstrap.sh
$ ./configure
$ make
$ make install
```

## Authors
[LeavaTail](https://github.com/LeavaTail)
