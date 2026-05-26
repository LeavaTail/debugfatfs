# debugfatfs

[![Test](https://github.com/LeavaTail/debugfatfs/actions/workflows/test.yml/badge.svg)](https://github.com/LeavaTail/debugfatfs/actions/workflows/test.yml)
[![codecov](https://codecov.io/gh/LeavaTail/debugfatfs/branch/main/graph/badge.svg)](https://codecov.io/gh/LeavaTail/debugfatfs)

debugfatfs is a utility for inspecting and updating FAT/exFAT volume images without mounting the filesystem.

It is intended for filesystem and tooling developers who need to inspect on-disk metadata, reproduce damaged layouts, or prepare images for bug analysis. FAT and exFAT structures are documented by Microsoft in the [FAT specification][1] and the [exFAT specification][2].

[1]: https://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/fatgen103.doc
[2]: https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification

## Features

- Print FAT/exFAT boot sector information
- Dump raw sector, cluster, FAT entry, and file data
- Print file and directory metadata without mounting the image
- Traverse directories from the command line or interactive shell
- Create and remove file or directory entries
- Allocate and release clusters
- Rewrite FAT entries
- Compact deleted directory entries
- Convert exFAT names through the up-case table

## Safety

debugfatfs opens images read-write by default. Use `-r` or `--ro` when you only want to inspect an image.

Do not run write commands against a mounted filesystem image. debugfatfs checks `/etc/mtab` and rejects mounted targets unless read-only mode is selected, but it is still safest to work on a copy of the image when reproducing corruption or testing repair logic.

## Requirements

Runtime:

- UTF-8 locale

Build:

- autoconf
- automake
- libtool
- make
- gcc or another C compiler
- pkg-config

Tests:

- expect
- CUnit development package, for example `libcunit1-dev`
- tar
- bzip2 or lbzip2
- md5sum
- gcovr, when coverage is enabled

## Build

```bash
$ ./script/bootstrap.sh
$ ./configure
$ make
```

Install:

```bash
$ sudo make install
```

Build with coverage instrumentation:

```bash
$ ./configure --enable-gcov
$ make
```

## Test

Run the integration tests:

```bash
$ make check
```

Run the unit tests:

```bash
$ make -C tests/unit_test
$ ./tests/unit_test/test
```

The integration tests extract FAT12, FAT16, FAT32, and exFAT sample images from `tests/sample/*.tar.bz2`. See `docs/test_specification.md` for the image layout and test coverage.

## Usage

```text
Usage: debugfatfs [OPTION]... FILE [PATH]
```

`FILE` is a FAT/exFAT volume image or block device. `PATH` is an optional path inside the image; when provided, debugfatfs prints file metadata for that path.

Options:

- `-a`, `--all`: traverse all directories and print additional filesystem information
- `-b`, `--byte=offset`: dump one sector-sized block starting at byte offset `offset`
- `-c`, `--cluster=index`: dump cluster `index`
- `-f`, `--fat=index`: read the FAT entry for cluster `index`
- `-i`, `--interactive`: start the interactive shell
- `-o`, `--output=file`: write output to `file` instead of stdout
- `-q`, `--quiet`: suppress boot sector output
- `-r`, `--ro`: open the image read-only
- `-s`, `--script=file`: run interactive shell commands from `file`
- `-u`, `--upper=string`: convert `string` through the exFAT up-case table
- `-v`, `--verbose`: increase message verbosity
- `--no-update-checksum`: do not refresh directory-entry checksums after dentry edits
- `--update-checksum`: refresh directory-entry checksums after dentry edits
- `--help`: display command-line help
- `--version`: display version information

## Examples

Print boot sector information:

```bash
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

Dump a cluster:

```bash
$ sudo debugfatfs -c 4 /dev/sdc1
Cluster #4:
00000000:  83 0B 44 00 44 00 44 00 44 00 44 00 44 00 44 00  ..D.D.D.D.D.D.D.
00000010:  44 00 44 00 44 00 44 00 00 00 00 00 00 00 00 00  D.D.D.D.........
*
00007FF0:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
```

Print file metadata without mounting the filesystem:

```bash
$ sudo debugfatfs /dev/sdc1 /00/FILE1.TXT
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

Read a FAT entry:

```bash
$ sudo debugfatfs -f 11 /dev/sdc1
Get: Cluster 11 is FAT entry 0000000e
```

Start interactive mode:

```bash
$ sudo debugfatfs -i /dev/sdc2
Welcome to debugfatfs 0.4.0 (Interactive Mode)

/> ls
----A        0 2020-11-21 08:01:46 FILE
/> stat FILE
File Name:   FILE
File Size:   0
```

Run shell commands from a script file:

```text
# commands.txt
cd /00
dentry-set FILE1.TXT fat.short.DIR_NTRes 0x12
dentry-raw FILE1.TXT short 0x0c 1 0x34
```

```bash
$ debugfatfs --no-update-checksum --script commands.txt fat16.img
```

## Interactive Commands

- `ls`: list current directory contents
- `cd path`: change directory
- `cluster cluster`: dump raw cluster data
- `alloc cluster`: allocate a cluster
- `release cluster`: release a cluster
- `fat index [entry]`: read or update a FAT entry. `entry` is parsed as hexadecimal
- `create file`: create a file directory entry
- `mkdir directory`: create a directory entry
- `remove file`: remove a file directory entry
- `rmdir directory`: remove a directory entry
- `trim`: remove deleted directory entries from the current directory
- `fill [count]`: fill the current directory with generated entries
- `tail file`: print file contents
- `stat file`: print file metadata
- `dentry file`: print FAT/exFAT directory entries for a file
- `dentry-set file field value`: update a FAT/exFAT directory-entry field without refreshing checksums
- `dentry-raw file entry offset size value`: update raw bytes in a FAT/exFAT directory entry
- `help`: display interactive help
- `exit`: exit interactive mode

Interactive commands can also be read from a file with `-s` or `--script`. Empty lines are ignored, lines starting with `#` are treated as comments, and reaching EOF exits the shell.

Commands that modify the image are intended for test-image preparation and bug reproduction. Prefer `-r` for inspection-only sessions.

The `dentry`, `dentry-set`, and `dentry-raw` commands support FAT and exFAT images. They are intended for creating corrupted images for fsck testing. By default they do not refresh FAT LFN checksums or exFAT set checksums; pass `--update-checksum` when you want checksums recalculated after the edit. For example:

```text
/> dentry /00/FILE1.TXT
/> dentry-set /00/FILE1.TXT fat.short.DIR_FileSize 0xffffffff
/> dentry-set /01/ABCDEFGHIJKLMNOPQRSTUVWXYZ! fat.lfn[0].LDIR_Chksum 0x00
/> dentry-set /00/FILE1.TXT exfat.stream.DataLength 0xffffffff
/> dentry-raw /00/FILE1.TXT short 0x0c 1 0x34
```

Supported `dentry-set` FAT fields:

- `fat.short.DIR_Attr`
- `fat.short.DIR_NTRes`
- `fat.short.DIR_CrtTimeTenth`
- `fat.short.DIR_CrtTime`
- `fat.short.DIR_CrtDate`
- `fat.short.DIR_LstAccDate`
- `fat.short.DIR_FstClusHI`
- `fat.short.DIR_WrtTime`
- `fat.short.DIR_WrtDate`
- `fat.short.DIR_FstClusLO`
- `fat.short.DIR_FileSize`
- `fat.lfn[N].LDIR_Ord`
- `fat.lfn[N].LDIR_Attr`
- `fat.lfn[N].LDIR_Type`
- `fat.lfn[N].LDIR_Chksum`
- `fat.lfn[N].LDIR_FstClusLO`

`dentry-raw` accepts `short`, `lfnN`, or `lfn[N]` as the entry selector. `offset`, `size`, and `value` accept decimal or `0x` hexadecimal numbers. The supported write sizes are 1, 2, 4, and 8 bytes.

Supported `dentry-set` exFAT fields:

- `exfat.file.EntryType`
- `exfat.file.SecondaryCount`
- `exfat.file.SetChecksum`
- `exfat.file.FileAttributes`
- `exfat.file.CreateTimestamp`
- `exfat.file.LastModifiedTimestamp`
- `exfat.file.LastAccessedTimestamp`
- `exfat.file.Create10msIncrement`
- `exfat.file.LastModified10msIncrement`
- `exfat.file.CreateUtcOffset`
- `exfat.file.LastModifiedUtcOffset`
- `exfat.file.LastAccessdUtcOffset`
- `exfat.stream.EntryType`
- `exfat.stream.GeneralSecondaryFlags`
- `exfat.stream.NameLength`
- `exfat.stream.NameHash`
- `exfat.stream.ValidDataLength`
- `exfat.stream.FirstCluster`
- `exfat.stream.DataLength`
- `exfat.name[N].EntryType`
- `exfat.name[N].GeneralSecondaryFlags`

For exFAT, `dentry-raw` accepts `file`, `stream`, `nameN`, or `name[N]` as the entry selector.

## Documentation

- `docs/test_specification.md`: sample image layout and tested behavior
- `docs/design_directory_chain.md`: in-memory directory cache design

## Authors

[LeavaTail](https://github.com/LeavaTail)
