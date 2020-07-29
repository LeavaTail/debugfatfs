#include <stdio.h>
#include <stdbool.h>
#include "dumpexfat.h"

int fat_show_boot_sec(struct device_info *, struct fat_bootsec *);
int fat16_show_boot_sec(struct device_info *, struct fat_bootsec *);
int fat32_show_boot_sec(struct device_info *, struct fat_bootsec *);
int fat_determine_type(struct device_info *, struct fat_bootsec *);

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

	info->sector_size = b->BPB_BytesPerSec;
	info->cluster_shift = b->BPB_SecPerClus;

	switch (info->fstype) {
		case FAT12_FILESYSTEM:
			/* FALLTHROUGH */
		case FAT16_FILESYSTEM:
			fat16_show_boot_sec(info, b);
			break;
		case FAT32_FILESYSTEM:
		{
			void *fsinfo;
			fat32_show_boot_sec(info, b);
			fsinfo = get_sector(info, b->reserved_info.fat32_reserved_info.BPB_FSInfo * info->sector_size, 1);
			free(fsinfo);
			break;
		}
		default:
			dump_err("Expected FAT filesystem, But this is not FAT filesystem.\n");
			return -1;
	}
	return 0;
}

/**
 * fat16_show_boot_sec - show boot sector in FAT12/16
 * @info:      structure to be shown device_info
 * @b:         boot sector pointer in FAT
 */
int fat16_show_boot_sec(struct device_info *info, struct fat_bootsec *b)
{
	int i;
	const char *type = (char *)b->reserved_info.fat16_reserved_info.BS_FilSysType;

	if(strncmp(type, "FAT", 3))
		dump_warn("BS_FilSysType is expected \"FAT     \", But this is %s\n", type);

	info->fat_offset = b->BPB_RevdSecCnt;
	info->fat_length = b->BPB_NumFATs * b->BPB_FATSz16;
	info->root_offset = info->fat_offset + info->fat_length;
	info->heap_offset = info->root_offset + (b->BPB_RootEntCnt * 32 + b->BPB_BytesPerSec - 1) / b->BPB_BytesPerSec;

	dump_notice("%-28s\t: ", "Volume ID");
	for(i = 0; i < VOLIDSIZE; i++)
		dump_notice("%x", b->reserved_info.fat16_reserved_info.BS_VolID[i]);
	dump_notice("\n");

	dump_notice("%-28s\t: ", "Volume Label");
	for(i = 0; i < VOLLABSIZE; i++)
		dump_notice("%c", b->reserved_info.fat16_reserved_info.BS_VolLab[i]);
	dump_notice("\n");
	return 0;
}

/**
 * fat32_show_boot_sec - show boot sector in FAT32
 * @info:      structure to be shown device_info
 * @b:         boot sector pointer in FAT
 */
int fat32_show_boot_sec(struct device_info *info, struct fat_bootsec *b)
{
	int i;
	const char *type = (char *)b->reserved_info.fat32_reserved_info.BS_FilSysType;

	if(strncmp(type, "FAT32", 5))
		dump_warn("BS_FilSysType is expected \"FAT32   \", But this is %s\n", type);

	info->fat_offset = b->BPB_RevdSecCnt;
	info->fat_length = b->BPB_NumFATs * b->reserved_info.fat32_reserved_info.BPB_FATSz32;
	info->heap_offset = info->fat_offset + info->fat_length;
	info->root_offset = info->heap_offset + (b->reserved_info.fat32_reserved_info.BPB_RootClus - 2) * b->BPB_SecPerClus * b->BPB_BytesPerSec;

	dump_notice("%-28s\t: ", "Volume ID");
	for(i = 0; i < VOLIDSIZE; i++)
		dump_notice("%x", b->reserved_info.fat32_reserved_info.BS_VolID[i]);
	dump_notice("\n");

	dump_notice("%-28s\t: ", "Volume Label");
	for(i = 0; i < VOLLABSIZE; i++)
		dump_notice("%c", b->reserved_info.fat32_reserved_info.BS_VolLab[i]);
	dump_notice("\n");

	dump_notice("%-28s\t: %8x\n", "Sectors Per FAT", b->reserved_info.fat32_reserved_info.BPB_FATSz32);
	dump_notice("%-28s\t: %8x (sector)\n", "The first sector of the Root", b->reserved_info.fat32_reserved_info.BPB_RootClus);
	dump_notice("%-28s\t: %8x (sector)\n", "FSINFO sector", b->reserved_info.fat32_reserved_info.BPB_FSInfo);
	dump_notice("%-28s\t: %8x (sector)\n", "Backup Boot sector", b->reserved_info.fat32_reserved_info.BPB_BkBootSec);
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
