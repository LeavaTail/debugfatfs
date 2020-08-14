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

dumpexfat can ontain these inforamtion.

 * Main Boot Sector field
 * Cluster raw data
 * Sector raw data

:warning: dumpexfat can write filesystem image. If you don't want, Please add `-r`(read only) option.

## Example
```sh
$ sudo dumpexfat /dev/sdc1
Size of exFAT volumes           : 60565504 (sector)
Bytes per sector                :      512 (byte)
Bytes per cluster               :    32768 (byte)
The number of FATs              :        1
The percentage of clusters      :        0 (%)

volume Label: VOLUME
Allocation Bitmap (#2):
2 3 4 5 6 7 8 9 10 
Upcase table (#6):
Upcase-table was skipped.
```

```sh
$ sudo dumpexfat -c 8 /dev/sdc1 | head -n 20
Size of exFAT volumes           : 60565504 (sector)
Bytes per sector                :      512 (byte)
Bytes per cluster               :    32768 (byte)
The number of FATs              :        1
The percentage of clusters      :        0 (%)

volume Label: VOLUME
Allocation Bitmap (#2):
2 3 4 5 6 7 8 9 10 
Upcase table (#6):
Upcase-table was skipped.
Cluster #8:
00000000:  85 02 6C B5 20 00 00 00 5C AE F9 50 5D AE F9 50  ..l. ...\..P]..P
00000010:  5D AE F9 50 64 00 A4 A4 A4 00 00 00 00 00 00 00  ]..Pd...........
00000020:  C0 03 00 0E AE 52 00 00 0C 00 00 00 00 00 00 00  .....R..........
00000030:  00 00 00 00 09 00 00 00 0C 00 00 00 00 00 00 00  ................
00000040:  C1 00 57 00 50 00 53 00 65 00 74 00 74 00 69 00  ..W.P.S.e.t.t.i.
00000050:  6E 00 67 00 73 00 2E 00 64 00 61 00 74 00 00 00  n.g.s...d.a.t...
00000060:  85 03 9A 6D 20 00 00 00 61 AE F9 50 62 AE F9 50  ...m ...a..Pb..P
00000070:  62 AE F9 50 22 00 A4 A4 A4 00 00 00 00 00 00 00  b..P"...........
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
