#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include "dumpexfat.h"

/* Print function prototype */
int exfat_show_boot_sec(struct device_info *, struct exfat_bootsec *);
static void exfat_print_allocation_bitmap(struct device_info *);
static void exfat_print_upcase_table(struct device_info *);
static void exfat_print_file_entry(struct device_info *,
		struct exfat_dentry *, struct exfat_dentry*, uint16_t*);
int exfat_print_cluster(struct device_info *, uint32_t);

/* Load function prototype */
static int exfat_create_allocation_chain(struct device_info *, void *);
static void exfat_load_filename(uint16_t*, uint64_t);
static void exfat_load_timestamp(struct tm *, char *,
		uint32_t, uint8_t, uint8_t);
int exfat_traverse_directories(struct device_info *, uint32_t);
int exfat_traverse_one_directory(struct device_info *, uint32_t);

/* Check function prototype */
static bool exfat_check_allocation_cluster(struct device_info *, uint32_t);
static uint32_t exfat_check_fatentry(struct device_info *, uint32_t);

static uint32_t exfat_concat_cluster(struct device_info *, uint32_t, void *, size_t);

/**
 * exfat_show_boot_sec - show boot sector in exFAT
 * @info:       Target device information
 * @b:          boot sector pointer in exFAT
 *
 * TODO: Unify the words (show? print? display?)
 */
int exfat_show_boot_sec(struct device_info *info, struct exfat_bootsec *b)
{
	dump_info("%-28s\t: %8lx (sector)\n", "media-relative sector offset",
			b->PartitionOffset);
	dump_info("%-28s\t: %8x (sector)\n", "Offset of the First FAT",
			b->FatOffset);
	dump_info("%-28s\t: %8u (sector)\n", "Length of FAT table",
			b->FatLength);
	dump_info("%-28s\t: %8x (sector)\n", "Offset of the Cluster Heap",
			b->ClusterHeapOffset);
	dump_info("%-28s\t: %8u (cluster)\n", "The number of clusters",
			b->ClusterCount);
	dump_info("%-28s\t: %8u (cluster)\n", "The first cluster of the root",
			b->FirstClusterOfRootDirectory);

	info->fat_offset = b->FatOffset;
	info->heap_offset = b->ClusterHeapOffset;
	info->root_offset = b->FirstClusterOfRootDirectory;
	info->sector_size  = 1 << b->BytesPerSectorShift;
	info->cluster_size = (1 << b->SectorsPerClusterShift) * info->sector_size;
	info->cluster_count = b->ClusterCount;
	info->fat_length = b->NumberOfFats * b->FatLength * info->sector_size;

	dump_notice("%-28s\t: %8lu (sector)\n", "Size of exFAT volumes",
			b->VolumeLength);
	dump_notice("%-28s\t: %8lu (byte)\n", "Bytes per sector",
			info->sector_size);
	dump_notice("%-28s\t: %8lu (byte)\n", "Bytes per cluster",
			info->cluster_size);

	dump_notice("%-28s\t: %8u\n", "The number of FATs",
			b->NumberOfFats);
	dump_notice("%-28s\t: %8u (%%)\n", "The percentage of clusters",
			b->PercentInUse);
	dump_notice("\n");

	return 0;
}

/**
 * exfat_print_allocation_bitmap - print allocation bitmap
 * @info:       Target device information
 */
static void exfat_print_allocation_bitmap(struct device_info *info)
{
	node_t *node = info->chain_head;
	while (node->next != NULL) {
		node = node->next;
		dump_notice("%u ", node->x);
	}
	dump_notice("\n");
}

/**
 * exfat_print_upcase-table - print upcase table
 * @info:       Target device information
 */
static void exfat_print_upcase_table(struct device_info *info)
{
	int byte, offset;
	size_t uni_count = 0x10 / sizeof(uint16_t);
	size_t length = info->upcase_size;

	/* Usually, we don't want to display raw upcase table */
	if (print_level < DUMP_INFO) {
		dump_notice("Upcase-table was skipped.\n");
		return;
	}

	/* Output table header */
	dump_info("Offset  ");
	for(byte = 0; byte < uni_count; byte++)
		dump_info("  +%u ",byte);
	dump_info("\n");

	/* Output Table contents */
	for(offset = 0; offset < length / uni_count; offset++) {
		dump_info("%04lxh:  ", offset * 0x10 / sizeof(uint16_t));
		for(byte = 0; byte < uni_count; byte++) {
			dump_info("%04x ", info->upcase_table[offset * uni_count + byte]);
		}
		dump_info("\n");
	}
}

/**
 * exfat_print_file_entry - print file metadata
 * @info:       Target device information
 * @file:       file dentry
 * @stream:     stream Extension dentry
 * @name:       File Name dentry
 */
static void exfat_print_file_entry(struct device_info *info,
		struct exfat_dentry *file, struct exfat_dentry *stream, uint16_t *uniname)
{
	struct tm ctime, mtime, atime;

	exfat_load_filename(uniname, stream->dentry.stream.NameLength);
	dump_info("Size: %lu\n", stream->dentry.stream.DataLength);
	dump_debug("First Cluster: %u\n", stream->dentry.stream.FirstCluster);

	exfat_load_timestamp(&ctime, "Create", file->dentry.file.CreateTimestamp,
			file->dentry.file.Create10msIncrement,
			file->dentry.file.CreateUtcOffset);
	exfat_load_timestamp(&mtime, "Modify", file->dentry.file.LastModifiedTimestamp,
			file->dentry.file.LastModified10msIncrement,
			file->dentry.file.LastModifiedUtcOffset);
	exfat_load_timestamp(&atime, "Access", file->dentry.file.LastAccessedTimestamp,
			0,
			file->dentry.file.LastAccessdUtcOffset);
	dump_info("\n");
}

/**
 * exfat_show_boot_sec - function to print any cluster
 * @info:       Target device information
 * @index:      cluster index to display
 *
 * TODO: Guess the entry in cluster
 */
int exfat_print_cluster(struct device_info *info, uint32_t index)
{
	if (!exfat_check_allocation_cluster(info, index) && !info->force) {
		dump_err("cluster %u is not allocated.\n", index);
		return -1;
	}

	void *data;
	data = malloc(info->cluster_size);
	get_cluster(info, data, index);
	dump_notice("Cluster #%u:\n", index);
	hexdump(output, data, info->cluster_size);
	free(data);
	return 0;
}

/**
 * exfat_create_allocation_chain - function to get cluster chain
 * @info:       Target device information
 * @void:       pointer to Bitmap table
 */
static int exfat_create_allocation_chain(struct device_info *info, void *bitmap)
{
	int i, bit;
	uint8_t entry;
	for (i = 0; i < (info->cluster_count / CHAR_BIT); i++) {
		entry = ((uint8_t *)bitmap)[i];
		if (!entry)
			continue;

		for (bit = 0; bit < CHAR_BIT; bit++, entry >>= 1) {
			if(entry & 0x01) {
				uint64_t clu = (i * CHAR_BIT) + bit + EXFAT_FIRST_CLUSTER;
				append_node(info->chain_head, clu);
			}
		}
	}
	return 0;
}

/**
 * exfat_load_filename - function to get filename
 * @name:           filename dentry
 * @name_len:       filename length
 * @count:          the number of filename dentries
 */
static void exfat_load_filename(uint16_t *uniname, uint64_t name_len)
{
	utf16_to_utf8(uniname, name_len, NULL);
}

/**
 * exfat_load_timestamp - function to get timestamp in file
 * @tm:             output pointer
 * @str             additional any messages
 * @time:           Timestamp Field in File Directory Entry
 * @subsec:         10msincrement Field in File Directory Entry
 * @tz:             UtcOffset in File Directory Entry
 */
static void exfat_load_timestamp(struct tm *t, char *str,
		uint32_t time, uint8_t subsec, uint8_t tz)
{
	int8_t offset = tz * 15;
	t->tm_year = (time >> EXFAT_YEAR) & 0x7f;
	t->tm_mon  = (time >> EXFAT_MONTH) & 0x0f;
	t->tm_mday = (time >> EXFAT_DAY) & 0x1f;
	t->tm_hour = (time >> EXFAT_HOUR) & 0x0f;
	t->tm_min  = (time >> EXFAT_MINUTE) & 0x3f;
	t->tm_sec  = (time & 0x1f) * 2;
	t->tm_sec += subsec / 100;
	dump_info("%s: %d-%02d-%02d %02d:%02d:%02d.%02d (UTC + %02d:%02d)\n", str,
			1980 + t->tm_year, t->tm_mon, t->tm_mday,
			t->tm_hour, t->tm_min,
			t->tm_sec,
			subsec % 100,
			offset / 60,
			offset % 60);
}

/**
 * exfat_traverse_directories - function interface to traverse all cluster
 * @info:          Target device information
 * @index:         index of the cluster want to check
 */
int exfat_traverse_directories(struct device_info *info, uint32_t index)
{
	info->root = (node2_t **)malloc(sizeof(node2_t *) * info->root_maxsize);
	return exfat_traverse_one_directory(info, index);
}

/**
 * exfat_traverse_one_directory - function to traverse one directory
 * @info:          Target device information
 * @index:         index of the cluster want to check
 * @count:         Directory depth count
 */
int exfat_traverse_one_directory(struct device_info *info, uint32_t index)
{
	int i, j, byte, name_len;
	uint8_t scount;
	uint16_t attr = 0;
	uint16_t uniname[255] = {0};
	uint32_t next_index;
	uint64_t c, len;
	size_t size = info->cluster_size;
	size_t entries = size / sizeof(struct exfat_dentry);
	void *clu, *clu_tmp;
	struct exfat_dentry d, next, name;

	clu = malloc(size);
	get_cluster(info, clu, index);
	if (info->root_size >= info->root_maxsize) {
		info->root_maxsize += DENTRY_LISTSIZE;
		node2_t **tmp = (node2_t **)realloc(info->root, sizeof(node2_t *) * info->root_maxsize);
		if (!tmp)
			/* Failed to expand area */
			return -1;
		info->root = tmp;
	}
	info->root[info->root_size] = init_node2(index, 0);
	do {
		for(i = 0; i < entries; i++){
			d = ((struct exfat_dentry *)clu)[i];

			switch (d.EntryType) {
				case DENTRY_UNUSED:
					dump_debug("End of cluster#%u\n", index);
					goto out;
				case DENTRY_BITMAP:
					dump_debug("Get: allocation table: cluster %x, size: %lx\n",
							d.dentry.bitmap.FirstCluster,
							d.dentry.bitmap.DataLength);
					info->chain_head = init_node();
					clu_tmp = malloc(info->cluster_size);
					get_cluster(info, clu_tmp, d.dentry.bitmap.FirstCluster);
					exfat_create_allocation_chain(info, clu_tmp);
					free(clu_tmp);

					dump_notice("Allocation Bitmap (#%u):\n", d.dentry.bitmap.FirstCluster);
					exfat_print_allocation_bitmap(info);
					break;
				case DENTRY_UPCASE:
					info->upcase_size = d.dentry.upcase.DataLength;
					len = (info->cluster_size / info->upcase_size) + 1;
					info->upcase_table = (uint16_t*)malloc(info->cluster_size * len);
					dump_debug("Get: Up-case table: cluster %x, size: %x\n",
							d.dentry.upcase.FirstCluster,
							d.dentry.upcase.DataLength);
					get_clusters(info,
							info->upcase_table, d.dentry.upcase.FirstCluster, len);
					dump_notice("Upcase table (#%u):\n", d.dentry.upcase.FirstCluster);
					exfat_print_upcase_table(info);
					break;
				case DENTRY_VOLUME:
					dump_notice("volume Label: ");
					/* FIXME: VolumeLabel is Unicode string, But %c is ASCII code */
					for(byte = 0; byte < d.dentry.vol.CharacterCount; byte++) {
						dump_notice("%c", d.dentry.vol.VolumeLabel[byte]);
					}
					dump_notice("\n");
					break;
				case DENTRY_FILE:
					scount = d.dentry.file.SecondaryCount;
					if (i + scount > entries) {
						index = exfat_concat_cluster(info, index, clu, size);
						size += info->cluster_size;
						entries = size / sizeof(struct exfat_dentry);
					}

					next = ((struct exfat_dentry *)clu)[i + 1];
					name = ((struct exfat_dentry *)clu)[i + 2];
					if (next.EntryType != DENTRY_STREAM || name.EntryType != DENTRY_NAME) {
						dump_warn("File should have stream/name entry, but This don't have.\n");
						return -1;
					} 
					name_len = next.dentry.stream.NameLength;
					for (j = 0; j < scount - 1; j++) {
						name_len = MIN(ENTRY_NAME_MAX, next.dentry.stream.NameLength - j * ENTRY_NAME_MAX);
						memcpy(uniname + j * ENTRY_NAME_MAX, (((struct exfat_dentry *)clu)[i + 2 + j]).dentry.name.FileName, name_len * sizeof(uint16_t));
					}
					attr = d.dentry.file.FileAttributes;
					c = next.dentry.stream.FirstCluster;
					insert_node2(info->root[info->root_size], c, attr);
					exfat_print_file_entry(info,
							&d, &next, uniname);
					i += scount;
					break;
				case DENTRY_STREAM:
					dump_warn("Stream needs be File entry, but This is not.\n");
					break;
			}
		}
		index = exfat_concat_cluster(info, index, clu, size);
		if (!index) 
			break;

		size += info->cluster_size;
		entries = size / sizeof(struct exfat_dentry);
	} while(1);
out:
	info->root_size++;
	free(clu);
	return 0;
}

/**
 * exfat_check_allocation_cluster - Whether or not cluster is allocated
 * @info:          Target device information
 * @index:         index of the cluster want to check
 */
static bool exfat_check_allocation_cluster(struct device_info *info, uint32_t index)
{
	node_t *node = search_node(info->chain_head, index);
	if (node)
		return true;
	return false;
}

/**
 * exfat_check_fatentry - Whether or not cluster is continuous
 * @info:          Target device information
 * @index:         index of the cluster want to check
 *
 * @retrun:        next cluster (@index has next cluster)
 *                 0            (@index doesn't have next cluster)
 */
static uint32_t exfat_check_fatentry(struct device_info *info, uint32_t index)
{
	uint32_t ret;
	size_t entry_per_sector = info->sector_size / sizeof(uint32_t);
	uint32_t fat_index = (info->fat_offset +  index / entry_per_sector) * info->sector_size;
	uint32_t *fat;

	fat = (uint32_t *)malloc(info->sector_size);
	get_sector(info, fat, fat_index, 1);
	/* validate index */
	if (index == EXFAT_BADCLUSTER) {
		ret = 0;
		dump_err("cluster: %u is bad cluster.\n", index);
	} else if (index == EXFAT_LASTCLUSTER) {
		ret = 0;
		dump_debug("cluster: %u is the last cluster of cluster chain.\n", index);
	} else if (index < EXFAT_FIRST_CLUSTER || index > info->cluster_count + 1) {
		ret = 0;
		dump_debug("cluster: %u is invalid.\n", index);
	} else {
		ret = fat[index];
		dump_debug("cluster: %u has chain. next is %u.\n", ret, fat[index]);
	}

	free(fat);
	return ret;
}

/**
 * exfat_concat_cluster - Contatenate cluster @data with next_cluster
 * @info:          Target device information
 * @index:         index of the cluster
 * @data:          The cluster
 * @size:          allocated size to store cluster data
 *
 * @retrun:        next cluster (@index has next cluster)
 *                 0            (@index doesn't have next cluster, or failed to realloc)
 */
static uint32_t exfat_concat_cluster(struct device_info *info, uint32_t index, void *data, size_t size)
{
	uint32_t ret;
	void *clu_tmp;
	ret = exfat_check_fatentry(info, index);

	if (ret) {
		clu_tmp = realloc(data, size + info->cluster_size);
		if (clu_tmp) {
			data = clu_tmp;
			get_cluster(info, data + size, ret);
			dump_notice("Concatenate cluster #%u with #%u\n.\n", index, ret);
			free(clu_tmp);
		} else {
			dump_err("Failed to Get new memory.\n");
			ret = 0;
		}
	}
	return ret;
}
