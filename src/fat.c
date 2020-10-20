// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2020 LeavaTail
 */
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
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
static int fat_traverse_directory(uint32_t);
static uint8_t fat_calculate_checksum(unsigned char *);
int fat_clean_dchain(uint32_t);

/* File function prototype */
static void fat_create_fileinfo(node2_t *, uint32_t, struct fat_dentry *, uint16_t *, size_t);
static void fat_convert_uniname(uint16_t *, uint64_t, unsigned char *);
static int fat_create_shortname(uint16_t *, char *);
static int fat_convert_shortname(const char *, char *);
static int fat_create_nameentry(const char *, char *, uint16_t *);
static void fat_convert_unixtime(struct tm *, uint16_t, uint16_t, uint8_t);
static int fat_validate_character(const char);
static int fat_query_timestamp(struct tm *, uint16_t *, uint16_t *, uint8_t *, int);

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
int fat_alloc_cluster(uint32_t);
int fat_release_cluster(uint32_t);
int fat_create(const char *, uint32_t, int);
int fat_remove(const char *, uint32_t, int);
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
	.alloc = fat_alloc_cluster,
	.release = fat_release_cluster,
	.create = fat_create,
	.remove = fat_remove,
	.trim = fat_trim,
};

static const struct query fat_create_prompt[] = {
	{"File Attributes", 5, (char *[]){
										 "  Bit0  ReadOnly",
										 "  Bit1  Hidden",
										 "  Bit2  System",
										 "  Bit3  Volume ID",
										 "  Bit4  Directory",
										 "  Bit5  Archive"}},
	{"NTRes", 2, (char *[]){
							"  0x08 filename is lower characters",
							"  0x10 only file externsion is lower characters"
						   }},
	{"Reserverd", 0, NULL},
	{"Name Length", 0, NULL},
	{"First Cluster", 0, NULL},
	{"Data Length", 0, NULL},
	{"Long File Name type", 0, NULL},
	{"Long File Name checksum", 0, NULL},
	{"Anything", 0, NULL},
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
 * @retrun:               next cluster (@clu has next cluster)
 *                        0            (@clu doesn't have next cluster, or failed to realloc)
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
 * @b:                boot sector pointer in FAT
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
		pr_msg("0x%x", b->reserved_info.fat32_reserved_info.BS_VolID[i]);
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
	uint8_t *fat;

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
	size_t size = info.cluster_size;
	size_t entries = size / sizeof(struct fat_dentry);
	size_t namelen = 0;
	void *data;
	struct fat_dentry d;

	if (f->datalen > 0) {
		pr_debug("Directory %s was already traversed.\n", f->name);
		return 0;
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

	do {
		for (i = 0; i < entries; i++) {
			namelen = 0;
			d = ((struct fat_dentry *)data)[i];
			attr = d.dentry.lfn.LDIR_Attr;
			ord = d.dentry.lfn.LDIR_Ord;
			/* Empty entry */
			if (ord == 0x00)
				goto out;
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
					if (i + ord >= entries) {
						clu = fat_concat_cluster(clu, &data, size);
						size += info.cluster_size;
						entries = size / sizeof(struct exfat_dentry);
					}
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
		clu = fat_concat_cluster(clu, &data, size);
		if (!clu)
			break;

		size += info.cluster_size;
		entries = size / sizeof(struct exfat_dentry);
	} while (1);
out:
	free(data);
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
	 ((struct fat_fileinfo *)(head->data))->datalen++;

	/* If this entry is Directory, prepare to create next chain */
	if ((f->attr & ATTR_DIRECTORY) && (!fat_check_dchain(next_clu))) {
		struct fat_fileinfo *d = malloc(sizeof(struct fat_fileinfo));
		strncpy((char *)d->name, (char *)f->name, 11);
		d->uniname = malloc(f->namelen + 1);
		strncpy((char *)d->uniname, (char *)f->uniname, f->namelen + 1);
		d->namelen = namelen;
		d->datalen = 0;
		d->attr = file->dentry.dir.DIR_Attr;

		index = fat_get_index(next_clu);
		info.root[index] = init_node2(next_clu, d);
	}
}

/**
 * fat_convert_uniname - function to get filename
 * @uniname:             filename dentry in UTF-16
 * @name_len:            filename length
 * @name:                filename in UTF-8 (output)
 */
static void fat_convert_uniname(uint16_t *uniname, uint64_t name_len, unsigned char *name)
{
	utf16s_to_utf8s(uniname, name_len, name);
}

/**
 * fat_create_shortname -  function to convert filename to shortname
 * @longname:              filename dentry in UTF-16
 * @name:                  filename in ascii (output)
 *
 * @return                 0 (Same as input name)
 *                         1 (Difference from input data)
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
 * fat_convert_shortname -  function to convert filename to shortname
 * @shortname:              filename dentry in ASCII
 * @name:                   filename in ascii (output)
 *
 * @return                  name length
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
 * @shortname:            filename in ASCII (output)
 * @longname:             filename in UTF-16 (output)
 *
 * @return                0 (Only shortname)
 *                      > 0 (longname length)
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
 * exfat_convert_unixname - function to get timestamp in file
 * @t:                      output pointer
 * @date:                   Date Field in File Directory Entry
 * @time:                   Timestamp Field in File Directory Entry
 * @subsec:                 10msincrement Field in File Directory Entry
 */
static void fat_convert_unixtime(struct tm *t, uint16_t date, uint16_t time, uint8_t subsec)
{
	t->tm_year = (date >> FAT_YEAR) & 0x7f;
	t->tm_mon  = (date >> FAT_MONTH) & 0x0f;
	t->tm_mday = (date >> FAT_DAY) & 0x1f;
	t->tm_hour = (time >> EXFAT_HOUR) & 0x0f;
	t->tm_min  = (time >> EXFAT_MINUTE) & 0x3f;
	t->tm_sec  = (time & 0x1f) * 2;
	t->tm_sec += subsec / 100;
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

/**
 * fat_query_timestamp - Prompt user for timestamp
 * @t:                   local timezone
 * @timestamp:           Time Field (Output)
 * @subsec:              Time subsecond Field (Output)
 * @quiet:               set parameter without ask
 *
 * @return               0 (Success)
 */
static int fat_query_timestamp(struct tm *t, uint16_t *__time, uint16_t *__date,
		uint8_t *subsec, int quiet)
{
	char buf[QUERY_BUFFER_SIZE] = {};

	if (!quiet) {
		pr_msg("Timestamp (UTC)\n");
		pr_msg("Select (Default: %d-%02d-%02d %02d:%02d:%02d.%02d): ",
				t->tm_year + 1900,
				t->tm_mon + 1,
				t->tm_mday,
				t->tm_hour,
				t->tm_min,
				t->tm_sec,
				0);
		fflush(stdout);

		if (!fgets(buf, QUERY_BUFFER_SIZE, stdin))
			return -1;

		if (buf[0] != '\n') {
			sscanf(buf, "%d-%02d-%02d %02d:%02d:%02d.%02hhd",
					&(t->tm_year),
					&(t->tm_mon),
					&(t->tm_mday),
					&(t->tm_hour),
					&(t->tm_min),
					&(t->tm_sec),
					subsec);
			t->tm_year -= 1900;
			t->tm_mon -= 1;
		}

		pr_msg("\n");
	}

	*__time = (t->tm_hour << EXFAT_HOUR) |
		(t->tm_min << EXFAT_MINUTE) |
		(t->tm_sec / 2);
	*__date = ((t->tm_year - 80) << FAT_YEAR) |
		((t->tm_mon + 1) << FAT_MONTH) |
		(t->tm_mday << EXFAT_DAY);
	*subsec += ((t->tm_sec % 2) * 100);
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
		if ((!info.root[index]) || (f->datalen == 0)) {
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
 * @return                0 (success)
 *                       -1 (failed to read)
 */
int fat_reload_directory(uint32_t clu)
{
	int index = fat_get_index(clu);
	struct fat_fileinfo *f = NULL;

	fat_clean_dchain(index);
	f = ((struct fat_fileinfo *)(info.root[index])->data);
	f->datalen = 0;
	return fat_traverse_directory(clu);
}

/**
 * fat_convert_character - Convert character by upcase-table
 * @src:                   Target characters in UTF-8
 * @len:                   Target characters length
 * @dist:                  convert result in UTF-8 (Output)
 *
 * return:                 0 (succeeded in obtaining filesystem)
 *                        -1 (failed)
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
 * @retrun:            0
 *                    -1 (invalid image)
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
 * @entry:             any cluster index
 *
 * @return             0
 *                    -1 (invalid image)
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
 * @return            0 (success)
 *                   -1 (failed to read)
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

	fat_traverse_directory(clu);
	while (n > entries) {
		fat_get_fat_entry(clu, &next_clu);
		if (!next_clu) {
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
		pr_msg("DIR_Attr                        : %02x\n", d.dentry.dir.DIR_Attr);
		pr_msg("DIR_NTRes                       : %02x\n", d.dentry.dir.DIR_NTRes);
		pr_msg("DIR_CrtTimeTenth                : %02x\n", d.dentry.dir.DIR_CrtTimeTenth);
		pr_msg("DIR_CrtTime                     : %04x\n", d.dentry.dir.DIR_CrtTime);
		pr_msg("DIR_CrtDate                     : %04x\n", d.dentry.dir.DIR_CrtDate);
		pr_msg("DIR_LstAccDate                  : %04x\n", d.dentry.dir.DIR_LstAccDate);
		pr_msg("DIR_FstClusHI                   : %04x\n", d.dentry.dir.DIR_FstClusHI);
		pr_msg("DIR_WrtTime                     : %04x\n", d.dentry.dir.DIR_WrtTime);
		pr_msg("DIR_WrtDate                     : %04x\n", d.dentry.dir.DIR_WrtDate);
		pr_msg("DIR_FstClusLO                   : %04x\n", d.dentry.dir.DIR_FstClusLO);
		pr_msg("DIR_FileSize                    : %08x\n", d.dentry.dir.DIR_FileSize);
	}

out:
	free(data);
	return 0;
}

/**
 * fat_alloc_cluster - function to allocate cluster
 * @clu:               cluster index
 *
 * @return             0 (success)
 */
int fat_alloc_cluster(uint32_t clu)
{
	int ret = 0;

	switch (info.fstype) {
		case FAT12_FILESYSTEM:
			fat12_set_fat_entry(clu, 0x001);
			break;
		case FAT16_FILESYSTEM:
			fat16_set_fat_entry(clu, 0x0001);
			break;
		case FAT32_FILESYSTEM:
			fat32_set_fat_entry(clu, 0x00000001);
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
 *                      -1 (failed)
 */
int fat_release_cluster(uint32_t clu)
{
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
	int quiet = 0;
	int long_len = 0;
	int count = 0;
	char shortname[11] = {0};
	uint16_t longname[MAX_NAME_LENGTH] = {0};
	uint8_t chksum = 0;
	void *data;
	uint8_t ord = LAST_LONG_ENTRY;
	uint32_t fstclus = 0;
	size_t size = info.cluster_size;
	size_t entries = size / sizeof(struct fat_dentry);
	uint8_t subsec;
	struct fat_dentry *d;
	uint16_t __time, __date;
	time_t t = time(NULL);
	struct tm *utc = gmtime(&t);
	
	long_len = fat_create_nameentry(name, shortname, longname);
	if (long_len)
		count = (long_len / 13) + 1;
	chksum = fat_calculate_checksum((unsigned char *)shortname);

	/* Lookup last entry */
	if (clu) {
		data = malloc(size);
		get_cluster(data, clu);
	} else {
		size = info.root_length * info.sector_size;
		entries = size / sizeof(struct fat_dentry);
		data = malloc(size);
		get_sector(data, (info.fat_offset + info.fat_length) * info.sector_size, info.root_length);
	}

	for (i = 0; i < entries; i++){
		d = ((struct fat_dentry *)data) + i;
		if (d->dentry.dir.DIR_Name[0] == DENTRY_UNUSED)
			break;
	}

	if (opt & OPTION_QUIET)
		quiet = 1;

	if (!long_len)
		goto create_short;

	if (!quiet) {
		char buf[4] = {0};
		pr_msg("Do you want to create Long File Name entry? (Default [y]/n): ");
		fflush(stdout);
		if (fgets(buf, 4, stdin) != NULL) {
			if (!strncmp(buf, "n", 1))
			goto create_short;
		}
	}

	for (j = count; j != 0; j--) {
		ord |= j;
		namei = count - j;
		memcpy(d->dentry.lfn.LDIR_Name1, longname + namei * 13, 5);
		memcpy(d->dentry.lfn.LDIR_Name1, longname + 6 + namei * 13, 6);
		memcpy(d->dentry.lfn.LDIR_Name1, longname + 11 + namei * 13, 2);
		query_param(fat_create_prompt[0], &(d->dentry.lfn.LDIR_Attr), ATTR_LONG_FILE_NAME, 1, quiet);
		query_param(fat_create_prompt[6], &(d->dentry.lfn.LDIR_Type), 0, 1, quiet);
		query_param(fat_create_prompt[7], &(d->dentry.lfn.LDIR_Chksum), chksum, 1, quiet);
		query_param(fat_create_prompt[4], &(d->dentry.lfn.LDIR_FstClusLO), 0, 1, quiet);
		ord = 0;
		d = ((struct fat_dentry *)data) + i + namei + 1;
	}

create_short:
	memcpy(d->dentry.dir.DIR_Name, shortname, 11);
	query_param(fat_create_prompt[0], &(d->dentry.dir.DIR_Attr), ATTR_ARCHIVE, 1, quiet);
	query_param(fat_create_prompt[1], &(d->dentry.dir.DIR_NTRes), 0, 1, quiet);
	fat_query_timestamp(utc, &__time, &__date, &subsec, quiet);
	d->dentry.dir.DIR_CrtTimeTenth = subsec;
	d->dentry.dir.DIR_CrtTime = __time;
	d->dentry.dir.DIR_CrtDate = __date;
	d->dentry.dir.DIR_LstAccDate = __date;
	d->dentry.dir.DIR_WrtTime = __time;
	d->dentry.dir.DIR_WrtDate = __date;
	query_param(fat_create_prompt[4], &fstclus, 0, 4, quiet);
	d->dentry.dir.DIR_FstClusHI = fstclus & 0x00ff;
	d->dentry.dir.DIR_FstClusLO = fstclus >> 16;
	query_param(fat_create_prompt[3], &(d->dentry.dir.DIR_FileSize), 0, 4, quiet);

	if (clu)
		set_cluster(data, clu);
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
 * @return      0 (Success)
 *             -1 (Not found)
 */
int fat_remove(const char *name, uint32_t clu, int opt)
{
	int i, j, ord;
	void *data;
	char shortname[11] = {0};
	uint16_t longname[MAX_NAME_LENGTH] = {0};
	size_t size = info.cluster_size;
	uint8_t chksum = 0;
	size_t entries = size / sizeof(struct fat_dentry);
	struct fat_dentry *d;

	/* convert UTF-8 to UTF16 */
	fat_create_nameentry(name, shortname, longname);
	chksum = fat_calculate_checksum((unsigned char *)shortname);

	/* Lookup last entry */
	if (clu) {
		data = malloc(size);
		get_cluster(data, clu);
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
		set_cluster(data, clu);
	else
		set_sector(data, (info.fat_offset + info.fat_length) * info.sector_size, info.root_length);
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
	size_t size = info.cluster_size;
	size_t entries = size / sizeof(struct fat_dentry);
	struct fat_dentry *src, *dist;

	/* Lookup last entry */
	data = malloc(size);
	get_cluster(data, clu);

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

	while (j < entries) {
		dist = ((struct fat_dentry *)data) + j++;
		memset(dist, 0, sizeof(struct fat_dentry));
		dist->dentry.dir.DIR_Name[0] = DENTRY_DELETED;
	}

	set_cluster(data, clu);
	free(data);
	return 0;
}
