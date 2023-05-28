// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2021 LeavaTail
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
static void exfat_print_fat(void);
static void exfat_print_bitmap(void);
static int exfat_load_bitmap(uint32_t);
static int exfat_save_bitmap(uint32_t, uint32_t);
static int exfat_load_bitmap_cluster(struct exfat_dentry);
static int exfat_load_upcase_cluster(struct exfat_dentry);
static int exfat_load_volume_label(struct exfat_dentry);

/* FAT-entry function prototype */
static int exfat_create_fat_chain(struct exfat_fileinfo *, uint32_t);

/* cluster function prototype */
static int exfat_get_last_cluster(struct exfat_fileinfo *, uint32_t);
static int exfat_alloc_clusters(struct exfat_fileinfo *, uint32_t, size_t);
static int exfat_free_clusters(struct exfat_fileinfo *, uint32_t, size_t);
static int exfat_new_clusters(size_t);

/* Directory chain function prototype */
static int exfat_check_dchain(uint32_t);
static int exfat_get_index(uint32_t);
static int exfat_load_extra_entry(void);
static int exfat_traverse_directory(uint32_t);
static int exfat_clean_dchain(uint32_t);
static struct exfat_fileinfo *exfat_search_fileinfo(node2_t *, const char *);

/* File function prototype */
static void exfat_create_fileinfo(node2_t *,
		uint32_t, struct exfat_dentry *, struct exfat_dentry *, uint16_t *);
static int exfat_init_file(struct exfat_dentry *, uint16_t *, size_t);
static int exfat_init_stream(struct exfat_dentry *, uint16_t *, size_t);
static int exfat_init_filename(struct exfat_dentry *, uint16_t *, size_t);
static uint16_t exfat_calculate_checksum(unsigned char *, unsigned char);
static uint32_t exfat_calculate_tablechecksum(unsigned char *, uint64_t);
static uint16_t exfat_calculate_namehash(uint16_t *, uint8_t);
static int exfat_update_filesize(struct exfat_fileinfo *, uint32_t);
static void exfat_convert_unixtime(struct tm *, uint32_t, uint8_t, uint8_t);
static int exfat_convert_timezone(uint8_t);
static void exfat_convert_exfattime(struct tm *, uint32_t *, uint8_t *);
static void exfat_convert_exfattimezone(uint8_t *, int);
static int exfat_parse_timezone(char *, uint8_t *);

/* File Name function prototype */
static void exfat_convert_uniname(uint16_t *, uint64_t, unsigned char *);
static uint16_t exfat_convert_upper(uint16_t);
static void exfat_convert_upper_character(uint16_t *, size_t, uint16_t *);
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
int exfat_validate_fat_entry(uint32_t);
int exfat_print_dentry(uint32_t, size_t);
int exfat_set_bitmap(uint32_t);
int exfat_clear_bitmap(uint32_t);
int exfat_create(const char *, uint32_t, int);
int exfat_remove(const char *, uint32_t, int);
int exfat_trim(uint32_t);
int exfat_fill(uint32_t, uint32_t);
int exfat_contents(const char *, uint32_t, int);
int exfat_stat(const char *, uint32_t);

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
	.validfat = exfat_validate_fat_entry,
	.dentry = exfat_print_dentry,
	.alloc = exfat_set_bitmap,
	.release = exfat_clear_bitmap,
	.create = exfat_create,
	.remove = exfat_remove,
	.trim = exfat_trim,
	.fill = exfat_fill,
	.contents = exfat_contents,
	.stat = exfat_stat,
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
	uint32_t next_clu = 0;
	uint32_t fst_clu = clu;
	size_t allocated = 1;
	size_t cluster_num = ROUNDUP(f->datalen, info.cluster_size);

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
	for (allocated = 1; allocated < cluster_num; allocated++) { 
		if (exfat_get_fat_entry(clu, &next_clu)) {
			pr_warn("Invalid FAT entry[%u]: 0x%x.\n", clu, next_clu);
			break;
		}
		if (next_clu == EXFAT_LASTCLUSTER)
			break;
		clu = next_clu;
	}

	if (!(tmp = realloc(*data, info.cluster_size * allocated)))
		return 0;
	*data = tmp;

	clu = fst_clu;
	for (i = 1; i < allocated; i++) {
		exfat_get_fat_entry(clu, &next_clu);
		get_cluster(*data + info.cluster_size * i, clu);
		clu = next_clu;
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
	uint32_t next_clu = 0;
	size_t allocated = 0;
	size_t cluster_num = ROUNDUP(f->datalen, info.cluster_size);

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
		if (exfat_get_fat_entry(clu, &next_clu)) {
			pr_warn("Invalid FAT entry[%u]: 0x%x.\n", clu, next_clu);
			break;
		}
		clu = next_clu;
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
		exfat_load_extra_entry();

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
		pr_msg("%04zxh:  ", offset * 0x10 / sizeof(uint16_t));
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
 * exfat_print_fat - print FAT
 */
static void exfat_print_fat(void)
{
	uint32_t i, j;
	uint32_t *fat;
	size_t sector_num = (info.fat_length + (info.sector_size - 1)) / info.sector_size;
	uint32_t offset = 0;
	bitmap_t b;

	init_bitmap(&b, info.cluster_count);

	fat = malloc(info.sector_size * sector_num);
	get_sector(fat, info.fat_offset * info.sector_size, sector_num);

	for (i = EXFAT_FIRST_CLUSTER; i < info.cluster_count; i++) {
		if (!exfat_load_bitmap(i)) {
			set_bitmap(&b, i);
			continue;
		}

		if (get_bitmap(&b, i))
			continue;

		offset = fat[i];
		if (offset >= EXFAT_FIRST_CLUSTER && offset < info.cluster_count) {
			set_bitmap(&b, offset);
			unset_bitmap(&b, i);
		} else {
			set_bitmap(&b, i);
		}
	}

	pr_msg("FAT:\n");
	for (i = EXFAT_FIRST_CLUSTER; i < info.cluster_count; i++) {
		if (get_bitmap(&b, i))
			continue;

		pr_msg("%u", i);
		offset = j = i;
		while (offset != EXFAT_LASTCLUSTER) {
			exfat_get_fat_entry(j, &offset);
			if (!exfat_load_bitmap(j))
				break;

			pr_msg(" -> %u", offset);
			j = offset;
		}

		pr_msg("\n");
	}

	free_bitmap(&b);
	free(fat);
}

/**
 * exfat_print_bitmap - print allocation bitmap
 */
static void exfat_print_bitmap(void)
{
	int offset, byte;
	uint8_t entry;
	uint32_t clu;

	pr_msg("Allocation Bitmap:\n");
	pr_msg("Offset    0 1 2 3 4 5 6 7 8 9 a b c d e f\n");
	/* Allocation bitmap consider first cluster is 2 */
	pr_msg("%08x  - - ", 0);

	for (clu = EXFAT_FIRST_CLUSTER; clu < info.cluster_size; clu++) {

		byte = (clu - EXFAT_FIRST_CLUSTER) / CHAR_BIT;
		offset = (clu - EXFAT_FIRST_CLUSTER) % CHAR_BIT;
		entry = info.alloc_table[byte];

		switch (clu % 0x10) {
			case 0x0:
				pr_msg("%08x  ", clu);
				pr_msg("%c ", ((entry >> offset) & 0x01) ? 'o' : '-');
				break;
			case 0xf:
				pr_msg("%c ", ((entry >> offset) & 0x01) ? 'o' : '-');
				pr_msg("\n");
				break;
			default:
				pr_msg("%c ", ((entry >> offset) & 0x01) ? 'o' : '-');
				break;
		}
	}
	pr_msg("\n");
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

	if (clu < EXFAT_FIRST_CLUSTER || clu > info.cluster_count + 1)
		return -1;

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

/**
 * exfat_load_bitmap_cluster - function to load Allocation Bitmap
 * @d:                         directory entry about allocation bitmap
 *
 * @return                      0 (success)
 *                             -1 (bitmap was already loaded)
 */
static int exfat_load_bitmap_cluster(struct exfat_dentry d)
{
	if (info.alloc_cluster)
		return -1;

	pr_debug("Get: allocation table: cluster 0x%x, size: 0x%" PRIx64 "\n",
			d.dentry.bitmap.FirstCluster,
			d.dentry.bitmap.DataLength);
	info.alloc_cluster = d.dentry.bitmap.FirstCluster;
	info.alloc_table = malloc(info.cluster_size);
	get_cluster(info.alloc_table, d.dentry.bitmap.FirstCluster);
	pr_info("Allocation Bitmap (#%u):\n", d.dentry.bitmap.FirstCluster);

	return 0;
}

/**
 * exfat_load_upcase_cluster - function to load Upcase table
 * @d:                         directory entry about Upcase table
 *
 * @return                      0 (success)
 *                             -1 (bitmap was already loaded)
 */
static int exfat_load_upcase_cluster(struct exfat_dentry d)
{
	uint32_t checksum = 0;
	uint64_t len;

	if (info.upcase_size)
		return -1;

	info.upcase_size = d.dentry.upcase.DataLength;
	len = (info.upcase_size + info.cluster_size - 1) / info.cluster_size;
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

	return 0;
}

/**
 * exfat_load_volume_label - function to load volume label
 * @d:                       directory entry about volume label
 *
 * @return                    0 (success)
 *                           -1 (bitmap was already loaded)
 */
static int exfat_load_volume_label(struct exfat_dentry d)
{
	if (info.vol_length)
		return -1;

	info.vol_length = d.dentry.vol.CharacterCount;
	if (info.vol_length) {
		info.vol_label = malloc(sizeof(uint16_t) * info.vol_length);
		pr_debug("Get: Volume label: size: 0x%x\n",
				d.dentry.vol.CharacterCount);
		memcpy(info.vol_label, d.dentry.vol.VolumeLabel,
				sizeof(uint16_t) * info.vol_length);
	}

	return 0;
}

/*************************************************************************************************/
/*                                                                                               */
/* FAT-ENTRY FUNCTION                                                                            */
/*                                                                                               */
/*************************************************************************************************/
/**
 * exfat_create_fat_chain - Change NoFatChain to FatChain in file
 * @f:                      file information pointer
 * @clu:                    first cluster
 *
 * @retrun:                 0 (success)
 */
static int exfat_create_fat_chain(struct exfat_fileinfo *f, uint32_t clu)
{
	size_t cluster_num = ROUNDUP(f->datalen, info.cluster_size);

	while (--cluster_num) {
		exfat_set_fat_entry(clu, clu + 1);
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
	size_t cluster_num = ROUNDUP(f->datalen, info.cluster_size);

	/* NO_FAT_CHAIN */
	if (f->flags & ALLOC_NOFATCHAIN)
		return clu + cluster_num - 1;

	/* FAT_CHAIN */
	for (i = 0; i < cluster_num; i++) {
		exfat_get_fat_entry(clu, &next_clu);
		if (next_clu == EXFAT_LASTCLUSTER)
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
		exfat_set_fat_entry(next_clu, EXFAT_LASTCLUSTER);
		exfat_set_fat_entry(clu, next_clu);
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
	uint32_t fst_clu = clu;
	uint32_t next_clu;
	size_t cluster_num = ROUNDUP(f->datalen, info.cluster_size);

	/* NO_FAT_CHAIN */
	if (f->flags & ALLOC_NOFATCHAIN) {
		for (i = cluster_num - num_alloc; i < cluster_num; i++)
			exfat_save_bitmap(clu + i, 0);
		return 0;
	}

	/* FAT_CHAIN */
	for (i = 0; i < cluster_num - num_alloc - 1; i++) {
		if (exfat_get_fat_entry(clu, &next_clu)) {
			pr_warn("Invalid FAT entry[%u]: 0x%x.\n", clu, next_clu);
			break;
		}
		clu = next_clu;
	}

	while (i++ < cluster_num - 1) {
		exfat_get_fat_entry(clu, &next_clu);
		exfat_set_fat_entry(clu, EXFAT_LASTCLUSTER);
		exfat_save_bitmap(next_clu, 0);
		clu = next_clu;
	}

	f->datalen -= num_alloc * info.cluster_size;
	exfat_update_filesize(f, fst_clu);
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
			exfat_set_fat_entry(fst_clu, EXFAT_LASTCLUSTER);
			exfat_save_bitmap(fst_clu, 1);
		} else {
			exfat_set_fat_entry(next_clu, EXFAT_LASTCLUSTER);
			exfat_set_fat_entry(clu, next_clu);
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
#ifdef DEBUGFATFS_DEBUG
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
 * exfat_load_extra_entry - function to load extra entry
 *
 * @return                  0 (success)
 *                          1 (already traverse)
 */
static int exfat_load_extra_entry(void)
{
	int i;
	size_t index = exfat_get_index(info.root_offset);
	struct exfat_fileinfo *f = (struct exfat_fileinfo *)info.root[index]->data;
	void *data;
	struct exfat_dentry d;

	if (f->cached) {
		pr_debug("Directory %s was already traversed.\n", f->name);
		return 1;
	}

	data = malloc(info.cluster_size);
	get_cluster(data, info.root_offset);

	for (i = 0; i < (info.cluster_size / sizeof(struct exfat_dentry)); i++) {
		d = ((struct exfat_dentry *)data)[i];
		switch (d.EntryType) {
			case DENTRY_BITMAP:
				exfat_load_bitmap_cluster(d);
				break;
			case DENTRY_UPCASE:
				exfat_load_upcase_cluster(d);
				break;
			case DENTRY_VOLUME:
				exfat_load_volume_label(d);
				break;
			case DENTRY_UNUSED:
			case DENTRY_FILE:
			case DENTRY_GUID:
			case DENTRY_STREAM:
			case DENTRY_NAME:
			case DENTRY_VENDOR:
			case DENTRY_VENDOR_ALLOC:
				goto out;
		}
	}
out:
	free(data);
	return 0;
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
				exfat_load_bitmap_cluster(d);
				break;
			case DENTRY_UPCASE:
				exfat_load_upcase_cluster(d);
				break;
			case DENTRY_VOLUME:
				exfat_load_volume_label(d);
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

/**
 * exfat_search_fileinfo - srach fileinfo from directory chain
 * @node:                  directory chain index
 * @name:                  filename
 *
 * @return                 fileinfo
 *                         NULL (Not found)
 */
static struct exfat_fileinfo *exfat_search_fileinfo(node2_t *node, const char *name)
{
	uint16_t uniname[MAX_NAME_LENGTH] = {0};
	uint16_t uppername[MAX_NAME_LENGTH] = {0};
	uint8_t len;
	uint16_t namehash = 0;
	node2_t *f_node;

	exfat_traverse_directory(node->index);
	/* convert UTF-8 to UTF16 */
	len = utf8s_to_utf16s((unsigned char *)name, strlen(name), uniname);
	exfat_convert_upper_character(uniname, len, uppername);
	namehash = exfat_calculate_namehash(uppername, len);

	if ((f_node = search_node2(node, (uint32_t)namehash)) != NULL)
		return f_node->data;
	return NULL;
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
	f->clu = next_index;
	f->dir = head->data;

	exfat_convert_unixtime(&f->ctime, file->dentry.file.CreateTimestamp,
			file->dentry.file.Create10msIncrement,
			file->dentry.file.CreateUtcOffset);
	exfat_convert_unixtime(&f->mtime, file->dentry.file.LastModifiedTimestamp,
			file->dentry.file.LastModified10msIncrement,
			file->dentry.file.LastModifiedUtcOffset);
	exfat_convert_unixtime(&f->atime, file->dentry.file.LastAccessedTimestamp,
			0,
			file->dentry.file.LastAccessdUtcOffset);
	append_node2(head, f->hash, f);
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
		d->clu = next_index;
		d->dir = head->data;

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
	struct tm tm;
	char buf[8] = {0};

	/* obrain current timezone */
	localtime_r(&t, &tm);
	strftime(buf, 8, "%z", &tm);
	exfat_parse_timezone(buf, &tz);
	gmtime_r(&t, &tm);
	exfat_convert_exfattime(&tm, &timestamp, &subsec);

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
	uint32_t parent_clu = 0, next_clu;
	size_t cluster_num;
	struct exfat_fileinfo *dir;
	struct exfat_dentry *d;
	void *data;

	if (clu == info.root_offset)
		return 0;

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

	cluster_num = ROUNDUP(dir->datalen, info.cluster_size);
	data = malloc(info.cluster_size);

	for (i = 0; i < cluster_num; i++) {
		get_cluster(data, parent_clu);
		for (j = 0; j < (info.cluster_size / sizeof(struct exfat_dentry)); j++) {
			d = ((struct exfat_dentry *)data) + j;
			if (d->EntryType == DENTRY_STREAM && d->dentry.stream.FirstCluster == clu) {
				d->dentry.stream.DataLength = f->datalen;
				d->dentry.stream.ValidDataLength = f->datalen;
				d->dentry.stream.GeneralSecondaryFlags = f->flags;
				goto out;
			}
		}
		/* traverse next cluster */
		if (dir->flags & ALLOC_NOFATCHAIN) {
			parent_clu++;
		} else {
			exfat_get_fat_entry(parent_clu, &next_clu); 
			parent_clu = next_clu;
		}
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
		min = exfat_convert_timezone(tz);
		tmp_time += (min * 60);
		localtime_r(&tmp_time, t);
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
/* FILE NAME FUNCTION                                                                            */
/*                                                                                               */
/*************************************************************************************************/
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
 * exfat_convert_upper - convert character to upper-character
 * @c:                   character in UTF-16
 *
 * @return:              upper character
 */
static uint16_t exfat_convert_upper(uint16_t c)
{
	return info.upcase_table[c] ? info.upcase_table[c] : c;
}

/**
 * exfat_convert_upper_character - convert string to upper-string
 * @src:                           Target characters in UTF-16
 * @len:                           Target characters length
 * @dist:                          convert result in UTF-16 (Output)
 */
static void exfat_convert_upper_character(uint16_t *src, size_t len, uint16_t *dist)
{
	int i;

	if (!info.upcase_table || (info.upcase_size == 0))
		exfat_load_extra_entry();

	for (i = 0; i < len; i++)
		dist[i] = exfat_convert_upper(src[i]);
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
	pr_msg("Sector size:     \t%zu\n", info.sector_size);
	pr_msg("Cluster size:    \t%zu\n", info.cluster_size);
	pr_msg("FAT offset:      \t%u\n", b->FatOffset);
	pr_msg("FAT size:        \t%zu\n", b->FatLength * info.sector_size);
	pr_msg("FAT count:       \t%u\n", b->NumberOfFats);

	pr_msg("Partition offset:\t%" PRIu64 "\n", b->PartitionOffset * info.sector_size);
	pr_msg("Volume size:     \t%" PRIu64 "\n", b->VolumeLength * info.sector_size);
	pr_msg("Cluster offset:  \t%zu\n", b->ClusterHeapOffset * info.sector_size);
	pr_msg("Cluster count:   \t%u\n", b->ClusterCount);
	pr_msg("First cluster:   \t%u\n", b->FirstClusterOfRootDirectory);
	pr_msg("Volume serial:   \t0x%x\n", b->VolumeSerialNumber);
	pr_msg("Filesystem revision:\t%x.%02x\n",
			b->FileSystemRevision / 0x100,
			b->FileSystemRevision % 0x100);
	pr_msg("Usage rate:      \t%u\n", b->PercentInUse);
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
	exfat_print_fat();
	exfat_print_bitmap();
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
	char *saveptr = NULL;
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
	path[depth] = strtok_r(fullpath, "/", &saveptr);
	while (path[depth] != NULL) {
		if (depth >= MAX_NAME_LENGTH) {
			pr_err("Pathname is too depth. (> %d)\n", MAX_NAME_LENGTH);
			return -1;
		}
		path[++depth] = strtok_r(NULL, "/", &saveptr);
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
				clu = f->clu;
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
	pr_debug("Convert \'%s\'(%zu) to \'%s\'(%d)\n", src, len, dist, utf8_len);

	free(utf16_upper);
	free(utf16_src);
	return 0;
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
	uint32_t ret;
	size_t entry_per_sector = info.sector_size / sizeof(uint32_t);
	uint32_t fat_index = (info.fat_offset +  clu / entry_per_sector) * info.sector_size;
	uint32_t *fat;
	uint32_t offset = (clu) % entry_per_sector;

	fat = malloc(info.sector_size);
	get_sector(fat, fat_index, 1);

	ret = fat[offset];
	fat[offset] = entry;

	set_sector(fat, fat_index, 1);
	pr_debug("Rewrite Entry(%u) 0x%x to 0x%x.\n", clu, ret, fat[offset]);

	free(fat);

	return 0;
}

/**
 * exfat_get_fat_entry - Get cluster is continuous
 * @clu:                 index of the cluster want to check
 * @entry:               any cluster index (Output)
 *
 * @retrun:              1 (@clu is invalid)
 *                       0 (@clu in valid)
 */
int exfat_get_fat_entry(uint32_t clu, uint32_t *entry)
{
	size_t entry_per_sector = info.sector_size / sizeof(uint32_t);
	uint32_t fat_index = (info.fat_offset +  clu / entry_per_sector) * info.sector_size;
	uint32_t *fat;
	uint32_t offset = (clu) % entry_per_sector;

	fat = malloc(info.sector_size);
	get_sector(fat, fat_index, 1);
	/* validate index */
	*entry = fat[offset];
	pr_debug("Get FAT entry(%u) 0x%x.\n", clu, fat[offset]);

	free(fat);

	return !exfat_validate_fat_entry(*entry);
}

/**
 * exfat_validate_fat_entry - Validate FAT entry
 * @clu:                      index of the cluster
 *
 * @retrun:                   1 (@clu is valid)
 *                            0 (@clu in invalid)
 */
int exfat_validate_fat_entry(uint32_t clu)
{
	int is_valid = 0;

	if (!exfat_load_bitmap(clu))
		is_valid = 0;

	if (EXFAT_FIRST_CLUSTER <= clu && clu <= info.cluster_count + 1)
		is_valid = 1;
	else if (clu == EXFAT_BADCLUSTER)
		is_valid = 0;
	else if (clu == EXFAT_LASTCLUSTER)
		is_valid = 1;

	return is_valid;
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
		if (exfat_get_fat_entry(clu, &next_clu)) {
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
			pr_msg("DataLength                      : %016" PRIx64 "\n", d.dentry.bitmap.DataLength);
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
			pr_msg("ValidDataLength                 : %016" PRIx64 "\n", d.dentry.stream.ValidDataLength);
			pr_msg("Reserved3                       : ");
			for (i = 0; i < 4; i++)
				pr_msg("%02x", d.dentry.stream.Reserved3[i]);
			pr_msg("\n");
			pr_msg("FirstCluster                    : %08x\n", d.dentry.stream.FirstCluster);
			pr_msg("DataLength                      : %016" PRIx64 "\n", d.dentry.stream.DataLength);
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
	uint16_t uppername[MAX_NAME_LENGTH] = {0};
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
	exfat_convert_upper_character(uniname, len, uppername);
	count = ROUNDUP(len, ENTRY_NAME_MAX) + 1;

	/* Prohibit duplicate filename */
	if (exfat_search_fileinfo(info.root[index], name)) {
		pr_err("cannot create %s: File exists\n", name);
		return -1;
	}

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

	new_cluster_num = ROUNDUP(((i + count + 2) * sizeof(struct exfat_dentry)), info.cluster_size);
	if (new_cluster_num > cluster_num) {
		exfat_alloc_clusters(f, clu, new_cluster_num - cluster_num);
		cluster_num = exfat_concat_cluster(f, clu, &data);
		entries = (cluster_num * info.cluster_size) / sizeof(struct exfat_dentry);
		d = ((struct exfat_dentry *)data) + i;
	}

	exfat_init_file(d, uniname, len);
	if (opt & CREATE_DIRECTORY)
		d->dentry.file.FileAttributes = ATTR_DIRECTORY;
	d = ((struct exfat_dentry *)data) + i + 1;
	exfat_init_stream(d, uppername, len);
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
	uint16_t uppername[MAX_NAME_LENGTH] = {0};
	uint16_t namehash = 0;
	uint8_t remaining;
	size_t index = exfat_get_index(clu);
	struct exfat_fileinfo *f = (struct exfat_fileinfo *)info.root[index]->data;
	size_t entries = info.cluster_size / sizeof(struct exfat_dentry);
	size_t cluster_num = 1;
	struct exfat_dentry *d, *s, *n;

	/* convert UTF-8 to UTF16 */
	name_len = utf8s_to_utf16s((unsigned char *)name, strlen(name), uniname);
	exfat_convert_upper_character(uniname, name_len, uppername);
	namehash = exfat_calculate_namehash(uppername, name_len);

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
	uint16_t uppername[MAX_NAME_LENGTH] = {0};
	uint8_t len;
	size_t index = exfat_get_index(clu);
	struct exfat_fileinfo *f = (struct exfat_fileinfo *)info.root[index]->data;
	size_t entries = info.cluster_size / sizeof(struct exfat_dentry);
	size_t cluster_num = 1;
	size_t new_cluster_num = 1;
	const size_t minimum_dentries = 3;
	size_t need_entries = 0;
	size_t blank_entries = 0;
	struct exfat_dentry *d;

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

	if (i > count - 1) {
		pr_debug("You want to fill %u dentries.\n", count);
		pr_debug("But this directory has already contained %d dentries.\n", i);
		goto out;
	}

	need_entries = count - i;
	new_cluster_num = ((count * sizeof(struct exfat_dentry) + info.cluster_size - 1 )/ info.cluster_size);

	if (new_cluster_num > cluster_num) {
		exfat_alloc_clusters(f, clu, new_cluster_num - cluster_num);
		cluster_num = exfat_concat_cluster(f, clu, &data);
		entries = (cluster_num * info.cluster_size) / sizeof(struct exfat_dentry);
	}

	for (blank_entries = need_entries % minimum_dentries; blank_entries > 0; blank_entries--) {
		d = ((struct exfat_dentry *)data) + i++;
		d->EntryType = DENTRY_FILE - EXFAT_INUSE;
	}

	for (j = 0; j < (need_entries / minimum_dentries); j++) {
		d = ((struct exfat_dentry *)data) + i + (j * minimum_dentries);
		gen_rand(name, ENTRY_NAME_MAX);
		len = utf8s_to_utf16s((unsigned char *)name, strlen(name), uniname);
		exfat_convert_upper_character(uniname, len, uppername);
		exfat_init_file(d, uniname, len);
		d = ((struct exfat_dentry *)data) + i + (j * minimum_dentries) + 1;
		exfat_init_stream(d, uppername, len);
		d = ((struct exfat_dentry *)data) + i + (j * minimum_dentries) + 2;
		exfat_init_filename(d, uniname, len);

		/* Calculate File entry checksumc */
		d = ((struct exfat_dentry *)data) + i + (j * minimum_dentries);
		d->dentry.file.SetChecksum =
			exfat_calculate_checksum(data + i * sizeof(struct exfat_dentry), minimum_dentries - 1);
	}

	exfat_set_cluster(f, clu, data);
out:
	free(data);
	return 0;
}

/**
 * exfat_contents - function interface to display file contents
 * @name:           Filename in UTF-8
 * @clu:            Current Directory Index
 * @opt:            create option
 *
 * @return         0 (Success)
 *                -1 (Not found)
 */
int exfat_contents(const char *name, uint32_t clu, int opt)
{
	int i, ret = 0;
	void *data;
	size_t index = 0;
	size_t lines = 0;
	size_t cluster_num = 1;
	char *ptr;
	struct exfat_fileinfo *f;

	index = exfat_get_index(clu);
	if ((f = exfat_search_fileinfo(info.root[index], name)) == NULL) {
		pr_err("File is not found.\n");
		return -1;
	}

	data = malloc(info.cluster_size);
	get_cluster(data, f->clu);
	cluster_num = exfat_concat_cluster(f, f->clu, &data);
	if (!cluster_num) {
		pr_err("Someting wrong in FAT chain.\n");
		ret = -1;
		goto out;
	}

	ptr = data + f->datalen - 1;
	for (i = 0; i < f->datalen - 1; i++) {
		if (*ptr == '\n')
			lines++;

		if (lines > TAIL_COUNT) {
			ptr++;
			break;
		}

		ptr--;
	}

	pr_msg("%s\n", ptr);

out:
	free(data);
	return ret;
}

/**
 * exfat_stat - function interface to display file status
 * @name:     Filename in UTF-8
 * @clu:      Directory cluster Index
 *
 * @return      0 (Success)
 *             -1 (Not found)
 */
int exfat_stat(const char *name, uint32_t clu)
{
	size_t index = 0;
	struct exfat_fileinfo *f;

	index = exfat_get_index(clu);
	if ((f = exfat_search_fileinfo(info.root[index], name)) == NULL) {
		pr_err("File is not found.\n");
		return -1;
	}

	pr_msg("File Name:   %s\n", f->name);
	pr_msg("File Size:   %zu\n", f->datalen);
	pr_msg("Clusters:    %zu\n", ROUNDUP(f->datalen, info.cluster_size));
	pr_msg("First Clu:   %u\n", f->clu);

	pr_msg("File Attr:   %c%c%c%c%c\n", f->attr & ATTR_READ_ONLY ? 'R' : '-',
			f->attr & ATTR_HIDDEN ? 'H' : '-',
			f->attr & ATTR_SYSTEM ? 'S' : '-',
			f->attr & ATTR_DIRECTORY ? 'D' : '-',
			f->attr & ATTR_ARCHIVE ? 'A' : '-');
	pr_msg("File Flags:  %s/ %s\n",
			f->flags & ALLOC_NOFATCHAIN ? "NoFatChain" : "FatChain",
			f->flags & ALLOC_POSIBLE ? "AllocationPossible" : "AllocationImpossible");

	pr_msg("Access Time: %02d-%02d-%02d %02d:%02d:%02d\n",
			1980 + f->atime.tm_year, f->atime.tm_mon, f->atime.tm_mday,
			f->atime.tm_hour, f->atime.tm_min, f->atime.tm_sec);
	pr_msg("Modify Time: %02d-%02d-%02d %02d:%02d:%02d\n",
			1980 + f->mtime.tm_year, f->mtime.tm_mon, f->mtime.tm_mday,
			f->mtime.tm_hour, f->mtime.tm_min, f->mtime.tm_sec);
	pr_msg("Create Time: %02d-%02d-%02d %02d:%02d:%02d\n",
			1980 + f->ctime.tm_year, f->ctime.tm_mon, f->ctime.tm_mday,
			f->ctime.tm_hour, f->ctime.tm_min, f->ctime.tm_sec);
	pr_msg("\n");

	return 0;
}

