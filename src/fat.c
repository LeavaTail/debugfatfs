#include <stdio.h>
#include <stdbool.h>
#include "debugfatfs.h"

/* Generic function prototype */
static uint32_t fat_concat_cluster(uint32_t, void **, size_t);
/* Boot sector function prototype */
static int fat_load_bootsec(struct fat_bootsec *);
static int fat_validate_bootsec(struct fat_bootsec *);
static int fat16_print_bootsec(struct fat_bootsec *);
static int fat32_print_bootsec(struct fat_bootsec *);
static int fat32_print_fsinfo(struct fat32_fsinfo *);
/* FAT-entry function prototype */
static int fat12_set_fat_entry(uint32_t, uint32_t);
static int fat16_set_fat_entry(uint32_t, uint32_t);
static int fat32_set_fat_entry(uint32_t, uint32_t);
static uint32_t fat12_get_fat_entry(uint32_t);
static uint32_t fat16_get_fat_entry(uint32_t);
static uint32_t fat32_get_fat_entry(uint32_t);
/* Directory chain function prototype */
static int fat_check_dchain(uint32_t);
static int fat_get_index(uint32_t);
/* File function prototype */
/* Operations function prototype */
int fat_print_bootsec(void);
int fat_set_fat_entry(uint32_t, uint32_t);
int fat_get_fat_entry(uint32_t, uint32_t *);

static const struct operations fat_ops = {
	.statfs = fat_print_bootsec,
	.setfat = fat_set_fat_entry,
	.getfat = fat_get_fat_entry,
};

/*************************************************************************************************/
/*                                                                                               */
/* GENERIC FUNCTION                                                                              */
/*                                                                                               */
/*************************************************************************************************/
/**
 * fat_concat_cluster   - Contatenate cluster @data with next_cluster
 * @clu:                  index of the cluster
 * @data:                 The cluster
 * @size:                 allocated size to store cluster data
 *
 * @retrun:        next cluster (@clu has next cluster)
 *                 0            (@clu doesn't have next cluster, or failed to realloc)
 */
static uint32_t fat_concat_cluster(uint32_t clu, void **data, size_t size)
{
	uint32_t ret;
	void *tmp;
	fat_get_fat_entry(clu, &ret);

	if (ret) {
		tmp = realloc(*data, size + info.cluster_size);
		if (tmp) {
			*data = tmp;
			get_cluster(tmp + size, ret);
			pr_debug("Concatenate cluster #%u with #%u\n", clu, ret);
		} else {
			pr_err("Failed to Get new memory.\n");
			ret = 0;
		}
	}
	return ret;
}

/**
 * fat_check_filesystem - Whether or not VFAT filesystem
 * @index:         index of the cluster want to check
 *
 * return:     1 (Image is FAT12/16/32 filesystem)
 *             0 (Image isn't FAT12/16/32 filesystem)
 */
int fat_check_filesystem(struct pseudo_bootsec *boot)
{
	struct fat_bootsec *b = (struct fat_bootsec *)boot;
	uint16_t RootDirSectors = ((b->BPB_RootEntCnt * 32) +
			(b->BPB_BytesPerSec - 1)) / b->BPB_BytesPerSec;
	uint32_t FATSz;
	uint32_t TotSec;
	uint32_t DataSec;
	uint32_t CountofClusters;

	if (!fat_validate_bootsec(b))
		return 0;

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
	info.cluster_count = CountofClusters;
	info.fat_offset = b->BPB_RevdSecCnt;
	info.fat_length = b->BPB_NumFATs * FATSz;
	info.heap_offset = info.fat_offset + info.fat_length;
	if (info.fstype == FAT32_FILESYSTEM)
		info.root_offset = b->reserved_info.fat32_reserved_info.BPB_RootClus;

	info.ops = &fat_ops;
	return 1;
}

/*************************************************************************************************/
/*                                                                                               */
/* BOOT SECTOR FUNCTION                                                                          */
/*                                                                                               */
/*************************************************************************************************/
/**
 * fat_load_bootsec - load boot sector in FAT
 * @b:         boot sector pointer in FAT
 */
static int fat_load_bootsec(struct fat_bootsec *b)
{
	return get_sector(b, 0, 1);
}

/**
 * fat_validate_bootsec - check whether boot sector is vaild
 * @b:                    boot sector pointer in FAT
 */
static int fat_validate_bootsec(struct fat_bootsec *b)
{
	int ret = 1;
	uint8_t media = b->BPB_Media;
	uint16_t sector = b->BPB_BytesPerSec / SECSIZE;
	uint8_t cluster = b->BPB_SecPerClus;

	if (!b->BPB_RevdSecCnt) {
		pr_debug("invalid reserved sectors: %x\n", b->BPB_RevdSecCnt);
		ret = 0;
	}

	if (!b->BPB_NumFATs) {
		pr_debug("invalid FAT structure: %x\n", b->BPB_NumFATs);
		ret = 0;
	}

	if (media != 0xf0 && media < 0xF8) {
		pr_debug("invalid Media value: %x\n", b->BPB_Media);
		ret = 0;
	}

	if (!is_power2(sector) || sector > 8) {
		pr_debug("invalid Sector size: %u\n", b->BPB_BytesPerSec);
		ret = 0;
	}

	if (!is_power2(cluster) || cluster > 128) {
		pr_debug("invalid Cluster size: %u\n", b->BPB_SecPerClus);
		ret = 0;
	}

	return ret;
}

/**
 * fat16_print_bootsec - print boot sector in FAT12/16
 * @b:         boot sector pointer in FAT
 */
static int fat16_print_bootsec(struct fat_bootsec *b)
{
	int i;
	const char *type = (char *)b->reserved_info.fat16_reserved_info.BS_FilSysType;

	if (strncmp(type, "FAT", 3))
		pr_warn("BS_FilSysType is expected \"FAT     \", But this is %s\n", type);

	pr_msg("%-28s\t: ", "Volume ID");
	for (i = 0; i < VOLIDSIZE; i++)
		pr_msg("%x", b->reserved_info.fat16_reserved_info.BS_VolID[i]);
	pr_msg("\n");

	pr_msg("%-28s\t: ", "Volume Label");
	for (i = 0; i < VOLLABSIZE; i++)
		pr_msg("%c", b->reserved_info.fat16_reserved_info.BS_VolLab[i]);
	pr_msg("\n");
	return 0;
}

/**
 * fat32_print_bootsec - print boot sector in FAT32
 * @b:         boot sector pointer in FAT
 */
static int fat32_print_bootsec(struct fat_bootsec *b)
{
	int i;
	const char *type = (char *)b->reserved_info.fat32_reserved_info.BS_FilSysType;

	if (strncmp(type, "FAT32", 5))
		pr_warn("BS_FilSysType is expected \"FAT32   \", But this is %s\n", type);

	pr_msg("%-28s\t: ", "Volume ID");
	for (i = 0; i < VOLIDSIZE; i++)
		pr_msg("%x", b->reserved_info.fat32_reserved_info.BS_VolID[i]);
	pr_msg("\n");

	pr_msg("%-28s\t: ", "Volume Label");
	for (i = 0; i < VOLLABSIZE; i++)
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
	if ((fsi->FSI_LeadSig != 0x41615252) ||
			(fsi->FSI_StrucSig != 0x61417272) ||
			(fsi->FSI_TrailSig != 0xAA550000))
		pr_warn("FSinfo is expected specific sigunature, But this is difference.\n");

	pr_msg("%-28s\t: %8u (cluster)\n", "free cluster count", fsi->FSI_Free_Count);
	pr_msg("%-28s\t: %8u (cluster)\n", "first available cluster", fsi->FSI_Nxt_Free);
	return 0;
}

/*************************************************************************************************/
/*                                                                                               */
/* FAT-ENTRY FUNCTION                                                                            */
/*                                                                                               */
/*************************************************************************************************/
/**
 * fat12_set_fat_entry - Set FAT Entry to any cluster
 * @clu:                 index of the cluster want to check
 * @entry:               any cluster index
 *
 * @retrun:              0
 */
static int fat12_set_fat_entry(uint32_t clu, uint32_t entry)
{
	uint32_t FATOffset = clu + (clu / 2);
	uint32_t ThisFATEntOffset = FATOffset % info.sector_size;

	uint16_t *fat;
	fat = malloc(info.sector_size);
	get_sector(fat, info.fat_offset * info.sector_size, 1);
	if (clu % 2) {
		*(fat + ThisFATEntOffset) = (fat[ThisFATEntOffset] & 0x0F) | entry << 4;
		*(fat + ThisFATEntOffset + 1) = entry >> 4;
	} else {
		*(fat + ThisFATEntOffset) = entry;
		*(fat + ThisFATEntOffset + 1) = (fat[ThisFATEntOffset + 1] & 0xF0) | ((entry >> 8) & 0x0F);
	}
	set_sector(fat, info.fat_offset * info.sector_size, 1);
	free(fat);
	return 0;
}

/**
 * fat16_set_fat_entry - Set FAT Entry to any cluster
 * @clu:                 index of the cluster want to check
 * @entry:               any cluster index
 *
 * @retrun:              0
 */
static int fat16_set_fat_entry(uint32_t clu, uint32_t entry)
{
	uint32_t FATOffset = clu * sizeof(uint16_t);
	uint32_t ThisFATEntOffset = FATOffset % info.sector_size;

	uint16_t *fat;
	fat = malloc(info.sector_size);
	get_sector(fat, info.fat_offset * info.sector_size, 1);
	*(fat + ThisFATEntOffset) = (uint16_t)entry;
	set_sector(fat, info.fat_offset * info.sector_size, 1);
	free(fat);
	return 0;
}

/**
 * fat32_set_fat_entry - Set FAT Entry to any cluster
 * @clu:                 index of the cluster want to check
 * @entry:               any cluster index
 *
 * @retrun:              0
 */
static int fat32_set_fat_entry(uint32_t clu, uint32_t entry)
{
	uint32_t FATOffset = clu * sizeof(uint32_t);
	uint32_t ThisFATEntOffset = FATOffset % info.sector_size;

	uint32_t *fat;
	fat = malloc(info.sector_size);
	get_sector(fat, info.fat_offset * info.sector_size, 1);
	*(fat + ThisFATEntOffset) = entry & 0x0FFFFFFF;
	set_sector(fat, info.fat_offset * info.sector_size, 1);
	free(fat);
	return 0;
}

/**
 * fat12_get_fat_entry - Get cluster is continuous
 * @clu:                 index of the cluster want to check
 *
 * @retrun:              next cluster (@clu has next cluster)
 *                       0            (@clu doesn't have next cluster)
 */
static uint32_t fat12_get_fat_entry(uint32_t clu)
{
	uint32_t ret = 0;
	uint32_t FATOffset = clu + (clu / 2);
	uint32_t ThisFATEntOffset = FATOffset % info.sector_size;

	uint16_t *fat;
	fat = malloc(info.sector_size);
	get_sector(fat, info.fat_offset * info.sector_size, 1);
	if (clu % 2) {
		ret = (fat[ThisFATEntOffset] >> 4)
			| ((uint16_t)fat[ThisFATEntOffset + 1] << 4);
	} else {
		ret = fat[ThisFATEntOffset]
			| ((uint16_t)(fat[ThisFATEntOffset + 1] & 0x0F) << 8);
	}
	free(fat);
	return ret;
}

/**
 * fat16_get_fat_entry - Get cluster is continuous
 * @clu:                 index of the cluster want to check
 *
 * @retrun:              next cluster (@clu has next cluster)
 *                       0            (@clu doesn't have next cluster)
 */
static uint32_t fat16_get_fat_entry(uint32_t clu)
{
	uint32_t ret = 0;
	uint32_t FATOffset = clu * sizeof(uint16_t);
	uint32_t ThisFATEntOffset = FATOffset % info.sector_size;

	uint16_t *fat;
	fat = malloc(info.sector_size);
	get_sector(fat, info.fat_offset * info.sector_size, 1);
	ret = fat[ThisFATEntOffset];
	free(fat);
	return ret;
}

/**
 * fat32_get_fat_entry - Get cluster is continuous
 * @clu:                 index of the cluster want to check
 *
 * @retrun:              next cluster (@clu has next cluster)
 *                       0            (@clu doesn't have next cluster)
 */
static uint32_t fat32_get_fat_entry(uint32_t clu)
{
	uint32_t ret = 0;
	uint32_t FATOffset = clu * sizeof(uint32_t);
	uint32_t ThisFATEntOffset = FATOffset % info.sector_size;

	uint32_t *fat;
	fat = malloc(info.sector_size);
	get_sector(fat, info.fat_offset * info.sector_size, 1);
	ret = fat[ThisFATEntOffset] & 0x0FFFFFFF;
	free(fat);
	return ret;
}

/*************************************************************************************************/
/*                                                                                               */
/* DIRECTORY CHAIN FUNCTION                                                                      */
/*                                                                                               */
/*************************************************************************************************/
/**
 * fat_check_dchain -   check whether @index has already loaded
 * @clu:                index of the cluster
 *
 * @retrun:        1 (@clu has loaded)
 *                 0 (@clu hasn't loaded)
 */
static int fat_check_dchain(uint32_t clu)
{
	int i;
	for (i = 0; info.root[i] && i < info.root_size; i++) {
		if (info.root[i]->index == clu)
			return 1;
	}
	return 0;
}

/**
 * fat_get_index        get directory chain index by argument
 * @clu:                index of the cluster
 *
 * @return:             directory chain index
 *                      Start of unused area (if doesn't lookup directory cache)
 */
static int fat_get_index(uint32_t clu)
{
	int i;
	for (i = 0; i < info.root_size && info.root[i]; i++) {
		if (info.root[i]->index == clu)
			return i;
	}

	info.root_size += DENTRY_LISTSIZE;
	node2_t **tmp = realloc(info.root, sizeof(node2_t *) * info.root_size);
	if (tmp) {
		info.root = tmp;
		info.root[i] = NULL;
	} else {
		pr_warn("Can't expand directory chain.\n");
		delete_node2(info.root[--i]);
	}

	return i;
}

/*************************************************************************************************/
/*                                                                                               */
/* OPERATIONS FUNCTION                                                                           */
/*                                                                                               */
/*************************************************************************************************/
/**
 * fat_print_bootsec - print boot sector in FAT12/16/32
 * @b:         boot sector pointer in FAT
 *
 * TODO: implement function in FAT12/16/32
 */
int fat_print_bootsec(void)
{
	int ret = 0;
	struct fat_bootsec *b = malloc(sizeof(struct fat_bootsec));

	fat_load_bootsec(b);
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
			fat16_print_bootsec(b);
			break;
		case FAT32_FILESYSTEM:
			{
				void *fsinfo;
				fsinfo = malloc(info.sector_size);
				fat32_print_bootsec(b);
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
 * fat_set_fat_entry -  Set FAT Entry to any cluster
 * @index:              index of the cluster want to check
 * @entry:              any cluster index
 *
 * @retrun:             0
 *                      -1 (invalid image)
 */
int fat_set_fat_entry(uint32_t clu, uint32_t entry)
{
	int ret = 0;

	switch (info.fstype) {
		case FAT12_FILESYSTEM:
			fat12_set_fat_entry(clu, entry);
			break;
		case FAT16_FILESYSTEM:
			fat16_set_fat_entry(clu, entry);
			break;
		case FAT32_FILESYSTEM:
			fat32_set_fat_entry(clu, entry);
			break;
		default:
			pr_err("Expected FAT filesystem, But this is not FAT filesystem.\n");
			ret = -1;
	}
	return ret;
}

/**
 * fat_get_fat_entry -  Get cluster is continuous
 * @index:              index of the cluster want to check
 * @entry:              any cluster index
 *
 * @return              0
 *                      -1 (invalid image)
 */
int fat_get_fat_entry(uint32_t clu, uint32_t *entry)
{
	int ret = 0;

	switch (info.fstype) {
		case FAT12_FILESYSTEM:
			*entry = fat12_get_fat_entry(clu);
			break;
		case FAT16_FILESYSTEM:
			*entry = fat16_get_fat_entry(clu);
			break;
		case FAT32_FILESYSTEM:
			*entry = fat32_get_fat_entry(clu);
			break;
		default:
			pr_err("Expected FAT filesystem, But this is not FAT filesystem.\n");
			ret = -1;
	}
	return ret;
}

