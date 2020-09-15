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
 * Create any directory entry (Only interactive mode)
 * Change any FAT entry (Only interactive mode)
 * Change any allocation bitmap (Only interactive mode)

:warning: debugfatfs can write filesystem image. If you don't want, Please add `-r`(read only) option.

## Example
**Command line mode**
```sh
$ sudo debugfatfs -c 5 /dev/sdc1
media-relative sector offset    : 0x00000000 (sector)
Offset of the First FAT         : 0x00000800 (sector)
Length of FAT table             :        200 (sector)
Offset of the Cluster Heap      : 0x00001000 (sector)
The number of clusters          :      24488 (cluster)
The first cluster of the root   :          5 (cluster)
Size of exFAT volumes           :     200000 (sector)
Bytes per sector                :        512 (byte)
Bytes per cluster               :       4096 (byte)
The number of FATs              :          1
The percentage of clusters      :          0 (%)

Read "/" Directory (0 entries).

Cluster #5:
00000000:  83 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
00000010:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
00000020:  81 00 00 00 20 00 00 00 00 00 00 00 00 00 00 00  .... ...........
00000030:  00 00 00 00 02 00 00 00 F5 0B 00 00 00 00 00 00  ................
00000040:  82 00 00 00 0D D3 19 E6 F6 75 AE 03 01 00 00 00  .........u......
00000050:  00 00 00 00 03 00 00 00 CC 16 00 00 00 00 00 00  ................
00000060:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
*
00000FF0:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................

```

**Interactive mode**
```sh
$ sudo debugfatfs -i /dev/sdc2
Welcome to debugfatfs 0.1 (Interactive Mode)

/> ls
-HSD-    32768 2020-09-14 00:29:39 System Volume Information 
---DA    32768 2020-09-14 00:30:04 01_LONGNAME 
---DA    32768 2020-09-14 00:30:04 02_UNICODE 
---DA    32768 2020-09-14 00:30:04 03_DELETE 
---DA    32768 2020-09-14 00:30:04 00_SIMPLE 

/> create TEST
Entry Type
  85  File
  C0  Stream
  C1  Filename
Select (Default 0x85): 

File Attributes
  Bit0  ReadOnly
  Bit1  Hidden
  Bit2  System
  Bit4  Directory
  Bit5  Archive
Select (Default 0x20): 3

Secondary Count
Select (Default 0x2): 

Reserverd
Select (Default 0x0): 8

Timestamp (UTC)
Select (Default: 2020-09-15 14:10:17.00): 

Timezone
Select (Default: +09:00): 

Do you want to create stream entry? (Default [y]/n): n
File should have stream entry, but This don't have.
/> 

/> exit
Goodbye!
```

## Usage
debugfatfs support support these optoin. (Please look at man-page)
* **-a**, **--all** --- Trverse all directories
* **-b**, **--byte**=*offset* --- dump the any byte after dump filesystem information
* **-c**, **--cluster**=*index* --- dump the cluster index after dump filesystem information
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
* **alloc** *cluster* --- allocate cluster
* **release** *cluster* --- release cluster
* **fat** *index* *[entry]* --- change File Allocation Table entry
* **create** *file* --- create directory entry
* **remove** *file* --- remove directory entry
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
