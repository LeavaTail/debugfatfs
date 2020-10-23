# debugfatfs
![C/C++ CI](https://github.com/LeavaTail/debugfatfs/workflows/C/C++%20CI/badge.svg)

FAT/exFAT file system debugger.

## Table of Contents
- [Description](#Description)
- [Example](#Example)
- [Usage](#Usage)
- [Requirement](#Requirement)
- [Install](#Install)
- [Authors](#Authors)

## Description
FAT/exFAT has filesystem information(e.g. cluster size, root directory cluster index, ...) in first Sector.  
Some users want to obtain these information to confirm filesystem status.  
In addition, Some developer want to create any FAT/exFAT filesystem image. 

debugfatfs can ontain these inforamtion.

 * Main Boot Sector field
 * Cluster raw data
 * Sector raw data
 * Convert any character
 * Change any FAT entry (Only interactive mode)
 * Change any allocation bitmap (Only interactive mode)
 * Trim deleted directory entry (Only interactive mode)

:warning: debugfatfs can write filesystem image. If you don't want, Please add `-r`(read only) option.

## Example
**Command line mode**
```sh
$ sudo debugfatfs -c 4 /dev/sdc1
media-relative sector offset    : 0x00000800 (sector)
Offset of the First FAT         : 0x00000800 (sector)
Length of FAT table             :       1088 (sector)
Offset of the Cluster Heap      : 0x00001000 (sector)
The number of clusters          :      65472 (cluster)
The first cluster of the root   :          4 (cluster)
Size of exFAT volumes           :    8388608 (sector)
Bytes per sector                :        512 (byte)
Bytes per cluster               :      32768 (byte)
The number of FATs              :          1
The percentage of clusters      :          0 (%)

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

**Interactive mode**
```sh
$ sudo debugfatfs -i /dev/sdc1
Welcome to debugfatfs 0.1 (Interactive Mode)

/> ls
-HSD-    32768 2020-09-14 00:29:39 System Volume Information 
---DA    32768 2020-09-14 00:30:04 01_LONGNAME 
---DA    32768 2020-09-14 00:30:04 02_UNICODE 
---DA    32768 2020-09-14 00:30:04 03_DELETE 
---DA    32768 2020-09-14 00:30:04 00_SIMPLE 

/> create TEST
/> entry 19
EntryType                       : 85
SecondaryCount                  : 02
SetChecksum                     : b102
FileAttributes                  : 0020
Reserved1                       : 0000
CreateTimestamp                 : 51576a41
LastModifiedTimestamp           : 51576a41
LastAccessedTimestamp           : 51576a41
Create10msIncrement             : 64
LastModified10msIncrement       : 64
CreateUtcOffset                 : a4
LastModifiedUtcOffset           : a4
LastAccessdUtcOffset            : a4
Reserved2                       : 00000000000000
/> exit
Goodbye!
```

## Usage
debugfatfs support support these optoin. (Please look at man-page)
* **-a**, **--all** --- Trverse all directories
* **-b**, **--byte**=*offset* --- dump the any byte after dump filesystem information
* **-c**, **--cluster**=*index* --- dump the cluster index after dump filesystem information
* **-d**, **--direcotry**=*path* --- read directory entry from path
* **-e**, **--entry=index** --- read raw directory entry in current directory
* **-f**, **--fource** --- write foucibly even if filesystem image has already mounted
* **-i**, **--interactive** --- prompt the user operate filesystem
* **-l**, **--load**=*file* --- Load Main boot region and FAT region from file
* **-o**, **--output**=*file* --- send output to file rather than stdout
* **-q**, **--quiet** --- Not prompting the user in interactive mode
* **-r**, **--ro** --- read only mode
* **-s**, **--save**=*file* --- Save Main boot region and FAT region in file
* **-u**, **--upper** --- convert into uppercase latter by up-case Table
* **-v**, **--verbose** --- Version mode

And, debugfatfs with interactive mode support these command.
* **ls** --- list current directory contents
* **cd** *path* --- change directory
* **cluster** *cluster* --- print cluster raw-data
* **entry** *index* --- print directory entry
* **alloc** *cluster* --- allocate cluster
* **release** *cluster* --- release cluster
* **fat** *index* *[entry]* --- change File Allocation Table entry
* **create** *file* --- create directory entry
* **remove** *file* --- remove directory entry
* **trim** --- trim deleted dentry
* **help** --- display this help
* **exit** --- exit interactive mode

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
