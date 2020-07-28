#include <stdio.h>
#include <stdbool.h>
#include "dumpexfat.h"

/**
 * fat_show_boot_sec - show boot sector in FAT12/16/32
 * @info:      structure to be shown device_info
 * @b:         boot sector pointer in FAT
 *
 * TODO: implement function in FAT12/16/32
 */
int fat_show_boot_sec(struct device_info *info, struct fat_bootsec *b)
{
	fat_determine_type(info, b);
	
	dump_notice("%-28s\t: %8u (byte)\n", "Bytes per Sector", b->BPB_BytesPerSec);
	dump_notice("%-28s\t: %8u (sector)\n", "Sectors per cluster", b->BPB_SecPerClus);
	dump_notice("%-28s\t: %8u (sector)\n", "Reserved Sector", b->BPB_RevdSecCnt);
	dump_notice("%-28s\t: %8u\n", "FAT count", b->BPB_NumFATs);
	dump_notice("%-28s\t: %8u\n", "Root Directory entry count", b->BPB_RootEntCnt);
	dump_notice("%-28s\t: %8u (sector)\n", "Sector count in Volume", b->BPB_TotSec16);

	if (b->BPB_FATSz16) {
		info->fstype = FAT12_FILESYSTEM;
	} else {
		info->fstype = FAT32_FILESYSTEM;
	}
	return 0;
}

/**
 * fat_determine_type - Determination of FAT type
 * @info:      structure to be shown device_info
 * @b:         boot sector pointer in FAT
 */
int fat_determine_type(struct device_info *info, struct fat_bootsec *b)
{
	uint16_t RootDirSectors = ((b->BPB_RootEntCnt * 32) + (b->BPB_BytesPerSec -1)) / b->BPB_BytesPerSec;
	uint32_t FATSz;
	uint32_t TotSec;
	uint32_t DataSec;
	uint32_t CountofClusters;

	if (b->BPB_FATSz16 != 0) {
		FATSz = b->BPB_FATSz16;
		TotSec = b->BPB_TotSec16;
	} else {
		FATSz = b->reserved_info.fat32_reserved_info.BPB_FATSz32;
		TotSec = b->BPB_TotSec32;
	}

	DataSec = TotSec - (b->BPB_RevdSecCnt + (b->BPB_NumFATs * FATSz) + RootDirSectors);
	CountofClusters = DataSec / b->BPB_SecPerClus;

	if (CountofClusters < FAT16_CLUSTERS - 1) {
		info->fstype = FAT12_FILESYSTEM;
	} else if (CountofClusters < FAT32_CLUSTERS - 1) {
		info->fstype = FAT16_FILESYSTEM;
	} else {
		info->fstype = FAT32_FILESYSTEM;
	}

	return 0;
}
