#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include "dumpexfat.h"

static void exfat_print_allocation_bitmap(struct device_info *info)
{
	node_t *node = info->chain_head;
	while (node->next != NULL) {
		node = node->next;
		dump_notice("%u ", node->x);
	}
	dump_notice("\n");
}

static void exfat_print_upcase_table(struct device_info *info)
{
	int byte, offset;
	size_t uni_count = 0x10 / sizeof(uint16_t);
	size_t length = info->upcase_size;

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

static void exfat_load_timestamp(struct tm *t, uint32_t time, uint8_t subsec, uint8_t tz)
{
	int8_t offset = tz * 15;
	t->tm_year = (time >> EXFAT_YEAR) & 0x7f;
	t->tm_mon  = (time >> EXFAT_MONTH) & 0x0f;
	t->tm_mday = (time >> EXFAT_DAY) & 0x1f;
	t->tm_hour = (time >> EXFAT_HOUR) & 0x0f;
	t->tm_min  = (time >> EXFAT_MINUTE) & 0x3f;
	t->tm_sec  = (time & 0x1f) * 2;
	t->tm_sec += subsec / 100;
	dump_debug("Time: %d-%02d-%02d %02d:%02d:%02d.%02d (UTC + %02d:%02d)\n",
		1980 + t->tm_year, t->tm_mon, t->tm_mday,
		t->tm_hour, t->tm_min,
		t->tm_sec,
		subsec % 100,
		offset / 60,
		offset % 60);
}

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
				uint8_t clu = (i * CHAR_BIT) + bit + EXFAT_FIRST_CLUSTER;
				append_node(info->chain_head, clu);
			}
		}
	}
	return 0;
}

static bool exfat_check_allocation_cluster(struct device_info *info, uint32_t index)
{
	node_t *node = search_node(info->chain_head, index);
	if (node)
		return true;
	return false;
}

int exfat_load_root_dentry(struct device_info *info, void *root)
{
	int i, byte;
	for(i = 0;
			i < (((1 << info->cluster_shift) * info->sector_size) / sizeof(struct exfat_dentry));
			i++) {
		struct exfat_dentry dentry = ((struct exfat_dentry *)root)[i];
		switch (dentry.EntryType) {
			case DENTRY_BITMAP:
				dump_debug("Get: allocation table: cluster %x, size: %lx\n",
						dentry.dentry.bitmap.FirstCluster,
						dentry.dentry.bitmap.DataLength);
				info->chain_head = init_node();
				void *data = get_cluster(info, dentry.dentry.bitmap.FirstCluster);
				exfat_create_allocation_chain(info, data);
				free(data);
				dump_notice("Allocation Bitmap (#%u):\n", dentry.dentry.bitmap.FirstCluster);
				exfat_print_allocation_bitmap(info);
				break;
			case DENTRY_UPCASE:
				info->upcase_size = dentry.dentry.upcase.DataLength;
				size_t clusize = 1 << info->cluster_shift * info->sector_size;
				size_t clusters = (clusize / info->upcase_size) + 1;
				info->upcase_table = (uint16_t*)malloc(clusize * clusters);
				dump_debug("Get: Up-case table: cluster %x, size: %x\n",
						dentry.dentry.upcase.FirstCluster,
						dentry.dentry.upcase.DataLength);
				info->upcase_table = get_clusters(info, dentry.dentry.upcase.FirstCluster, clusters);
				dump_notice("Allocation Bitmap (#%u):\n", dentry.dentry.upcase.FirstCluster);
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
	return 0;
}

int exfat_show_boot_sec(struct device_info *info, struct exfat_bootsec *b)
{
	dump_info("%-28s\t: %8lx (sector)\n", "media-relative sector offset", b->PartitionOffset);
	dump_info("%-28s\t: %8x (sector)\n", "Offset of the First FAT", b->FatOffset);
	dump_info("%-28s\t: %8u (sector)\n", "Length of FAT table", b->FatLength);
	dump_info("%-28s\t: %8x (sector)\n", "Offset of the Cluster Heap", b->ClusterHeapOffset);
	dump_info("%-28s\t: %8u (cluster)\n", "The number of clusters", b->ClusterCount);
	dump_info("%-28s\t: %8u (cluster)\n", "The first cluster of the root", b->FirstClusterOfRootDirectory);

	info->fat_offset = b->FatOffset;
	info->heap_offset = b->ClusterHeapOffset;
	info->root_offset = b->FirstClusterOfRootDirectory;
	info->sector_size  = 1 << b->BytesPerSectorShift;
	info->cluster_shift = b->SectorsPerClusterShift;
	info->cluster_count = b->ClusterCount;
	info->fat_length = b->NumberOfFats * b->FatLength * info->sector_size;

	dump_notice("%-28s\t: %8lu (sector)\n", "Size of exFAT volumes", b->VolumeLength);
	dump_notice("%-28s\t: %8lu (byte)\n", "Bytes per sector", info->sector_size);
	dump_notice("%-28s\t: %8lu (byte)\n", "Bytes per cluster", info->sector_size * (1 << info->cluster_shift));

	dump_notice("%-28s\t: %8u\n", "The number of FATs", b->NumberOfFats);
	dump_notice("%-28s\t: %8u (%%)\n", "The percentage of clusters", b->PercentInUse);
	dump_notice("\n");

	return 0;
}

int exfat_print_cluster(struct device_info *info, uint32_t index)
{
	if (!exfat_check_allocation_cluster(info, index) && !info->force) {
		dump_err("cluster %u is not allocated.\n", index);
		return -1;
	}

	void *data = get_cluster(info, index);
	if (!data)
		return -1;

	dump_notice("Cluster #%u:\n", index);
	size_t size = ((1 << info->cluster_shift) * info->sector_size);
	hexdump(output, data, size);
	return 0;
}
