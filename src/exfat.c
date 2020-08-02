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
		union dentry, union dentry, union dentry, uint8_t);
int exfat_print_cluster(struct device_info *, uint32_t);

/* Load function prototype */
static int exfat_create_allocation_chain(struct device_info *, void *);
static void exfat_load_filename(union dentry, uint64_t, uint8_t);
static void exfat_load_timestamp(struct tm *, char *,
		uint32_t, uint8_t, uint8_t);
int exfat_load_root_dentry(struct device_info *);
int exfat_traverse_directory(struct device_info *, uint32_t);
static int __exfat_traverse_directory(struct device_info *, uint32_t, size_t);

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
		union dentry file, union dentry stream, union dentry name,
		uint8_t count)
{
	struct tm ctime, mtime, atime;

	exfat_load_filename(name, stream.stream.NameLength, count);
	dump_info("Size: %lu\n", stream.stream.DataLength);
	dump_debug("First Cluster: %u\n", stream.stream.FirstCluster);

	exfat_load_timestamp(&ctime, "Create", file.file.CreateTimestamp,
			file.file.Create10msIncrement,
			file.file.CreateUtcOffset);
	exfat_load_timestamp(&mtime, "Modify", file.file.LastModifiedTimestamp,
			file.file.LastModified10msIncrement,
			file.file.LastModifiedUtcOffset);
	exfat_load_timestamp(&atime, "Access", file.file.LastAccessedTimestamp,
			0,
			file.file.LastAccessdUtcOffset);
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
static void exfat_load_filename(union dentry name, uint64_t name_len, uint8_t count)
{
	/* FIXME: decode and output name */
	dump_info("Name: Optimized out. (%lu)\n", name_len);
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
 * exfat_load_root_dentry - load extra entry(bitmap/upcase/volume)
 * @info:          Target device information
 * @root:          root directory dentry
 */
int exfat_load_root_dentry(struct device_info *info)
{
	int i, byte, ret = 0;
	void *root, *data;
	root = malloc(info->cluster_size);

	ret = get_cluster(info, root, info->root_offset);
	if (ret)
		goto out;

	for(i = 0;
			i < (info->cluster_size / sizeof(struct exfat_dentry));
			i++) {
		struct exfat_dentry dentry = ((struct exfat_dentry *)root)[i];
		switch (dentry.EntryType) {
			case DENTRY_BITMAP:
				dump_debug("Get: allocation table: cluster %x, size: %lx\n",
						dentry.dentry.bitmap.FirstCluster,
						dentry.dentry.bitmap.DataLength);
				info->chain_head = init_node();
				data = malloc(info->cluster_size);
				get_cluster(info, data, dentry.dentry.bitmap.FirstCluster);
				exfat_create_allocation_chain(info, data);
				free(data);

				dump_notice("Allocation Bitmap (#%u):\n", dentry.dentry.bitmap.FirstCluster);
				exfat_print_allocation_bitmap(info);
				break;
			case DENTRY_UPCASE:
				info->upcase_size = dentry.dentry.upcase.DataLength;
				size_t clusters = (info->cluster_size / info->upcase_size) + 1;
				info->upcase_table = (uint16_t*)malloc(info->cluster_size * clusters);
				dump_debug("Get: Up-case table: cluster %x, size: %x\n",
						dentry.dentry.upcase.FirstCluster,
						dentry.dentry.upcase.DataLength);
				get_clusters(info,
						info->upcase_table, dentry.dentry.upcase.FirstCluster, clusters);
				dump_notice("Upcase table (#%u):\n", dentry.dentry.upcase.FirstCluster);
				exfat_print_upcase_table(info);
				break;
			case DENTRY_VOLUME:
				dump_notice("volume Label: ");
				/* FIXME: VolumeLabel is Unicode string, But %c is ASCII code */
				for(byte = 0; byte < dentry.dentry.vol.CharacterCount; byte++) {
					dump_notice("%c", dentry.dentry.vol.VolumeLabel[byte]);
				}
				dump_notice("\n");
				break;
			case DENTRY_GUID:
				break;
			case DENTRY_UNUSED:
				goto out;
		}
	}

out:
	free(root);
	return ret;
}

/**
 * __exfat_traverse_directory - function interface to traverse all cluster
 * @info:          Target device information
 * @index:         index of the cluster want to check
 */
int exfat_traverse_directory(struct device_info *info, uint32_t index)
{
	info->root = (node2_t **)malloc(sizeof(node2_t *) * info->root_maxsize);
	return __exfat_traverse_directory(info, index, 0);
}

/**
 * __exfat_traverse_directory - function to traverse all cluster
 * @info:          Target device information
 * @index:         index of the cluster want to check
 * @count:         Directory depth count
 *
 * TODO: check FAT entry for discontinuous index
 */
static int __exfat_traverse_directory(struct device_info *info, uint32_t index, size_t count)
{
	int i;
	uint16_t attr = 0;
	uint32_t next_index;
	uint64_t c, len;
	size_t size = info->cluster_size;
	size_t entries = size / sizeof(struct exfat_dentry);
	void *clu, *clu_tmp;
	struct exfat_dentry d, next;

	clu = malloc(size);
	get_cluster(info, clu, index);
	if (count >= info->root_maxsize) {
		info->root_maxsize += DENTRY_LISTSIZE;
		node2_t **tmp = (node2_t **)realloc(info->root, sizeof(node2_t *) * info->root_maxsize);
		if (!tmp)
			/* Failed to expand area */
			return -1;
		info->root = tmp;
	}
	info->root_size = count + 1;
	info->root[count] = init_node2(index, 0);
	do {
		for(i = 0; i < entries; i++){
			d = ((struct exfat_dentry *)clu)[i];

			switch (d.EntryType) {
				case DENTRY_UNUSED:
					return 0;
				case DENTRY_BITMAP:
					c = d.dentry.bitmap.FirstCluster;
					len = d.dentry.bitmap.DataLength;
					dump_debug("Insert Bitmap table to %lu\n", c);
					break;
				case DENTRY_UPCASE:
					c = d.dentry.upcase.FirstCluster;
					len = d.dentry.upcase.DataLength;
					dump_debug("Insert Upcase table to %lu\n", c);
					break;
				case DENTRY_FILE:
					if (i + d.dentry.file.SecondaryCount > entries) {
						index = exfat_concat_cluster(info, index, clu, size);
						size += info->cluster_size;
						entries = size / sizeof(struct exfat_dentry);
					}

					next = ((struct exfat_dentry *)clu)[i + 1];
					if (next.EntryType != DENTRY_STREAM) {
						dump_warn("File should have stream entry, but This don't have.\n");
						return -1;
					}
					attr = d.dentry.file.FileAttributes;
					c = next.dentry.stream.FirstCluster;
					len = next.dentry.stream.DataLength;
					if (attr == ATTR_DIRECTORY) {
						insert_node2(info->root[count], c, len);
						__exfat_traverse_directory(info, c, count + 1);
					} else {
						insert_node2(info->root[count], c, len);
					}
					exfat_print_file_entry(info,
							d.dentry, next.dentry, d.dentry, d.dentry.file.SecondaryCount);
					i += d.dentry.file.SecondaryCount;
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
