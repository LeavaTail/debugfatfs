// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2020 LeavaTail
 */
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>

#include "debugfatfs.h"

/* Generic function prototype */
static uint32_t exfat_concat_cluster(struct exfat_fileinfo *, uint32_t, void **);
static uint32_t exfat_set_cluster(struct exfat_fileinfo *, uint32_t, void *);

/* Boot sector function prototype */
static int exfat_load_bootsec(struct exfat_bootsec *);
static void exfat_print_upcase(void);
static void exfat_print_label(void);
static int exfat_load_bitmap(uint32_t);
static int exfat_save_bitmap(uint32_t, uint32_t);

/* FAT-entry function prototype */
static uint32_t exfat_check_fat_entry(uint32_t);
static uint32_t exfat_update_fat_entry(uint32_t, uint32_t);
static int exfat_create_fat_chain(struct exfat_fileinfo *, uint32_t);

/* cluster function prototype */
static int exfat_get_last_cluster(struct exfat_fileinfo *, uint32_t);
static int exfat_alloc_clusters(struct exfat_fileinfo *, uint32_t, size_t);
static int exfat_free_clusters(struct exfat_fileinfo *, uint32_t, size_t);
static int exfat_new_clusters(size_t);

/* Directory chain function prototype */
static int exfat_check_dchain(uint32_t);
static int exfat_get_index(uint32_t);
static int exfat_traverse_directory(uint32_t);
static int exfat_clean_dchain(uint32_t);

/* File function prototype */
static void exfat_create_fileinfo(node2_t *,
		uint32_t, struct exfat_dentry *, struct exfat_dentry *, uint16_t *);
static int exfat_init_file(struct exfat_dentry *, uint16_t *, size_t);
static int exfat_init_stream(struct exfat_dentry *, uint16_t *, size_t);
static int exfat_init_filename(struct exfat_dentry *, uint16_t *, size_t);
static int exfat_init_bitmap(struct exfat_dentry *);
static int exfat_init_upcase(struct exfat_dentry *);
static int exfat_init_volume(struct exfat_dentry *, uint16_t *, size_t);
static int exfat_update_file(struct exfat_dentry *, struct exfat_dentry *);
static int exfat_update_stream(struct exfat_dentry *, struct exfat_dentry *);
static int exfat_update_filename(struct exfat_dentry *, struct exfat_dentry *);
static int exfat_update_bitmap(struct exfat_dentry *, struct exfat_dentry *);
static int exfat_update_upcase(struct exfat_dentry *, struct exfat_dentry *);
static int exfat_update_volume(struct exfat_dentry *, struct exfat_dentry *);
static uint16_t exfat_calculate_checksum(unsigned char *, unsigned char);
static uint32_t exfat_calculate_tablechecksum(unsigned char *, uint64_t);
static void exfat_convert_uniname(uint16_t *, uint64_t, unsigned char *);
static uint16_t exfat_calculate_namehash(uint16_t *, uint8_t);
static int exfat_update_filesize(struct exfat_fileinfo *, uint32_t);
static void exfat_convert_unixtime(struct tm *, uint32_t, uint8_t, uint8_t);
static int exfat_convert_timezone(uint8_t);
static void exfat_convert_exfattime(struct tm *, uint32_t *, uint8_t *);
static void exfat_convert_exfattimezone(uint8_t *, int);
static int exfat_parse_timezone(char *, uint8_t *);

/* Operations function prototype */
int exfat_print_bootsec(void);
int exfat_print_fsinfo(void);
int exfat_lookup(uint32_t, char *);
int exfat_readdir(struct directory *, size_t, uint32_t);
int exfat_reload_directory(uint32_t);
int exfat_convert_character(const char *, size_t, char *);
int exfat_clean(uint32_t);
int exfat_set_fat_entry(uint32_t, uint32_t);
int exfat_get_fat_entry(uint32_t, uint32_t *);
int exfat_print_dentry(uint32_t, size_t);
int exfat_set_bitmap(uint32_t);
int exfat_clear_bitmap(uint32_t);
int exfat_create(const char *, uint32_t, int);
int exfat_remove(const char *, uint32_t, int);
int exfat_update_dentry(uint32_t, int);
int exfat_trim(uint32_t);
int exfat_fill(uint32_t, uint32_t);

static const struct operations exfat_ops = {
	.statfs = exfat_print_bootsec,
	.info = exfat_print_fsinfo,
	.lookup =  exfat_lookup,
	.readdir = exfat_readdir,
	.reload = exfat_reload_directory,
	.convert = exfat_convert_character,
	.clean = exfat_clean,
	.setfat = exfat_set_fat_entry,
	.getfat = exfat_get_fat_entry,
	.dentry = exfat_print_dentry,
	.alloc = exfat_set_bitmap,
	.release = exfat_clear_bitmap,
	.create = exfat_create,
	.remove = exfat_remove,
	.update = exfat_update_dentry,
	.trim = exfat_trim,
	.fill = exfat_fill,
};

/*************************************************************************************************/
/*                                                                                               */
/* GENERIC FUNCTION                                                                              */
/*                                                                                               */
/*************************************************************************************************/
/**
 * exfat_concat_cluster - Contatenate cluster @data with next_cluster
 * @f:                    file information pointer
 * @clu:                  index of the cluster
 * @data:                 The cluster (Output)
 *
 * @retrun:               cluster count (@clu has next cluster)
 *                        0             (@clu doesn't have next cluster, or failed to realloc)
 */
static uint32_t exfat_concat_cluster(struct exfat_fileinfo *f, uint32_t clu, void **data)
{
	int i;
	void *tmp;
	uint32_t tmp_clu = 0;
	size_t allocated = 1;
	size_t cluster_num = (f->datalen + (info.cluster_size - 1)) / info.cluster_size;

	if (cluster_num <= 1)
		return cluster_num;

	/* NO_FAT_CHAIN */
	if (f->flags & ALLOC_NOFATCHAIN) {
		if (!(tmp = realloc(*data, info.cluster_size * cluster_num)))
			return 0;
		*data = tmp;
		for (i = 1; i < cluster_num; i++) {
			if (exfat_load_bitmap(clu + i) != 1) {
				pr_warn("cluster %u isn't allocated cluster.\n", clu + i);
				break;
			}
		}
		get_clusters(*data + info.cluster_size, clu + 1, cluster_num - 1);
		return cluster_num;
	}

	/* FAT_CHAIN */
	for (tmp_clu = clu; allocated < cluster_num; allocated++) { 
		if (!(tmp_clu = exfat_check_fat_entry(tmp_clu)))
			break;
		if (exfat_load_bitmap(tmp_clu) != 1)
			pr_warn("cluster %u isn't allocated cluster.\n", tmp_clu);
	}

	if (!(tmp = realloc(*data, info.cluster_size * allocated)))
		return 0;
	*data = tmp;

	for (i = 1; i < allocated; i++) {
		clu = exfat_check_fat_entry(clu);
		get_cluster(*data + info.cluster_size * i, clu);
	}

	return allocated;
}

/**
 * exfat_set_cluster - Set Raw-Data from any sector
 * @f:                 file information pointer
 * @clu:               index of the cluster
 * @data:              The cluster
 *
 * @retrun:            cluster count (@clu has next cluster)
 *                     0             (@clu doesn't have next cluster, or failed to realloc)
 */
static uint32_t exfat_set_cluster(struct exfat_fileinfo *f, uint32_t clu, void *data)
{
	size_t allocated = 0;
	size_t cluster_num = (f->datalen + (info.cluster_size - 1)) / info.cluster_size;

	if (cluster_num <= 1) {
		set_cluster(data, clu);
		return cluster_num;
	}
	/* NO_FAT_CHAIN */
	if (f->flags & ALLOC_NOFATCHAIN) {
		set_clusters(data, clu, cluster_num);
		return cluster_num;
	}

	/* FAT_CHAIN */
	for (allocated = 0; allocated < cluster_num; allocated++) {
		set_cluster(data + info.cluster_size * allocated, clu);
		if (!(clu = exfat_check_fat_entry(clu)))
			break;
	}

	return allocated + 1;
}

/**
 * exfat_check_filesystem - Whether or not exFAT filesystem
 * @boot:                   boot sector pointer
 *
 * @return:                 1 (Image is exFAT filesystem)
 *                          0 (Image isn't exFAT filesystem)
 */
int exfat_check_filesystem(struct pseudo_bootsec *boot)
{
	int ret = 0;
	struct exfat_bootsec *b;
	struct exfat_fileinfo *f;

	b = (struct exfat_bootsec *)boot;
	if (!strncmp((char *)boot->FileSystemName, "EXFAT   ", 8)) {
		info.fstype = EXFAT_FILESYSTEM;

		info.fat_offset = b->FatOffset;
		info.heap_offset = b->ClusterHeapOffset;
		info.root_offset = b->FirstClusterOfRootDirectory;
		info.sector_size  = 1 << b->BytesPerSectorShift;
		info.cluster_size = (1 << b->SectorsPerClusterShift) * info.sector_size;
		info.cluster_count = b->ClusterCount;
		info.fat_length = b->NumberOfFats * b->FatLength * info.sector_size;
		f = malloc(sizeof(struct exfat_fileinfo));
		f->name = malloc(sizeof(unsigned char *) * (strlen("/") + 1));
		strncpy((char *)f->name, "/", strlen("/") + 1);
		f->namelen = 1;
		f->datalen = info.cluster_count * info.cluster_size;
		f->attr = ATTR_DIRECTORY;
		f->hash = 0;
		info.root[0] = init_node2(info.root_offset, f);

		info.ops = &exfat_ops;
		ret = 1;
	}

	return ret;
}

/*************************************************************************************************/
/*                                                                                               */
/* BOOT SECTOR FUNCTION                                                                          */
/*                                                                                               */
/*************************************************************************************************/
/**
 * exfat_load_bootsec - load boot sector
 * @b:                  boot sector pointer in exFAT (Output)
 *
 * @return               0 (success)
 *                      -1 (failed to read)
 */
static int exfat_load_bootsec(struct exfat_bootsec *b)
{
	return get_sector(b, 0, 1);
}

/**
 * exfat_print_upcase - print upcase table
 */
static void exfat_print_upcase(void)
{
	int byte, offset;
	size_t uni_count = 0x10 / sizeof(uint16_t);
	size_t length = info.upcase_size;

	/* Output table header */
	pr_msg("Offset  ");
	for (byte = 0; byte < uni_count; byte++)
		pr_msg("  +%u ",byte);
	pr_msg("\n");

	/* Output Table contents */
	for (offset = 0; offset < length / uni_count; offset++) {
		pr_msg("%04lxh:  ", offset * 0x10 / sizeof(uint16_t));
		for (byte = 0; byte < uni_count; byte++) {
			pr_msg("%04x ", info.upcase_table[offset * uni_count + byte]);
		}
		pr_msg("\n");
	}
}

/**
 * exfat_print_label - print volume label
 */
static void exfat_print_label(void)
{
	unsigned char *name;

	pr_msg("volume Label: ");
	name = malloc(info.vol_length * sizeof(uint16_t) + 1);
	memset(name, '\0', info.vol_length * sizeof(uint16_t) + 1);
	utf16s_to_utf8s(info.vol_label, info.vol_length, name);
	pr_msg("%s\n", name);
	free(name);
}

/**
 * exfat_load_bitmap - function to load allocation table
 * @clu:               cluster index
 *
 * @return              0 (cluster as available for allocation)
 *                      1 (cluster as not available for allocation)
 *                     -1 (failed)
 */
static int exfat_load_bitmap(uint32_t clu)
{
	int offset, byte;
	uint8_t entry;

	if (clu < EXFAT_FIRST_CLUSTER || clu > info.cluster_count + 1) {
		pr_warn("cluster: %u is invalid.\n", clu);
		return -1;
	}

	clu -= EXFAT_FIRST_CLUSTER;
	byte = clu / CHAR_BIT;
	offset = clu % CHAR_BIT;

	entry = info.alloc_table[byte];
	return (entry >> offset) & 0x01;
}

/**
 * exfat_save_bitmap - function to save allocation table
 * @clu:               cluster index
 * @value:             Bit
 *
 * @return              0 (success)
 *                     -1 (failed)
 */
static int exfat_save_bitmap(uint32_t clu, uint32_t value)
{
	int offset, byte;
	uint8_t mask = 0x01;
	uint8_t *raw_bitmap;

	if (clu < EXFAT_FIRST_CLUSTER || clu > info.cluster_count + 1) {
		pr_err("cluster: %u is invalid.\n", clu);
		return -1;
	}

	clu -= EXFAT_FIRST_CLUSTER;
	byte = clu / CHAR_BIT;
	offset = clu % CHAR_BIT;

	pr_debug("index %u: allocation bitmap is 0x%x ->", clu, info.alloc_table[byte]);
	mask <<= offset;
	if (value)
		info.alloc_table[byte] |= mask;
	else
		info.alloc_table[byte] &= ~mask;

	pr_debug("0x%x\n", info.alloc_table[byte]);
	raw_bitmap = malloc(info.cluster_size);
	get_cluster(raw_bitmap, info.alloc_cluster);
	if (value)
		raw_bitmap[byte] |= mask;
	else
		raw_bitmap[byte] &= ~mask;
	set_cluster(raw_bitmap, info.alloc_cluster);
	free(raw_bitmap);
	return 0;
}

/*************************************************************************************************/
/*                                                                                               */
/* FAT-ENTRY FUNCTION                                                                            */
/*                                                                                               */
/*************************************************************************************************/
/**
 * exfat_check_fat_entry - Whether or not cluster is continuous
 * @clu:                   index of the cluster want to check
 *
 * @retrun:                next cluster (@clu has next cluster)
 *                         0            (@clu doesn't have next cluster)
 */
static uint32_t exfat_check_fat_entry(uint32_t clu)
{
	uint32_t ret;
	size_t entry_per_sector = info.sector_size / sizeof(uint32_t);
	uint32_t fat_index = (info.fat_offset +  clu / entry_per_sector) * info.sector_size;
	uint32_t *fat;
	uint32_t offset = (clu) % entry_per_sector;

	fat = malloc(info.sector_size);
	get_sector(fat, fat_index, 1);
	/* validate index */
	if (clu == EXFAT_BADCLUSTER) {
		ret = 0;
		pr_err("cluster: %u is bad cluster.\n", clu);
	} else if (clu == EXFAT_LASTCLUSTER) {
		ret = 0;
		pr_debug("cluster: %u is the last cluster of cluster chain.\n", clu);
	} else if (clu < EXFAT_FIRST_CLUSTER || clu > info.cluster_count + 1) {
		ret = 0;
		pr_debug("cluster: %u is invalid.\n", clu);
	} else {
		ret = fat[offset];
		if (ret == EXFAT_LASTCLUSTER)
			ret = 0;
		else
			pr_debug("cluster: %u has chain. next is 0x%x.\n", clu, fat[offset]);
	}

	free(fat);
	return ret;
}

/**
 * exfat_update_fat_entry - Update FAT Entry to any cluster
 * @clu:                    index of the cluster want to check
 * @entry:                  any cluster index
 *
 * @retrun:                 previous FAT entry
 */
static uint32_t exfat_update_fat_entry(uint32_t clu, uint32_t entry)
{
	uint32_t ret;
	size_t entry_per_sector = info.sector_size / sizeof(uint32_t);
	uint32_t fat_index = (info.fat_offset +  clu / entry_per_sector) * info.sector_size;
	uint32_t *fat;
	uint32_t offset = (clu) % entry_per_sector;

	if (clu > info.cluster_count + 1) {
		pr_info("This Filesystem doesn't have Entry %u\n", clu);
		return 0;
	}

	fat = malloc(info.sector_size);
	get_sector(fat, fat_index, 1);

	ret = fat[offset];
	fat[offset] = entry;

	set_sector(fat, fat_index, 1);
	pr_debug("Rewrite Entry(%u) 0x%x to 0x%x.\n", clu, ret, fat[offset]);

	free(fat);
	return ret;
}

/**
 * exfat_create_fat_chain - Change NoFatChain to FatChain in file
 * @f:                      file information pointer
 * @clu:                    first cluster
 *
 * @retrun:                 0 (success)
 */
static int exfat_create_fat_chain(struct exfat_fileinfo *f, uint32_t clu)
{
	size_t cluster_num = (f->datalen + (info.cluster_size - 1)) / info.cluster_size;

	while (--cluster_num) {
		exfat_update_fat_entry(clu, clu + 1);
		clu++;
	}
	return 0;
}

/*************************************************************************************************/
/*                                                                                               */
/* CLUSTER FUNCTION FUNCTION                                                                     */
/*                                                                                               */
/*************************************************************************************************/
/**
 * exfat_get_last_cluster - find last cluster in file
 * @f:                      file information pointer
 * @clu:                    first cluster
 *
 * @return                  Last cluster
 *                          -1 (Failed)
 */
static int exfat_get_last_cluster(struct exfat_fileinfo *f, uint32_t clu)
{
	int i;
	uint32_t next_clu;
	size_t cluster_num = (f->datalen + (info.cluster_size - 1)) / info.cluster_size;

	/* NO_FAT_CHAIN */
	if (f->flags & ALLOC_NOFATCHAIN)
		return clu + cluster_num - 1;

	/* FAT_CHAIN */
	for (i = 0; i < cluster_num; i++) {
		next_clu = exfat_check_fat_entry(clu);
		if (!next_clu)
			return clu;
		clu = next_clu;
	}
	return -1;
}

/**
 * exfat_alloc_clusters - Allocate cluster to file
 * @f:                    file information pointer
 * @clu:                  first cluster
 * @num_alloc:            number of cluster
 *
 * @return                the number of allocated cluster
 */
static int exfat_alloc_clusters(struct exfat_fileinfo *f, uint32_t clu, size_t num_alloc)
{
	uint32_t tmp = clu;
	uint32_t next_clu;
	uint32_t last_clu;
	int total_alloc = num_alloc;
	bool nofatchain = true;

	clu = next_clu = last_clu = exfat_get_last_cluster(f, clu);
	for (next_clu = last_clu + 1; next_clu != last_clu; next_clu++) {
		if (next_clu > info.cluster_count - 1)
			next_clu = EXFAT_FIRST_CLUSTER;

		if (exfat_load_bitmap(next_clu))
			continue;

		if (nofatchain && (next_clu - clu != 1))
			nofatchain = false;
		exfat_update_fat_entry(next_clu, EXFAT_LASTCLUSTER);
		exfat_update_fat_entry(clu, next_clu);
		exfat_save_bitmap(next_clu, 1);
		clu = next_clu;
		if (--total_alloc == 0)
			break;

	}
	if ((f->flags & ALLOC_NOFATCHAIN) && !nofatchain) {
		f->flags &= ~ALLOC_NOFATCHAIN;
		exfat_create_fat_chain(f, tmp);
	}
	f->datalen += num_alloc * info.cluster_size;
	exfat_update_filesize(f, tmp);
	return total_alloc;
}

/**
 * exfat_free_clusters - Free cluster in file
 * @f:                   file information pointer
 * @clu:                 first cluster
 * @num_alloc:           number of cluster
 *
 * @return               0 (success)
 */
static int exfat_free_clusters(struct exfat_fileinfo *f, uint32_t clu, size_t num_alloc)
{
	int i;
	uint32_t tmp = clu;
	uint32_t next_clu;
	size_t cluster_num = (f->datalen + (info.cluster_size - 1)) / info.cluster_size;

	/* NO_FAT_CHAIN */
	if (f->flags & ALLOC_NOFATCHAIN) {
		for (i = cluster_num - num_alloc; i < cluster_num; i++)
			exfat_save_bitmap(clu + i, 0);
		return 0;
	}

	/* FAT_CHAIN */
	for (i = 0; i < cluster_num - num_alloc - 1; i++)
		clu = exfat_check_fat_entry(clu);

	while (i++ < cluster_num - 1) {
		next_clu = exfat_check_fat_entry(clu);
		exfat_update_fat_entry(clu, EXFAT_LASTCLUSTER);
		exfat_save_bitmap(next_clu, 0);
		clu = next_clu;
	}

	f->datalen -= num_alloc * info.cluster_size;
	exfat_update_filesize(f, tmp);
	return 0;
}

/**
 * exfat_new_cluster - Prepare to new cluster
 * @num_alloc:         number of cluster
 *
 * @return             allocated first cluster index
 */
static int exfat_new_clusters(size_t num_alloc)
{
	uint32_t next_clu, clu;
	uint32_t fst_clu = 0;

	for (next_clu = EXFAT_FIRST_CLUSTER; next_clu < info.cluster_count; next_clu++) {
		if (exfat_load_bitmap(next_clu))
			continue;

		if (!fst_clu) {
			fst_clu = clu = next_clu;
			exfat_update_fat_entry(fst_clu, EXFAT_LASTCLUSTER);
			exfat_save_bitmap(fst_clu, 1);
		} else {
			exfat_update_fat_entry(next_clu, EXFAT_LASTCLUSTER);
			exfat_update_fat_entry(clu, next_clu);
			exfat_save_bitmap(clu, 1);
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
 * exfat_print_dchain - print directory chain
 */
static void exfat_print_dchain(void)
{
	int i;
	node2_t *tmp;
	struct exfat_fileinfo *f;

	for (i = 0; i < info.root_size && info.root[i]; i++) {
		tmp = info.root[i];
		f = (struct exfat_fileinfo *)info.root[i]->data;
		pr_msg("%-16s(%u) | ", f->name, tmp->index);
		while (tmp->next != NULL) {
			tmp = tmp->next;
			f = (struct exfat_fileinfo *)tmp->data;
			pr_msg("%s(%u) ", f->name, tmp->index);
		}
		pr_msg("\n");
	}
	pr_msg("\n");
}
#endif
/**
 * exfat_check_dchain - check whether @index has already loaded
 * @clu:                index of the cluster
 *
 * @retrun:             1 (@clu has loaded)
 *                      0 (@clu hasn't loaded)
 */
static int exfat_check_dchain(uint32_t clu)
{
	int i;

	for (i = 0; info.root[i] && i < info.root_size; i++) {
		if (info.root[i]->index == clu)
			return 1;
	}
	return 0;
}

/**
 * exfat_get_index - get directory chain index by argument
 * @clu:             index of the cluster
 *
 * @return:          directory chain index
 *                   Start of unused area (if doesn't lookup directory cache)
 */
static int exfat_get_index(uint32_t clu)
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
 * exfat_traverse_directory - function to traverse one directory
 * @clu:                      index of the cluster want to check
 *
 * @return                    0 (success)
 *                           -1 (failed to read)
 */
static int exfat_traverse_directory(uint32_t clu)
{
	int i, j, name_len;
	uint8_t remaining;
	uint16_t uniname[MAX_NAME_LENGTH] = {0};
	uint32_t checksum = 0;
	uint64_t len;
	size_t index = exfat_get_index(clu);
	struct exfat_fileinfo *f = (struct exfat_fileinfo *)info.root[index]->data;
	size_t entries = info.cluster_size / sizeof(struct exfat_dentry);
	size_t cluster_num = 1;
	void *data;
	struct exfat_dentry d, next, name;

	if (f->cached) {
		pr_debug("Directory %s was already traversed.\n", f->name);
		return 0;
	}

	data = malloc(info.cluster_size);
	get_cluster(data, clu);

	cluster_num = exfat_concat_cluster(f, clu, &data);
	entries = (cluster_num * info.cluster_size) / sizeof(struct exfat_dentry);

	for (i = 0; i < entries; i++) {
		d = ((struct exfat_dentry *)data)[i];

		switch (d.EntryType) {
			case DENTRY_UNUSED:
				break;
			case DENTRY_BITMAP:
				pr_debug("Get: allocation table: cluster 0x%x, size: 0x%lx\n",
						d.dentry.bitmap.FirstCluster,
						d.dentry.bitmap.DataLength);
				info.alloc_cluster = d.dentry.bitmap.FirstCluster;
				info.alloc_table = malloc(info.cluster_size);
				get_cluster(info.alloc_table, d.dentry.bitmap.FirstCluster);
				pr_info("Allocation Bitmap (#%u):\n", d.dentry.bitmap.FirstCluster);
				break;
			case DENTRY_UPCASE:
				info.upcase_size = d.dentry.upcase.DataLength;
				len = (info.cluster_size / info.upcase_size) + 1;
				info.upcase_table = malloc(info.cluster_size * len);
				pr_debug("Get: Up-case table: cluster 0x%x, size: 0x%x\n",
						d.dentry.upcase.FirstCluster,
						d.dentry.upcase.DataLength);
				get_clusters(info.upcase_table, d.dentry.upcase.FirstCluster, len);
				checksum = exfat_calculate_tablechecksum((unsigned char *)info.upcase_table, info.upcase_size);
				if (checksum != d.dentry.upcase.TableCheckSum)
					pr_warn("Up-case table checksum is difference. (dentry: %x, calculate: %x)\n",
							d.dentry.upcase.TableCheckSum,
							checksum);
				break;
			case DENTRY_VOLUME:
				info.vol_length = d.dentry.vol.CharacterCount;
				if (info.vol_length) {
					info.vol_label = malloc(sizeof(uint16_t) * info.vol_length);
					pr_debug("Get: Volume label: size: 0x%x\n",
							d.dentry.vol.CharacterCount);
					memcpy(info.vol_label, d.dentry.vol.VolumeLabel,
							sizeof(uint16_t) * info.vol_length);
				}
				break;
			case DENTRY_FILE:
				remaining = d.dentry.file.SecondaryCount;
				/* Stream entry */
				next = ((struct exfat_dentry *)data)[i + 1];
				while ((!(next.EntryType & EXFAT_INUSE)) && (next.EntryType != DENTRY_UNUSED)) {
					pr_debug("This entry was deleted (0x%x).\n", next.EntryType);
					next = ((struct exfat_dentry *)data)[++i + 1];
				}
				if (next.EntryType != DENTRY_STREAM) {
					pr_info("File should have stream entry, but This don't have.\n");
					continue;
				}
				/* Filename entry */
				name = ((struct exfat_dentry *)data)[i + 2];
				while ((!(name.EntryType & EXFAT_INUSE)) && (name.EntryType != DENTRY_UNUSED)) {
					pr_debug("This entry was deleted (0x%x).\n", name.EntryType);
					name = ((struct exfat_dentry *)data)[++i + 2];
				}
				if (name.EntryType != DENTRY_NAME) {
					pr_info("File should have name entry, but This don't have.\n");
					return -1;
				}
				name_len = next.dentry.stream.NameLength;
				for (j = 0; j < remaining - 1; j++) {
					name_len = MIN(ENTRY_NAME_MAX,
							next.dentry.stream.NameLength - j * ENTRY_NAME_MAX);
					memcpy(uniname + j * ENTRY_NAME_MAX,
							(((struct exfat_dentry *)data)[i + 2 + j]).dentry.name.FileName,
							name_len * sizeof(uint16_t));
				}

				exfat_create_fileinfo(info.root[index], clu,
						&d, &next, uniname);
				i += remaining;
				break;
		}
	}
	free(data);
	return 0;
}

/**
 * exfat_clean_dchain - function to clean opeartions
 * @index:              directory chain index
 *
 * @return              0 (success)
 *                     -1 (already released)
 */
static int exfat_clean_dchain(uint32_t index)
{
	node2_t *tmp;
	struct exfat_fileinfo *f;

	if ((!info.root[index])) {
		pr_warn("index %d was already released.\n", index);
		return -1;
	}

	tmp = info.root[index];

	while (tmp->next != NULL) {
		tmp = tmp->next;
		f = (struct exfat_fileinfo *)tmp->data;
		free(f->name);
		f->name = NULL;
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
 * exfat_create_fileinfo - Create file infomarion
 * @head:                  Directory chain head
 * @clu:                   parent Directory cluster index
 * @file:                  file dentry
 * @stream:                stream Extension dentry
 * @uniname:               File Name dentry
 */
static void exfat_create_fileinfo(node2_t *head, uint32_t clu,
		struct exfat_dentry *file, struct exfat_dentry *stream, uint16_t *uniname)
{
	int index, next_index = stream->dentry.stream.FirstCluster;
	struct exfat_fileinfo *f;
	size_t namelen = stream->dentry.stream.NameLength;

	f = malloc(sizeof(struct exfat_fileinfo));
	memset(f, '\0', sizeof(struct exfat_fileinfo));
	f->name = malloc(namelen * UTF8_MAX_CHARSIZE + 1);
	memset(f->name, '\0', namelen * UTF8_MAX_CHARSIZE + 1);

	exfat_convert_uniname(uniname, namelen, f->name);
	f->namelen = namelen;
	f->datalen = stream->dentry.stream.DataLength;
	f->attr = file->dentry.file.FileAttributes;
	f->flags = stream->dentry.stream.GeneralSecondaryFlags;
	f->hash = stream->dentry.stream.NameHash;

	exfat_convert_unixtime(&f->ctime, file->dentry.file.CreateTimestamp,
			file->dentry.file.Create10msIncrement,
			file->dentry.file.CreateUtcOffset);
	exfat_convert_unixtime(&f->mtime, file->dentry.file.LastModifiedTimestamp,
			file->dentry.file.LastModified10msIncrement,
			file->dentry.file.LastModifiedUtcOffset);
	exfat_convert_unixtime(&f->atime, file->dentry.file.LastAccessedTimestamp,
			0,
			file->dentry.file.LastAccessdUtcOffset);
	append_node2(head, next_index, f);
	((struct exfat_fileinfo *)(head->data))->cached = 1;

	/* If this entry is Directory, prepare to create next chain */
	if ((f->attr & ATTR_DIRECTORY) && (!exfat_check_dchain(next_index))) {
		struct exfat_fileinfo *d = malloc(sizeof(struct exfat_fileinfo));
		d->name = malloc(f->namelen + 1);
		strncpy((char *)d->name, (char *)f->name, f->namelen + 1);
		d->namelen = namelen;
		d->datalen = stream->dentry.stream.DataLength;
		d->attr = file->dentry.file.FileAttributes;
		d->flags = stream->dentry.stream.GeneralSecondaryFlags;
		d->hash = stream->dentry.stream.NameHash;

		index = exfat_get_index(next_index);
		info.root[index] = init_node2(next_index, d);
	}
}

/**
 * exfat_init_file - function interface to create entry
 * @d:               directory entry (Output)
 * @name:            filename in UTF-16
 * @namelen:         filename length
 *
 * @return           0 (Success)
 */
static int exfat_init_file(struct exfat_dentry *d, uint16_t *name, size_t namelen)
{
	uint32_t timestamp;
	uint8_t subsec, tz;
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	char buf[8] = {0};

	/* obrain current timezone */
	strftime(buf, 8, "%z", tm);
	exfat_parse_timezone(buf, &tz);
	tm = gmtime(&t);
	exfat_convert_exfattime(tm, &timestamp, &subsec);

	d->EntryType = 0x85;
	d->dentry.file.SetChecksum = 0;
	d->dentry.file.FileAttributes = ATTR_ARCHIVE;
	d->dentry.file.SecondaryCount = 1 + ((namelen + 14) / 15);
	memset(d->dentry.file.Reserved1, 0, 2);
	d->dentry.file.CreateTimestamp = timestamp;
	d->dentry.file.LastAccessedTimestamp = timestamp;
	d->dentry.file.LastModifiedTimestamp = timestamp;
	d->dentry.file.Create10msIncrement = subsec;
	d->dentry.file.LastModified10msIncrement = subsec;
	d->dentry.file.CreateUtcOffset = tz | 0x80;
	d->dentry.file.LastAccessdUtcOffset = tz | 0x80;
	d->dentry.file.LastModifiedUtcOffset = tz | 0x80;
	memset(d->dentry.file.Reserved2, 0, 7);

	return 0;
}

/**
 * exfat_init_stream - function interface to create entry
 * @d:                 directory entry (Output)
 * @name:              filename in UTF-16
 * @namelen:           filename length
 *
 * @return             0 (Success)
 */
static int exfat_init_stream(struct exfat_dentry *d, uint16_t *name, size_t namelen)
{
	d->EntryType = 0xC0;
	d->dentry.stream.GeneralSecondaryFlags = ALLOC_POSIBLE | ALLOC_NOFATCHAIN;
	d->dentry.stream.Reserved1 = 0x00;
	d->dentry.stream.NameLength = namelen;
	d->dentry.stream.NameHash = exfat_calculate_namehash(name, namelen);
	memset(d->dentry.stream.Reserved2, 0, 2);
	d->dentry.stream.ValidDataLength = 0;
	memset(d->dentry.stream.Reserved3, 0, 4);
	d->dentry.stream.FirstCluster = 0;
	d->dentry.stream.DataLength = 0;

	return 0;
}

/**
 * exfat_init_filename - function interface to create entry
 * @d:                   directory entry (Output)
 * @name:                filename in UTF-16
 * @namelen:             filename length
 *
 * @return               0 (Success)
 */
static int exfat_init_filename(struct exfat_dentry *d, uint16_t *name, size_t namelen)
{
	d->EntryType = 0xC1;
	d->dentry.stream.GeneralSecondaryFlags = 0x00;
	memcpy(d->dentry.name.FileName,	name, namelen * sizeof(uint16_t));

	return 0;
}

/**
 * exfat_init_bitmap - function interface to create allocation bitmap
 * @d:                 directory entry (Output)
 *
 * @return             0 (Success)
 */
static int exfat_init_bitmap(struct exfat_dentry *d)
{
	d->EntryType = 0x81;
	d->dentry.bitmap.BitmapFlags = 0x00;
	memset(d->dentry.bitmap.Reserved, 0, 18);
	d->dentry.bitmap.FirstCluster = 0;
	d->dentry.bitmap.DataLength = 0;

	return 0;
}

/**
 * exfat_init_upcase - function interface to create Upcase Table
 * @d:                 directory entry (Output)
 *
 * @return             0 (Success)
 */
static int exfat_init_upcase(struct exfat_dentry *d)
{
	d->EntryType = 0x82;
	memset(d->dentry.upcase.Reserved1, 0, 3);
	d->dentry.upcase.TableCheckSum = 0x00;
	memset(d->dentry.upcase.Reserved2, 0, 12);
	d->dentry.upcase.FirstCluster = 0;
	d->dentry.upcase.DataLength = 0;

	return 0;
}

/**
 * exfat_init_volume - function interface to create Volume label
 * @d:                 directory entry (OUtput)
 * @name:              filename in UTF-16
 * @namelen:           filename length
 *
 * @return             0 (Success)
 */
static int exfat_init_volume(struct exfat_dentry *d, uint16_t *name, size_t namelen)
{
	d->EntryType = 0x83;
	d->dentry.vol.CharacterCount = namelen;
	memcpy(d->dentry.vol.VolumeLabel, name, namelen * sizeof(uint16_t));
	memset(d->dentry.vol.Reserved, 0, 8);

	return 0;
}

/**
 * exfat_update_file - update file entry
 * @old:               directory entry before update
 * @new:               directory entry after update (Output)
 *
 * @return             0 (Success)
 */
static int exfat_update_file(struct exfat_dentry *old, struct exfat_dentry *new)
{
	int i;
	uint64_t tmp = 0;
	struct tm tm;
	char buf[64] = {0};
	const char attrchar[][16] = {
		"ReadOnly",
		"Hidden",
		"System",
		"Reserved",
		"Directory",
		"Archive",
	};
	const char timechar[][16] = {
		"Create",
		"LastModified",
		"LastAccess",
	};
	uint32_t timestamps[3] = {0};
	uint8_t subsecs[3] = {0};
	uint8_t utcoffsets[3] = {0};

	input("Please input a Secondary Count", buf);
	sscanf(buf, "%02hhd", (uint8_t *)&tmp);
	new->dentry.file.SecondaryCount = tmp;
	input("Please input a SetChecksum", buf);
	sscanf(buf, "%04hx", (uint16_t *)&tmp);
	new->dentry.file.SetChecksum = tmp;

	pr_msg("Please select a File Attribute.\n");
	for (i = 0; i < 6; i++) {
		pr_msg("   %s [N/y] ", attrchar[i]);
		fflush(stdout);
		if (fgets(buf, 8, stdin) == NULL)
			return 1;
		if (toupper(buf[0]) == 'Y' && buf[1] == '\n')
			new->dentry.file.FileAttributes |= (1 << i);
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
		pr_msg("   UTC offset: ");
		fflush(stdout);
		if (fgets(buf, 32, stdin) == NULL)
			return 1;
		exfat_parse_timezone(buf, utcoffsets + i);
		exfat_convert_exfattime(&tm, timestamps + i, subsecs + i);
	}

	new->dentry.file.CreateTimestamp = timestamps[0];
	new->dentry.file.LastAccessedTimestamp = timestamps[1];
	new->dentry.file.LastModifiedTimestamp = timestamps[2];
	new->dentry.file.Create10msIncrement = subsecs[0];
	new->dentry.file.LastModified10msIncrement = subsecs[2];
	new->dentry.file.CreateUtcOffset = utcoffsets[0];
	new->dentry.file.LastAccessdUtcOffset = utcoffsets[1];
	new->dentry.file.LastModifiedUtcOffset = utcoffsets[2];

	return 0;
}

/**
 * exfat_update_stream - update stream entry
 * @old:                 directory entry before update
 * @new:                 directory entry after update (Output)
 *
 * @return               0 (Success)
 */
static int exfat_update_stream(struct exfat_dentry *old, struct exfat_dentry *new)
{
	int i;
	uint64_t tmp = 0;
	char buf[64] = {0};
	const char flags[][32] = {
		"AllocationPossible",
		"NoFatChain",
	};

	pr_msg("Please select a GeneralSecondaryFlags\n");
	for (i = 0; i < 2; i++) {
		pr_msg("   %s [N/y] ", flags[i]);
		fflush(stdout);
		if (fgets(buf, 8, stdin) == NULL)
			return 1;
		if (toupper(buf[0]) == 'Y' && buf[1] == '\n')
			new->dentry.stream.GeneralSecondaryFlags |= (1 << i);
	}
	input("Please input a Name Length", buf);
	sscanf(buf, "%02hhx", (uint8_t *)&tmp);
	new->dentry.stream.NameLength = tmp;
	input("Please input a Name Hash", buf);
	sscanf(buf, "%04hx", (uint16_t *)&tmp);
	new->dentry.stream.NameHash = tmp;
	input("Please input a Valid Data Length", buf);
	sscanf(buf, "%08lx", (uint64_t *)&tmp);
	new->dentry.stream.ValidDataLength = tmp;
	input("Please input a First Cluster", buf);
	sscanf(buf, "%04x", (uint32_t *)&tmp);
	new->dentry.stream.FirstCluster = tmp;
	input("Please input a Data Length", buf);
	sscanf(buf, "%08lx", (uint64_t *)&tmp);
	new->dentry.stream.DataLength = tmp;

	return 0;
}

/**
 * exfat_update_filename - update file name entry
 * @old:                   directory entry before update
 * @new:                   directory entry after update (Output)
 *
 * @return                 0 (Success)
 */
static int exfat_update_filename(struct exfat_dentry *old, struct exfat_dentry *new)
{
	int i;
	char buf[64] = {0};
	const char flags[][32] = {
		"AllocationPossible",
		"NoFatChain",
	};

	pr_msg("Please select a GeneralSecondaryFlags\n");
	for (i = 0; i < 2; i++) {
		pr_msg("   %s [N/y] ", flags[i]);
		fflush(stdout);
		if (fgets(buf, 8, stdin) == NULL)
			return 1;
		if (toupper(buf[0]) == 'Y' && buf[1] == '\n')
			new->dentry.name.GeneralSecondaryFlags |= (1 << i);
	}
	input("Please input a FileName", buf);
	memcpy(new->dentry.name.FileName, buf, 30);

	return 0;
}

/**
 * exfat_update_bitmap - update allocation bitmap
 * @old:                 directory entry before update
 * @new:                 directory entry after update (Output)
 *
 * @return               0 (Success)
 */
static int exfat_update_bitmap(struct exfat_dentry *old, struct exfat_dentry *new)
{
	uint64_t tmp = 0;
	char buf[64] = {0};

	pr_msg("Please select a GeneralSecondaryFlags\n");
	pr_msg("   Use 2nd Allocation Bitmap? [N/y] ");
	fflush(stdout);
	if (fgets(buf, 8, stdin) == NULL)
		return 1;
	if (toupper(buf[0]) == 'Y' && buf[1] == '\n')
		new->dentry.bitmap.BitmapFlags = 0x01;

	input("Please input a First Cluster", buf);
	sscanf(buf, "%04x", (uint32_t *)&tmp);
	new->dentry.bitmap.FirstCluster = tmp;
	input("Please input a Data Length", buf);
	sscanf(buf, "%08lx", (uint64_t *)&tmp);
	new->dentry.bitmap.DataLength = tmp;

	return 0;
}

/**
 * exfat_update_upcase - update Up-case Table entry
 * @old:                 directory entry before update
 * @new:                 directory entry after update (Output)
 *
 * @return               0 (Success)
 */
static int exfat_update_upcase(struct exfat_dentry *old, struct exfat_dentry *new)
{
	uint64_t tmp = 0;
	char buf[64] = {0};

	input("Please input a Table Checksum", buf);
	sscanf(buf, "%04hx", (uint16_t *)&tmp);
	new->dentry.upcase.TableCheckSum = tmp;
	input("Please input a First Cluster", buf);
	sscanf(buf, "%04x", (uint32_t *)&tmp);
	new->dentry.upcase.FirstCluster = tmp;
	input("Please input a Data Length", buf);
	sscanf(buf, "%08lx", (uint64_t *)&tmp);
	new->dentry.upcase.DataLength = tmp;

	return 0;
}

/**
 * exfat_update_volume - update Volume label dentry
 * @old:                 directory entry before update
 * @new:                 directory entry after update (Output)
 *
 * @return               0 (Success)
 */
static int exfat_update_volume(struct exfat_dentry *old, struct exfat_dentry *new)
{
	uint64_t tmp = 0;
	char buf[64] = {0};

	input("Please input a Character Count", buf);
	sscanf(buf, "%02hhx", (uint8_t *)&tmp);
	new->dentry.vol.CharacterCount = tmp;
	input("Please input a FileName", buf);
	memcpy(new->dentry.vol.VolumeLabel, buf, 22);

	return 0;
}

/**
 * exfat_calculate_checksum - Calculate file entry Checksum
 * @entry:                    points to an in-memory copy of the directory entry set
 * @count:                    the number of secondary directory entries
 *
 * @return                    Checksum
 */
static uint16_t exfat_calculate_checksum(unsigned char *entry, unsigned char count)
{
	uint16_t bytes = ((uint16_t)count + 1) * 32;
	uint16_t checksum = 0;
	uint16_t index;

	for (index = 0; index < bytes; index++) {
		if ((index == 2) || (index == 3))
			continue;
		checksum = ((checksum & 1) ? 0x8000 : 0) + (checksum >> 1) +  (uint16_t)entry[index];
	}
	return checksum;
}

/**
 * exfat_calculate_Tablechecksum - Calculate Up-case table Checksum
 * @entry:                         points to an in-memory copy of the directory entry set
 * @count:                         the number of secondary directory entries
 *
 * @return                         Checksum
 */
static uint32_t exfat_calculate_tablechecksum(unsigned char *table, uint64_t length)
{
	uint32_t checksum = 0;
	uint64_t index;

	for (index = 0; index < length; index++)
		checksum = ((checksum & 1) ? 0x80000000 : 0) + (checksum >> 1) + (uint32_t)table[index];

	return checksum;
}

/**
 * exfat_convert_uniname - function to get filename
 * @uniname:               filename dentry in UTF-16
 * @name_len:              filename length
 * @name:                  filename in UTF-8 (Output)
 */
static void exfat_convert_uniname(uint16_t *uniname, uint64_t name_len, unsigned char *name)
{
	utf16s_to_utf8s(uniname, name_len, name);
}

/**
 * exfat_calculate_namehash - Calculate name hash
 * @name:                     points to an in-memory copy of the up-cased file name
 * @len:                      Name length
 *
 * @return                    NameHash
 */
static uint16_t exfat_calculate_namehash(uint16_t *name, uint8_t len)
{
	unsigned char* buffer = (unsigned char *)name;
	uint16_t bytes = (uint16_t)len * 2;
	uint16_t hash = 0;
	uint16_t index;

	for (index = 0; index < bytes; index++)
		hash = ((hash & 1) ? 0x8000 : 0) + (hash >> 1) + (uint16_t)buffer[index];

	return hash;
}

/**
 * exfat_update_filesize - flush filesize to disk
 * @f:                     file information pointer
 * @clu:                   first cluster
 *
 * @return                  0 (success)
 *                         -1 (Failed)
 */
static int exfat_update_filesize(struct exfat_fileinfo *f, uint32_t clu)
{
	int i, j;
	uint32_t parent_clu = 0;
	size_t cluster_num;
	struct exfat_fileinfo *dir;
	struct exfat_dentry d;
	void *data;

	for (i = 0; i < info.root_size && info.root[i]; i++) {
		if (search_node2(info.root[i], clu)) {
			parent_clu = info.root[i]->index;
			dir = info.root[i]->data;
			break;
		}
	}

	if (!parent_clu) {
		pr_err("Can't find cluster %u parent directory.\n", clu);
		return -1;
	}

	cluster_num = (dir->datalen + (info.cluster_size - 1)) / info.cluster_size;
	data = malloc(info.cluster_size);

	for (i = 0; i < cluster_num; i++) {
		get_cluster(data, parent_clu);
		for (j = 0; j < (info.cluster_size / sizeof(struct exfat_dentry)); j++) {
			d = ((struct exfat_dentry *)data)[j];
			if (d.EntryType == DENTRY_STREAM && d.dentry.stream.FirstCluster == clu) {
				d.dentry.stream.DataLength = f->datalen;
				d.dentry.stream.GeneralSecondaryFlags = f->flags;
				goto out;
			}
		}
		/* traverse next cluster */
		if (dir->flags & ALLOC_NOFATCHAIN)
			parent_clu++;
		else
			parent_clu = exfat_check_fat_entry(parent_clu);
	}
	parent_clu = 0;
out:
	set_cluster(data, parent_clu);
	free(data);
	return 0;
}

/**
 * exfat_convert_unixname - function to get timestamp in file
 * @t:                      output pointer (Output)
 * @time:                   Timestamp Field in File Directory Entry
 * @subsec:                 10msincrement Field in File Directory Entry
 * @tz:                     UtcOffset in File Directory Entry
 */
static void exfat_convert_unixtime(struct tm *t, uint32_t time, uint8_t subsec, uint8_t tz)
{
	t->tm_year = (time >> EXFAT_YEAR) & 0x7f;
	t->tm_mon  = (time >> EXFAT_MONTH) & 0x0f;
	t->tm_mday = (time >> EXFAT_DAY) & 0x1f;
	t->tm_hour = (time >> EXFAT_HOUR) & 0x1f;
	t->tm_min  = (time >> EXFAT_MINUTE) & 0x3f;
	t->tm_sec  = (time & 0x1f) * 2;
	t->tm_sec += subsec / 100;
	/* OffsetValid */
	if (tz & 0x80) {
		int min = 0;
		time_t tmp_time = mktime(t);
		struct tm *t2;
		min = exfat_convert_timezone(tz);
		tmp_time += (min * 60);
		t2 = localtime(&tmp_time);
		*t = *t2;
	}
}

/**
 * exfat_convert_timezone - function to get timezone in file
 * @tz:                     UtcOffset in File Directory Entry
 *
 * @return                  difference minutes from timezone
 */
static int exfat_convert_timezone(uint8_t tz)
{
	int ex_min = 0;
	int ex_hour = 0;
	char offset = tz & 0x7f;

	/* OffsetValid */
	if (!(tz & 0x80))
		return 0;
	/* negative value */
	if (offset & 0x40) {
		offset = ((~offset) + 1) & 0x7f;
		ex_min = ((offset % 4) * 15) * -1;
		ex_hour = (offset / 4) * -1;
	} else {
		ex_min = (offset % 4) * 15;
		ex_hour = offset / 4;
	}
	return ex_min + ex_hour * 60;
}

/**
 * exfat_convert_exfattime - function to get timestamp in file
 * @t:                       Timestamp
 * @time:                    Timestamp Field in File Directory Entry (Output)
 * @subsec:                  10msincrement Field in File Directory Entry (Output)
 */
static void exfat_convert_exfattime(struct tm *t, uint32_t *timestamp, uint8_t *subsec)
{
	*timestamp = *subsec = 0;
	*timestamp |= ((t->tm_year - 80) << EXFAT_YEAR);
	*timestamp |= ((t->tm_mon + 1) << EXFAT_MONTH);
	*timestamp |= (t->tm_mday << EXFAT_DAY);
	*timestamp |= (t->tm_hour << EXFAT_HOUR);
	*timestamp |= (t->tm_min << EXFAT_MINUTE);
	*timestamp |= t->tm_sec / 2;
	*subsec += ((t->tm_sec % 2) * 100);
}

/**
 * exfat_convert_exfattimezone - function to get timezone in file
 * @tz:                          UtcOffset in File Directory Entry (Output)
 * @min:                         Utc Offset
 */
static void exfat_convert_exfattimezone(uint8_t *tz, int min)
{
	*tz = (min / 15) & 0x7f;
}

/**
 * exfat_parse_timezone - Prompt user for timestamp
 * @buf:                  difference localtime and UTCtime
 * @tz:                   offset from UTC Field (Output)
 *
 * @return                0 (Success)
 */
static int exfat_parse_timezone(char *buf, uint8_t *tz)
{
	char op = ' ';
	char min = 0, hour = 0;
	int ex = 0;

	if (buf[0] == '\n')
		return 0;

	if (isdigit(buf[0]))
		sscanf(buf, "%02hhd%02hhd",
				&hour,
				&min);
	else
		sscanf(buf, "%c%02hhd%02hhd",
				&op,
				&hour,
				&min);
	ex = hour * 60 + min;

	switch (op) {
		case '-':
			exfat_convert_exfattimezone(tz, -ex);
			break;
		case '+':
		case ' ':
			exfat_convert_exfattimezone(tz, ex);
			break;
		default:
			pr_debug("Invalid operation. you can use only ('+' or '-').\n");
			*tz = 0;
			break;
	}

	return 0;
}
/*************************************************************************************************/
/*                                                                                               */
/* OPERATIONS FUNCTION                                                                           */
/*                                                                                               */
/*************************************************************************************************/
/**
 * exfat_print_bootsec - print boot sector in exFAT
 *
 * @return        0 (success)
 */
int exfat_print_bootsec(void)
{
	struct exfat_bootsec *b = malloc(sizeof(struct exfat_bootsec));

	exfat_load_bootsec(b);
	pr_msg("%-28s\t: 0x%08lx (sector)\n", "media-relative sector offset",
			b->PartitionOffset);
	pr_msg("%-28s\t: 0x%08x (sector)\n", "Offset of the First FAT",
			b->FatOffset);
	pr_msg("%-28s\t: %10u (sector)\n", "Length of FAT table",
			b->FatLength);
	pr_msg("%-28s\t: 0x%08x (sector)\n", "Offset of the Cluster Heap",
			b->ClusterHeapOffset);
	pr_msg("%-28s\t: %10u (cluster)\n", "The number of clusters",
			b->ClusterCount);
	pr_msg("%-28s\t: %10u (cluster)\n", "The first cluster of the root",
			b->FirstClusterOfRootDirectory);
	pr_msg("%-28s\t: %10lu (sector)\n", "Size of exFAT volumes",
			b->VolumeLength);
	pr_msg("%-28s\t: %10lu (byte)\n", "Bytes per sector",
			info.sector_size);
	pr_msg("%-28s\t: %10lu (byte)\n", "Bytes per cluster",
			info.cluster_size);
	pr_msg("%-28s\t: %10u\n", "The number of FATs",
			b->NumberOfFats);
	pr_msg("%-28s\t: %10u (%%)\n", "The percentage of clusters",
			b->PercentInUse);
	pr_msg("\n");

	free(b);
	return 0;
}

/**
 * exfat_print_fsinfo - print filesystem information in exFAT
 *
 * @return              0 (success)
 */
int exfat_print_fsinfo(void)
{
	exfat_print_upcase();
	exfat_print_label();
	return 0;
}

/**
 * exfat_lookup - function interface to lookup pathname
 * @clu:          directory cluster index
 * @name:         file name
 *
 * @return:       cluster index
 *                -1 (Not found)
 */
int exfat_lookup(uint32_t clu, char *name)
{
	int index, i = 0, depth = 0;
	bool found = false;
	char *path[MAX_NAME_LENGTH] = {};
	char fullpath[PATHNAME_MAX + 1] = {};
	node2_t *tmp;
	struct exfat_fileinfo *f;

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
		index = exfat_get_index(clu);
		f = (struct exfat_fileinfo *)info.root[index]->data;
		if ((!info.root[index]) || (!(f->cached))) {
			pr_debug("Directory hasn't load yet, or This Directory doesn't exist in filesystem.\n");
			exfat_traverse_directory(clu);
			index = exfat_get_index(clu);
			if (!info.root[index]) {
				pr_warn("This Directory doesn't exist in filesystem.\n");
				return -1;
			}
		}

		tmp = info.root[index];
		while (tmp->next != NULL) {
			tmp = tmp->next;
			f = (struct exfat_fileinfo *)tmp->data;
			if (!strcmp(path[i], (char *)f->name)) {
				clu = tmp->index;
				found = true;
				break;
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
 * exfat_readdir - function interface to read a directory
 * @dir:           directory entry list (Output)
 * @count:         Allocated space in @dir
 * @clu:           Directory cluster index
 *
 * @return         >= 0 (Number of entry)
 *                  < 0 (Number of entry can't read)
 */
int exfat_readdir(struct directory *dir, size_t count, uint32_t clu)
{
	int i;
	node2_t *tmp;
	struct exfat_fileinfo *f;

	exfat_traverse_directory(clu);
	i = exfat_get_index(clu);
	tmp = info.root[i];

	for (i = 0; i < count && tmp->next != NULL; i++) {
		tmp = tmp->next;
		f = (struct exfat_fileinfo *)(tmp->data);
		dir[i].name = malloc(sizeof(uint32_t) * (f->namelen + 1));
		strncpy((char *)dir[i].name, (char *)f->name, sizeof(uint32_t) * (f->namelen + 1));
		dir[i].namelen = f->namelen;
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
 * exfat_reload_directory - function interface to read directories
 * @clu                     cluster index
 *
 * @return                   0 (success)
 *                          -1 (failed to read)
 */
int exfat_reload_directory(uint32_t clu)
{
	int index = exfat_get_index(clu);
	struct exfat_fileinfo *f = NULL;

	exfat_clean_dchain(index);
	f = ((struct exfat_fileinfo *)(info.root[index])->data);
	f->cached = 0;
	return exfat_traverse_directory(clu);
}

/**
 * exfat_convert_character - Convert character by upcase-table
 * @src:                     Target characters in UTF-8
 * @len:                     Target characters length
 * @dist:                    convert result in UTF-8 (Output)
 *
 * return:                    0 (succeeded in obtaining filesystem)
 *                           -1 (failed)
 */
int exfat_convert_character(const char *src, size_t len, char *dist)
{
	int i, utf16_len, utf8_len;

	uint16_t *utf16_src;
	uint16_t *utf16_upper;

	if (!info.upcase_table || (info.upcase_size == 0)) {
		pr_err("This exFAT filesystem doesn't have upcase-table.\n");
		return -1;
	}

	/* convert UTF-8 to UTF16 */
	utf16_src = malloc(sizeof(char) * len * UTF8_MAX_CHARSIZE);
	utf16_len = utf8s_to_utf16s((unsigned char *)src, len, utf16_src);

	/* convert UTF-16 char to UTF-16 only upper letter char */
	utf16_upper = malloc(sizeof(uint16_t) * utf16_len);
	for (i = 0; i < utf16_len; i++) {
		if (utf16_src[i] > info.upcase_size)
			utf16_upper[i] = utf16_src[i];
		else
			utf16_upper[i] = info.upcase_table[utf16_src[i]];
	}

	/* convert UTF-16 to convert UTF-8 */
	utf8_len = utf16s_to_utf8s(utf16_upper, utf16_len, (unsigned char *)dist);

	free(utf16_upper);
	free(utf16_src);
	return utf8_len;
}

/**
 * exfat_clean - function to clean opeartions
 * @index:       directory chain index
 *
 * @return        0 (success)
 *               -1 (already released)
 */
int exfat_clean(uint32_t index)
{
	node2_t *tmp;
	struct exfat_fileinfo *f;

	if ((!info.root[index])) {
		pr_warn("index %d was already released.\n", index);
		return -1;
	}

	tmp = info.root[index];
	f = (struct exfat_fileinfo *)tmp->data;
	free(f->name);
	f->name = NULL;

	exfat_clean_dchain(index);
	free(tmp->data);
	free(tmp);
	return 0;
}

/**
 * exfat_set_fat_entry - Set FAT Entry to any cluster
 * @clu:                 index of the cluster want to check
 * @entry:               any cluster index
 *
 * @retrun:              0
 */
int exfat_set_fat_entry(uint32_t clu, uint32_t entry)
{
	exfat_update_fat_entry(clu, entry);
	return 0;
}

/**
 * exfat_get_fat_entry - Get cluster is continuous
 * @clu:                 index of the cluster want to check
 * @entry:               any cluster index (Output)
 *
 * @return:              0
 */
int exfat_get_fat_entry(uint32_t clu, uint32_t *entry)
{
	*entry = exfat_check_fat_entry(clu);
	return 0;
}

/**
 * exfat_print_dentry - function to print any directory entry
 * @clu:                index of the cluster want to check
 * @n:                  directory entry index
 *
 * @return               0 (success)
 *                      -1 (failed to read)
 */
int exfat_print_dentry(uint32_t clu, size_t n)
{
	int i, min;
	uint32_t next_clu = 0;
	size_t size = info.cluster_size;
	size_t entries = size / sizeof(struct exfat_dentry);
	void *data;
	struct exfat_dentry d;
	struct tm ctime, mtime, atime;

	exfat_traverse_directory(clu);
	while (n > entries) {
		next_clu = exfat_check_fat_entry(clu);
		if (!next_clu) {
			pr_err("Directory size limit exceeded.\n");
			return -1;
		}
		n -= entries;
		clu = next_clu;
	}

	data = malloc(size);
	get_cluster(data, clu);
	d = ((struct exfat_dentry *)data)[n];

	pr_msg("EntryType                       : %02x\n", d.EntryType);
	pr_info("  TypeCode                      : %02x\n", d.EntryType & 0x1F);
	pr_info("  TypeImportance                : %02x\n", (d.EntryType >> 5) & 0x01);
	pr_info("  TypeCategory                  : %02x\n", (d.EntryType >> 6) & 0x01);
	pr_info("  InUse                         : %02x\n", (d.EntryType >> 7) & 0x01);
	switch (d.EntryType) {
		case DENTRY_UNUSED:
			break;
		case DENTRY_BITMAP:
			pr_msg("BitmapFlags                     : %02x\n", d.dentry.bitmap.BitmapFlags);
			pr_info("  %s Allocation Bitmap\n", d.dentry.bitmap.BitmapFlags & ACTIVEFAT ? "2nd" : "1st");
			pr_msg("Reserved                        : ");
			for (i = 0; i < 18; i++)
				pr_msg("%02x", d.dentry.bitmap.Reserved[i]);
			pr_msg("\n");
			pr_msg("BitmapFlags                     : %08x\n", d.dentry.bitmap.FirstCluster);
			pr_msg("DataLength                      : %016lx\n", d.dentry.bitmap.DataLength);
			break;
		case DENTRY_UPCASE:
			pr_msg("Reserved1                       : ");
			for (i = 0; i < 3; i++)
				pr_msg("%02x", d.dentry.upcase.Reserved1[i]);
			pr_msg("\n");
			pr_msg("TableCheckSum                   : %08x\n", d.dentry.upcase.TableCheckSum);
			pr_msg("Reserved2                       : ");
			for (i = 0; i < 12; i++)
				pr_msg("%02x", d.dentry.upcase.Reserved2[i]);
			pr_msg("\n");
			pr_msg("FirstCluster                    : %08x\n", d.dentry.upcase.FirstCluster);
			pr_msg("DataLength                      : %016x\n", d.dentry.upcase.DataLength);
			break;
		case DENTRY_VOLUME:
			pr_msg("CharacterCount                  : %02x\n", d.dentry.vol.CharacterCount);
			pr_msg("VolumeLabel                     : ");
			for (i = 0; i < 22; i++)
				pr_msg("%02x", ((uint8_t *)d.dentry.vol.VolumeLabel)[i]);
			pr_msg("\n");
			pr_msg("Reserved2                       : ");
			for (i = 0; i < 8; i++)
				pr_msg("%02x", d.dentry.vol.Reserved[i]);
			pr_msg("\n");
			break;
		case DENTRY_FILE:
			pr_msg("SecondaryCount                  : %02x\n", d.dentry.file.SecondaryCount);
			pr_msg("SetChecksum                     : %04x\n", d.dentry.file.SetChecksum);
			pr_msg("FileAttributes                  : %04x\n", d.dentry.file.FileAttributes);
			if (d.dentry.file.FileAttributes & ATTR_READ_ONLY)
				pr_info("  * ReadOnly\n");
			if (d.dentry.file.FileAttributes & ATTR_HIDDEN)
				pr_info("  * Hidden\n");
			if (d.dentry.file.FileAttributes & ATTR_SYSTEM)
				pr_info("  * System\n");
			if (d.dentry.file.FileAttributes & ATTR_DIRECTORY)
				pr_info("  * Directory\n");
			if (d.dentry.file.FileAttributes & ATTR_ARCHIVE)
				pr_info("  * Archive\n");
			pr_msg("Reserved1                       : ");
			for (i = 0; i < 2; i++)
				pr_msg("%02x", d.dentry.file.Reserved1[i]);
			pr_msg("\n");
			exfat_convert_unixtime(&ctime, d.dentry.file.CreateTimestamp, 0, 0);
			exfat_convert_unixtime(&mtime, d.dentry.file.LastModifiedTimestamp, 0, 0);
			exfat_convert_unixtime(&atime, d.dentry.file.LastAccessedTimestamp, 0, 0);
			pr_msg("CreateTimestamp                 : %08x\n", d.dentry.file.CreateTimestamp);
			pr_info("  %d-%02d-%02d %02d:%02d:%02d\n",
					ctime.tm_year + 1980, ctime.tm_mon, ctime.tm_mday,
					ctime.tm_hour, ctime.tm_min, ctime.tm_sec);
			pr_msg("LastModifiedTimestamp           : %08x\n", d.dentry.file.LastModifiedTimestamp);
			pr_info("  %d-%02d-%02d %02d:%02d:%02d\n",
					mtime.tm_year + 1980, mtime.tm_mon, mtime.tm_mday,
					mtime.tm_hour, mtime.tm_min, mtime.tm_sec);
			pr_msg("LastAccessedTimestamp           : %08x\n", d.dentry.file.LastAccessedTimestamp);
			pr_info("  %d-%02d-%02d %02d:%02d:%02d\n",
					atime.tm_year + 1980, atime.tm_mon, atime.tm_mday,
					atime.tm_hour, atime.tm_min, atime.tm_sec);
			pr_msg("Create10msIncrement             : %02x\n", d.dentry.file.Create10msIncrement);
			pr_info("  %0d.%02d\n",
					d.dentry.file.Create10msIncrement / 100,
					d.dentry.file.Create10msIncrement % 100);
			pr_msg("LastModified10msIncrement       : %02x\n", d.dentry.file.LastModified10msIncrement);
			pr_info("  %0d.%02d\n",
					d.dentry.file.LastModified10msIncrement / 100,
					d.dentry.file.LastModified10msIncrement % 100);
			pr_msg("CreateUtcOffset                 : %02x\n", d.dentry.file.CreateUtcOffset);
			if (d.dentry.file.CreateUtcOffset & 0x80) {
				min = exfat_convert_timezone(d.dentry.file.CreateUtcOffset);
				pr_info("  %02d:%02d\n", min / 60, (abs(min) % 60));
			}
			pr_msg("LastModifiedUtcOffset           : %02x\n", d.dentry.file.LastModifiedUtcOffset);
			if (d.dentry.file.LastModifiedUtcOffset & 0x80) {
				min = exfat_convert_timezone(d.dentry.file.LastModifiedUtcOffset);
				pr_info("  %02d:%02d\n", min / 60, (abs(min) % 60));
			}
			pr_msg("LastAccessdUtcOffset            : %02x\n", d.dentry.file.LastAccessdUtcOffset);
			if (d.dentry.file.LastAccessdUtcOffset & 0x80) {
				min = exfat_convert_timezone(d.dentry.file.LastAccessdUtcOffset);
				pr_info("  %02d:%02d\n", min / 60, (abs(min) % 60));
			}
			pr_msg("Reserved2                       : ");
			for (i = 0; i < 7; i++)
				pr_msg("%02x", d.dentry.file.Reserved2[i]);
			pr_msg("\n");
			break;
		case DENTRY_STREAM:
			pr_msg("GeneralSecondaryFlags           : %02x\n", d.dentry.stream.GeneralSecondaryFlags);
			if (d.dentry.stream.GeneralSecondaryFlags & ALLOC_POSIBLE)
				pr_info("  * AllocationPossible\n");
			if (d.dentry.stream.GeneralSecondaryFlags & ALLOC_NOFATCHAIN)
				pr_info("  * NoFatChain\n");
			pr_msg("Reserved1                       : %02x\n", d.dentry.stream.Reserved1);
			pr_msg("NameLength                      : %02x\n", d.dentry.stream.NameLength);
			pr_msg("NameHash                        : %04x\n", d.dentry.stream.NameHash);
			pr_msg("Reserved2                       : ");
			for (i = 0; i < 2; i++)
				pr_msg("%02x", d.dentry.stream.Reserved2[i]);
			pr_msg("\n");
			pr_msg("ValidDataLength                 : %016lx\n", d.dentry.stream.ValidDataLength);
			pr_msg("Reserved3                       : ");
			for (i = 0; i < 4; i++)
				pr_msg("%02x", d.dentry.stream.Reserved3[i]);
			pr_msg("\n");
			pr_msg("FirstCluster                    : %08x\n", d.dentry.stream.FirstCluster);
			pr_msg("DataLength                      : %016lx\n", d.dentry.stream.DataLength);
			break;
		case DENTRY_NAME:
			pr_msg("GeneralSecondaryFlags           : %02x\n", d.dentry.name.GeneralSecondaryFlags);
			if (d.dentry.stream.GeneralSecondaryFlags & ALLOC_POSIBLE)
				pr_info("  * AllocationPossible\n");
			if (d.dentry.stream.GeneralSecondaryFlags & ALLOC_NOFATCHAIN)
				pr_info("  * NoFatChain\n");
			pr_msg("FileName                        : ");
			for (i = 0; i < 30; i++)
				pr_msg("%02x", ((uint8_t *)d.dentry.name.FileName)[i]);
			pr_msg("\n");
			break;
	}
	free(data);
	return 0;
}

/**
 * exfat_set_bitmap - function to allocate cluster
 * @clu:              cluster index
 *
 * @return             0 (success)
 *                    -1 (failed)
 */
int exfat_set_bitmap(uint32_t clu)
{
	if (exfat_load_bitmap(clu)) {
		pr_warn("Cluster %u is already allocated.\n", clu);
		return 0;
	}
	return exfat_save_bitmap(clu, 1);
}

/**
 * exfat_clear_bitmap - function to release cluster
 * @clu:                cluster index
 *
 * @return               0 (success)
 *                      -1 (failed)
 */
int exfat_clear_bitmap(uint32_t clu)
{
	if (!exfat_load_bitmap(clu)) {
		pr_warn("Cluster %u is already freed.\n", clu);
		return 0;
	}
	return exfat_save_bitmap(clu, 0);
}

/**
 * exfat_create - function interface to create entry
 * @name:         Filename in UTF-8
 * @clu:          Current Directory Index
 * @opt:          create option
 *
 * @return        0 (Success)
 */
int exfat_create(const char *name, uint32_t clu, int opt)
{
	int i, namei;
	void *data;
	uint16_t uniname[MAX_NAME_LENGTH] = {0};
	uint8_t len;
	uint8_t count;
	size_t index = exfat_get_index(clu);
	struct exfat_fileinfo *f = (struct exfat_fileinfo *)info.root[index]->data;
	size_t entries = info.cluster_size / sizeof(struct exfat_dentry);
	size_t cluster_num = 1;
	size_t new_cluster_num = 1;
	size_t name_len;
	struct exfat_dentry *d;

	/* convert UTF-8 to UTF16 */
	len = utf8s_to_utf16s((unsigned char *)name, strlen(name), uniname);
	count = ((len + ENTRY_NAME_MAX - 1) / ENTRY_NAME_MAX) + 1;

	/* Lookup last entry */
	data = malloc(info.cluster_size);
	get_cluster(data, clu);

	cluster_num = exfat_concat_cluster(f, clu, &data);
	entries = (cluster_num * info.cluster_size) / sizeof(struct exfat_dentry);

	for (i = 0; i < entries; i++) {
		d = ((struct exfat_dentry *)data) + i;
		if (d->EntryType == DENTRY_UNUSED)
			break;
	}

	new_cluster_num = (i + count + 2) * sizeof(struct exfat_dentry) / info.cluster_size;
	if (new_cluster_num > cluster_num) {
		exfat_alloc_clusters(f, clu, new_cluster_num - cluster_num);
		cluster_num = exfat_concat_cluster(f, clu, &data);
		entries = (cluster_num * info.cluster_size) / sizeof(struct exfat_dentry);
	}

	exfat_init_file(d, uniname, len);
	if (opt & CREATE_DIRECTORY)
		d->dentry.file.FileAttributes = ATTR_DIRECTORY;
	d = ((struct exfat_dentry *)data) + i + 1;
	exfat_init_stream(d, uniname, len);
	if (opt & CREATE_DIRECTORY)
		d->dentry.stream.FirstCluster = exfat_new_clusters(1);
	d = ((struct exfat_dentry *)data) + i + 2;
	for (namei = 0; namei < count - 1; namei++) {
		name_len = MIN(ENTRY_NAME_MAX, len - namei * ENTRY_NAME_MAX);
		exfat_init_filename(d, uniname, name_len);
		d = ((struct exfat_dentry *)data) + i + 2 + namei;
		d->EntryType = DENTRY_NAME;
	}

	/* Calculate File entry checksumc */
	d = ((struct exfat_dentry *)data) + i;
	d->dentry.file.SetChecksum =
		exfat_calculate_checksum(data + i * sizeof(struct exfat_dentry), count);

	exfat_set_cluster(f, clu, data);
	free(data);
	return 0;
}

/**
 * exfat_remove - function interface to remove entry
 * @name:         Filename in UTF-8
 * @clu:          Current Directory Index
 * @opt:          create option
 *
 * @return         0 (Success)
 *                -1 (Not found)
 */
int exfat_remove(const char *name, uint32_t clu, int opt)
{
	int i, j, name_len, name_len2, ret = 0;
	void *data;
	uint16_t uniname[MAX_NAME_LENGTH] = {0};
	uint16_t uniname2[MAX_NAME_LENGTH] = {0};
	uint16_t namehash = 0;
	uint8_t remaining;
	size_t index = exfat_get_index(clu);
	struct exfat_fileinfo *f = (struct exfat_fileinfo *)info.root[index]->data;
	size_t entries = info.cluster_size / sizeof(struct exfat_dentry);
	size_t cluster_num = 1;
	struct exfat_dentry *d, *s, *n;

	/* convert UTF-8 to UTF16 */
	name_len = utf8s_to_utf16s((unsigned char *)name, strlen(name), uniname);
	namehash = exfat_calculate_namehash(uniname, name_len);

	/* Lookup last entry */
	data = malloc(info.cluster_size);
	get_cluster(data, clu);

	cluster_num = exfat_concat_cluster(f, clu, &data);
	entries = (cluster_num * info.cluster_size) / sizeof(struct exfat_dentry);

	for (i = 0; i < entries; i++) {
		d = ((struct exfat_dentry *)data) + i;

		switch (d->EntryType) {
			case DENTRY_UNUSED:
				ret = -1;
				goto out;
			case DENTRY_FILE:
				remaining = d->dentry.file.SecondaryCount;
				/* Stream entry */
				s = ((struct exfat_dentry *)data) + i + 1;
				while ((!(s->EntryType & EXFAT_INUSE)) && (s->EntryType != DENTRY_UNUSED)) {
					pr_debug("This entry was deleted (0x%x).\n", s->EntryType);
					s = ((struct exfat_dentry *)data) + (++i) + 1;
				}
				if (s->EntryType != DENTRY_STREAM) {
					pr_debug("File should have stream entry, but This don't have.\n");
					continue;
				}

				if (s->dentry.stream.NameHash != namehash) {
					i += remaining - 1;
					continue;
				}

				/* Filename entry */
				n = ((struct exfat_dentry *)data) + i + 2;
				if (n->EntryType != DENTRY_NAME) {
					pr_debug("File should have name entry, but This don't have.\n");
					return -1;
				}

				name_len2 = s->dentry.stream.NameLength;
				if (name_len != name_len2) {
					i += remaining - 1;
					continue;
				}

				for (j = 0; j < remaining - 1; j++) {
					name_len2 = MIN(ENTRY_NAME_MAX,
							s->dentry.stream.NameLength - j * ENTRY_NAME_MAX);
					memcpy(uniname2 + j * ENTRY_NAME_MAX,
							(((struct exfat_dentry *)data) + i + 2 + j)->dentry.name.FileName,
							name_len2 * sizeof(uint16_t));
				}
				if (!memcmp(uniname, uniname2, name_len2)) {
					d->EntryType &= ~EXFAT_INUSE;
					s->EntryType &= ~EXFAT_INUSE;
					n->EntryType &= ~EXFAT_INUSE;
					goto out;
				}
				i += remaining;
				break;
		}
	}
out:
	exfat_set_cluster(f, clu, data);
	free(data);
	return ret;
}

/**
 * exfat_update_dentry - function interface to update directory entry
 * @clu:                 Current Directory cluster
 * @i:                   Directory entry index
 *
 * @return               0 (Success)
 */
int exfat_update_dentry(uint32_t clu, int i)
{
	int type = 0;
	char buf[64] = {0};
	struct exfat_dentry new = {0};
	void *data;
	size_t size = info.cluster_size;
	struct exfat_dentry *old;

	/* Lookup last entry */
	data = malloc(size);
	get_cluster(data, clu);

	old = ((struct exfat_dentry *)data) + i;

	pr_msg("Please select a Entry type.\n");
	pr_msg("1) File\n");
	pr_msg("2) Stream\n");
	pr_msg("3) File Name\n");
	pr_msg("4) Allocation Bitmap\n");
	pr_msg("5) Up-case Table\n");
	pr_msg("6) Volume Label\n");
	pr_msg("7) Other\n");

	while (1) {
		pr_msg("#? ");
		fflush(stdout);
		if (fgets(buf, 32, stdin) == NULL)
			continue;
		sscanf(buf, "%d", &type);
		if (0 < type && type < 8)
			break;
	}
	pr_msg("\n");

	switch (type) {
		case 1:
			exfat_init_file(&new, (uint16_t *)"", 0);
			exfat_update_file(old, &new);
			break;
		case 2:
			exfat_init_stream(&new, (uint16_t *)"", 0);
			exfat_update_stream(old, &new);
			break;
		case 3:
			exfat_init_filename(&new, (uint16_t *)"", 0);
			exfat_update_filename(old, &new);
			break;
		case 4:
			exfat_init_bitmap(&new);
			exfat_update_bitmap(old, &new);
			break;
		case 5:
			exfat_init_upcase(&new);
			exfat_update_upcase(old, &new);
			break;
		case 6:
			exfat_init_volume(&new, (uint16_t *)"", 0);
			exfat_update_volume(old, &new);
			break;
		default:
			memset(buf, 0x00, 64);
			input("Please input any strings", buf);
			for (i = 0; i < sizeof(struct exfat_dentry); i++)
				sscanf(buf + (i * 2), "%02hhx", ((uint8_t *)&new) + i);
			break;
	}
	for (i = 0; i < sizeof(struct exfat_dentry); i++) {
		pr_msg("%x ", *(((uint8_t *)&new) + i));
	}
	pr_msg("\n");
	memcpy(old, &new, sizeof(struct exfat_dentry));

	set_cluster(data, clu);
	free(data);
	return 0;
}

/**
 * exfat_trim - function interface to trim cluster
 * @clu:        Current Directory Index
 *
 * @return      0 (Success)
 */
int exfat_trim(uint32_t clu)
{
	int i, j;
	uint8_t used = 0;
	void *data;
	size_t index = exfat_get_index(clu);
	struct exfat_fileinfo *f = (struct exfat_fileinfo *)info.root[index]->data;
	size_t entries = info.cluster_size / sizeof(struct exfat_dentry);
	size_t cluster_num = 1;
	size_t allocate_cluster = 1;
	struct exfat_dentry *src, *dist;

	/* Lookup last entry */
	data = malloc(info.cluster_size);
	get_cluster(data, clu);

	cluster_num = exfat_concat_cluster(f, clu, &data);
	entries = (cluster_num * info.cluster_size) / sizeof(struct exfat_dentry);

	for (i = 0, j = 0; i < entries; i++) {
		src = ((struct exfat_dentry *)data) + i;
		dist = ((struct exfat_dentry *)data) + j;
		if (!src->EntryType)
			break;

		used = src->EntryType & EXFAT_INUSE;
		if (!used)
			continue;

		if (i != j++)
			memcpy(dist, src, sizeof(struct exfat_dentry));
	}

	allocate_cluster = ((sizeof(struct exfat_dentry) * j) / info.cluster_size) + 1;
	while (j < entries) {
		dist = ((struct exfat_dentry *)data) + j++;
		memset(dist, 0, sizeof(struct exfat_dentry));
	}

	exfat_set_cluster(f, clu, data);
	exfat_free_clusters(f, clu, cluster_num - allocate_cluster);
	free(data);
	return 0;
}

/**
 * exfat_fill - function interface to fill in directory
 * @clu:        Current Directory Index
 * @count:      Number of dentry
 *
 * @return      0 (Success)
 */
int exfat_fill(uint32_t clu, uint32_t count)
{
	int i, j;
	void *data;
	char name[MAX_NAME_LENGTH] = {0};
	uint16_t uniname[MAX_NAME_LENGTH] = {0};
	uint8_t len;
	size_t entries = info.cluster_size / sizeof(struct exfat_dentry);
	const size_t minimum_dentries = 3;
	size_t need_entries = 0;
	size_t blank_entries = 0;
	struct exfat_dentry *d;

	/* Lookup last entry */
	data = malloc(info.cluster_size);
	get_cluster(data, clu);

	for (i = 0; i < entries; i++) {
		d = ((struct exfat_dentry *)data) + i;
		if (d->EntryType == DENTRY_UNUSED)
			break;
	}

	if (i > count - 1) {
		pr_debug("You want to fill %u dentries.\n", count);
		pr_debug("But this directory has already contained %d dentries.\n", i);
		goto out;
	}
	need_entries = count - i;

	for (blank_entries = need_entries % minimum_dentries; blank_entries > 0; blank_entries--) {
		d = ((struct exfat_dentry *)data) + i++;
		d->EntryType = DENTRY_FILE - EXFAT_INUSE;
	}

	for (j = 0; j < (need_entries / minimum_dentries); j++) {
		d = ((struct exfat_dentry *)data) + i + (j * minimum_dentries);
		gen_rand(name, ENTRY_NAME_MAX);
		len = utf8s_to_utf16s((unsigned char *)name, strlen(name), uniname);
		exfat_init_file(d, uniname, len);
		d = ((struct exfat_dentry *)data) + i + (j * minimum_dentries) + 1;
		exfat_init_stream(d, uniname, len);
		d = ((struct exfat_dentry *)data) + i + (j * minimum_dentries) + 2;
		exfat_init_filename(d, uniname, len);

		/* Calculate File entry checksumc */
		d = ((struct exfat_dentry *)data) + i + (j * minimum_dentries);
		d->dentry.file.SetChecksum =
			exfat_calculate_checksum(data + i * sizeof(struct exfat_dentry), minimum_dentries - 1);
	}

	set_cluster(data, clu);
out:
	free(data);
	return 0;
}
