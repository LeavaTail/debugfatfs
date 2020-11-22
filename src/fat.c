// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2020 LeavaTail
 */
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include "debugfatfs.h"

/* Generic function prototype */
static uint32_t fat_concat_cluster(struct fat_fileinfo *, uint32_t, void **);
static uint32_t fat_set_cluster(struct fat_fileinfo *, uint32_t, void *);

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

/* cluster function prototype */
static int fat_check_last_cluster(uint32_t);
static int fat_get_last_cluster(struct fat_fileinfo *, uint32_t);
static int fat_alloc_clusters(struct fat_fileinfo *, uint32_t, size_t);
static int fat_free_clusters(struct fat_fileinfo *, uint32_t, size_t);
static int fat_new_clusters(size_t);

/* Directory chain function prototype */
static int fat_check_dchain(uint32_t);
static int fat_get_index(uint32_t);
static int fat_traverse_directory(uint32_t);
int fat_clean_dchain(uint32_t);

/* File function prototype */
static void fat_create_fileinfo(node2_t *, uint32_t, struct fat_dentry *, uint16_t *, size_t);
static int fat_init_dentry(struct fat_dentry *, unsigned char *, size_t);
static int fat_init_lfn(struct fat_dentry *, uint16_t *, size_t, unsigned char *, uint8_t);
static int fat_update_file(struct fat_dentry *, struct fat_dentry *);
static int fat_update_lfn(struct fat_dentry *, struct fat_dentry *);
static void fat_convert_uniname(uint16_t *, uint64_t, unsigned char *);
static int fat_create_shortname(uint16_t *, char *);
static int fat_convert_shortname(const char *, char *);
static int fat_create_nameentry(const char *, char *, uint16_t *);
static uint8_t fat_calculate_checksum(unsigned char *);
static void fat_convert_unixtime(struct tm *, uint16_t, uint16_t, uint8_t);
static void fat_convert_fattime(struct tm *, uint16_t *, uint16_t *, uint8_t *);
static int fat_validate_character(const char);

/* Operations function prototype */
int fat_print_bootsec(void);
int fat_print_vollabel(void);
int fat_lookup(uint32_t, char *);
int fat_readdir(struct directory *, size_t, uint32_t);
int fat_reload_directory(uint32_t);
int fat_convert_character(const char *, size_t, char *);
int fat_clean(uint32_t);
int fat_set_fat_entry(uint32_t, uint32_t);
int fat_get_fat_entry(uint32_t, uint32_t *);
int fat_print_dentry(uint32_t, size_t);
int fat_set_bogus_entry(uint32_t);
int fat_release_cluster(uint32_t);
int fat_create(const char *, uint32_t, int);
int fat_remove(const char *, uint32_t, int);
int fat_update_dentry(uint32_t, int);
int fat_trim(uint32_t);

static const struct operations fat_ops = {
	.statfs = fat_print_bootsec,
	.info = fat_print_vollabel,
	.lookup =  fat_lookup,
	.readdir = fat_readdir,
	.reload = fat_reload_directory,
	.convert = fat_convert_character,
	.clean = fat_clean,
	.setfat = fat_set_fat_entry,
	.getfat = fat_get_fat_entry,
	.dentry = fat_print_dentry,
	.alloc = fat_set_bogus_entry,
	.release = fat_release_cluster,
	.create = fat_create,
	.remove = fat_remove,
	.update = fat_update_dentry,
	.trim = fat_trim,
};

/*************************************************************************************************/
/*                                                                                               */
/* GENERIC FUNCTION                                                                              */
/*                                                                                               */
/*************************************************************************************************/
/**
 * fat_concat_cluster - Contatenate cluster @data with next_cluster
 * @f:                  file information pointer
 * @clu:                index of the cluster
 * @data:               The cluster (Output)
 *
 * @retrun:             cluster count (@clu has next cluster)
 *                      0             (@clu doesn't have next cluster, or failed to realloc)
 */
static uint32_t fat_concat_cluster(struct fat_fileinfo *f, uint32_t clu, void **data)
{
	int i;
	uint32_t ret = FAT_FSTCLUSTER;
	uint32_t tmp_clu = clu;
	void *tmp;
	size_t allocated = 1;

	for (allocated = 0; fat_check_last_cluster(ret) == 0; allocated++, tmp_clu = ret)
		fat_get_fat_entry(tmp_clu, &ret);

	if (!(tmp = realloc(*data, info.cluster_size * allocated)))
		return 0;
	*data = tmp;

	for (i = 1; i < allocated; i++) {
		fat_get_fat_entry(clu, &ret);
		get_cluster(*data + info.cluster_size * i, ret);
		clu = ret;
	}

	return allocated;
}

/**
 * fat_set_cluster - Set Raw-Data from any sector.
 * @f:               file information pointer
 * @clu:             index of the cluster
 * @data:            The cluster
 *
 * @retrun:          cluster count (@clu has next cluster)
 *                   0             (@clu doesn't have next cluster, or failed to realloc)
 */
static uint32_t fat_set_cluster(struct fat_fileinfo *f, uint32_t clu, void *data)
{
	uint32_t ret = FAT_FSTCLUSTER;
	uint32_t tmp_clu = clu;
	size_t allocated = 0;
	size_t cluster_num = 0;

	for (cluster_num = 0; fat_check_last_cluster(ret) == 0; cluster_num++, tmp_clu = ret)
		fat_get_fat_entry(tmp_clu, &ret);

	ret = clu;
	for (allocated = 0; allocated < cluster_num; allocated++) {
		set_cluster(data + info.cluster_size * allocated, ret);
		fat_get_fat_entry(clu, &ret);
		clu = ret;
	}

	return allocated + 1;
}

/**
 * fat_check_filesystem - Whether or not VFAT filesystem
 * @boot:                 boot sector pointer
 *
 * return:                1 (Image is FAT12/16/32 filesystem)
 *                        0 (Image isn't FAT12/16/32 filesystem)
 */
int fat_check_filesystem(struct pseudo_bootsec *boot)
{
	struct fat_bootsec *b = (struct fat_bootsec *)boot;
	struct fat_fileinfo *f;
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
	} else {
		FATSz = b->reserved_info.fat32_reserved_info.BPB_FATSz32;
	}

	if (b->BPB_TotSec16 != 0) {
		TotSec = b->BPB_TotSec16;
	} else {
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
	if (info.fstype == FAT32_FILESYSTEM) {
		info.root_offset = b->reserved_info.fat32_reserved_info.BPB_RootClus;
		info.root_length = info.cluster_size;
		info.heap_offset = info.fat_offset + info.fat_length;
	} else {
		info.root_offset = 0;
		info.root_length = (32 * b->BPB_RootEntCnt + b->BPB_BytesPerSec - 1) / b->BPB_BytesPerSec;
		info.heap_offset = info.fat_offset + info.fat_length + info.root_length;
	}

	f = malloc(sizeof(struct fat_fileinfo));
	strncpy((char *)f->name, "/", strlen("/") + 1);
	f->uniname = NULL;
	f->namelen = 1;
	f->datalen = 0;
	f->attr = ATTR_DIRECTORY;
	info.root[0] = init_node2(info.root_offset, f);
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
 * @b:                boot sector pointer in FAT (Output)
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
		pr_debug("invalid reserved sectors: 0x%x\n", b->BPB_RevdSecCnt);
		ret = 0;
	}

	if (!b->BPB_NumFATs) {
		pr_debug("invalid FAT structure: 0x%x\n", b->BPB_NumFATs);
		ret = 0;
	}

	if (media != 0xf0 && media < 0xF8) {
		pr_debug("invalid Media value: 0x%x\n", b->BPB_Media);
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
 * @b:                   boot sector pointer in FAT
 */
static int fat16_print_bootsec(struct fat_bootsec *b)
{
	int i;
	const char *type = (char *)b->reserved_info.fat16_reserved_info.BS_FilSysType;

	if (strncmp(type, "FAT", 3))
		pr_warn("BS_FilSysType is expected \"FAT     \", But this is %s\n", type);

	pr_msg("%-28s\t: 0x", "Volume ID");
	for (i = 0; i < VOLIDSIZE; i++)
		pr_msg("%02x", b->reserved_info.fat16_reserved_info.BS_VolID[i]);
	pr_msg("\n");

	pr_msg("%-28s\t: ", "Volume Label");
	for (i = 0; i < VOLLABSIZE; i++)
		pr_msg("%c", b->reserved_info.fat16_reserved_info.BS_VolLab[i]);
	pr_msg("\n");
	return 0;
}

/**
 * fat32_print_bootsec - print boot sector in FAT32
 * @b:                   boot sector pointer in FAT
 */
static int fat32_print_bootsec(struct fat_bootsec *b)
{
	int i;
	const char *type = (char *)b->reserved_info.fat32_reserved_info.BS_FilSysType;

	if (strncmp(type, "FAT32", 5))
		pr_warn("BS_FilSysType is expected \"FAT32   \", But this is %s\n", type);

	pr_msg("%-28s\t: 0x", "Volume ID");
	for (i = 0; i < VOLIDSIZE; i++)
		pr_msg("%02x", b->reserved_info.fat32_reserved_info.BS_VolID[i]);
	pr_msg("\n");

	pr_msg("%-28s\t: ", "Volume Label");
	for (i = 0; i < VOLLABSIZE; i++)
		pr_msg("%c", b->reserved_info.fat32_reserved_info.BS_VolLab[i]);
	pr_msg("\n");

	pr_msg("%-28s\t: 0x%08x\n", "Sectors Per FAT",
			b->reserved_info.fat32_reserved_info.BPB_FATSz32);
	pr_msg("%-28s\t: 0x%08x (sector)\n", "The first sector of the Root",
			b->reserved_info.fat32_reserved_info.BPB_RootClus);
	pr_msg("%-28s\t: 0x%08x (sector)\n", "FSINFO sector",
			b->reserved_info.fat32_reserved_info.BPB_FSInfo);
	pr_msg("%-28s\t: 0x%08x (sector)\n", "Backup Boot sector",
			b->reserved_info.fat32_reserved_info.BPB_BkBootSec);
	return 0;
}

/**
 * fat32_print_fsinfo - print FSinfo Structure in FAT32
 * @fsi:                fsinfo pointer in FAT
 */
static int fat32_print_fsinfo(struct fat32_fsinfo *fsi)
{
	if ((fsi->FSI_LeadSig != 0x41615252) ||
			(fsi->FSI_StrucSig != 0x61417272) ||
			(fsi->FSI_TrailSig != 0xAA550000))
		pr_warn("FSinfo is expected specific sigunature, But this is difference.\n");

	pr_msg("%-28s\t: %10u (cluster)\n", "free cluster count", fsi->FSI_Free_Count);
	pr_msg("%-28s\t: %10u (cluster)\n", "first available cluster", fsi->FSI_Nxt_Free);
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
	uint32_t ThisFATEntOffset = clu % info.sector_size;
	uint32_t *fat;

	fat = malloc(info.sector_size);
	get_sector(fat, info.fat_offset * info.sector_size, 1);
	fat[ThisFATEntOffset] = entry & 0x0FFFFFFF;
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
	uint32_t ThisFATSecNum = info.fat_offset + (FATOffset / info.sector_size); 
	uint32_t ThisFATEntOffset = FATOffset % info.sector_size;
	uint16_t *fat;

	fat = malloc(info.sector_size);
	get_sector(fat, ThisFATSecNum * info.sector_size, 1);
	if (clu % 2) {
		ret = (fat[ThisFATEntOffset] >> 4)
			| (fat[ThisFATEntOffset + 1] << 4);
	} else {
		ret = fat[ThisFATEntOffset]
			| ((fat[ThisFATEntOffset + 1] & 0x0F) << 8);
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
	uint32_t ThisFATEntOffset = clu % info.sector_size;
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
	uint32_t ThisFATEntOffset = clu % info.sector_size;
	uint32_t *fat;

	fat = malloc(info.sector_size);
	get_sector(fat, info.fat_offset * info.sector_size, 1);
	ret = fat[ThisFATEntOffset] & 0x0FFFFFFF;
	free(fat);

	return ret;
}

/*************************************************************************************************/
/*                                                                                               */
/* CLUSTER FUNCTION FUNCTION                                                                     */
/*                                                                                               */
/*************************************************************************************************/
/**
 * fat_check_last_cluster - check whether cluster is last or not
 * @clu:                    first cluster
 *
 * @return                   1 (Last cluster)
 *                           0 (Not last cluster)
 *                          -1 (invalid image)
 */
static int fat_check_last_cluster(uint32_t clu)
{
	switch (info.fstype) {
		case FAT12_FILESYSTEM:
			return (clu < FAT_FSTCLUSTER || FAT12_RESERVED <= clu);
		case FAT16_FILESYSTEM:
			return (clu < FAT_FSTCLUSTER || FAT16_RESERVED <= clu);
		case FAT32_FILESYSTEM:
			return (clu < FAT_FSTCLUSTER || FAT32_RESERVED <= clu);
		default:
			pr_err("Expected FAT filesystem, But this is not FAT filesystem.\n");
			return -1;
	}
}

/**
 * fat_get_last_cluster - find last cluster in file
 * @f:                    file information pointer
 * @clu:                  first cluster
 *
 * @return                Last cluster
 *                        -1 (Failed)
 */
static int fat_get_last_cluster(struct fat_fileinfo *f, uint32_t clu)
{
	uint32_t ret = FAT_FSTCLUSTER;
	size_t allocated = 1;

	for (allocated = 0; fat_check_last_cluster(ret) == 0; allocated++, clu = ret)
		fat_get_fat_entry(clu, &ret);

	return clu;
}

/**
 * fat_alloc_clusters - Allocate cluster to file
 * @f:                  file information pointer
 * @clu:                first cluster
 * @num_alloc:          number of cluster
 *
 * @return              the number of allocated cluster
 */
static int fat_alloc_clusters(struct fat_fileinfo *f, uint32_t clu, size_t num_alloc)
{
	uint32_t next_clu;
	uint32_t last_clu;
	int total_alloc = num_alloc;

	clu = next_clu = last_clu = fat_get_last_cluster(f, clu);
	for (next_clu = last_clu + 1; next_clu != last_clu; next_clu++) {
		if (next_clu > info.cluster_count - 1)
			next_clu = FAT_FSTCLUSTER;

		fat_set_fat_entry(next_clu, EXFAT_LASTCLUSTER);
		fat_set_fat_entry(clu, next_clu);
		clu = next_clu;
		if (--total_alloc == 0)
			break;

	}
	return total_alloc;
}

/**
 * fat_free_clusters - Free cluster in file
 * @f:                 file information pointer
 * @clu:               first cluster
 * @num_alloc:         number of cluster
 *
 * @return             0 (success)
 */
static int fat_free_clusters(struct fat_fileinfo *f, uint32_t clu, size_t num_alloc)
{
	int i;
	uint32_t tmp = clu;
	uint32_t next_clu = FAT_FSTCLUSTER;
	size_t cluster_num = 0;
	uint32_t ret = EXFAT_LASTCLUSTER;

	for (cluster_num = 0; fat_check_last_cluster(next_clu) == 0 ;cluster_num++) {
		fat_get_fat_entry(tmp, &next_clu);
		tmp = next_clu;
	}

	for (i = 0; i < cluster_num - num_alloc - 1; i++) {
		fat_get_fat_entry(clu, &next_clu);
		clu = next_clu;
	}
	while (i++ < cluster_num - 1) {
		fat_get_fat_entry(clu, &next_clu);
		fat_set_fat_entry(clu, ret);
		ret = 0;
		clu = next_clu;
	}

	return 0;
}

/**
 * fat_new_cluster - Prepare to new cluster
 * @num_alloc:       number of cluster
 *
 * @return           allocated first cluster index
 */
static int fat_new_clusters(size_t num_alloc)
{
	uint32_t next_clu, clu;
	uint32_t fst_clu = 0;

	for (clu = FAT_FSTCLUSTER; clu < info.cluster_count; clu++) {
		fat_get_fat_entry(clu, &next_clu);
		if (!fat_check_last_cluster(next_clu))
			continue;

		if (!fst_clu) {
			fst_clu = clu = next_clu;
			fat_set_fat_entry(fst_clu, EXFAT_LASTCLUSTER);
		} else {
			fat_set_fat_entry(next_clu, EXFAT_LASTCLUSTER);
			fat_set_fat_entry(clu, next_clu);
			clu = next_clu;
		}

		if (--num_alloc == 0)
			break;
	}

	return fst_clu;
}
/*************************************************************************************************/
/*                                                                                               */
/* DIRECTORY CHAIN FUNCTION                                                                      */
/*                                                                                               */
/*************************************************************************************************/
#if 0
/**
 * fat_print_dchain - print directory chain
 */
static void fat_print_dchain(void)
{
	int i;
	node2_t *tmp;
	struct fat_fileinfo *f;

	for (i = 0; i < info.root_size && info.root[i]; i++) {
		tmp = info.root[i];
		f = (struct fat_fileinfo *)info.root[i]->data;
		pr_msg("%-16s(%s) [%u] | ", f->name, f->uniname, tmp->index);
		while (tmp->next != NULL) {
			tmp = tmp->next;
			f = (struct fat_fileinfo *)tmp->data;
			pr_msg("%s(%s) [%u] ", f->name, f->uniname, tmp->index);
		}
		pr_msg("\n");
	}
	pr_msg("\n");
}
#endif
/**
 * fat_check_dchain - check whether @index has already loaded
 * @clu:              index of the cluster
 *
 * @retrun:           1 (@clu has loaded)
 *                    0 (@clu hasn't loaded)
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
 * fat_get_index - get directory chain index by argument
 * @clu:           index of the cluster
 *
 * @return:        directory chain index
 *                 Start of unused area (if doesn't lookup directory cache)
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
		pr_warn("Can't expand directory chain, so delete last chain.\n");
		delete_node2(info.root[--i]);
	}

	return i;
}

/**
 * fat_traverse_directory - function to traverse one directory
 * @clu:                    index of the cluster want to check
 *
 * @return                  0 (success)
 *                         -1 (failed to read)
 */
static int fat_traverse_directory(uint32_t clu)
{
	int i, j;
	uint8_t ord = 0, attr = 0;
	uint16_t uniname[MAX_NAME_LENGTH] = {0};
	size_t index = fat_get_index(clu);
	struct fat_fileinfo *f = (struct fat_fileinfo *)info.root[index]->data;
	size_t entries;
	size_t cluster_num = 1;
	size_t namelen = 0;
	void *data;
	struct fat_dentry d;

	if (f->cached) {
		pr_debug("Directory %s was already traversed.\n", f->name);
		return 0;
	}

	if (clu) {
		data = malloc(info.cluster_size);
		get_cluster(data, clu);
		cluster_num = fat_concat_cluster(f, clu, &data);
		entries = (cluster_num * info.cluster_size) / sizeof(struct fat_dentry);
	} else {
		data = malloc(info.root_length * info.sector_size);
		get_sector(data, (info.fat_offset + info.fat_length) * info.sector_size, info.root_length);
		entries = (info.root_length * info.sector_size) / sizeof(struct fat_dentry);
	}

	for (i = 0; i < entries; i++) {
		namelen = 0;
		d = ((struct fat_dentry *)data)[i];
		attr = d.dentry.lfn.LDIR_Attr;
		ord = d.dentry.lfn.LDIR_Ord;
		/* Empty entry */
		if (ord == 0x00)
			break;
		if (ord == 0xe5)
			continue;
		/* First entry should be checked */
		switch (attr) {
			case ATTR_VOLUME_ID:
				info.vol_length = 11;
				info.vol_label = calloc(11 + 1, sizeof(unsigned char));
				memcpy(info.vol_label, d.dentry.dir.DIR_Name,
						sizeof(unsigned char) * 11);
				continue;
			case ATTR_LONG_FILE_NAME:
				ord &= ~LAST_LONG_ENTRY;
				for (j = 0; j < ord; j++) {
					memcpy(uniname + j * LONGNAME_MAX,
							(((struct fat_dentry *)data)[i + ord - j - 1]).dentry.lfn.LDIR_Name1,
							5 * sizeof(uint16_t));
					memcpy(uniname + j * LONGNAME_MAX + 5,
							(((struct fat_dentry *)data)[i + ord - j - 1]).dentry.lfn.LDIR_Name2,
							6 * sizeof(uint16_t));
					memcpy(uniname + j * LONGNAME_MAX + 11,
							(((struct fat_dentry *)data)[i + ord - j - 1]).dentry.lfn.LDIR_Name3,
							2 * sizeof(uint16_t));
					namelen += LONGNAME_MAX;
				}
				d = ((struct fat_dentry *)data)[i + ord];
				i += ord;
				break;
			default:
				d = ((struct fat_dentry *)data)[i];
				break;
		}
		fat_create_fileinfo(info.root[index], clu, &d, uniname, namelen);
	}
	free(data);
	return 0;
}

/**
 * fat_clean_dchain - function to clean opeartions
 * @index:            directory chain index
 *
 * @return            0 (success)
 *                   -1 (already released)
 */
int fat_clean_dchain(uint32_t index)
{
	node2_t *tmp;
	struct fat_fileinfo *f;

	if ((!info.root[index])) {
		pr_warn("index %d was already released.\n", index);
		return -1;
	}

	tmp = info.root[index];

	while (tmp->next != NULL) {
		tmp = tmp->next;
		f = (struct fat_fileinfo *)tmp->data;
		free(f->uniname);
		f->uniname = NULL;
	}
	free_list2(info.root[index]);
	return 0;
}

/*************************************************************************************************/
/*                                                                                               */
/* FILE FUNCTION                                                                                 */
/*                                                                                               */
/*************************************************************************************************/
/**
 * fat_create_file_entry - Create file infomarion
 * @head:                  Directory chain head
 * @clu:                   parent Directory cluster index
 * @file:                  file dentry
 * @uniname:               Long File name
 * @namelen:               Long File name length
 */
static void fat_create_fileinfo(node2_t *head, uint32_t clu,
		struct fat_dentry *file, uint16_t *uniname, size_t namelen)
{
	int index, next_clu = 0;
	struct fat_fileinfo *f;

	next_clu |= (file->dentry.dir.DIR_FstClusHI << 16) | file->dentry.dir.DIR_FstClusLO;
	f = malloc(sizeof(struct fat_fileinfo));
	memset(f->name, '\0', 13);
	f->namelen = fat_convert_shortname((char *)file->dentry.dir.DIR_Name, (char *)f->name);

	f->uniname = malloc(namelen * UTF8_MAX_CHARSIZE + 1);
	memset(f->uniname, '\0', namelen * UTF8_MAX_CHARSIZE + 1);
	fat_convert_uniname(uniname, namelen, f->uniname);

	f->namelen = strlen((char *)f->uniname);
	f->datalen = file->dentry.dir.DIR_FileSize;
	f->attr = file->dentry.dir.DIR_Attr;

	fat_convert_unixtime(&f->ctime, file->dentry.dir.DIR_CrtDate,
			file->dentry.dir.DIR_CrtTime,
			file->dentry.dir.DIR_CrtTimeTenth);
	fat_convert_unixtime(&f->mtime, file->dentry.dir.DIR_WrtDate,
			file->dentry.dir.DIR_WrtTime,
			0);
	fat_convert_unixtime(&f->atime, file->dentry.dir.DIR_LstAccDate,
			0,
			0);
	append_node2(head, next_clu, f);
	 ((struct fat_fileinfo *)(head->data))->cached = 1;

	/* If this entry is Directory, prepare to create next chain */
	if ((f->attr & ATTR_DIRECTORY) && (!fat_check_dchain(next_clu))) {
		struct fat_fileinfo *d = malloc(sizeof(struct fat_fileinfo));
		strncpy((char *)d->name, (char *)f->name, 11);
		d->uniname = malloc(f->namelen + 1);
		strncpy((char *)d->uniname, (char *)f->uniname, f->namelen + 1);
		d->namelen = namelen;
		d->datalen = file->dentry.dir.DIR_FileSize;
		d->attr = file->dentry.dir.DIR_Attr;

		index = fat_get_index(next_clu);
		info.root[index] = init_node2(next_clu, d);
	}
}

/**
 * fat_init_dentry - initialize directory entry
 * @d:               directory entry (Output)
 * @shortname:       filename in ASCII
 * @namelen:         filename length
 *
 * @return           0 (Success)
 */
static int fat_init_dentry(struct fat_dentry *d, unsigned char *shortname, size_t namelen)
{
	uint16_t __date, __time;
	uint8_t __subsec;
	time_t t = time(NULL);
	struct tm *utc = gmtime(&t);

	fat_convert_fattime(utc, &__date, &__time, &__subsec);
	memcpy(d->dentry.dir.DIR_Name, shortname, 11);
	d->dentry.dir.DIR_Attr = ATTR_ARCHIVE;
	d->dentry.dir.DIR_NTRes = 0;
	d->dentry.dir.DIR_CrtTimeTenth = __subsec;
	d->dentry.dir.DIR_CrtTime = __time;
	d->dentry.dir.DIR_CrtDate = __date;
	d->dentry.dir.DIR_LstAccDate = __date;
	d->dentry.dir.DIR_WrtTime = __time;
	d->dentry.dir.DIR_WrtDate = __date;
	d->dentry.dir.DIR_FstClusHI = 0x00;
	d->dentry.dir.DIR_FstClusLO = 0x00;
	d->dentry.dir.DIR_FileSize = 0x0000;

	return 0;
}

/**
 * fat_init_lfn - initialize long file name entry
 * @d:            directory entry (Output)
 * @name:         filename in UTF-16
 * @namelen:      filename length
 * @ord:          The order of the entry
 *
 * @return        0 (Success)
 */
static int fat_init_lfn(struct fat_dentry *d,
		uint16_t *name, size_t namelen, unsigned char *shortname, uint8_t ord)
{
	d->dentry.lfn.LDIR_Ord = ord;
	memcpy(d->dentry.lfn.LDIR_Name1, name, 10);
	d->dentry.lfn.LDIR_Attr = ATTR_LONG_FILE_NAME;
	d->dentry.lfn.LDIR_Type = 0;
	d->dentry.lfn.LDIR_Chksum = fat_calculate_checksum(shortname);
	memcpy(d->dentry.lfn.LDIR_Name2, name + 5, 12);
	d->dentry.lfn.LDIR_FstClusLO = 0;
	memcpy(d->dentry.lfn.LDIR_Name3, name + 6, 4);

	return 0;
}

/**
 * fat_update_file - Update directory entry
 * @old:             directory entry before update
 * @new:             directory entry after update (Output)
 *
 * @return           0 (Success)
 */
static int fat_update_file(struct fat_dentry *old, struct fat_dentry *new)
{
	int i;
	uint32_t tmp = 0;
	struct tm tm;
	char buf[64] = {0};
	const char attrchar[][16] = {
		"ReadOnly",
		"Hidden",
		"System",
		"Volume ID",
		"Directory",
		"Archive",
		"Long Filename",
	};
	const char timechar[][16] = {
		"Create",
		"LastModified",
		"LastAccess",
	};
	uint16_t dates[3] = {0};
	uint16_t times[3] = {0};
	uint8_t subsecs[3] = {0};

	input("Please input a FileName", buf);
	memcpy(new->dentry.dir.DIR_Name, buf, 11);

	pr_msg("Please select a File Attribute.\n");
	for (i = 0; i < 7; i++) {
		pr_msg("   %s [N/y] ", attrchar[i]);
		fflush(stdout);
		if (fgets(buf, 8, stdin) == NULL)
			return 1;
		if (toupper(buf[0]) == 'Y' && buf[1] == '\n')
			new->dentry.dir.DIR_Attr |= (1 << i);
	}

	for (i = 0; i < 3; i++) {
		pr_msg("Please input a %s Timestamp.\n", timechar[i]);
		do {
			input_time("Year", &tmp);
		} while (tmp < 1980 || tmp > 2107);
		tm.tm_year = tmp - 1900;
		do {
			input_time("Month", &tmp);
		} while (tmp < 1 || tmp > 12);
		tm.tm_mon = tmp - 1;
		do {
			input_time("Day", &tmp);
		} while (tmp < 1 || tmp > 31);
		tm.tm_mday = tmp;

		if (i == 2)
			break;

		do {
			input_time("Hour", &tmp);
		} while (tmp < 0 || tmp > 23);
		tm.tm_hour = tmp;
		do {
			input_time("Min", &tmp);
		} while (tmp < 0 || tmp > 59);
		tm.tm_min = tmp;
		do {
			input_time("Sec", &tmp);
		} while (tmp < 0 || tmp > 59);
		tm.tm_sec = tmp;
		fat_convert_fattime(&tm, dates + i, times + i, subsecs + i);
	}

	new->dentry.dir.DIR_CrtTimeTenth = subsecs[0];
	new->dentry.dir.DIR_CrtTime = times[0];
	new->dentry.dir.DIR_CrtDate = dates[0];
	new->dentry.dir.DIR_LstAccDate = dates[1];
	new->dentry.dir.DIR_WrtTime= times[2];
	new->dentry.dir.DIR_WrtDate = times[2];

	input("Please input a First Cluster", buf);
	sscanf(buf, "%04x", (uint32_t *)&tmp);
	new->dentry.dir.DIR_FstClusHI = tmp >> 16;
	new->dentry.dir.DIR_FstClusLO = tmp & 0x0000ffff;
	input("Please input a Data Length", buf);
	sscanf(buf, "%04x", (uint32_t *)&tmp);
	new->dentry.dir.DIR_FileSize = tmp;

	return 0;
}

/**
 * fat_update_lfn - Update Long filename entry
 * @old:            directory entry before update
 * @new:            directory entry after update (Output)
 *
 * @return          0 (Success)
 */
static int fat_update_lfn(struct fat_dentry *old, struct fat_dentry *new)
{
	char shortname[11] = {0};
	uint16_t longname[MAX_NAME_LENGTH] = {0};
	uint32_t tmp = 0;
	char buf[64] = {0};

	input("Please input a FileName", buf);
	fat_create_nameentry(buf, shortname, longname);
	memcpy(new->dentry.lfn.LDIR_Name1, longname, sizeof(uint16_t) * 5);
	memcpy(new->dentry.lfn.LDIR_Name2, longname + 5, sizeof(uint16_t) * 6);
	memcpy(new->dentry.lfn.LDIR_Name3, longname + 11, sizeof(uint16_t) * 2);

	input("Please input a the order of this entry", buf);
	sscanf(buf, "%02hhx", (uint8_t *)&tmp);
	new->dentry.lfn.LDIR_Ord = tmp;
	input("Please input a Checksum", buf);
	sscanf(buf, "%02hhx", (uint8_t *)&tmp);
	new->dentry.lfn.LDIR_Chksum = tmp;

	return 0;
}

/**
 * fat_convert_uniname - function to get filename
 * @uniname:             filename dentry in UTF-16
 * @name_len:            filename length
 * @name:                filename in UTF-8 (Output)
 */
static void fat_convert_uniname(uint16_t *uniname, uint64_t name_len, unsigned char *name)
{
	utf16s_to_utf8s(uniname, name_len, name);
}

/**
 * fat_create_shortname - function to convert filename to shortname
 * @longname:             filename dentry in UTF-16
 * @name:                 filename in ascii (Output)
 *
 * @return                0 (Same as input name)
 *                        1 (Difference from input data)
 */
static int fat_create_shortname(uint16_t *longname, char *name)
{
	int ch;

	/* Pick up Only ASCII code */
	if (*longname < 0x0080) {
		/* Upper character is directly set */
		if(isupper(*longname) || isdigit(*longname)) {
			*name = *longname;
		/* otherwise (lower character or othse)  */
		} else {
			/* lower character case */
			if ((ch = toupper(*longname)) != *longname) {
				*name = ch;
			} else {
				*name = '_';
			}
			return 1;
		}
	} else {
		*name = '_';
		return 1;
	}
	return 0;
}

/**
 * fat_convert_shortname - function to convert filename to shortname
 * @shortname:             filename dentry in ASCII
 * @name:                  filename in ascii (Output)
 *
 * @return                 name length
 */
static int fat_convert_shortname(const char *shortname, char *name)
{
	int i, j;

	/* filename */
	for (i = 0, j = 0; i < 8; i++) {
		if (!fat_validate_character(shortname[i])) {
			name[j++] = shortname[i];
		}
	}

	/* add file extension */
	if (shortname[i] != ' ') {
		name[j++] = '.';
		for (i = 8; i < 11; i++) {
			if (!fat_validate_character(shortname[i])) {
				name[j++] = shortname[i];
			}
		}
	}

	return j;
}

/**
 * fat_create_nameentry - function to get filename
 * @name:                 filename dentry in UTF-8
 * @shortname:            filename in ASCII (Output)
 * @longname:             filename in UTF-16 (Output)
 *
 * @return                  0 (Only shortname)
 *                        > 0 (longname length)
 */
static int fat_create_nameentry(const char *name, char *shortname, uint16_t *longname)
{
	int i, j;
	bool changed = false;
	size_t name_len;

	memset(shortname, ' ', 8 + 3);
	name_len = utf8s_to_utf16s((unsigned char *)name, strlen(name), longname);
	for (i = 0, j = 0; i < 8 && longname[j] != '.'; i++, j++) {
		if (i > name_len)
			goto numtail;
		if (!longname[j])
			goto numtail;
		if (fat_create_shortname(&longname[j], &shortname[i]))
			changed = true;
	}
	/* This is not 8.3 format */
	if (longname[j] != '.') {
		changed = true;
		goto numtail;
	}
	j++;

	/* Extension */
	for (i = 8; i < 11; i++, j++) {
		if (j > name_len)
			goto numtail;
		if (fat_create_shortname(&longname[j], &shortname[i]))
			changed = true;
	}

numtail:
	if (changed) {
		shortname[6] = '~';
		shortname[7] = '1';
		return name_len;
	}
	return 0;
}

/**
 * fat_calculate_checksum - Calculate file entry Checksum
 * @DIR_Name:               shortname
 *
 * @return                  Checksum
 */
static uint8_t fat_calculate_checksum(unsigned char *DIR_Name)
{
	int i;
	uint8_t chksum;

	for (i = 11; i != 0; i--) {
		chksum = ((chksum & 1) & 0x80) + (chksum >> 1) + *DIR_Name++;
	}
	return chksum;
}

/**
 * fat_convert_unixtime - function to get timestamp in file
 * @t:                    output pointer (Output)
 * @date:                 Date Field in File Directory Entry
 * @time:                 Timestamp Field in File Directory Entry
 * @subsec:               10msincrement Field in File Directory Entry
 */
static void fat_convert_unixtime(struct tm *t, uint16_t date, uint16_t time, uint8_t subsec)
{
	t->tm_year = (date >> FAT_YEAR) & 0x7f;
	t->tm_mon  = (date >> FAT_MONTH) & 0x0f;
	t->tm_mday = (date >> FAT_DAY) & 0x1f;
	t->tm_hour = (time >> EXFAT_HOUR) & 0x1f;
	t->tm_min  = (time >> EXFAT_MINUTE) & 0x3f;
	t->tm_sec  = (time & 0x1f) * 2;
	t->tm_sec += subsec / 100;
}

/**
 * fat_convert_fattime - function to get timestamp in file
 * @t:                   timestamp (GMT)
 * @date:                Date Field in File Directory Entry (Output)
 * @time:                Timestamp Field in File Directory Entry (Output)
 * @subsec:              10msincrement Field in File Directory Entry (Output)
 */
static void fat_convert_fattime(struct tm *t, uint16_t *date, uint16_t *time, uint8_t *subsec)
{
	*time = (t->tm_hour << EXFAT_HOUR) |
		(t->tm_min << EXFAT_MINUTE) |
		(t->tm_sec / 2);
	*date = ((t->tm_year - 80) << FAT_YEAR) |
		((t->tm_mon + 1) << FAT_MONTH) |
		(t->tm_mday);
	*subsec += ((t->tm_sec % 2) * 100);
}

/**
 * fat_validate_character- validate that character is ASCII as 8.3 format
 * @ch:                    ASCII character
 *
 * @return                 0 (as 8.3 format)
 *                         1 (not 8.3 format)
 */
static int fat_validate_character(const char ch)
{
	/* " / \ [] : ; = ,  */
	int i = 0, c;
	char bad[] = {0x22, 0x2f, 0x5c, 0x5b, 0x5d, 0x3a, 0x3b, 0x3d, 0x2c, 0x20, 0x00};

	while ((c = bad[i++]) != 0x00) {
		if (ch == c)
			return 1;
	}
	return 0;
}

/*************************************************************************************************/
/*                                                                                               */
/* OPERATIONS FUNCTION                                                                           */
/*                                                                                               */
/*************************************************************************************************/
/**
 * fat_print_bootsec - print boot sector in FAT12/16/32
 *
 * @return             0 (success)
 *                    -1 (this image isn't FAT filesystem)
 */
int fat_print_bootsec(void)
{
	int ret = 0;
	struct fat_bootsec *b = malloc(sizeof(struct fat_bootsec));

	fat_load_bootsec(b);
	pr_msg("%-28s\t: %10u (byte)\n", "Bytes per Sector", b->BPB_BytesPerSec);
	pr_msg("%-28s\t: %10u (sector)\n", "Sectors per cluster", b->BPB_SecPerClus);
	pr_msg("%-28s\t: %10u (sector)\n", "Reserved Sector", b->BPB_RevdSecCnt);
	pr_msg("%-28s\t: %10u\n", "FAT count", b->BPB_NumFATs);
	pr_msg("%-28s\t: %10u\n", "Root Directory entry count", b->BPB_RootEntCnt);
	pr_msg("%-28s\t: %10u (sector)\n", "Sector count in Volume", b->BPB_TotSec16);

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
	pr_msg("\n");

	free(b);
	return ret;
}

/**
 * fat_print_fsinfo - print filesystem information in FAT
 *
 * @return            0 (success)
 */
int fat_print_vollabel(void)
{
	pr_msg("volume Label: ");
	pr_msg("%s\n", (char *)info.vol_label);
	return 0;
}

/**
 * fat_lookup - function interface to lookup pathname
 * @clu:        directory cluster index
 * @name:       file name
 *
 * @return:     cluster index
 *              -1 (Not found)
 */
int fat_lookup(uint32_t clu, char *name)
{
	int index, i = 0, depth = 0;
	bool found = false;
	char *path[MAX_NAME_LENGTH] = {};
	char fullpath[PATHNAME_MAX + 1] = {};
	node2_t *tmp;
	struct fat_fileinfo *f;

	if (!name) {
		pr_err("invalid pathname.\n");
		return -1;
	}

	/* Absolute path */
	if (name[0] == '/') {
		pr_debug("\"%s\" is Absolute path, so change current directory(%u) to root(%u)\n",
				name, clu, info.root_offset);
		clu = info.root_offset;
	}

	/* Separate pathname by slash */
	strncpy(fullpath, name, PATHNAME_MAX);
	path[depth] = strtok(fullpath, "/");
	while (path[depth] != NULL) {
		if (depth >= MAX_NAME_LENGTH) {
			pr_err("Pathname is too depth. (> %d)\n", MAX_NAME_LENGTH);
			return -1;
		}
		path[++depth] = strtok(NULL, "/");
	};

	for (i = 0; path[i] && i < depth + 1; i++) {
		pr_debug("Lookup %s to %d\n", path[i], clu);
		found = false;
		index = fat_get_index(clu);
		f = (struct fat_fileinfo *)info.root[index]->data;
		if ((!info.root[index]) || (!(f->cached))) {
			pr_debug("Directory hasn't load yet, or This Directory doesn't exist in filesystem.\n");
			fat_traverse_directory(clu);
			index = fat_get_index(clu);
			if (!info.root[index]) {
				pr_warn("This Directory doesn't exist in filesystem.\n");
				return -1;
			}
		}

		tmp = info.root[index];
		while (tmp->next != NULL) {
			tmp = tmp->next;
			f = (struct fat_fileinfo *)tmp->data;
			if (f->namelen) {
				if (!strncmp(path[i], (char *)f->uniname, strlen(path[i]))) {
					clu = tmp->index;
					found = true;
					break;
				}
			} else {
				if (!strncmp(path[i], (char *)f->name, strlen(path[i]))) {
					clu = tmp->index;
					found = true;
					break;
				}
			}
		}

		if (!found) {
			pr_warn("'%s': No such file or directory.\n", name);
			return -1;
		}
	}

	return clu;
}

/**
 * fat_readdir - function interface to read a directory
 * @dir:         directory entry list (Output)
 * @count:       Allocated space in @dir
 * @clu:         Directory cluster index
 *
 * @return       >= 0 (Number of entry)
 *                < 0 (Number of entry can't read)
 */
int fat_readdir(struct directory *dir, size_t count, uint32_t clu)
{
	int i;
	node2_t *tmp;
	struct fat_fileinfo *f;

	fat_traverse_directory(clu);
	i = fat_get_index(clu);
	tmp = info.root[i];

	for (i = 0; i < count && tmp->next != NULL; i++) {
		tmp = tmp->next;
		f = (struct fat_fileinfo *)(tmp->data);
		if (!f->namelen) {
			dir[i].name = calloc(12 + 1, sizeof(unsigned char));
			strncpy((char *)dir[i].name, (char *)f->name, sizeof(unsigned char) * 12);
			dir[i].namelen = 11;
		} else {
			dir[i].name = malloc(sizeof(uint32_t) * (f->namelen + 1));
			strncpy((char *)dir[i].name, (char *)f->uniname, sizeof(uint32_t) * (f->namelen + 1));
			dir[i].namelen = f->namelen;
		}
		dir[i].datalen = f->datalen;
		dir[i].attr = f->attr;
		dir[i].ctime = f->ctime;
		dir[i].atime = f->atime;
		dir[i].mtime = f->mtime;
	}
	/* If Dentry remains, Return error */
	if (tmp->next != NULL) {
		for (i = 0; tmp->next != NULL; i--, tmp = tmp->next);
	}

	return i;
}

/**
 * fat_reload_directory - function interface to read directories
 * @clu                   cluster index
 *
 * @return                 0 (success)
 *                        -1 (failed to read)
 */
int fat_reload_directory(uint32_t clu)
{
	int index = fat_get_index(clu);
	struct fat_fileinfo *f = NULL;

	fat_clean_dchain(index);
	f = ((struct fat_fileinfo *)(info.root[index])->data);
	f->cached = 0;
	return fat_traverse_directory(clu);
}

/**
 * fat_convert_character - Convert character by upcase-table
 * @src:                   Target characters in UTF-8
 * @len:                   Target characters length
 * @dist:                  convert result in UTF-8 (Output)
 *
 * return:                  0 (succeeded in obtaining filesystem)
 *                         -1 (failed)
 */
int fat_convert_character(const char *src, size_t len, char *dist)
{
	pr_warn("FAT: convert function isn't implemented.\n");
	return 0;
}

/**
 * fat_clean - function to clean opeartions
 * @index:     directory chain index
 *
 * @return     0 (success)
 *            -1 (already released)
 */
int fat_clean(uint32_t index)
{
	node2_t *tmp;
	struct fat_fileinfo *f;

	if ((!info.root[index])) {
		pr_warn("index %d was already released.\n", index);
		return -1;
	}

	tmp = info.root[index];
	f = (struct fat_fileinfo *)tmp->data;
	free(f->uniname);
	f->uniname = NULL;

	fat_clean_dchain(index);
	free(tmp->data);
	free(tmp);
	return 0;
}

/**
 * fat_set_fat_entry - Set FAT Entry to any cluster
 * @clu:               index of the cluster want to check
 * @entry:             any cluster index
 *
 * @retrun:             0
 *                     -1 (invalid image)
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
 * fat_get_fat_entry - Get cluster is continuous
 * @clu:               index of the cluster want to check
 * @entry:             any cluster index (Output)
 *
 * @return              0
 *                     -1 (invalid image)
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

/**
 * fat_print_dentry - function to print any directory entry
 * @clu:              index of the cluster want to check
 * @n:                directory entry index
 *
 * @return             0 (success)
 *                    -1 (failed to read)
 */
int fat_print_dentry(uint32_t clu, size_t n)
{
	int i;
	uint8_t ord = 0, attr = 0;
	uint32_t next_clu;
	size_t size = info.cluster_size;
	size_t entries = size / sizeof(struct fat_dentry);
	void *data;
	struct fat_dentry d;
	struct tm ctime, mtime, atime;

	fat_traverse_directory(clu);
	while (n > entries) {
		fat_get_fat_entry(clu, &next_clu);
		if (!fat_check_last_cluster(next_clu)) {
			pr_err("Directory size limit exceeded.\n");
			return -1;
		}
		n -= entries;
		clu = next_clu;
	}

	if (clu) {
		data = malloc(size);
		get_cluster(data, clu);
	} else {
		size = info.root_length * info.sector_size;
		entries = size / sizeof(struct fat_dentry);
		data = malloc(size);
		get_sector(data, (info.fat_offset + info.fat_length) * info.sector_size, info.root_length);
	}

	d = ((struct fat_dentry *)data)[n];
	ord = d.dentry.lfn.LDIR_Ord;
	attr = d.dentry.lfn.LDIR_Attr;

	/* Empty entry */
	if (ord == 0x00 || ord == 0xe5)
		goto out;
	/* Long File Name */
	if (attr == ATTR_LONG_FILE_NAME) {
		pr_msg("LDIR_Ord                        : %02x\n", d.dentry.lfn.LDIR_Ord);
		pr_msg("LDIR_Name1                      : ");
		for (i = 0; i < 10; i++)
			pr_msg("%02x", ((uint8_t *)d.dentry.lfn.LDIR_Name1)[i]);
		pr_msg("\n");
		pr_msg("LDIR_Attr                       : %02x\n", d.dentry.lfn.LDIR_Attr);
		pr_msg("LDIR_Type                       : %02x\n", d.dentry.lfn.LDIR_Type);
		pr_msg("LDIR_Chksum                     : %02x\n", d.dentry.lfn.LDIR_Chksum);
		pr_msg("LDIR_Name2                      : ");
		for (i = 0; i < 12; i++)
			pr_msg("%02x", ((uint8_t *)d.dentry.lfn.LDIR_Name2)[i]);
		pr_msg("\n");
		pr_msg("LDIR_FstClusLO                  : %02x\n", d.dentry.lfn.LDIR_FstClusLO);
		pr_msg("LDIR_Name3                      : ");
		for (i = 0; i < 4; i++)
			pr_msg("%02x", ((uint8_t *)d.dentry.lfn.LDIR_Name3)[i]);
		pr_msg("\n");
	} else {
		/* Directory Structure */
		pr_msg("DIR_Name                        : ");
		for (i = 0; i < 11; i++)
			pr_msg("%02x", d.dentry.dir.DIR_Name[i]);
		pr_msg("\n");
		pr_info("  ");
		for (i = 0; i < 11; i++)
			pr_info("%c", d.dentry.dir.DIR_Name[i]);
		pr_info("\n");
		pr_msg("DIR_Attr                        : %02x\n", d.dentry.dir.DIR_Attr);
		if (d.dentry.dir.DIR_Attr & ATTR_READ_ONLY)
			pr_info("  * ReadOnly\n");
		if (d.dentry.dir.DIR_Attr & ATTR_HIDDEN)
			pr_info("  * Hidden\n");
		if (d.dentry.dir.DIR_Attr & ATTR_SYSTEM)
			pr_info("  * System\n");
		if (d.dentry.dir.DIR_Attr & ATTR_VOLUME_ID)
			pr_info("  * Volume\n");
		if (d.dentry.dir.DIR_Attr & ATTR_DIRECTORY)
			pr_info("  * Directory\n");
		if (d.dentry.dir.DIR_Attr & ATTR_ARCHIVE)
			pr_info("  * Archive\n");
		pr_msg("DIR_NTRes                       : %02x\n", d.dentry.dir.DIR_NTRes);
		fat_convert_unixtime(&ctime, d.dentry.dir.DIR_CrtDate, d.dentry.dir.DIR_CrtTime, 0);
		pr_msg("DIR_CrtTimeTenth                : %02x\n", d.dentry.dir.DIR_CrtTimeTenth);
		pr_msg("DIR_CrtTime                     : %04x\n", d.dentry.dir.DIR_CrtTime);
		pr_msg("DIR_CrtDate                     : %04x\n", d.dentry.dir.DIR_CrtDate);
		pr_info("  %d-%02d-%02d %02d:%02d:%02d +%0d.%02d(s)\n",
				ctime.tm_year + 1980, ctime.tm_mon, ctime.tm_mday,
				ctime.tm_hour, ctime.tm_min, ctime.tm_sec,
				d.dentry.dir.DIR_CrtTimeTenth / 100,
				d.dentry.dir.DIR_CrtTimeTenth % 100);
		fat_convert_unixtime(&atime, d.dentry.dir.DIR_LstAccDate, 0, 0);
		pr_msg("DIR_LstAccDate                  : %04x\n", d.dentry.dir.DIR_LstAccDate);
		pr_info("  %d-%02d-%02d %02d:%02d:%02d\n",
				atime.tm_year + 1980, atime.tm_mon, atime.tm_mday,
				atime.tm_hour, atime.tm_min, atime.tm_sec);
		pr_msg("DIR_FstClusHI                   : %04x\n", d.dentry.dir.DIR_FstClusHI);
		fat_convert_unixtime(&mtime, d.dentry.dir.DIR_WrtDate, d.dentry.dir.DIR_WrtTime, 0);
		pr_msg("DIR_WrtTime                     : %04x\n", d.dentry.dir.DIR_WrtTime);
		pr_msg("DIR_WrtDate                     : %04x\n", d.dentry.dir.DIR_WrtDate);
		pr_info("  %d-%02d-%02d %02d:%02d:%02d\n",
				mtime.tm_year + 1980, mtime.tm_mon, mtime.tm_mday,
				mtime.tm_hour, mtime.tm_min, mtime.tm_sec);
		pr_msg("DIR_FstClusLO                   : %04x\n", d.dentry.dir.DIR_FstClusLO);
		pr_msg("DIR_FileSize                    : %08x\n", d.dentry.dir.DIR_FileSize);
	}

out:
	free(data);
	return 0;
}

/**
 * fat_set_bogus_entry - function to allocate cluster
 * @clu:                 cluster index
 *
 * @return               0 (success)
 */
int fat_set_bogus_entry(uint32_t clu)
{
	int ret = 0;
	uint32_t prev = 0;

	fat_get_fat_entry(clu, &prev);
	if (prev) {
		pr_warn("Cluster %u is already allocated.\n", clu);
		return 0;
	}

	switch (info.fstype) {
		case FAT12_FILESYSTEM:
			fat12_set_fat_entry(clu, 0xFFF);
			break;
		case FAT16_FILESYSTEM:
			fat16_set_fat_entry(clu, 0xFFFF);
			break;
		case FAT32_FILESYSTEM:
			fat32_set_fat_entry(clu, 0x0FFFFFFF);
			break;
		default:
			pr_err("Expected FAT filesystem, But this is not FAT filesystem.\n");
			ret = -1;
	}
	return ret;
}

/**
 * fat_release_cluster - function to release cluster
 * @clu:                 cluster index
 *
 * @return               0 (success)
 */
int fat_release_cluster(uint32_t clu)
{
	uint32_t prev = 0;
	
	fat_get_fat_entry(clu, &prev);
	if (!prev) {
		pr_warn("Cluster %u is already freed.\n", clu);
		return 0;
	}

	return fat_set_fat_entry(clu, 0x0);
}

/**
 * fat_create - function interface to create entry
 * @name:       Filename in UTF-8
 * @index:      Current Directory Index
 * @opt:        create option
 *
 * @return      0 (Success)
 */
int fat_create(const char *name, uint32_t clu, int opt)
{
	int i, j, namei;
	int long_len = 0;
	int count = 0;
	char shortname[11] = {0};
	uint16_t longname[MAX_NAME_LENGTH] = {0};
	void *data;
	uint8_t ord = LAST_LONG_ENTRY;
	uint32_t fst_clu = 0;
	size_t index = fat_get_index(clu);
	struct fat_fileinfo *f = (struct fat_fileinfo *)info.root[index]->data;
	size_t size;
	size_t entries;
	size_t cluster_num = 1;
	size_t new_cluster_num = 1;
	size_t name_len;
	struct fat_dentry *d;
	
	long_len = fat_create_nameentry(name, shortname, longname);
	if (long_len)
		count = (long_len / 13) + 1;

	/* Lookup last entry */
	if (clu) {
		data = malloc(info.cluster_size);
		get_cluster(data, clu);
		cluster_num = fat_concat_cluster(f, clu, &data);
		size = info.cluster_size * cluster_num;
		entries = (cluster_num * info.cluster_size) / sizeof(struct fat_dentry);
	} else {
		size = info.root_length * info.sector_size;
		entries = size / sizeof(struct fat_dentry);
		data = malloc(size);
		get_sector(data, (info.fat_offset + info.fat_length) * info.sector_size, info.root_length);
	}

	for (i = 0; i < entries; i++) {
		d = ((struct fat_dentry *)data) + i;
		if (d->dentry.dir.DIR_Name[0] == DENTRY_UNUSED)
			break;
	}

	if (clu) {
		new_cluster_num = (i + count + 1) * sizeof(struct fat_dentry) / info.cluster_size;
		if (new_cluster_num > cluster_num) {
			fat_alloc_clusters(f, clu, new_cluster_num - cluster_num);
			cluster_num = fat_concat_cluster(f, clu, &data);
			entries = (cluster_num * info.cluster_size) / sizeof(struct fat_dentry);
		}
	} else {
		if (((i + count + 1) * info.sector_size) > size) {
			pr_err("Can't create file entry in root directory.\n");
			return -1;
		}
	}

	if (!long_len)
		goto create_short;

	for (j = count; j != 0; j--) {
		namei = count - j;
		name_len = MIN(13, long_len - namei * 13);
		fat_init_lfn(d, longname, name_len, (unsigned char *)shortname, j | ord);
		ord = 0;
		d = ((struct fat_dentry *)data) + i + namei + 1;
	}

create_short:
	fat_init_dentry(d, (unsigned char *)shortname, 11);
	if (opt & CREATE_DIRECTORY) {
		d->dentry.dir.DIR_Attr = ATTR_DIRECTORY;
		fst_clu = fat_new_clusters(1);
		d->dentry.dir.DIR_FstClusHI = fst_clu >> 16;
		d->dentry.dir.DIR_FstClusLO = fst_clu & 0x0000ffff;
	}

	if (clu)
		fat_set_cluster(f, clu, data);
	else
		set_sector(data, (info.fat_offset + info.fat_length) * info.sector_size, info.root_length);

	free(data);
	return 0;
}

/**
 * fat_remove - function interface to remove entry
 * @name:       Filename in UTF-8
 * @index:      Current Directory Index
 * @opt:        create option
 *
 * @return       0 (Success)
 *              -1 (Not found)
 */
int fat_remove(const char *name, uint32_t clu, int opt)
{
	int i, j, ord;
	void *data;
	char shortname[11] = {0};
	uint16_t longname[MAX_NAME_LENGTH] = {0};
	size_t index = fat_get_index(clu);
	struct fat_fileinfo *f = (struct fat_fileinfo *)info.root[index]->data;
	size_t size;
	size_t entries;
	size_t cluster_num = 1;
	uint8_t chksum = 0;
	struct fat_dentry *d;

	/* convert UTF-8 to UTF16 */
	fat_create_nameentry(name, shortname, longname);
	chksum = fat_calculate_checksum((unsigned char *)shortname);

	/* Lookup last entry */
	if (clu) {
		data = malloc(info.cluster_size);
		get_cluster(data, clu);
		cluster_num = fat_concat_cluster(f, clu, &data);
		size = info.cluster_size * cluster_num;
		entries = (cluster_num * info.cluster_size) / sizeof(struct fat_dentry);
	} else {
		size = info.root_length * info.sector_size;
		entries = size / sizeof(struct fat_dentry);
		data = malloc(size);
		get_sector(data, (info.fat_offset + info.fat_length) * info.sector_size, info.root_length);
	}

	for (i = 0; i < entries; i++) {
		d = ((struct fat_dentry *)data) + i;
		if (d->dentry.lfn.LDIR_Ord == DENTRY_UNUSED)
			goto out;

		if (d->dentry.lfn.LDIR_Ord == DENTRY_DELETED)
			continue;

		if (d->dentry.lfn.LDIR_Attr == ATTR_LONG_FILE_NAME) {
			ord = d->dentry.lfn.LDIR_Ord & (~LAST_LONG_ENTRY);
			if (d->dentry.lfn.LDIR_Chksum != chksum) {
				i += ord;
				continue;
			}
			for (j = 0; j < ord; j++) {
				d = ((struct fat_dentry *)data) + i;
				d->dentry.lfn.LDIR_Ord = DENTRY_DELETED;
			}
		} else {
			if (!strncmp(shortname, (char *)d->dentry.dir.DIR_Name, 11))
				d->dentry.lfn.LDIR_Ord = DENTRY_DELETED;
		}
	}
out:

	if (clu)
		fat_set_cluster(f, clu, data);
	else
		set_sector(data, (info.fat_offset + info.fat_length) * info.sector_size, info.root_length);
	free(data);
	return 0;
}

/**
 * fat_update_dentry - function interface to update directory entry
 * @clu:               Current Directory cluster
 * @i:                 Directory entry index
 *
 * @return             0 (Success)
 */
int fat_update_dentry(uint32_t clu, int i)
{
	int type = 0;
	char buf[64] = {0};
	struct fat_dentry new = {0};
	void *data;
	size_t size = info.cluster_size;
	struct fat_dentry *old;

	/* Lookup last entry */
	data = malloc(size);
	get_cluster(data, clu);

	old = ((struct fat_dentry *)data) + i;

	pr_msg("Please select a Entry type.\n");
	pr_msg("1) Directory Structure\n");
	pr_msg("2) Long File Name\n");
	pr_msg("3) Other\n");

	while (1) {
		pr_msg("#? ");
		fflush(stdout);
		if (fgets(buf, 32, stdin) == NULL)
			continue;
		sscanf(buf, "%d", &type);
		if (0 < type && type < 4)
			break;
	}
	pr_msg("\n");

	switch (type) {
		case 1:
			fat_init_dentry(&new, (unsigned char *)"", 0);
			fat_update_file(old, &new);
			break;
		case 2:
			fat_init_lfn(&new, (uint16_t *)"", 0, (unsigned char*)"", 0);
			fat_update_lfn(old, &new);
			break;
		default:
			memset(buf, 0x00, 64);
			input("Please input any strings", buf);
			for (i = 0; i < sizeof(struct fat_dentry); i++)
				sscanf(buf + (i * 2), "%02hhx", ((uint8_t *)&new) + i);
			break;
	}
	for (i = 0; i < sizeof(struct fat_dentry); i++) {
		pr_msg("%x ", *(((uint8_t *)&new) + i));
	}
	pr_msg("\n");
	memcpy(old, &new, sizeof(struct fat_dentry));

	set_cluster(data, clu);
	free(data);
	return 0;
}

/**
 * fat_trim -  function interface to trim cluster
 * @clu:       Current Directory Index
 *
 * @return     0 (Success)
 */
int fat_trim(uint32_t clu)
{
	int i, j;
	void *data;
	size_t index = fat_get_index(clu);
	struct fat_fileinfo *f = (struct fat_fileinfo *)info.root[index]->data;
	size_t size;
	size_t entries;
	size_t cluster_num = 1;
	size_t allocate_cluster = 1;
	struct fat_dentry *src, *dist;

	/* Lookup last entry */
	if (clu) {
		data = malloc(info.cluster_size);
		get_cluster(data, clu);
		cluster_num = fat_concat_cluster(f, clu, &data);
		size = info.cluster_size * cluster_num;
		entries = (cluster_num * info.cluster_size) / sizeof(struct fat_dentry);
	} else {
		size = info.root_length * info.sector_size;
		entries = size / sizeof(struct fat_dentry);
		data = malloc(size);
		get_sector(data, (info.fat_offset + info.fat_length) * info.sector_size, info.root_length);
	}

	for (i = 0, j = 0; i < entries; i++) {
		src = ((struct fat_dentry *)data) + i;
		dist = ((struct fat_dentry *)data) + j;
		if (!src->dentry.dir.DIR_Name[0])
			break;

		if (src->dentry.dir.DIR_Name[0] == DENTRY_DELETED)
			continue;

		if (i != j++)
			memcpy(dist, src, sizeof(struct fat_dentry));
	}

	allocate_cluster = ((sizeof(struct fat_dentry) * j) / info.cluster_size) + 1;
	while (j < entries) {
		dist = ((struct fat_dentry *)data) + j++;
		memset(dist, 0, sizeof(struct fat_dentry));
	}

	if (clu) {
		fat_set_cluster(f, clu, data);
		fat_free_clusters(f, clu, cluster_num - allocate_cluster);
	} else {
		set_sector(data, (info.fat_offset + info.fat_length) * info.sector_size, info.root_length);
	}
	free(data);
	return 0;
}
