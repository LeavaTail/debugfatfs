#include <stdio.h>
#include <stdbool.h>
#include "dumpexfat.h"

int exfat_get_root_dir(struct device_info *info, void *data)
{
	int i;
	for(i = 0;
			i < (((1 << info->cluster_shift) * info->sector_size) / sizeof(struct exfat_dentry));
			i++) {
		struct exfat_dentry root = ((struct exfat_dentry *)data)[i];
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
