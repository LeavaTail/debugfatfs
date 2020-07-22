#include <stdio.h>
#include <stdbool.h>
#include "dumpexfat.h"

int fat_show_boot_sec(struct device_info *info, struct fat_bootsec *b)
{
	fprintf(stdout, "%-28s\t: %8u (byte)\n", "Bytes per Sector", b->BPB_BytesPerSec);
	fprintf(stdout, "%-28s\t: %8u (sector)\n", "Sectors per cluster", b->BPB_SecPerClus);
	fprintf(stdout, "%-28s\t: %8u (sector)\n", "Reserved Sector", b->BPB_RevdSecCnt);
	fprintf(stdout, "%-28s\t: %8u\n", "FAT count", b->BPB_NumFATs);
	fprintf(stdout, "%-28s\t: %8u\n", "Root Directory entry count", b->BPB_RootEntCnt);
	fprintf(stdout, "%-28s\t: %8u (sector)\n", "Sector count in Volume", b->BPB_TotSec16);

	if (b->BPB_FATSz16) {
		info->fstype = FAT12_FILESYSTEM;
	} else {
		info->fstype = FAT32_FILESYSTEM;
	}
	return 0;
}
