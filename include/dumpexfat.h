#ifndef _DUMPEXFAT_H
#define _DUMPEXFAT_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "list.h"
/**
 * Program Name, version, author.
 * displayed when 'usage' and 'version'
 */
#define PROGRAM_NAME	"dumpexfat"
#define PROGRAM_VERSION	"0.1"
#define PROGRAM_AUTHOR	"LeavaTail"
#define COPYRIGHT_YEAR	"2020"

/**
 * Debug code
 */
extern unsigned int print_level;
extern FILE *output;
#define DUMP_EMERG    0
#define DUMP_ALERT    1
#define DUMP_CRIT     2
#define DUMP_ERR      3
#define DUMP_WARNING  4
#define DUMP_NOTICE   5
#define DUMP_INFO     6
#define DUMP_DEBUG    7 

#define dump_msg(level, fmt, ...)							\
	do {													\
		if (print_level >= level) {							\
			if (level <= DUMP_ERR)							\
			fprintf( stderr, "" fmt, ##__VA_ARGS__);		\
			else if(level <= DUMP_INFO)						\
			fprintf( output, "" fmt, ##__VA_ARGS__); 		\
			else if(level <= DUMP_DEBUG)					\
			fprintf( output, "(%s:%u): " fmt,				\
					__func__, __LINE__, ##__VA_ARGS__);		\
		}													\
	} while (0) \

#define dump_err(fmt, ...)  dump_msg(DUMP_ERR, fmt, ##__VA_ARGS__)
#define dump_warn(fmt, ...)  dump_msg(DUMP_WARNING, fmt, ##__VA_ARGS__)
#define dump_notice(fmt, ...)  dump_msg(DUMP_NOTICE, fmt, ##__VA_ARGS__)
#define dump_info(fmt, ...)  dump_msg(DUMP_INFO, fmt, ##__VA_ARGS__)
#define dump_debug(fmt, ...)  dump_msg(DUMP_DEBUG, fmt, ##__VA_ARGS__)

#define SECSIZE 512

/*
 * FAT definition
 */
#define FAT16_CLUSTERS	4096
#define FAT32_CLUSTERS	65526
#define VOLIDSIZE		4
#define VOLLABSIZE		11
#define FILSYSTYPESIZE	8
#define BOOTCODESIZE	448
#define BOOTSIGNSIZE	2
#define FATSZ32SIZE		4
#define EXTFLAGSSIZE	2
#define FSVERSIZE		2
#define ROOTCLUSSIZE	4
#define FSINFOSIZE		2
#define BKBOOTSECSIZE	2
#define RESERVEDSIZE	12
#define BOOTCODE32SIZE	420

/*
 * exFAT definition
 */
#define ACTIVEFAT		0x0001
#define VOLUMEDIRTY		0x0002
#define MEDIAFAILURE	0x0004
#define CLEARTOZERO		0x0008

#define EXFAT_FIRST_CLUSTER	2

/*
 * FAT/exFAT definition
 */
#define JMPBOOTSIZE			3
#define ORMNAMESIZE			8
#define VOLUME_LABEL_MAX	11
#define ENTRY_NAME_MAX		15

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
	bool force;
	size_t total_size;
	size_t sector_size;
	uint8_t cluster_shift;
	uint16_t cluster_count;
	enum FStype fstype;
	unsigned short flags;
	uint32_t fat_offset;
	uint32_t fat_length;
	uint32_t heap_offset;
	uint32_t root_offset;
	node_t *chain_head;
	uint16_t *upcase_table;
	size_t upcase_size;
};

struct pseudo_bootsector {
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
		} __attribute__((packed)) fat12_reserved_info;

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

struct exfat_dentry {
	uint8_t EntryType;
	union dentry {
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

/* FAT dentry type */
#define ATTR_READ_ONLY		0x01
#define ATTR_HIDDEN			0x02
#define ATTR_SYSTEM			0x04
#define ATTR_VOLUME_ID		0x08
#define ATTR_DIRECTORY		0x10
#define ATTR_ARCHIVE		0x20
/* exFAT dentry type */
#define DENTRY_UNUSED		0x00
#define DENTRY_BITMAP		0x81
#define DENTRY_UPCASE		0x82
#define DENTRY_VOLUME		0x83
#define DENTRY_FILE			0x85
#define DENTRY_GUID			0xA0
#define DENTRY_STREAM		0xC0
#define DENTRY_NAME			0xC1
#define DENTRY_VENDOR		0xE0
#define DENTRY_VENDOR_ALLOC	0xE1

/* TimeStamp */
#define EXFAT_DSEC		0
#define EXFAT_MINUTE	5
#define EXFAT_HOUR		11
#define EXFAT_DAY		16
#define EXFAT_MONTH		21
#define EXFAT_YEAR		25

/* General function */
void *get_sector(struct device_info *, off_t, size_t);
void *get_cluster(struct device_info *, off_t);
void *get_clusters(struct device_info *, off_t, size_t);
void hexdump(FILE *, void *, size_t);

/* FAT function*/
int fat_show_boot_sec(struct device_info *, struct fat_bootsec *);

/* exFAT function */
int exfat_show_boot_sec(struct device_info *, struct exfat_bootsec *);
int exfat_load_root_dentry(struct device_info *, void *);
int exfat_print_cluster(struct device_info *, uint32_t);

#endif /*_DUMPEXFAT_H */
