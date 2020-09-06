#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include "debugfatfs.h"

/* Generic function prototype */
static uint32_t exfat_concat_cluster(uint32_t, void **, size_t);

/* Boot sector function prototype */
static int exfat_load_bootsec(struct exfat_bootsec *);
static void exfat_print_upcase(void);
static void exfat_print_label(void);
static int exfat_save_bitmap(uint32_t, uint32_t);

/* FAT-entry function prototype */
static uint32_t exfat_check_fat_entry(uint32_t);
static uint32_t exfat_update_fat_entry(uint32_t, uint32_t);

/* Directory chain function prototype */
static int exfat_check_dchain(uint32_t);
static int exfat_get_index(uint32_t);
static int exfat_get_freed_index(uint32_t *);
static int exfat_traverse_directory(uint32_t);

/* File function prototype */
static void exfat_create_fileinfo(node2_t *,
		uint32_t, struct exfat_dentry *, struct exfat_dentry *, uint16_t *);
static uint16_t exfat_calculate_checksum(unsigned char *, unsigned char);
static void exfat_convert_uniname(uint16_t *, uint64_t, unsigned char *);
static uint16_t exfat_calculate_namehash(uint16_t *, uint8_t);
static void exfat_convert_unixtime(struct tm *, uint32_t, uint8_t, uint8_t);
static int exfat_query_timestamp(struct tm *, uint32_t *, uint8_t *);
static int exfat_query_timezone(int, uint8_t *);

/* Operations function prototype */
int exfat_print_bootsec(void);
int exfat_print_fsinfo(void);
int exfat_lookup(uint32_t, char *);
int exfat_readdir(struct directory *, size_t, uint32_t);
int exfat_reload_directory(uint32_t);
int exfat_convert_character(const char *, size_t, char *);
int exfat_clean_dchain(uint32_t);
int exfat_set_fat_entry(uint32_t, uint32_t);
int exfat_get_fat_entry(uint32_t, uint32_t *);
int exfat_alloc_cluster(uint32_t);
int exfat_release_cluster(uint32_t);
int exfat_create(const char *, uint32_t, int);
int exfat_remove(const char *, uint32_t, int);

static const struct operations exfat_ops = {
	.statfs = exfat_print_bootsec,
	.info = exfat_print_fsinfo,
	.lookup =  exfat_lookup,
	.readdir = exfat_readdir,
	.reload = exfat_reload_directory,
	.convert = exfat_convert_character,
	.clean = exfat_clean_dchain,
	.setfat = exfat_set_fat_entry,
	.getfat = exfat_get_fat_entry,
	.alloc = exfat_alloc_cluster,
	.release = exfat_release_cluster,
	.create = exfat_create,
	.remove = exfat_remove,
};

static const struct query create_prompt[] = {
	{"Entry Type", 3, (char *[]){"  85  File","  C0  Stream", "  C1  Filename"}},
	{"File Attributes", 5, (char *[]){
										 "  Bit0  ReadOnly",
										 "  Bit1  Hidden",
										 "  Bit2  System",
										 "  Bit4  Directory",
										 "  Bit5  Archive"}},
	{"Secondary Count", 0, NULL},
	{"Reserverd", 0, NULL},
	{"General Seconday Flags", 2, (char *[]){"  Bit0 AllocationPossible", "  Bit1 NoFatChain"}},
	{"Name Length", 0, NULL},
	{"Name Hash", 0, NULL},
	{"Valid Data Length", 0, NULL},
	{"First Cluster", 0, NULL},
	{"Data Length", 0, NULL},
	{"Anything", 0, NULL},
};

/*************************************************************************************************/
/*                                                                                               */
/* GENERIC FUNCTION                                                                              */
/*                                                                                               */
/*************************************************************************************************/
/**
 * exfat_concat_cluster - Contatenate cluster @data with next_cluster
 * @clu:                  index of the cluster
 * @data:                 The cluster
 * @size:                 allocated size to store cluster data
 *
 * @retrun:        next cluster (@clu has next cluster)
 *                 0            (@clu doesn't have next cluster, or failed to realloc)
 */
static uint32_t exfat_concat_cluster(uint32_t clu, void **data, size_t size)
{
	uint32_t ret;
	void *tmp;
	ret = exfat_check_fat_entry(clu);

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
 * exfat_check_filesystem - Whether or not exFAT filesystem
 * @boot:      boot sector pointer
 * @ops:       filesystem operations
 *
 * @return:     1 (Image is exFAT filesystem)
 *              0 (Image isn't exFAT filesystem)
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
		f->datalen = 0;
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
 * @b:          boot sector pointer in exFAT
 *
 * @return        0 (success)
 *               -1 (failed to read)
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
#if 0
/**
 * exfat_load_bitmap - function to load allocation table
 * @clu:                          cluster index
 *
 * @return                        0 (cluster as available for allocation)
 *                                1 (cluster as not available for allocation)
 *                               -1 (failed)
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
#endif
/**
 * exfat_save_bitmap - function to save allocation table
 * @clu:                          cluster index
 * @value:                        Bit
 *
 * @return                        0 (success)
 *                               -1 (failed)
 */
static int exfat_save_bitmap(uint32_t clu, uint32_t value)
{
	int offset, byte;
	uint8_t mask = 0x01;

	if (clu < EXFAT_FIRST_CLUSTER || clu > info.cluster_count + 1) {
		pr_warn("cluster: %u is invalid.\n", clu);
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
		info.alloc_table[byte] &= mask;

	pr_debug("0x%x\n", info.alloc_table[byte]);
	return 0;
}

/*************************************************************************************************/
/*                                                                                               */
/* FAT-ENTRY FUNCTION                                                                            */
/*                                                                                               */
/*************************************************************************************************/
/**
 * exfat_check_fat_entry - Whether or not cluster is continuous
 * @clu:                  index of the cluster want to check
 *
 * @retrun:        next cluster (@clu has next cluster)
 *                 0            (@clu doesn't have next cluster)
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
		pr_debug("cluster: %u has chain. next is 0x%x.\n", clu, fat[offset]);
	}

	free(fat);
	return ret;
}

/**
 * exfat_update_fat_entry - Update FAT Entry to any cluster
 * @clu:                 index of the cluster want to check
 * @entry:                 any cluster index
 *
 * @retrun:                previous FAT entry
 */
static uint32_t exfat_update_fat_entry(uint32_t clu, uint32_t entry)
{
	uint32_t ret;
	size_t entry_per_sector = info.sector_size / sizeof(uint32_t);
	uint32_t fat_index = (info.fat_offset +  clu / entry_per_sector) * info.sector_size;
	uint32_t *fat;
	uint32_t offset = (clu) % entry_per_sector;

	if (clu > info.cluster_count + 1) {
		pr_warn("This Filesystem doesn't have Entry %u\n", clu);
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
 * @clu:                         index of the cluster
 *
 * @retrun:        1 (@clu has loaded)
 *                 0 (@clu hasn't loaded)
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
 * exfat_get_index      get directory chain index by argument
 * @clu:                index of the cluster
 *
 * @return:             directory chain index
 *                      Start of unused area (if doesn't lookup directory cache)
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
		pr_warn("Can't expand directory chain.\n");
		delete_node2(info.root[--i]);
	}

	return i;
}

/**
 * exfat_get_freed_index          get unused cluster index by allocation bitmap
 * @clup:                         cluster index pointer (Output)
 *
 * @return                        0 (found unused allocation cluster)
 *                               -1 (failed)
 */
static int exfat_get_freed_index(uint32_t *clup)
{
	int offset, byte;
	uint8_t mask = 0x01, entry;

	if (!info.alloc_table)
		exfat_traverse_directory(info.root_offset);

	for (byte = 0; byte < (info.cluster_count / CHAR_BIT); byte++) {
		entry = info.alloc_table[byte];
		for (offset = 0; offset < CHAR_BIT; offset++, entry >>= 1) {
			if (!(entry & mask)) {
				*clup = (byte * CHAR_BIT) + offset + EXFAT_FIRST_CLUSTER;
				exfat_save_bitmap(*clup, 1);
				return 0;
			}
		}
	}

	return -1;
}

/**
 * exfat_traverse_directory - function to traverse one directory
 * @clu:                          index of the cluster want to check
 *
 * @return        0 (success)
 *               -1 (failed to read)
 */
static int exfat_traverse_directory(uint32_t clu)
{
	int i, j, name_len;
	uint8_t remaining;
	uint16_t uniname[MAX_NAME_LENGTH] = {0};
	uint64_t len;
	size_t index = exfat_get_index(clu);
	struct exfat_fileinfo *f = (struct exfat_fileinfo *)info.root[index]->data;
	size_t size = info.cluster_size;
	size_t entries = size / sizeof(struct exfat_dentry);
	void *data;
	struct exfat_dentry d, next, name;

	if (f->datalen > 0) {
		pr_debug("Directory %s was already traversed.\n", f->name);
		return 0;
	}

	data = malloc(size);
	get_cluster(data, clu);

	do {
		for (i = 0; i < entries; i++){
			d = ((struct exfat_dentry *)data)[i];

			switch (d.EntryType) {
				case DENTRY_UNUSED:
					goto out;
				case DENTRY_BITMAP:
					pr_debug("Get: allocation table: cluster 0x%x, size: 0x%lx\n",
							d.dentry.bitmap.FirstCluster,
							d.dentry.bitmap.DataLength);
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
					if (i + remaining >= entries) {
						clu = exfat_concat_cluster(clu, &data, size);
						size += info.cluster_size;
						entries = size / sizeof(struct exfat_dentry);
					}

					next = ((struct exfat_dentry *)data)[i + 1];
					while ((!(next.EntryType & EXFAT_INUSE)) && (next.EntryType != DENTRY_UNUSED)) {
						pr_debug("This entry was deleted (0x%x).\n", next.EntryType);
						if (++i + remaining >= entries) {
							clu = exfat_concat_cluster(clu, &data, size);
							size += info.cluster_size;
							entries = size / sizeof(struct exfat_dentry);
						}
						next = ((struct exfat_dentry *)data)[i + 1];
					}
					if (next.EntryType != DENTRY_STREAM) {
						pr_warn("File should have stream entry, but This don't have.\n");
						continue;
					}

					name = ((struct exfat_dentry *)data)[i + 2];
					while ((!(name.EntryType & EXFAT_INUSE)) && (name.EntryType != DENTRY_UNUSED)) {
						pr_debug("This entry was deleted (0x%x).\n", name.EntryType);
						if (++i + remaining >= entries - 1) {
							clu = exfat_concat_cluster(clu, &data, size);
							size += info.cluster_size;
							entries = size / sizeof(struct exfat_dentry);
						}
						name = ((struct exfat_dentry *)data)[i + 2];
					}
					if (name.EntryType != DENTRY_NAME) {
						pr_warn("File should have name entry, but This don't have.\n");
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
				case DENTRY_STREAM:
					pr_warn("Stream needs be File entry, but This is not.\n");
					break;
			}
		}
		clu = exfat_concat_cluster(clu, &data, size);
		if (!clu)
			break;

		size += info.cluster_size;
		entries = size / sizeof(struct exfat_dentry);
	} while (1);
out:
	free(data);
	return 0;
}

/*************************************************************************************************/
/*                                                                                               */
/* FILE FUNCTION                                                                                 */
/*                                                                                               */
/*************************************************************************************************/
/**
 * exfat_create_file_entry - Create file infomarion
 * @head:       Directory chain head
 * @clu:        parent Directory cluster index
 * @file:       file dentry
 * @stream:     stream Extension dentry
 * @uniname:    File Name dentry
 */
static void exfat_create_fileinfo(node2_t *head, uint32_t clu,
		struct exfat_dentry *file, struct exfat_dentry *stream, uint16_t *uniname)
{
	int index, next_index = stream->dentry.stream.FirstCluster;
	struct exfat_fileinfo *f;
	size_t namelen = stream->dentry.stream.NameLength;

	f = malloc(sizeof(struct exfat_fileinfo));
	f->name = malloc(namelen * UTF8_MAX_CHARSIZE + 1);
	memset(f->name, '\0', namelen * UTF8_MAX_CHARSIZE + 1);

	exfat_convert_uniname(uniname, namelen, f->name);
	f->namelen = namelen;
	f->datalen = stream->dentry.stream.DataLength;
	f->attr = file->dentry.file.FileAttributes;
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
	 ((struct exfat_fileinfo *)(head->data))->datalen++;

	/* If this entry is Directory, prepare to create next chain */
	if ((f->attr & ATTR_DIRECTORY) && (!exfat_check_dchain(next_index))) {
		struct exfat_fileinfo *d = malloc(sizeof(struct exfat_fileinfo));
		d->name = malloc(f->namelen + 1);
		strncpy((char *)d->name, (char *)f->name, f->namelen + 1);
		d->namelen = namelen;
		d->datalen = 0;
		d->attr = file->dentry.file.FileAttributes;
		d->hash = stream->dentry.stream.NameHash;

		index = exfat_get_index(next_index);
		info.root[index] = init_node2(next_index, head);
	}
}

/**
 * exfat_calculate_checksum - Calculate file entry Checksum
 * @entry:                    points to an in-memory copy of the directory entry set
 * @count:                    the number of secondary directory entries
 *
 * @return                   Checksum
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
 * exfat_convert_uniname     function to get filename
 * @uniname:                 filename dentry in UTF-16
 * @name_len:                filename length
 * @name:                    filename in UTF-8 (output)
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
 * @return                NameHash
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
 * exfat_convert_unixname    function to get timestamp in file
 * @t:                       output pointer
 * @time:                    Timestamp Field in File Directory Entry
 * @subsec:                  10msincrement Field in File Directory Entry
 * @tz:                      UtcOffset in File Directory Entry
 */
static void exfat_convert_unixtime(struct tm *t, uint32_t time, uint8_t subsec, uint8_t tz)
{
	t->tm_year = (time >> EXFAT_YEAR) & 0x7f;
	t->tm_mon  = (time >> EXFAT_MONTH) & 0x0f;
	t->tm_mday = (time >> EXFAT_DAY) & 0x1f;
	t->tm_hour = (time >> EXFAT_HOUR) & 0x0f;
	t->tm_min  = (time >> EXFAT_MINUTE) & 0x3f;
	t->tm_sec  = (time & 0x1f) * 2;
	t->tm_sec += subsec / 100;
	/* OffsetValid */
	if (tz & 0x80) {
		int ex_min = 0;
		int ex_hour = 0;
		char offset = tz & 0x7f;
		/* negative value */
		if (offset & 0x40) {
			offset = ((~offset) + 1) & 0x7f;
			ex_min = ((offset % 4) * 15) * -1;
			ex_hour = (offset / 4) * -1;
		} else {
			ex_min = (offset % 4) * 15;
			ex_hour = offset / 4;
		}
		t->tm_hour += ex_hour;
		t->tm_min  += ex_min;
	}
}

/**
 * exfat_query_timestamp - Prompt user for timestamp
 * @tz:                    local timezone
 * @time:                  Time Field (Output)
 * @subsec:                Time subsecond Field (Output)
 * @tz:                    offset from UTC Field (Output)
 *
 * @return        0 (Success)
 */
static int exfat_query_timestamp(struct tm *t,
		uint32_t *timestamp, uint8_t *subsec)
{
	char buf[QUERY_BUFFER_SIZE] = {};

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

	*timestamp |= ((t->tm_year - 80) << EXFAT_YEAR);
	*timestamp |= ((t->tm_mon + 1) << EXFAT_MONTH);
	*timestamp |= (t->tm_mday << EXFAT_DAY);
	*timestamp |= (t->tm_hour << EXFAT_HOUR);
	*timestamp |= (t->tm_min << EXFAT_MINUTE);
	*timestamp |= t->tm_sec / 2;
	*subsec += ((t->tm_sec % 2) * 100);
	return 0;
}

/**
 * exfat_query_timezone  - Prompt user for timestamp
 * @diff:                  difference localtime and UTCtime
 * @tz:                    offset from UTC Field (Output)
 *
 * @return        0 (Success)
 */
static int exfat_query_timezone(int diff, uint8_t *tz)
{
	char buf[QUERY_BUFFER_SIZE] = {};
	char op = (*tz & 0x40) ? '-' : '+';
	char min = 0, hour = 0;

	pr_msg("Timezone\n");
	pr_msg("Select (Default: %c%02d:%02d): ",
			op,
			diff / 60,
			diff % 60);
	fflush(stdout);

	if (!fgets(buf, QUERY_BUFFER_SIZE, stdin))
		return -1;

	if (buf[0] != '\n') {
		sscanf(buf, "%c%02hhd%02hhd",
				&op,
				&hour,
				&min);
		*tz = hour * 4 + min / 15;

		if (op == '-') {
			*tz = ~(*tz) + 1;
		} else if (op != '+' && op != ' ') {
			pr_debug("Invalid operation. you can use only ('+' or '-').\n");
			*tz = 0;
		}
	}

	pr_msg("\n");
	return 0;
}
/*************************************************************************************************/
/*                                                                                               */
/* OPERATIONS FUNCTION                                                                           */
/*                                                                                               */
/*************************************************************************************************/
/**
 * exfat_print_bootsec - print boot sector in exFAT
 * @b:          boot sector pointer in exFAT
 *
 * @return        0 (success)
 */
int exfat_print_bootsec(void)
{
	struct exfat_bootsec *b = malloc(sizeof(struct exfat_bootsec));

	exfat_load_bootsec(b);
	pr_msg("%-28s\t: 0x%8lx (sector)\n", "media-relative sector offset",
			b->PartitionOffset);
	pr_msg("%-28s\t: 0x%8x (sector)\n", "Offset of the First FAT",
			b->FatOffset);
	pr_msg("%-28s\t: %8u (sector)\n", "Length of FAT table",
			b->FatLength);
	pr_msg("%-28s\t: 0x%8x (sector)\n", "Offset of the Cluster Heap",
			b->ClusterHeapOffset);
	pr_msg("%-28s\t: %8u (cluster)\n", "The number of clusters",
			b->ClusterCount);
	pr_msg("%-28s\t: %8u (cluster)\n", "The first cluster of the root",
			b->FirstClusterOfRootDirectory);
	pr_msg("%-28s\t: %8lu (sector)\n", "Size of exFAT volumes",
			b->VolumeLength);
	pr_msg("%-28s\t: %8lu (byte)\n", "Bytes per sector",
			info.sector_size);
	pr_msg("%-28s\t: %8lu (byte)\n", "Bytes per cluster",
			info.cluster_size);
	pr_msg("%-28s\t: %8u\n", "The number of FATs",
			b->NumberOfFats);
	pr_msg("%-28s\t: %8u (%%)\n", "The percentage of clusters",
			b->PercentInUse);
	pr_msg("\n");

	free(b);
	return 0;
}

/**
 * exfat_print_fsinfo - print filesystem information in exFAT
 *
 * @return        0 (success)
 */
int exfat_print_fsinfo(void)
{
	exfat_print_upcase();
	exfat_print_label();
	return 0;
}

/**
 * exfat_lookup       - function interface to lookup pathname
 * @clu:                directory cluster index
 * @name:               file name
 *
 * @return:              cluster index
 *                       0 (Not found)
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
			pr_warn("Pathname is too depth. (> %d)\n", MAX_NAME_LENGTH);
			return -1;
		}
		path[++depth] = strtok(NULL, "/");
	};

	for (i = 0; path[i] && i < depth + 1; i++) {
		pr_debug("Lookup %s to %d\n", path[i], clu);
		found = false;
		index = exfat_get_index(clu);
		f = (struct exfat_fileinfo *)info.root[index]->data;
		if ((!info.root[index]) || (f->namelen == 0)) {
			pr_debug("Directory hasn't load yet, or This Directory doesn't exist in filesystem.\n");
			exfat_traverse_directory(clu);
			index = exfat_get_index(clu);
			if (!info.root[index]) {
				pr_warn("This Directory doesn't exist in filesystem.\n");
				return 0;
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
			return 0;
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
 */
int exfat_reload_directory(uint32_t clu)
{
	int index = exfat_get_index(clu);
	struct exfat_fileinfo *f = NULL;

	exfat_clean_dchain(index);
	f = ((struct exfat_fileinfo *)(info.root[index])->data);
	f->datalen = 0;
	return exfat_traverse_directory(clu);
}

/**
 * exfat_convert_character - Convert character by upcase-table
 * @src:           Target characters in UTF-8
 * @len:           Target characters length
 * @dist:          convert result in UTF-8 (Output)
 *
 * return:     0  (succeeded in obtaining filesystem)
 *             -1 (failed)
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
 * exfat_clean_dchain            function to clean opeartions
 * @index:                       directory chain index
 *
 * @return        0 (success)
 *               -1 (already released)
 */
int exfat_clean_dchain(uint32_t index)
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
 * exfat_set_fat_entry - Set FAT Entry to any cluster
 * @index:              index of the cluster want to check
 * @entry:              any cluster index
 *
 * @retrun:             0
 */
int exfat_set_fat_entry(uint32_t clu, uint32_t entry)
{
	exfat_update_fat_entry(clu, entry);
	return 0;
}

/**
 * exfat_get_fat_entry - Get cluster is continuous
 * @index:              index of the cluster want to check
 * @entry:              any cluster index
 *
 *                      0
 */
int exfat_get_fat_entry(uint32_t clu, uint32_t *entry)
{
	*entry = exfat_check_fat_entry(clu);
	return 0;
}

/**
 * exfat_alloc_cluster - function to allocate cluster
 * @index:               cluster index
 *
 * @return                0 (success)
 *                       -1 (failed)
 */
int exfat_alloc_cluster(uint32_t clu)
{
	return exfat_save_bitmap(clu, 1);
}

/**
 * exfat_release_cluster - function to release cluster
 * @index:                 cluster index
 *
 * @return                  0 (success)
 *                         -1 (failed)
 */
int exfat_release_cluster(uint32_t clu)
{
	return exfat_save_bitmap(clu, 0);
}

/**
 * exfat_create -  function interface to create entry
 * @name:          Filename in UTF-8
 * @index:         Current Directory Index
 * @opt:           create option
 *
 * @return        0 (Success)
 *
 * NOTE: Tentative implemetation
 */
int exfat_create(const char *name, uint32_t clu, int opt)
{
	int i, namei, lasti;
	uint8_t attr = 0;
	void *data;
	uint16_t uniname[MAX_NAME_LENGTH] = {0};
	uint16_t namehash = 0;
	uint8_t len;
	uint8_t count;
	uint32_t stamp;
	uint32_t unused = 0;
	uint8_t subsec, tz;
	size_t size = info.cluster_size;
	size_t entries = size / sizeof(struct exfat_dentry);
	size_t name_len;
	struct exfat_dentry *d;
	time_t t = time(NULL);
	struct tm *local = localtime(&t);
	int diff = local->tm_min + (local->tm_hour * 60);
	struct tm *utc = gmtime(&t);
	diff -= utc->tm_min + (utc->tm_hour * 60);

	/* convert UTF-8 to UTF16 */
	len = utf8s_to_utf16s((unsigned char *)name, strlen(name), uniname);
	count = ((len + ENTRY_NAME_MAX - 1) / ENTRY_NAME_MAX) + 1;
	namehash = exfat_calculate_namehash(uniname, len);

	/* Set up timezone */
	tz = (diff / 15) & 0x7f;
	/* Lookup last entry */
	data = malloc(size);
	get_cluster(data, clu);
	for (i = 0; i < entries; i++){
		d = ((struct exfat_dentry *)data) + i;
		if (d->EntryType == DENTRY_UNUSED)
			break;
	}

	query_param(create_prompt[0], &(d->EntryType), 0x85, 1);
	lasti = i;
	switch (d->EntryType) {
		case DENTRY_FILE:
			query_param(create_prompt[1], &attr, ATTR_ARCHIVE, 2);
			d->dentry.file.FileAttributes = attr;
			query_param(create_prompt[2], &(d->dentry.file.SecondaryCount), count, 1);
			query_param(create_prompt[3], &(d->dentry.file.Reserved1), 0x0, 2);
			exfat_query_timestamp(local, &stamp, &subsec);
			exfat_query_timezone(diff, &tz);
			d->dentry.file.CreateTimestamp = stamp;
			d->dentry.file.LastAccessedTimestamp = stamp;
			d->dentry.file.LastModifiedTimestamp = stamp;
			d->dentry.file.Create10msIncrement = subsec;
			d->dentry.file.LastModified10msIncrement = subsec;
			d->dentry.file.CreateUtcOffset = tz | 0x80;
			d->dentry.file.LastAccessdUtcOffset = tz | 0x80;
			d->dentry.file.LastModifiedUtcOffset = tz | 0x80;
			pr_msg("DO you want to create stream entry? (Default [y]/n): ");
			fflush(stdout);
			if (getchar() == 'n')
				break;
			lasti = i + 1;
			d = ((struct exfat_dentry *)data) + lasti;
			d->EntryType = DENTRY_STREAM;
		case DENTRY_STREAM:
			query_param(create_prompt[4], &(d->dentry.stream.GeneralSecondaryFlags), 0x01, 1);
			query_param(create_prompt[3], &(d->dentry.stream.Reserved1), 0x00, 1);
			query_param(create_prompt[5], &(d->dentry.stream.NameLength), len, 1);
			query_param(create_prompt[6], &(d->dentry.stream.NameHash), namehash, 2);
			query_param(create_prompt[3], &(d->dentry.stream.Reserved2), 0x00, 2);
			query_param(create_prompt[7], &(d->dentry.stream.ValidDataLength), 0x00, 8);
			query_param(create_prompt[3], &(d->dentry.stream.Reserved3), 0x00, 4);
			if (attr & ATTR_DIRECTORY)
				exfat_get_freed_index(&unused);
			query_param(create_prompt[8], &(d->dentry.stream.FirstCluster), unused, 4);
			query_param(create_prompt[9], &(d->dentry.stream.DataLength), 0x00, 8);
			pr_msg("DO you want to create Name entry? (Default [y]/n): ");
			fflush(stdout);
			if (getchar() == 'n')
				break;
			lasti = i + 2;
			d = ((struct exfat_dentry *)data) + lasti;
			d->EntryType = DENTRY_NAME;
		case DENTRY_NAME:
			for (namei = 0; namei < count - 1; namei++) {
				name_len = MIN(ENTRY_NAME_MAX, len - namei * ENTRY_NAME_MAX);
				memcpy(d->dentry.name.FileName,
						uniname + namei * name_len, name_len * sizeof(uint16_t));
				d = ((struct exfat_dentry *)data) + lasti + namei;
				d->EntryType = DENTRY_NAME;
			}
			break;
		default:
			query_param(create_prompt[10], d, 0x00, 32);
			break;
	}
	if (attr) {
		d = ((struct exfat_dentry *)data) + i;
		d->dentry.file.SetChecksum =
			exfat_calculate_checksum(data+ i * sizeof(struct exfat_dentry), count);
	}
	set_cluster(data, clu);
	free(data);
	return 0;
}

/**
 * exfat_remove -  function interface to remove entry
 * @name:          Filename in UTF-8
 * @index:         Current Directory Index
 * @opt:           create option
 *
 * @return        0 (Success)
 * @return       -1 (Not found)
 *
 * NOTE: Tentative implemetation
 */
int exfat_remove(const char *name, uint32_t clu, int opt)
{
	int i, j, name_len, name_len2;
	void *data;
	uint16_t uniname[MAX_NAME_LENGTH] = {0};
	uint16_t uniname2[MAX_NAME_LENGTH] = {0};
	uint16_t namehash = 0;
	uint8_t remaining;
	size_t size = info.cluster_size;
	size_t entries = size / sizeof(struct exfat_dentry);
	struct exfat_dentry *d, *s, *n;

	/* convert UTF-8 to UTF16 */
	name_len = utf8s_to_utf16s((unsigned char *)name, strlen(name), uniname);
	namehash = exfat_calculate_namehash(uniname, name_len);

	/* Lookup last entry */
	data = malloc(size);
	get_cluster(data, clu);

	for (i = 0; i < entries; i++) {
		d = ((struct exfat_dentry *)data) + i;
		switch (d->EntryType) {
			case DENTRY_UNUSED:
				free(data);
				return -1;
			case DENTRY_FILE:
				remaining = d->dentry.file.SecondaryCount;
				if (i + remaining >= entries) {
					clu = exfat_concat_cluster(clu, &data, size);
					size += info.cluster_size;
					entries = size / sizeof(struct exfat_dentry);
				}

				s = ((struct exfat_dentry *)data) + i + 1;
				if (s->EntryType != DENTRY_STREAM) {
					pr_warn("File should have stream entry, but This don't have.\n");
					continue;
				}

				if (s->dentry.stream.NameHash != namehash) {
					i += remaining - 1;
					continue;
				}

				n = ((struct exfat_dentry *)data) + i + 2;
				if (n->EntryType != DENTRY_NAME) {
					pr_warn("File should have name entry, but This don't have.\n");
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
			default:
				break;
		}
	}
out:
	set_cluster(data, clu);
	free(data);
	return 0;
}
