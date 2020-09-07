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
$ sudo debugfatfs /dev/sdc1
media-relative sector offset    : 0x00000800 (sector)
Offset of the First FAT         : 0x00000080 (sector)
Length of FAT table             :        320 (sector)
Offset of the Cluster Heap      : 0x00000200 (sector)
The number of clusters          :      32760 (cluster)
The first cluster of the root   :          4 (cluster)
Size of exFAT volumes           :    2097152 (sector)
Bytes per sector                :        512 (byte)
Bytes per cluster               :      32768 (byte)
The number of FATs              :          1
The percentage of clusters      :          0 (%)

Read "/" Directory (4 entries).
System Volume Information 新しいフォルダー NEW_DIRECTORY Directory 
```

**Interactive mode**
```sh
$ sudo debugfatfs -i /dev/sdc1
Welcome to debugfatfs 0.1 (Interactive Mode)

/> ls
-HSD-    32768 2020-09-04 15:10:24 System Volume Information 
---D-    32768 2020-09-04 15:13:37 新しいフォルダー 
---D-    32768 2020-09-04 15:14:06 NEW_DIRECTORY 
---D-    32768 2020-09-04 15:14:23 Directory 

/> cd Directory
/Directory> ls

/Directory> create NEW_TEXT.TXT
/Directory> ls
----A        0 2020-09-06 19:20:23 NEW_TEXT.TXT 

/Directory> exit
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
* **create** *[option]* *file* --- create directory entry
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
