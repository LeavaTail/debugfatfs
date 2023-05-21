// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2021 LeavaTail
 */
#ifndef _DEBUGFATFS_H
#define _DEBUGFATFS_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <inttypes.h>

#include "list.h"
#include "nls.h"
#include "shell.h"
/**
 * Program Name, version, author.
 * displayed when 'usage' and 'version'
 */
#define PROGRAM_NAME     "debugfatfs"
#define PROGRAM_VERSION  "0.3.0"
#define PROGRAM_AUTHOR   "LeavaTail"
#define COPYRIGHT_YEAR   "2021"

/**
 * Debug code
 */
extern unsigned int print_level;
extern FILE *output;
#define PRINT_ERR      1
#define PRINT_WARNING  2
#define PRINT_INFO     3
#define PRINT_DEBUG    4

#define print(level, fmt, ...) \
	do { \
		if (print_level >= level) { \
			if (level == PRINT_DEBUG) \
			fprintf( output, "(%s:%u): " fmt, \
					__func__, __LINE__, ##__VA_ARGS__); \
			else \
			fprintf( output, "" fmt, ##__VA_ARGS__); \
		} \
	} while (0) \

#define pr_err(fmt, ...)   print(PRINT_ERR, fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)  print(PRINT_WARNING, fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)  print(PRINT_INFO, fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...) print(PRINT_DEBUG, fmt, ##__VA_ARGS__)
#define pr_msg(fmt, ...)   fprintf(output, fmt, ##__VA_ARGS__)

#define SECSIZE          512
#define DENTRY_LISTSIZE  1024
#define PATHNAME_MAX     4096

/*
 * FAT definition
 */
#define FAT16_CLUSTERS  4096
#define FAT32_CLUSTERS  65526
#define VOLIDSIZE       4
#define VOLLABSIZE      11
#define FILSYSTYPESIZE  8
#define BOOTCODESIZE    448
#define BOOTSIGNSIZE    2
#define FATSZ32SIZE     4
#define EXTFLAGSSIZE    2
#define FSVERSIZE       2
#define ROOTCLUSSIZE    4
#define FSINFOSIZE      2
#define BKBOOTSECSIZE   2
#define RESERVEDSIZE    12
#define BOOTCODE32SIZE  420
#define FSIRESV1SIZE    480
#define FSIRESV2SIZE    12

#define FAT_FSTCLUSTER  0x002
#define FAT12_RESERVED  0xFF8
#define FAT16_RESERVED  0xFFF8
#define FAT32_RESERVED  0x0FFFFFF8
/*
 * exFAT definition
 */
#define ACTIVEFAT     0x0001
#define VOLUMEDIRTY   0x0002
#define MEDIAFAILURE  0x0004
#define CLEARTOZERO   0x0008

#define EXFAT_FIRST_CLUSTER  2
#define EXFAT_BADCLUSTER     0xFFFFFFF7
#define EXFAT_LASTCLUSTER    0xFFFFFFFF

/*
 * FAT/exFAT definition
 */
#define JMPBOOTSIZE       3
#define ORMNAMESIZE       8
#define VOLUME_LABEL_MAX  11
#define LONGNAME_MAX      13
#define ENTRY_NAME_MAX    15
#define MAX_NAME_LENGTH   255

enum FStype
{
	FAT12_FILESYSTEM,
	FAT16_FILESYSTEM,
	FAT32_FILESYSTEM,
	EXFAT_FILESYSTEM,
	UNKNOWN,
};

struct device_info {
	char name[255];
	int fd;
	uint32_t attr;
	size_t total_size;
	size_t sector_size;
	size_t cluster_size;
	uint16_t cluster_count;
	enum FStype fstype;
	uint8_t flags;
	uint32_t fat_offset;
	uint32_t fat_length;
	uint32_t heap_offset;
	uint32_t root_offset;
	uint32_t root_length;
	uint8_t *alloc_table;
	uint32_t alloc_cluster;
	uint16_t *upcase_table;
	size_t upcase_size;
	uint16_t *vol_label;
	uint8_t vol_length;
	node2_t **root;
	size_t root_size;
	const struct operations *ops;
};

#define OPTION_ALL          (1 << 0)
#define OPTION_QUIET        (1 << 1)
#define OPTION_CLUSTER      (1 << 2)
#define OPTION_INTERACTIVE  (1 << 3)
#define OPTION_OUTPUT       (1 << 4)
#define OPTION_SECTOR       (1 << 5)
#define OPTION_UPPER        (1 << 6)
#define OPTION_SAVE         (1 << 7)
#define OPTION_LOAD         (1 << 8)
#define OPTION_READONLY     (1 << 9)
#define OPTION_DIRECTORY    (1 << 10)
#define OPTION_FORCE        (1 << 11)
#define OPTION_ENTRY        (1 << 12)

#define CREATE_DIRECTORY    (1 << 0)

struct directory {
	unsigned char *name;
	size_t namelen;
	size_t datalen;
	uint16_t attr;
	struct tm ctime;
	struct tm atime;
	struct tm mtime;
	uint16_t hash;
};

#define DIRECTORY_FILES  1024

struct fat_fileinfo {
	unsigned char name[13];
	unsigned char *uniname;
	size_t namelen;
	size_t datalen;
	uint8_t cached;
	uint16_t attr;
	struct tm ctime;
	struct tm atime;
	struct tm mtime;
};

struct exfat_fileinfo {
	unsigned char *name;
	size_t namelen;
	size_t datalen;
	uint8_t cached;
	uint16_t attr;
	uint8_t flags;
	struct tm ctime;
	struct tm atime;
	struct tm mtime;
	uint16_t hash;
};

struct pseudo_bootsec {
	unsigned char JumpBoot[JMPBOOTSIZE];
	unsigned char FileSystemName[ORMNAMESIZE];
	unsigned char reserved1[SECSIZE - JMPBOOTSIZE - ORMNAMESIZE];
};

struct fat_bootsec {
	unsigned char BS_JmpBoot[JMPBOOTSIZE];
	unsigned char BS_ORMName[ORMNAMESIZE];
	uint16_t BPB_BytesPerSec;
	uint8_t  BPB_SecPerClus;
	uint16_t BPB_RevdSecCnt;
	uint8_t  BPB_NumFATs;
	uint16_t BPB_RootEntCnt;
	uint16_t BPB_TotSec16;
	uint8_t  BPB_Media;
	uint16_t BPB_FATSz16;
	uint16_t BPB_SecPerTrk;
	uint16_t BPB_NumHeads;
	uint32_t BPB_HiddSec;
	uint32_t BPB_TotSec32;
	union reserved {
		struct {
			unsigned char BS_DrvNum;
			uint8_t  BS_Reserved1;
			unsigned char BS_BootSig;
			unsigned char BS_VolID[VOLIDSIZE];
			unsigned char BS_VolLab[VOLLABSIZE];
			unsigned char BS_FilSysType[FILSYSTYPESIZE];
			unsigned char BS_BootCode[BOOTCODESIZE];
			unsigned char BS_BootSign[BOOTSIGNSIZE];
		} __attribute__((packed)) fat16_reserved_info;

		struct {
			uint32_t BPB_FATSz32;
			unsigned char BPB_ExtFlags[EXTFLAGSSIZE];
			unsigned char BPB_FSVer[FSVERSIZE];
			uint32_t BPB_RootClus;
			uint16_t BPB_FSInfo;
			uint16_t BPB_BkBootSec;
			unsigned char BPB_Reserved[RESERVEDSIZE];
			unsigned char BS_DrvNum;
			uint8_t  BS_Reserved1;
			unsigned char BS_BootSig;
			unsigned char BS_VolID[VOLIDSIZE];
			unsigned char BS_VolLab[VOLLABSIZE];
			unsigned char BS_FilSysType[FILSYSTYPESIZE];
			unsigned char BS_BootCode32[BOOTCODE32SIZE];
			unsigned char BS_BootSign[BOOTSIGNSIZE];
		} __attribute__((packed)) fat32_reserved_info;
	} __attribute__((packed)) reserved_info;
} __attribute__((packed)) ;

struct fat32_fsinfo {
	u_int32_t FSI_LeadSig;
	unsigned char FSI_Reserved1[FSIRESV1SIZE];
	u_int32_t FSI_StrucSig;
	u_int32_t FSI_Free_Count;
	u_int32_t FSI_Nxt_Free;
	unsigned char FSI_Reserved2[FSIRESV2SIZE];
	u_int32_t FSI_TrailSig;
};

struct exfat_bootsec {
	unsigned char JumpBoot[JMPBOOTSIZE];
	unsigned char FileSystemName[ORMNAMESIZE];
	unsigned char MustBeZero[53];
	uint64_t  PartitionOffset;
	uint64_t VolumeLength;
	uint32_t FatOffset;
	uint32_t FatLength;
	uint32_t ClusterHeapOffset;
	uint16_t ClusterCount;
	uint32_t FirstClusterOfRootDirectory;
	uint32_t VolumeSerialNumber;
	uint16_t FileSystemRevision;
	uint16_t VolumeFlags;
	uint8_t BytesPerSectorShift;
	uint8_t SectorsPerClusterShift;
	uint8_t NumberOfFats;
	uint8_t DriveSelect;
	uint8_t PercentInUse;
	unsigned char Reserved[7];
	unsigned char BootCode[390];
	unsigned char BootSignature[2];
};

struct fat_dentry {
	union {
		/* Directory Structure */
		struct {
			unsigned char DIR_Name[11];
			uint8_t DIR_Attr;
			uint8_t DIR_NTRes;
			uint8_t DIR_CrtTimeTenth;
			uint16_t DIR_CrtTime;
			uint16_t DIR_CrtDate;
			uint16_t DIR_LstAccDate;
			uint16_t DIR_FstClusHI;
			uint16_t DIR_WrtTime;
			uint16_t DIR_WrtDate;
			uint16_t DIR_FstClusLO;
			uint32_t DIR_FileSize;
		} __attribute__((packed)) dir;
		/* Long File Name */
		struct {
			uint8_t LDIR_Ord;
			uint16_t LDIR_Name1[5];
			uint8_t LDIR_Attr;
			uint8_t LDIR_Type;
			uint8_t LDIR_Chksum;
			uint16_t LDIR_Name2[6];
			uint16_t LDIR_FstClusLO;
			uint16_t LDIR_Name3[2];
		} __attribute__((packed)) lfn;
	} __attribute__((packed)) dentry;
} __attribute__ ((packed));

struct exfat_dentry {
	uint8_t EntryType;
	union {
		/* Allocation Bitmap Directory Entry */
		struct {
			uint8_t BitmapFlags;
			unsigned char Reserved[18];
			uint32_t FirstCluster;
			uint64_t DataLength;
		} __attribute__((packed)) bitmap;
		/* Up-case Table Directory Entry */
		struct {
			unsigned char Reserved1[3];
			uint32_t TableCheckSum;
			unsigned char Reserved2[12];
			uint32_t FirstCluster;
			uint32_t DataLength;
		} __attribute__((packed)) upcase;
		/* Volume Label Directory Entry */
		struct {
			uint8_t CharacterCount;
			uint16_t VolumeLabel[VOLUME_LABEL_MAX];
			unsigned char Reserved[8];
		} __attribute__((packed)) vol;
		/* File Directory Entry */
		struct {
			uint8_t SecondaryCount;
			uint16_t SetChecksum;
			uint16_t FileAttributes;
			unsigned char Reserved1[2];
			uint32_t CreateTimestamp;
			uint32_t LastModifiedTimestamp;
			uint32_t LastAccessedTimestamp;
			uint8_t Create10msIncrement;
			uint8_t LastModified10msIncrement;
			uint8_t CreateUtcOffset;
			uint8_t LastModifiedUtcOffset;
			uint8_t LastAccessdUtcOffset;
			unsigned char Reserved2[7];
		} __attribute__((packed)) file;
		/* Volume GUID Directory Entry */
		struct {
			uint8_t SecondaryCount;
			uint16_t SetChecksum;
			uint16_t GeneralPrimaryFlags;
			unsigned char VolumeGuid[16];
			unsigned char Reserved[10];
		} __attribute__((packed)) guid;
		/* Stream Extension Directory Entry */
		struct {
			uint8_t GeneralSecondaryFlags;
			unsigned char Reserved1;
			uint8_t NameLength;
			uint16_t NameHash;
			unsigned char Reserved2[2];
			uint64_t ValidDataLength;
			unsigned char Reserved3[4];
			uint32_t FirstCluster;
			uint64_t DataLength;
		} __attribute__((packed)) stream;
		/* File Name Directory Entry */
		struct {
			uint8_t GeneralSecondaryFlags;
			uint16_t FileName[ENTRY_NAME_MAX];
		} __attribute__((packed)) name;
		/* Vendor Extension Directory Entry */
		struct {
			uint8_t GeneralSecondaryFlags;
			unsigned char VendorGuid[16];
			unsigned char VendorDefined[14];
		} __attribute__((packed)) vendor;
		/* Vendor Allocation Directory Entry */
		struct {
			uint8_t GeneralSecondaryFlags;
			unsigned char VendorGuid[16];
			unsigned char VendorDefined[2];
			uint32_t FirstCluster;
			uint64_t DataLength;
		} __attribute__((packed)) vendor_alloc;
	} __attribute__((packed)) dentry;
} __attribute__ ((packed));

struct operations {
	int (*statfs)(void);
	int (*info)(void);
	int (*lookup)(uint32_t, char *);
	int (*readdir)(struct directory *, size_t, uint32_t);
	int (*reload)(uint32_t);
	int (*convert)(const char *, size_t, char *);
	int (*clean)(uint32_t);
	int (*setfat)(uint32_t, uint32_t);
	int (*getfat)(uint32_t, uint32_t *);
	int (*dentry)(uint32_t, size_t);
	int (*alloc)(uint32_t);
	int (*release)(uint32_t);
	int (*create)(const char *, uint32_t, int);
	int (*remove)(const char *, uint32_t, int);
	int (*trim)(uint32_t);
	int (*fill)(uint32_t, uint32_t);
	int (*contents)(const char *, uint32_t, int);
};

#define TAIL_COUNT           10

/* FAT/exFAT File Attributes */
#define ATTR_READ_ONLY       0x01
#define ATTR_HIDDEN          0x02
#define ATTR_SYSTEM          0x04
#define ATTR_VOLUME_ID       0x08
#define ATTR_DIRECTORY       0x10
#define ATTR_ARCHIVE         0x20
#define ATTR_LONG_FILE_NAME  (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)
/* FAT dentry type */
#define LAST_LONG_ENTRY      0x40
#define DENTRY_DELETED       0xE5
/* exFAT dentry type */
#define DENTRY_UNUSED        0x00
#define DENTRY_BITMAP        0x81
#define DENTRY_UPCASE        0x82
#define DENTRY_VOLUME        0x83
#define DENTRY_FILE          0x85
#define DENTRY_GUID          0xA0
#define DENTRY_STREAM        0xC0
#define DENTRY_NAME          0xC1
#define DENTRY_VENDOR        0xE0
#define DENTRY_VENDOR_ALLOC  0xE1

/* exFAT EntryType */
#define EXFAT_TYPECODE       0x1F
#define EXFAT TYPEIMPORTANCE 0x20
#define EXFAT_CATEGORY       0x40
#define EXFAT_INUSE          0x80

/* exFAT GeneralSecondaryFlags */
#define ALLOC_POSIBLE         0x01
#define ALLOC_NOFATCHAIN      0x02

/* TimeStamp */
#define FAT_DAY      0
#define FAT_MONTH    5
#define FAT_YEAR     9
#define EXFAT_DSEC   0
#define EXFAT_MINUTE 5
#define EXFAT_HOUR   11
#define EXFAT_DAY    16
#define EXFAT_MONTH  21
#define EXFAT_YEAR   25

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static inline bool is_power2(unsigned int n)
{
	return (n != 0 && ((n & (n - 1)) == 0));
}

extern struct device_info info;

/* General function */
int get_sector(void *, off_t, size_t);
int get_cluster(void *, off_t);
int get_clusters(void *, off_t, size_t);
int set_sector(void *, off_t, size_t);
int set_cluster(void *, off_t);
int set_clusters(void *, off_t, size_t);
int print_cluster(uint32_t);
void hexdump(void *, size_t);
void gen_rand(char *, size_t);

/* exFAT/FAT check function */
int exfat_check_filesystem(struct pseudo_bootsec *);
int fat_check_filesystem(struct pseudo_bootsec *);

/* nls function */
int utf16_to_utf8(uint16_t *, uint16_t, unsigned char*);

#endif /*_DEBUGFATFS_H */
