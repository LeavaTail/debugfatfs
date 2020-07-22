#ifndef _DUMPEXFAT_H
#define _DUMPEXFAT_H

#include <stdint.h>
#include <string.h>

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
#define DUMP_EMERG    0
#define DUMP_ALERT    1
#define DUMP_CRIT     2
#define DUMP_ERR      3
#define DUMP_WARNING  4
#define DUMP_NOTICE   5
#define DUMP_INFO     6
#define DUMP_DEBUG    7 

#define dump_msg(level, fmt, ...)								\
	do {														\
		if (print_level >= level) {								\
			if (print_level <= DUMP_ERR)						\
			fprintf( stderr, "(%s.%u):" fmt, 				\
					__func__, __LINE__, ##__VA_ARGS__);		\
			else												\
			fprintf( stdout, "(%s:%u):" fmt,				\
					__func__, __LINE__, ##__VA_ARGS__); 	\
		}														\
	} while (0) \

#define dump_err(fmt, ...)  dump_msg(DUMP_ERR, fmt, ##__VA_ARGS__)
#define dump_warn(fmt, ...)  dump_msg(DUMP_WARNING, fmt, ##__VA_ARGS__)
#define dump_notice(fmt, ...)  dump_msg(DUMP_NOTICE, fmt, ##__VA_ARGS__)
#define dump_info(fmt, ...)  dump_msg(DUMP_INFO, fmt, ##__VA_ARGS__)
#define dump_debug(fmt, ...)  dump_msg(DUMP_DEBUG, fmt, ##__VA_ARGS__)

bool verbose;
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
/*
 * FAT/exFAT definition
 */
#define JMPBOOTSIZE  3
#define ORMNAMESIZE  8

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
	size_t total_size;
	size_t sector_size;
	uint8_t cluster_shift;
	enum FStype fstype;
	unsigned short flags;
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
		} fat12_reserved_info;

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
		} fat32_reserved_info;
	} reserved_info;
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

/* FAT function*/
int fat_show_boot_sec(struct device_info *, struct fat_bootsec *);

/* exFAT function */
int exfat_show_boot_sec(struct device_info *, struct exfat_bootsec *);

#endif /*_DUMPEXFAT_H */
