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

## Example
```sh
$ sudo dumpexfat /dev/sdc1
Size of exFAT volumes           : 16000000 (sector)
Bytes per sector                :      512 (byte)
Bytes per cluster               :    32768 (byte)
The number of FATs              :        1
The percentage of clusters      :        0 (%)
```

```sh
$ sudo dumpexfat -c 3 /dev/sdc1 | head -n 20
Size of exFAT volumes           : 16000000 (sector)
Bytes per sector                :      512 (byte)
Bytes per cluster               :    32768 (byte)
The number of FATs              :        1
The percentage of clusters      :        0 (%)
Cluster #3:
00000000:  00 00 01 00 02 00 03 00 04 00 05 00 06 00 07 00  ................
00000010:  08 00 09 00 0A 00 0B 00 0C 00 0D 00 0E 00 0F 00  ................
00000020:  10 00 11 00 12 00 13 00 14 00 15 00 16 00 17 00  ................
00000030:  18 00 19 00 1A 00 1B 00 1C 00 1D 00 1E 00 1F 00  ................
00000040:  20 00 21 00 22 00 23 00 24 00 25 00 26 00 27 00   .!.".#.$.%.&.'.
00000050:  28 00 29 00 2A 00 2B 00 2C 00 2D 00 2E 00 2F 00  (.).*.+.,.-.../.
00000060:  30 00 31 00 32 00 33 00 34 00 35 00 36 00 37 00  0.1.2.3.4.5.6.7.
00000070:  38 00 39 00 3A 00 3B 00 3C 00 3D 00 3E 00 3F 00  8.9.:.;.<.=.>.?.
00000080:  40 00 41 00 42 00 43 00 44 00 45 00 46 00 47 00  @.A.B.C.D.E.F.G.
00000090:  48 00 49 00 4A 00 4B 00 4C 00 4D 00 4E 00 4F 00  H.I.J.K.L.M.N.O.
000000A0:  50 00 51 00 52 00 53 00 54 00 55 00 56 00 57 00  P.Q.R.S.T.U.V.W.
000000B0:  58 00 59 00 5A 00 5B 00 5C 00 5D 00 5E 00 5F 00  X.Y.Z.[.\.].^._.
000000C0:  60 00 41 00 42 00 43 00 44 00 45 00 46 00 47 00  `.A.B.C.D.E.F.G.
000000D0:  48 00 49 00 4A 00 4B 00 4C 00 4D 00 4E 00 4F 00  H.I.J.K.L.M.N.O.
```

## Requirements
* [autoconf](http://www.gnu.org/software/autoconf/)
* [automake](https://www.gnu.org/software/automake/)
* [libtool](https://www.gnu.org/software/libtool/)

## Install
```sh
$ ./script/bootstrap.sh
$ ./configure
$ make
$ make install
```

## Authors
[LeavaTail](https://github.com/LeavaTail)
