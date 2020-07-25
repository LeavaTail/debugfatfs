#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include "dumpexfat.h"

static void exfat_print_allocation_bitmap(struct device_info *info, uint32_t index)
{
	dump_notice("Allocation Bitmap (#%u):\n", index);
	node_t *node = info->chain_head;
	while (node->next != NULL) {
		node = node->next;
		dump_notice("%u ", node->x);
	}
	dump_notice("\n");
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
	int i;
	void *data;
	info->chain_head = init_node();
	for(i = 0;
			i < (((1 << info->cluster_shift) * info->sector_size) / sizeof(struct exfat_dentry));
			i++) {
		struct exfat_dentry dentry = ((struct exfat_dentry *)root)[i];
		switch (dentry.EntryType) {
			case DENTRY_BITMAP:
				dump_debug("Get: allocation table: cluster %x, size: %lx\n",
						dentry.dentry.bitmap.FirstCluster,
						dentry.dentry.bitmap.DataLength);
				data = get_cluster(info, dentry.dentry.bitmap.FirstCluster);
				exfat_create_allocation_chain(info, data);
				free(data);
				exfat_print_allocation_bitmap(info, dentry.dentry.bitmap.FirstCluster);
				break;
			case DENTRY_UPCASE:
				break;
			case DENTRY_VOLUME:
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
