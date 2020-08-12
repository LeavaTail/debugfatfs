#include <stdio.h>
#include <stdbool.h>
#include "dumpexfat.h"

static int fat16_print_boot_sec(struct fat_bootsec *);
static int fat32_print_boot_sec(struct fat_bootsec *);
static int fat32_print_fsinfo(struct fat32_fsinfo *);
static int fat_load_boot_sec(struct fat_bootsec *);

/**
 * fat_print_boot_sec - print boot sector in FAT12/16/32
 * @b:         boot sector pointer in FAT
 *
 * TODO: implement function in FAT12/16/32
 */
int fat_print_boot_sec(void)
{
	int ret = 0;
	struct fat_bootsec *b = (struct fat_bootsec *)malloc(sizeof(struct fat_bootsec));

	fat_load_boot_sec(b);
	pr_msg("%-28s\t: %8u (byte)\n", "Bytes per Sector", b->BPB_BytesPerSec);
	pr_msg("%-28s\t: %8u (sector)\n", "Sectors per cluster", b->BPB_SecPerClus);
	pr_msg("%-28s\t: %8u (sector)\n", "Reserved Sector", b->BPB_RevdSecCnt);
	pr_msg("%-28s\t: %8u\n", "FAT count", b->BPB_NumFATs);
	pr_msg("%-28s\t: %8u\n", "Root Directory entry count", b->BPB_RootEntCnt);
	pr_msg("%-28s\t: %8u (sector)\n", "Sector count in Volume", b->BPB_TotSec16);

	switch (info.fstype) {
		case FAT12_FILESYSTEM:
			/* FALLTHROUGH */
		case FAT16_FILESYSTEM:
			fat16_print_boot_sec(b);
			break;
		case FAT32_FILESYSTEM:
			{
				void *fsinfo;
				fsinfo = malloc(info.sector_size);
				fat32_print_boot_sec(b);
				get_sector(fsinfo,
						b->reserved_info.fat32_reserved_info.BPB_FSInfo * info.sector_size, 1);
				fat32_print_fsinfo(fsinfo);
				free(fsinfo);
				break;
			}
		default:
			pr_err("Expected FAT filesystem, But this is not FAT filesystem.\n");
			ret = -1;
	}
	free(b);
	return ret;
}

/**
 * fat16_print_boot_sec - print boot sector in FAT12/16
 * @b:         boot sector pointer in FAT
 */
static int fat16_print_boot_sec(struct fat_bootsec *b)
{
	int i;
	const char *type = (char *)b->reserved_info.fat16_reserved_info.BS_FilSysType;

	if(strncmp(type, "FAT", 3))
		pr_warn("BS_FilSysType is expected \"FAT     \", But this is %s\n", type);

	info.fat_offset = b->BPB_RevdSecCnt;
	info.fat_length = b->BPB_NumFATs * b->BPB_FATSz16;
	info.root_offset = info.fat_offset + info.fat_length;
	info.heap_offset = info.root_offset +
		(b->BPB_RootEntCnt * 32 + b->BPB_BytesPerSec - 1) / b->BPB_BytesPerSec;

	pr_msg("%-28s\t: ", "Volume ID");
	for(i = 0; i < VOLIDSIZE; i++)
		pr_msg("%x", b->reserved_info.fat16_reserved_info.BS_VolID[i]);
	pr_msg("\n");

	pr_msg("%-28s\t: ", "Volume Label");
	for(i = 0; i < VOLLABSIZE; i++)
		pr_msg("%c", b->reserved_info.fat16_reserved_info.BS_VolLab[i]);
	pr_msg("\n");
	return 0;
}

/**
 * fat32_print_boot_sec - print boot sector in FAT32
 * @b:         boot sector pointer in FAT
 */
static int fat32_print_boot_sec(struct fat_bootsec *b)
{
	int i;
	const char *type = (char *)b->reserved_info.fat32_reserved_info.BS_FilSysType;

	if(strncmp(type, "FAT32", 5))
		pr_warn("BS_FilSysType is expected \"FAT32   \", But this is %s\n", type);

	info.fat_offset = b->BPB_RevdSecCnt;
	info.fat_length = b->BPB_NumFATs * b->reserved_info.fat32_reserved_info.BPB_FATSz32;
	info.heap_offset = info.fat_offset + info.fat_length;
	info.root_offset = info.heap_offset +
		(b->reserved_info.fat32_reserved_info.BPB_RootClus - 2) *
		b->BPB_SecPerClus * b->BPB_BytesPerSec;

	pr_msg("%-28s\t: ", "Volume ID");
	for(i = 0; i < VOLIDSIZE; i++)
		pr_msg("%x", b->reserved_info.fat32_reserved_info.BS_VolID[i]);
	pr_msg("\n");

	pr_msg("%-28s\t: ", "Volume Label");
	for(i = 0; i < VOLLABSIZE; i++)
		pr_msg("%c", b->reserved_info.fat32_reserved_info.BS_VolLab[i]);
	pr_msg("\n");

	pr_msg("%-28s\t: %8x\n", "Sectors Per FAT",
			b->reserved_info.fat32_reserved_info.BPB_FATSz32);
	pr_msg("%-28s\t: %8x (sector)\n", "The first sector of the Root",
			b->reserved_info.fat32_reserved_info.BPB_RootClus);
	pr_msg("%-28s\t: %8x (sector)\n", "FSINFO sector",
			b->reserved_info.fat32_reserved_info.BPB_FSInfo);
	pr_msg("%-28s\t: %8x (sector)\n", "Backup Boot sector",
			b->reserved_info.fat32_reserved_info.BPB_BkBootSec);
	return 0;
}

/**
 * fat32_print_fsinfo - print FSinfo Structure in FAT32
 * @b:         boot sector pointer in FAT
 */
static int fat32_print_fsinfo(struct fat32_fsinfo *fsi)
{
	if((fsi->FSI_LeadSig != 0x41615252) ||
			(fsi->FSI_StrucSig != 0x61417272) ||
			(fsi->FSI_TrailSig != 0xAA550000))
		pr_warn("FSinfo is expected specific sigunature, But this is difference.\n");

	pr_msg("%-28s\t: %8u (cluster)\n", "free cluster count", fsi->FSI_Free_Count);
	pr_msg("%-28s\t: %8u (cluster)\n", "first available cluster", fsi->FSI_Nxt_Free);
	return 0;
}

int fat_print_cluster(uint32_t index)
{
	void *data;
	data = malloc(info.cluster_size);
	get_cluster(data, index);
	pr_msg("Cluster #%u:\n", index);
	hexdump(output, data, info.cluster_size);
	free(data);
	return 0;
}

/**
 * fat_load_boot_sec - load boot sector in FAT
 * @b:         boot sector pointer in FAT
 */
static int fat_load_boot_sec(struct fat_bootsec *b)
{
	return get_sector(b, 0, 1);
}

/**
 * fat_check_filesystem - Whether or not VFAT filesystem
 * @index:         index of the cluster want to check
 *
 * return:     1 (Image is FAT12/16/32 filesystem)
 *             0 (Image isn't FAT12/16/32 filesystem)
 */
int fat_check_filesystem(struct pseudo_bootsec *boot, struct operations *ops)
{
	struct fat_bootsec *b = (struct fat_bootsec*)boot;
	uint16_t RootDirSectors = ((b->BPB_RootEntCnt * 32) +
			(b->BPB_BytesPerSec -1)) / b->BPB_BytesPerSec;
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
		info.fstype = FAT12_FILESYSTEM;
	} else if (CountofClusters < FAT32_CLUSTERS - 1) {
		info.fstype = FAT16_FILESYSTEM;
	} else {
		info.fstype = FAT32_FILESYSTEM;
	}

	info.sector_size = b->BPB_BytesPerSec;
	info.cluster_size = b->BPB_SecPerClus * b->BPB_BytesPerSec;

	ops->statfs = fat_print_boot_sec;
	ops->lookup = NULL;
	ops->readdir = NULL;
	ops->readdirs = NULL;
	ops->convert = NULL;
	ops->print_cluster = fat_print_cluster;

	return 0;
}
