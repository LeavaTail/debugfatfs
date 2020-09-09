#include <stdio.h>
#include <stdbool.h>
#include "debugfatfs.h"

/* Generic function prototype */
static uint32_t fat_concat_cluster(uint32_t, void **, size_t);

/* Boot sector function prototype */
static int fat_load_bootsec(struct fat_bootsec *);
static int fat_validate_bootsec(struct fat_bootsec *);
static int fat16_print_bootsec(struct fat_bootsec *);
static int fat32_print_bootsec(struct fat_bootsec *);
static int fat32_print_fsinfo(struct fat32_fsinfo *);

/* FAT-entry function prototype */
static int fat12_set_fat_entry(uint32_t, uint32_t);
static int fat16_set_fat_entry(uint32_t, uint32_t);
static int fat32_set_fat_entry(uint32_t, uint32_t);
static uint32_t fat12_get_fat_entry(uint32_t);
static uint32_t fat16_get_fat_entry(uint32_t);
static uint32_t fat32_get_fat_entry(uint32_t);

/* Directory chain function prototype */
static int fat_check_dchain(uint32_t);
static int fat_get_index(uint32_t);
static int fat_traverse_directory(uint32_t);

/* File function prototype */
static void fat_create_fileinfo(node2_t *, uint32_t, struct fat_dentry *, uint16_t *, size_t);
static void fat_convert_uniname(uint16_t *, uint64_t, unsigned char *);
static void fat_convert_unixtime(struct tm *, uint16_t, uint16_t, uint8_t);

/* Operations function prototype */
int fat_print_bootsec(void);
int fat_print_vollabel(void);
int fat_lookup(uint32_t, char *);
int fat_readdir(struct directory *, size_t, uint32_t);
int fat_reload_directory(uint32_t);
int fat_convert_character(const char *, size_t, char *);
int fat_clean_dchain(uint32_t);
int fat_set_fat_entry(uint32_t, uint32_t);
int fat_get_fat_entry(uint32_t, uint32_t *);
int fat_alloc_cluster(uint32_t);
int fat_release_cluster(uint32_t);
int fat_create(const char *, uint32_t, int);
int fat_remove(const char *, uint32_t, int);

static const struct operations fat_ops = {
	.statfs = fat_print_bootsec,
	.info = fat_print_vollabel,
	.lookup =  fat_lookup,
	.readdir = fat_readdir,
	.reload = fat_reload_directory,
	.convert = fat_convert_character,
	.clean = fat_clean_dchain,
	.setfat = fat_set_fat_entry,
	.getfat = fat_get_fat_entry,
	.alloc = fat_alloc_cluster,
	.release = fat_release_cluster,
	.create = fat_create,
	.remove = fat_remove,
};

/*************************************************************************************************/
/*                                                                                               */
/* GENERIC FUNCTION                                                                              */
/*                                                                                               */
/*************************************************************************************************/
/**
 * fat_concat_cluster   - Contatenate cluster @data with next_cluster
 * @clu:                  index of the cluster
 * @data:                 The cluster
 * @size:                 allocated size to store cluster data
 *
 * @retrun:               next cluster (@clu has next cluster)
 *                        0            (@clu doesn't have next cluster, or failed to realloc)
 */
static uint32_t fat_concat_cluster(uint32_t clu, void **data, size_t size)
{
	uint32_t ret;
	void *tmp;
	fat_get_fat_entry(clu, &ret);

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
 * fat_check_filesystem - Whether or not VFAT filesystem
 * @boot:                 boot sector pointer
 *
 * return:                1 (Image is FAT12/16/32 filesystem)
 *                        0 (Image isn't FAT12/16/32 filesystem)
 */
int fat_check_filesystem(struct pseudo_bootsec *boot)
{
	struct fat_bootsec *b = (struct fat_bootsec *)boot;
	struct fat_fileinfo *f;
	uint16_t RootDirSectors = ((b->BPB_RootEntCnt * 32) +
			(b->BPB_BytesPerSec - 1)) / b->BPB_BytesPerSec;
	uint32_t FATSz;
	uint32_t TotSec;
	uint32_t DataSec;
	uint32_t CountofClusters;

	if (!fat_validate_bootsec(b))
		return 0;

	if (b->BPB_FATSz16 != 0) {
		FATSz = b->BPB_FATSz16;
		TotSec = b->BPB_TotSec16;
	} else {
		FATSz = b->reserved_info.fat32_reserved_info.BPB_FATSz32;
		TotSec = b->BPB_TotSec32;
	}

	DataSec = TotSec - (b->BPB_RevdSecCnt + (b->BPB_NumFATs * FATSz) + RootDirSectors);
	CountofClusters = DataSec / b->BPB_SecPerClus;

	if (CountofClusters < FAT16_CLUSTERS - 1) {
		info.fstype = FAT12_FILESYSTEM;
	} else if (CountofClusters < FAT32_CLUSTERS - 1) {
		info.fstype = FAT16_FILESYSTEM;
	} else {
		info.fstype = FAT32_FILESYSTEM;
	}

	info.sector_size = b->BPB_BytesPerSec;
	info.cluster_size = b->BPB_SecPerClus * b->BPB_BytesPerSec;
	info.cluster_count = CountofClusters;
	info.fat_offset = b->BPB_RevdSecCnt;
	info.fat_length = b->BPB_NumFATs * FATSz;
	info.heap_offset = info.fat_offset + info.fat_length;
	if (info.fstype == FAT32_FILESYSTEM)
		info.root_offset = b->reserved_info.fat32_reserved_info.BPB_RootClus;

	f = malloc(sizeof(struct fat_fileinfo));
	strncpy((char *)f->name, "/", strlen("/") + 1);
	f->uniname = NULL;
	f->namelen = 1;
	f->datalen = 0;
	f->attr = ATTR_DIRECTORY;
	info.root[0] = init_node2(info.root_offset, f);
	info.ops = &fat_ops;
	return 1;
}

/*************************************************************************************************/
/*                                                                                               */
/* BOOT SECTOR FUNCTION                                                                          */
/*                                                                                               */
/*************************************************************************************************/
/**
 * fat_load_bootsec - load boot sector in FAT
 * @b:                boot sector pointer in FAT
 */
static int fat_load_bootsec(struct fat_bootsec *b)
{
	return get_sector(b, 0, 1);
}

/**
 * fat_validate_bootsec - check whether boot sector is vaild
 * @b:                    boot sector pointer in FAT
 */
static int fat_validate_bootsec(struct fat_bootsec *b)
{
	int ret = 1;
	uint8_t media = b->BPB_Media;
	uint16_t sector = b->BPB_BytesPerSec / SECSIZE;
	uint8_t cluster = b->BPB_SecPerClus;

	if (!b->BPB_RevdSecCnt) {
		pr_debug("invalid reserved sectors: 0x%x\n", b->BPB_RevdSecCnt);
		ret = 0;
	}

	if (!b->BPB_NumFATs) {
		pr_debug("invalid FAT structure: 0x%x\n", b->BPB_NumFATs);
		ret = 0;
	}

	if (media != 0xf0 && media < 0xF8) {
		pr_debug("invalid Media value: 0x%x\n", b->BPB_Media);
		ret = 0;
	}

	if (!is_power2(sector) || sector > 8) {
		pr_debug("invalid Sector size: %u\n", b->BPB_BytesPerSec);
		ret = 0;
	}

	if (!is_power2(cluster) || cluster > 128) {
		pr_debug("invalid Cluster size: %u\n", b->BPB_SecPerClus);
		ret = 0;
	}

	return ret;
}

/**
 * fat16_print_bootsec - print boot sector in FAT12/16
 * @b:                   boot sector pointer in FAT
 */
static int fat16_print_bootsec(struct fat_bootsec *b)
{
	int i;
	const char *type = (char *)b->reserved_info.fat16_reserved_info.BS_FilSysType;

	if (strncmp(type, "FAT", 3))
		pr_warn("BS_FilSysType is expected \"FAT     \", But this is %s\n", type);

	pr_msg("%-28s\t: ", "Volume ID");
	for (i = 0; i < VOLIDSIZE; i++)
		pr_msg("0x%x", b->reserved_info.fat16_reserved_info.BS_VolID[i]);
	pr_msg("\n");

	pr_msg("%-28s\t: ", "Volume Label");
	for (i = 0; i < VOLLABSIZE; i++)
		pr_msg("%c", b->reserved_info.fat16_reserved_info.BS_VolLab[i]);
	pr_msg("\n");
	return 0;
}

/**
 * fat32_print_bootsec - print boot sector in FAT32
 * @b:         boot sector pointer in FAT
 */
static int fat32_print_bootsec(struct fat_bootsec *b)
{
	int i;
	const char *type = (char *)b->reserved_info.fat32_reserved_info.BS_FilSysType;

	if (strncmp(type, "FAT32", 5))
		pr_warn("BS_FilSysType is expected \"FAT32   \", But this is %s\n", type);

	pr_msg("%-28s\t: ", "Volume ID");
	for (i = 0; i < VOLIDSIZE; i++)
		pr_msg("0x%x", b->reserved_info.fat32_reserved_info.BS_VolID[i]);
	pr_msg("\n");

	pr_msg("%-28s\t: ", "Volume Label");
	for (i = 0; i < VOLLABSIZE; i++)
		pr_msg("%c", b->reserved_info.fat32_reserved_info.BS_VolLab[i]);
	pr_msg("\n");

	pr_msg("%-28s\t: 0x%08x\n", "Sectors Per FAT",
			b->reserved_info.fat32_reserved_info.BPB_FATSz32);
	pr_msg("%-28s\t: 0x%08x (sector)\n", "The first sector of the Root",
			b->reserved_info.fat32_reserved_info.BPB_RootClus);
	pr_msg("%-28s\t: 0x%08x (sector)\n", "FSINFO sector",
			b->reserved_info.fat32_reserved_info.BPB_FSInfo);
	pr_msg("%-28s\t: 0x%08x (sector)\n", "Backup Boot sector",
			b->reserved_info.fat32_reserved_info.BPB_BkBootSec);
	return 0;
}

/**
 * fat32_print_fsinfo - print FSinfo Structure in FAT32
 * @fsi:                fsinfo pointer in FAT
 */
static int fat32_print_fsinfo(struct fat32_fsinfo *fsi)
{
	if ((fsi->FSI_LeadSig != 0x41615252) ||
			(fsi->FSI_StrucSig != 0x61417272) ||
			(fsi->FSI_TrailSig != 0xAA550000))
		pr_warn("FSinfo is expected specific sigunature, But this is difference.\n");

	pr_msg("%-28s\t: %10u (cluster)\n", "free cluster count", fsi->FSI_Free_Count);
	pr_msg("%-28s\t: %10u (cluster)\n", "first available cluster", fsi->FSI_Nxt_Free);
	return 0;
}

/*************************************************************************************************/
/*                                                                                               */
/* FAT-ENTRY FUNCTION                                                                            */
/*                                                                                               */
/*************************************************************************************************/
/**
 * fat12_set_fat_entry - Set FAT Entry to any cluster
 * @clu:                 index of the cluster want to check
 * @entry:               any cluster index
 *
 * @retrun:              0
 */
static int fat12_set_fat_entry(uint32_t clu, uint32_t entry)
{
	uint32_t FATOffset = clu + (clu / 2);
	uint32_t ThisFATEntOffset = FATOffset % info.sector_size;

	uint16_t *fat;
	fat = malloc(info.sector_size);
	get_sector(fat, info.fat_offset * info.sector_size, 1);
	if (clu % 2) {
		*(fat + ThisFATEntOffset) = (fat[ThisFATEntOffset] & 0x0F) | entry << 4;
		*(fat + ThisFATEntOffset + 1) = entry >> 4;
	} else {
		*(fat + ThisFATEntOffset) = entry;
		*(fat + ThisFATEntOffset + 1) = (fat[ThisFATEntOffset + 1] & 0xF0) | ((entry >> 8) & 0x0F);
	}
	set_sector(fat, info.fat_offset * info.sector_size, 1);
	free(fat);
	return 0;
}

/**
 * fat16_set_fat_entry - Set FAT Entry to any cluster
 * @clu:                 index of the cluster want to check
 * @entry:               any cluster index
 *
 * @retrun:              0
 */
static int fat16_set_fat_entry(uint32_t clu, uint32_t entry)
{
	uint32_t FATOffset = clu * sizeof(uint16_t);
	uint32_t ThisFATEntOffset = FATOffset % info.sector_size;

	uint16_t *fat;
	fat = malloc(info.sector_size);
	get_sector(fat, info.fat_offset * info.sector_size, 1);
	*(fat + ThisFATEntOffset) = (uint16_t)entry;
	set_sector(fat, info.fat_offset * info.sector_size, 1);
	free(fat);
	return 0;
}

/**
 * fat32_set_fat_entry - Set FAT Entry to any cluster
 * @clu:                 index of the cluster want to check
 * @entry:               any cluster index
 *
 * @retrun:              0
 */
static int fat32_set_fat_entry(uint32_t clu, uint32_t entry)
{
	uint32_t FATOffset = clu * sizeof(uint32_t);
	uint32_t ThisFATEntOffset = FATOffset % info.sector_size;

	uint32_t *fat;
	fat = malloc(info.sector_size);
	get_sector(fat, info.fat_offset * info.sector_size, 1);
	*(fat + ThisFATEntOffset) = entry & 0x0FFFFFFF;
	set_sector(fat, info.fat_offset * info.sector_size, 1);
	free(fat);
	return 0;
}

/**
 * fat12_get_fat_entry - Get cluster is continuous
 * @clu:                 index of the cluster want to check
 *
 * @retrun:              next cluster (@clu has next cluster)
 *                       0            (@clu doesn't have next cluster)
 */
static uint32_t fat12_get_fat_entry(uint32_t clu)
{
	uint32_t ret = 0;
	uint32_t FATOffset = clu + (clu / 2);
	uint32_t ThisFATEntOffset = FATOffset % info.sector_size;

	uint16_t *fat;
	fat = malloc(info.sector_size);
	get_sector(fat, info.fat_offset * info.sector_size, 1);
	if (clu % 2) {
		ret = (fat[ThisFATEntOffset] >> 4)
			| ((uint16_t)fat[ThisFATEntOffset + 1] << 4);
	} else {
		ret = fat[ThisFATEntOffset]
			| ((uint16_t)(fat[ThisFATEntOffset + 1] & 0x0F) << 8);
	}
	free(fat);
	return ret;
}

/**
 * fat16_get_fat_entry - Get cluster is continuous
 * @clu:                 index of the cluster want to check
 *
 * @retrun:              next cluster (@clu has next cluster)
 *                       0            (@clu doesn't have next cluster)
 */
static uint32_t fat16_get_fat_entry(uint32_t clu)
{
	uint32_t ret = 0;
	uint32_t FATOffset = clu * sizeof(uint16_t);
	uint32_t ThisFATEntOffset = FATOffset % info.sector_size;

	uint16_t *fat;
	fat = malloc(info.sector_size);
	get_sector(fat, info.fat_offset * info.sector_size, 1);
	ret = fat[ThisFATEntOffset];
	free(fat);
	return ret;
}

/**
 * fat32_get_fat_entry - Get cluster is continuous
 * @clu:                 index of the cluster want to check
 *
 * @retrun:              next cluster (@clu has next cluster)
 *                       0            (@clu doesn't have next cluster)
 */
static uint32_t fat32_get_fat_entry(uint32_t clu)
{
	uint32_t ret = 0;
	uint32_t FATOffset = clu * sizeof(uint32_t);
	uint32_t ThisFATEntOffset = FATOffset % info.sector_size;

	uint32_t *fat;
	fat = malloc(info.sector_size);
	get_sector(fat, info.fat_offset * info.sector_size, 1);
	ret = fat[ThisFATEntOffset] & 0x0FFFFFFF;
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
 * fat_print_dchain - print directory chain
 */
static void fat_print_dchain(void)
{
	int i;
	node2_t *tmp;
	struct fat_fileinfo *f;

	for (i = 0; i < info.root_size && info.root[i]; i++) {
		tmp = info.root[i];
		f = (struct fat_fileinfo *)info.root[i]->data;
		pr_msg("%-16s(%s) [%u] | ", f->name, f->uniname, tmp->index);
		while (tmp->next != NULL) {
			tmp = tmp->next;
			f = (struct fat_fileinfo *)tmp->data;
			pr_msg("%s(%s) [%u] ", f->name, f->uniname, tmp->index);
		}
		pr_msg("\n");
	}
	pr_msg("\n");
}
#endif
/**
 * fat_check_dchain - check whether @index has already loaded
 * @clu:              index of the cluster
 *
 * @retrun:           1 (@clu has loaded)
 *                    0 (@clu hasn't loaded)
 */
static int fat_check_dchain(uint32_t clu)
{
	int i;
	for (i = 0; info.root[i] && i < info.root_size; i++) {
		if (info.root[i]->index == clu)
			return 1;
	}
	return 0;
}

/**
 * fat_get_index - get directory chain index by argument
 * @clu:           index of the cluster
 *
 * @return:        directory chain index
 *                 Start of unused area (if doesn't lookup directory cache)
 */
static int fat_get_index(uint32_t clu)
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
 * fat_traverse_directory - function to traverse one directory
 * @clu:                    index of the cluster want to check
 *
 * @return                  0 (success)
 *                         -1 (failed to read)
 */
static int fat_traverse_directory(uint32_t clu)
{
	int i, j;
	uint8_t ord = 0, attr = 0;
	uint16_t uniname[MAX_NAME_LENGTH] = {0};
	size_t index = fat_get_index(clu);
	struct fat_fileinfo *f = (struct fat_fileinfo *)info.root[index]->data;
	size_t size = info.cluster_size;
	size_t entries = size / sizeof(struct fat_dentry);
	size_t namelen = 0;
	void *data;
	struct fat_dentry d;

	if (f->datalen > 0) {
		pr_debug("Directory %s was already traversed.\n", f->name);
		return 0;
	}
	data = malloc(size);
	get_cluster(data, clu);
	do {
		for (i = 0; i < entries; i++) {
			namelen = 0;
			d = ((struct fat_dentry *)data)[i];
			attr = d.dentry.lfn.LDIR_Attr;
			ord = d.dentry.lfn.LDIR_Ord;
			/* Empty entry */
			if (ord == 0x00)
				goto out;
			if (ord == 0xe5)
				continue;
			/* First entry should be checked */
			switch (attr) {
				case ATTR_VOLUME_ID:
					info.vol_length = 11;
					info.vol_label = calloc(11 + 1, sizeof(unsigned char));
					memcpy(info.vol_label, d.dentry.dir.DIR_Name,
							sizeof(unsigned char) * 11);
					continue;
				case ATTR_LONG_FILE_NAME:
					ord &= ~LAST_LONG_ENTRY;
					if (i + ord >= entries) {
						clu = fat_concat_cluster(clu, &data, size);
						size += info.cluster_size;
						entries = size / sizeof(struct exfat_dentry);
					}
					for (j = 0; j < ord; j++) {
						memcpy(uniname + j * LONGNAME_MAX,
								(((struct fat_dentry *)data)[i + ord - j - 1]).dentry.lfn.LDIR_Name1,
								5 * sizeof(uint16_t));
						memcpy(uniname + j * LONGNAME_MAX + 5,
								(((struct fat_dentry *)data)[i + ord - j - 1]).dentry.lfn.LDIR_Name2,
								6 * sizeof(uint16_t));
						memcpy(uniname + j * LONGNAME_MAX + 11,
								(((struct fat_dentry *)data)[i + ord - j - 1]).dentry.lfn.LDIR_Name3,
								2 * sizeof(uint16_t));
						namelen += LONGNAME_MAX;
					}
					d = ((struct fat_dentry *)data)[i + ord];
					i += ord;
					break;
				default:
					d = ((struct fat_dentry *)data)[i];
					break;
			}
			fat_create_fileinfo(info.root[index], clu, &d, uniname, namelen);
		}
		clu = fat_concat_cluster(clu, &data, size);
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
 * fat_create_file_entry - Create file infomarion
 * @head:                  Directory chain head
 * @clu:                   parent Directory cluster index
 * @file:                  file dentry
 * @uniname:               Long File name
 * @namelen:               Long File name length
 */
static void fat_create_fileinfo(node2_t *head, uint32_t clu,
		struct fat_dentry *file, uint16_t *uniname, size_t namelen)
{
	int index, next_clu = 0;
	struct fat_fileinfo *f;

	next_clu |= (file->dentry.dir.DIR_FstClusHI << 16) | file->dentry.dir.DIR_FstClusLO;
	f = malloc(sizeof(struct fat_fileinfo));
	memset(f->name, '\0', 12);
	strncpy((char *)f->name, (char *)file->dentry.dir.DIR_Name, 11);
	f->uniname = malloc(namelen * UTF8_MAX_CHARSIZE + 1);
	memset(f->uniname, '\0', namelen * UTF8_MAX_CHARSIZE + 1);
	fat_convert_uniname(uniname, namelen, f->uniname);

	f->namelen = strlen((char *)f->uniname);
	f->datalen = file->dentry.dir.DIR_FileSize;
	f->attr = file->dentry.dir.DIR_Attr;

	fat_convert_unixtime(&f->ctime, file->dentry.dir.DIR_CrtDate,
			file->dentry.dir.DIR_CrtTime,
			file->dentry.dir.DIR_CrtTimeTenth);
	fat_convert_unixtime(&f->mtime, file->dentry.dir.DIR_WrtDate,
			file->dentry.dir.DIR_WrtTime,
			0);
	fat_convert_unixtime(&f->atime, file->dentry.dir.DIR_LstAccDate,
			0,
			0);
	append_node2(head, next_clu, f);
	 ((struct fat_fileinfo *)(head->data))->datalen++;

	/* If this entry is Directory, prepare to create next chain */
	if ((f->attr & ATTR_DIRECTORY) && (!fat_check_dchain(next_clu))) {
		struct fat_fileinfo *d = malloc(sizeof(struct fat_fileinfo));
		strncpy((char *)d->name, (char *)f->name, 11);
		d->uniname = malloc(f->namelen + 1);
		strncpy((char *)d->uniname, (char *)f->uniname, f->namelen + 1);
		d->namelen = namelen;
		d->datalen = 0;
		d->attr = file->dentry.dir.DIR_Attr;

		index = fat_get_index(next_clu);
		info.root[index] = init_node2(next_clu, head);
	}
}

/**
 * fat_convert_uniname - function to get filename
 * @uniname:             filename dentry in UTF-16
 * @name_len:            filename length
 * @name:                filename in UTF-8 (output)
 */
static void fat_convert_uniname(uint16_t *uniname, uint64_t name_len, unsigned char *name)
{
	utf16s_to_utf8s(uniname, name_len, name);
}

/**
 * exfat_convert_unixname - function to get timestamp in file
 * @t:                      output pointer
 * @date:                   Date Field in File Directory Entry
 * @time:                   Timestamp Field in File Directory Entry
 * @subsec:                 10msincrement Field in File Directory Entry
 */
static void fat_convert_unixtime(struct tm *t, uint16_t date, uint16_t time, uint8_t subsec)
{
	t->tm_year = (date >> FAT_YEAR) & 0x7f;
	t->tm_mon  = (date >> FAT_MONTH) & 0x0f;
	t->tm_mday = (date >> FAT_DAY) & 0x1f;
	t->tm_hour = (time >> EXFAT_HOUR) & 0x0f;
	t->tm_min  = (time >> EXFAT_MINUTE) & 0x3f;
	t->tm_sec  = (time & 0x1f) * 2;
	t->tm_sec += subsec / 100;
}

/*************************************************************************************************/
/*                                                                                               */
/* OPERATIONS FUNCTION                                                                           */
/*                                                                                               */
/*************************************************************************************************/
/**
 * fat_print_bootsec - print boot sector in FAT12/16/32
 *
 * @return             0 (success)
 *                    -1 (this image isn't FAT filesystem)
 */
int fat_print_bootsec(void)
{
	int ret = 0;
	struct fat_bootsec *b = malloc(sizeof(struct fat_bootsec));

	fat_load_bootsec(b);
	pr_msg("%-28s\t: %10u (byte)\n", "Bytes per Sector", b->BPB_BytesPerSec);
	pr_msg("%-28s\t: %10u (sector)\n", "Sectors per cluster", b->BPB_SecPerClus);
	pr_msg("%-28s\t: %10u (sector)\n", "Reserved Sector", b->BPB_RevdSecCnt);
	pr_msg("%-28s\t: %10u\n", "FAT count", b->BPB_NumFATs);
	pr_msg("%-28s\t: %10u\n", "Root Directory entry count", b->BPB_RootEntCnt);
	pr_msg("%-28s\t: %10u (sector)\n", "Sector count in Volume", b->BPB_TotSec16);

	switch (info.fstype) {
		case FAT12_FILESYSTEM:
			/* FALLTHROUGH */
		case FAT16_FILESYSTEM:
			fat16_print_bootsec(b);
			break;
		case FAT32_FILESYSTEM:
			{
				void *fsinfo;
				fsinfo = malloc(info.sector_size);
				fat32_print_bootsec(b);
				get_sector(fsinfo,
						b->reserved_info.fat32_reserved_info.BPB_FSInfo * info.sector_size, 1);
				fat32_print_fsinfo(fsinfo);
				free(fsinfo);
				break;
			}
		default:
			pr_err("Expected FAT filesystem, But this is not FAT filesystem.\n");
			ret = -1;
	}
	free(b);
	return ret;
}

/**
 * fat_print_fsinfo - print filesystem information in FAT
 *
 * @return            0 (success)
 */
int fat_print_vollabel(void)
{
	pr_msg("volume Label: ");
	pr_msg("%s\n", (char *)info.vol_label);
	return 0;
}

/**
 * fat_lookup - function interface to lookup pathname
 * @clu:        directory cluster index
 * @name:       file name
 *
 * @return:     cluster index
 *              0 (Not found)
 */
int fat_lookup(uint32_t clu, char *name)
{
	int index, i = 0, depth = 0;
	bool found = false;
	char *path[MAX_NAME_LENGTH] = {};
	char fullpath[PATHNAME_MAX + 1] = {};
	node2_t *tmp;
	struct fat_fileinfo *f;

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
		index = fat_get_index(clu);
		f = (struct fat_fileinfo *)info.root[index]->data;
		if ((!info.root[index]) || (f->namelen == 0)) {
			pr_debug("Directory hasn't load yet, or This Directory doesn't exist in filesystem.\n");
			fat_traverse_directory(clu);
			index = fat_get_index(clu);
			if (!info.root[index]) {
				pr_warn("This Directory doesn't exist in filesystem.\n");
				return 0;
			}
		}

		tmp = info.root[index];
		while (tmp->next != NULL) {
			tmp = tmp->next;
			f = (struct fat_fileinfo *)tmp->data;
			if (f->namelen) {
				if (!strncmp(path[i], (char *)f->uniname, strlen(path[i]))) {
					clu = tmp->index;
					found = true;
					break;
				}
			} else {
				if (!strncmp(path[i], (char *)f->name, strlen(path[i]))) {
					clu = tmp->index;
					found = true;
					break;
				}
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
 * fat_readdir - function interface to read a directory
 * @dir:         directory entry list (Output)
 * @count:       Allocated space in @dir
 * @clu:         Directory cluster index
 *
 * @return       >= 0 (Number of entry)
 *                < 0 (Number of entry can't read)
 */
int fat_readdir(struct directory *dir, size_t count, uint32_t clu)
{
	int i;
	node2_t *tmp;
	struct fat_fileinfo *f;

	fat_traverse_directory(clu);
	i = fat_get_index(clu);
	tmp = info.root[i];

	for (i = 0; i < count && tmp->next != NULL; i++) {
		tmp = tmp->next;
		f = (struct fat_fileinfo *)(tmp->data);
		if (!f->namelen) {
			dir[i].name = calloc(11 + 1, sizeof(unsigned char));
			strncpy((char *)dir[i].name, (char *)f->name, sizeof(unsigned char) * 11);
			dir[i].namelen = 11;
		} else {
			dir[i].name = malloc(sizeof(uint32_t) * (f->namelen + 1));
			strncpy((char *)dir[i].name, (char *)f->uniname, sizeof(uint32_t) * (f->namelen + 1));
			dir[i].namelen = f->namelen;
		}
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
 * fat_reload_directory - function interface to read directories
 * @clu                   cluster index
 *
 * @return                0 (success)
 *                       -1 (failed to read)
 */
int fat_reload_directory(uint32_t clu)
{
	int index = fat_get_index(clu);
	struct fat_fileinfo *f = NULL;

	fat_clean_dchain(index);
	f = ((struct fat_fileinfo *)(info.root[index])->data);
	f->datalen = 0;
	return fat_traverse_directory(clu);
}

/**
 * fat_convert_character - Convert character by upcase-table
 * @src:                   Target characters in UTF-8
 * @len:                   Target characters length
 * @dist:                  convert result in UTF-8 (Output)
 *
 * return:                 0 (succeeded in obtaining filesystem)
 *                        -1 (failed)
 */
int fat_convert_character(const char *src, size_t len, char *dist)
{
	pr_warn("FAT: convert function isn't implemented.\n");
	return 0;
}

/**
 * fat_clean_dchain - function to clean opeartions
 * @index:            directory chain index
 *
 * @return            0 (success)
 *                   -1 (already released)
 */
int fat_clean_dchain(uint32_t index)
{
	node2_t *tmp;
	struct fat_fileinfo *f;

	if ((!info.root[index])) {
		pr_warn("index %d was already released.\n", index);
		return -1;
	}

	tmp = info.root[index];

	while (tmp->next != NULL) {
		tmp = tmp->next;
		f = (struct fat_fileinfo *)tmp->data;
		free(f->uniname);
		f->uniname = NULL;
	}
	free_list2(info.root[index]);
	return 0;
}

/**
 * fat_set_fat_entry - Set FAT Entry to any cluster
 * @clu:               index of the cluster want to check
 * @entry:             any cluster index
 *
 * @retrun:            0
 *                    -1 (invalid image)
 */
int fat_set_fat_entry(uint32_t clu, uint32_t entry)
{
	int ret = 0;

	switch (info.fstype) {
		case FAT12_FILESYSTEM:
			fat12_set_fat_entry(clu, entry);
			break;
		case FAT16_FILESYSTEM:
			fat16_set_fat_entry(clu, entry);
			break;
		case FAT32_FILESYSTEM:
			fat32_set_fat_entry(clu, entry);
			break;
		default:
			pr_err("Expected FAT filesystem, But this is not FAT filesystem.\n");
			ret = -1;
	}
	return ret;
}

/**
 * fat_get_fat_entry - Get cluster is continuous
 * @clu:               index of the cluster want to check
 * @entry:             any cluster index
 *
 * @return             0
 *                    -1 (invalid image)
 */
int fat_get_fat_entry(uint32_t clu, uint32_t *entry)
{
	int ret = 0;

	switch (info.fstype) {
		case FAT12_FILESYSTEM:
			*entry = fat12_get_fat_entry(clu);
			break;
		case FAT16_FILESYSTEM:
			*entry = fat16_get_fat_entry(clu);
			break;
		case FAT32_FILESYSTEM:
			*entry = fat32_get_fat_entry(clu);
			break;
		default:
			pr_err("Expected FAT filesystem, But this is not FAT filesystem.\n");
			ret = -1;
	}
	return ret;
}

/**
 * fat_alloc_cluster - function to allocate cluster
 * @clu:               cluster index
 *
 * @return             0 (success)
 */
int fat_alloc_cluster(uint32_t clu)
{
	pr_warn("FAT: alloc function isn't implemented.\n");
	return 0;
}

/**
 * fat_release_cluster - function to release cluster
 * @clu:                 cluster index
 *
 * @return               0 (success)
 *                      -1 (failed)
 */
int fat_release_cluster(uint32_t clu)
{
	pr_warn("FAT: release function isn't implemented.\n");
	return 0;
}

/**
 * fat_create - function interface to create entry
 * @name:       Filename in UTF-8
 * @index:      Current Directory Index
 * @opt:        create option
 *
 * @return      0 (Success)
 */
int fat_create(const char *name, uint32_t clu, int opt)
{
	pr_warn("FAT: create function isn't implemented.\n");
	return 0;
}

/**
 * fat_remove - function interface to remove entry
 * @name:       Filename in UTF-8
 * @index:      Current Directory Index
 * @opt:        create option
 *
 * @return      0 (Success)
 */
int fat_remove(const char *name, uint32_t clu, int opt)
{
	pr_warn("FAT: remove function isn't implemented.\n");
	return 0;
}
