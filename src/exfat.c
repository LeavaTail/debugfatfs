#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include "dumpexfat.h"

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

int exfat_get_allocation_bitmap(struct device_info *info, void *root)
{
	int i;
	info->chain_head = init_node();
	for(i = 0;
			i < (((1 << info->cluster_shift) * info->sector_size) / sizeof(struct exfat_dentry));
			i++) {
		struct exfat_dentry dentry = ((struct exfat_dentry *)root)[i];

		if (dentry.EntryType == DENTRY_BITMAP) {
			dump_debug("Get: allocation table: cluster %x, size: %lx\n",
					dentry.dentry.bitmap.FirstCluster,
					dentry.dentry.bitmap.DataLength);
			void *allocation = get_cluster(info, dentry.dentry.bitmap.FirstCluster);
			exfat_create_allocation_chain(info, allocation);
			free(allocation);
			return 0;
		}
	}

	return 0;
}

int exfat_show_boot_sec(struct device_info *info, struct exfat_bootsec *b)
{
	FILE* fp = info->out;

	if (print_level >= DUMP_INFO) {
		fprintf(fp, "%-28s\t: %8lx (sector)\n", "media-relative sector offset", b->PartitionOffset);
		fprintf(fp, "%-28s\t: %8x (sector)\n", "Offset of the First FAT", b->FatOffset);
		fprintf(fp, "%-28s\t: %8u (sector)\n", "Length of FAT table", b->FatLength);
		fprintf(fp, "%-28s\t: %8x (sector)\n", "Offset of the Cluster Heap", b->ClusterHeapOffset);
		fprintf(fp, "%-28s\t: %8u (cluster)\n", "The number of clusters", b->ClusterCount);
		fprintf(fp, "%-28s\t: %8u (cluster)\n", "The first cluster of the root", b->FirstClusterOfRootDirectory);
	}

	info->fat_offset = b->FatOffset;
	info->heap_offset = b->ClusterHeapOffset;
	info->root_offset = b->FirstClusterOfRootDirectory;
	info->sector_size  = 1 << b->BytesPerSectorShift;
	info->cluster_shift = b->SectorsPerClusterShift;
	info->cluster_count = b->ClusterCount;
	info->fat_length = b->NumberOfFats * b->FatLength * info->sector_size;

	fprintf(fp, "%-28s\t: %8lu (sector)\n", "Size of exFAT volumes", b->VolumeLength);
	fprintf(fp, "%-28s\t: %8lu (byte)\n", "Bytes per sector", info->sector_size);
	fprintf(fp, "%-28s\t: %8lu (byte)\n", "Bytes per cluster", info->sector_size * (1 << info->cluster_shift));

	fprintf(fp, "%-28s\t: %8u\n", "The number of FATs", b->NumberOfFats);
	fprintf(fp, "%-28s\t: %8u (%%)\n", "The percentage of clusters", b->PercentInUse);

	return 0;
}

int exfat_print_cluster(struct device_info *info, uint32_t index)
{
	void *data = get_cluster(info, index);
	if (data) {
		fprintf(info->out, "Cluster #%u:\n", index);
		size_t size = ((1 << info->cluster_shift) * info->sector_size);
		hexdump(info->out, data, size);	
		return 0;
	}
	return -1;
}
