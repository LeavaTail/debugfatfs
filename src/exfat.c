#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include "dumpexfat.h"

/* Print function prototype */
static void exfat_print_allocation_bitmap(void);
static void exfat_print_upcase_table(void);
static void exfat_print_volume_label(uint16_t *, int);
static void exfat_print_directory_chain(void);

/* Load function prototype */
static int exfat_load_boot_sec(struct exfat_bootsec *);
static int exfat_create_allocation_chain(void *);
static void exfat_load_filename(uint16_t *, uint64_t, unsigned char *);
static void exfat_load_timestamp(struct tm *, char *,
		uint32_t, uint8_t, uint8_t);

/* Search function prototype */
static int exfat_get_dirindex(uint32_t);

/* Check function prototype */
static bool exfat_check_allocation_cluster(uint32_t);
static uint32_t exfat_check_fatentry(uint32_t);
static int exfat_check_exist_directory(uint32_t);

/* Create function prototype */
static uint32_t exfat_concat_cluster(uint32_t, void **, size_t);
static void exfat_create_fileinfo(node2_t *, uint32_t, struct exfat_dentry *, struct exfat_dentry *, uint16_t *);

/**
 * exfat_print_boot_sec - print boot sector in exFAT
 * @b:          boot sector pointer in exFAT
 *
 * @return        0 (success)
 */
int exfat_print_boot_sec(void)
{
	struct exfat_bootsec *b = malloc(sizeof(struct exfat_bootsec));

	exfat_load_boot_sec(b);
	pr_msg("%-28s\t: %8lx (sector)\n", "media-relative sector offset",
			b->PartitionOffset);
	pr_msg("%-28s\t: %8x (sector)\n", "Offset of the First FAT",
			b->FatOffset);
	pr_msg("%-28s\t: %8u (sector)\n", "Length of FAT table",
			b->FatLength);
	pr_msg("%-28s\t: %8x (sector)\n", "Offset of the Cluster Heap",
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
 * exfat_print_allocation_bitmap - print allocation bitmap
 */
static void exfat_print_allocation_bitmap(void)
{
	node_t *node = info.chain_head;
	while (node->next != NULL) {
		node = node->next;
		pr_info("%u ", node->x);
	}
	pr_info("\n");
}

/**
 * exfat_print_upcase-table - print upcase table
 */
static void exfat_print_upcase_table(void)
{
	int byte, offset;
	size_t uni_count = 0x10 / sizeof(uint16_t);
	size_t length = info.upcase_size;

	/* Usually, we don't want to display raw upcase table */
	if (print_level < PRINT_INFO) {
		pr_msg("Upcase-table was skipped.\n");
		return;
	}

	/* Output table header */
	pr_info("Offset  ");
	for (byte = 0; byte < uni_count; byte++)
		pr_info("  +%u ",byte);
	pr_info("\n");

	/* Output Table contents */
	for (offset = 0; offset < length / uni_count; offset++) {
		pr_info("%04lxh:  ", offset * 0x10 / sizeof(uint16_t));
		for (byte = 0; byte < uni_count; byte++) {
			pr_info("%04x ", info.upcase_table[offset * uni_count + byte]);
		}
		pr_info("\n");
	}
}

/**
 * exfat_print_volume_label - print volume label
 * @src:        volume label in UTF16
 * @len:        volume label length
 */
static void exfat_print_volume_label(uint16_t *src, int len)
{
	unsigned char *name;

	name = malloc(len * sizeof(uint16_t) + 1);
	memset(name, '\0', len * sizeof(uint16_t) + 1);
	utf16s_to_utf8s(src, len, name);
	printf("%s\n", name);
	free(name);
}

/**
 * exfat_print_directory_chain - print directory chain
 */
static void exfat_print_directory_chain(void)
{
	int i;
	node2_t *tmp;
	struct exfat_fileinfo *f;
	struct exfat_dirinfo *d;

	for (i = 0; i < info.root_size && info.root[i]; i++) {
		tmp = info.root[i];
		d = (struct exfat_dirinfo *)info.root[i]->data;
		pr_msg("%-16s(%u) | ", d->name, tmp->index);
		while (tmp->next != NULL) {
			tmp = tmp->next;
			f = (struct exfat_fileinfo *)tmp->data;
			pr_msg("%s(%u) ", f->name, tmp->index);
		}
		pr_msg("\n");
	}
	pr_msg("\n");
}

/**
 * exfat_show_boot_sec - function to print any cluster
 * @index:      cluster index to display
 *
 * @return        0 (success)
 *               -1 (cluster isn't allocated)
 */
int exfat_print_cluster(uint32_t index)
{
	if (!exfat_check_allocation_cluster(index) && !(info.attr & OPTION_FORCE)) {
		pr_err("cluster %u is not allocated.\n", index);
		return -1;
	}

	void *data;
	data = malloc(info.cluster_size);
	get_cluster(data, index);
	pr_msg("Cluster #%u:\n", index);
	hexdump(output, data, info.cluster_size);
	free(data);
	return 0;
}

/**
 * exfat_load_boot_sec - load boot sector
 * @b:          boot sector pointer in exFAT
 *
 * @return        0 (success)
 *               -1 (failed to read)
 */
static int exfat_load_boot_sec(struct exfat_bootsec *b)
{
	return get_sector(b, 0, 1);
}

/**
 * exfat_create_allocation_chain - function to get cluster chain
 * @bitmap:       pointer to Bitmap table
 *
 * @return        0 (success)
 */
static int exfat_create_allocation_chain(void *bitmap)
{
	int i, bit;
	uint8_t entry;
	for (i = 0; i < (info.cluster_count / CHAR_BIT); i++) {
		entry = ((uint8_t *)bitmap)[i];
		if (!entry)
			continue;

		for (bit = 0; bit < CHAR_BIT; bit++, entry >>= 1) {
			if (entry & 0x01) {
				uint64_t clu = (i * CHAR_BIT) + bit + EXFAT_FIRST_CLUSTER;
				append_node(info.chain_head, clu);
			}
		}
	}
	return 0;
}

/**
 * exfat_clean_directory_chain - function to clean opeartions
 * @index:                       directory chain index
 *
 * @return        0 (success)
 *               -1 (already released)
 */
int exfat_clean_directory_chain(uint32_t index)
{
	node2_t *tmp;
	struct exfat_dirinfo *d;
	struct exfat_fileinfo *f;

	if ((!info.root[index])) {
		pr_warn("index %d was already released.\n", index);
		return -1;
	}

	tmp = info.root[index];
	d = (struct exfat_dirinfo *)tmp->data;
	free(d->name);
	d->name = NULL;

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
 * exfat_load_filename - function to get filename
 * @uniname:        filename dentry in UTF-16
 * @name_len:       filename length
 * @name:           filename in UTF-8 (output)
 */
static void exfat_load_filename(uint16_t *uniname, uint64_t name_len, unsigned char *name)
{
	utf16s_to_utf8s(uniname, name_len, name);
}

/**
 * exfat_load_timestamp - function to get timestamp in file
 * @t:              output pointer
 * @str             additional any messages
 * @time:           Timestamp Field in File Directory Entry
 * @subsec:         10msincrement Field in File Directory Entry
 * @tz:             UtcOffset in File Directory Entry
 */
static void exfat_load_timestamp(struct tm *t, char *str,
		uint32_t time, uint8_t subsec, uint8_t tz)
{
	t->tm_year = (time >> EXFAT_YEAR) & 0x7f;
	t->tm_mon  = (time >> EXFAT_MONTH) & 0x0f;
	t->tm_mday = (time >> EXFAT_DAY) & 0x1f;
	t->tm_hour = (time >> EXFAT_HOUR) & 0x0f;
	t->tm_min  = (time >> EXFAT_MINUTE) & 0x3f;
	t->tm_sec  = (time & 0x1f) * 2;
	t->tm_sec += subsec / 100;
}

/**
 * exfat_lookup       - function interface to lookup pathname
 * @dir:                directory cluster index
 * @name:               file name
 *
 * @return:              cluster index
 */
int exfat_lookup(uint32_t dir, char *name)
{
	int index;
	node2_t *tmp;
	struct exfat_fileinfo *file;

	index = exfat_get_dirindex(dir);
	if (!info.root[index]) {
		pr_debug("Directory hasn't load yet, or This Directory doesn't exist in filesystem.\n");
		exfat_traverse_directories(info.root_offset, INT_MAX);
		index = exfat_get_dirindex(dir);
			if (!info.root[index]) {
				pr_warn("This Directory doesn't exist in filesystem.\n");
				return 0;
			}
	}
	
	tmp = info.root[index];
	while (tmp->next != NULL) {
		tmp = tmp->next;
		file = (struct exfat_fileinfo *)tmp->data;
		if (!strcmp(name, (char *)file->name))
			return tmp->index;
	}

	return 0;
}

/**
 * exfat_get_dirindex - get directory chain index by argument
 * @index:              index of the cluster
 *
 * @return:              index
 *                      Start of unused area (if doesn't lookup directory cache)
 */
static int exfat_get_dirindex(uint32_t index)
{
	int i;
	for (i = 0; i < info.root_size && info.root[i]; i++) {
		if (info.root[i]->index == index)
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
	struct exfat_fileinfo *finfo;

	exfat_traverse_one_directory(clu);
	i = exfat_get_dirindex(clu);
	tmp = info.root[i];

	for (i = 0; i < count && tmp->next != NULL; i++) {
		tmp = tmp->next;
		finfo = (struct exfat_fileinfo *)(tmp->data);
		dir[i].name = malloc(sizeof(unsigned char *) * (finfo->namelen + 1));
		strncpy((char *)dir[i].name, (char *)finfo->name, finfo->namelen + 1);
		dir[i].namelen = finfo->namelen;
		dir[i].datalen = finfo->datalen;
		dir[i].ctime = finfo->ctime;
		dir[i].atime = finfo->atime;
		dir[i].mtime = finfo->mtime;
	}
	/* If Dentry remains, Return error */
	if (tmp->next != NULL) {
		for (i = 0; tmp->next != NULL; i--, tmp = tmp->next);
	}

	return i;
}

/**
 * exfat_reload_directory - function interface to read directories
 * @from            cluster index to start search
 * @to              cluster index to end search
 *
 * TODO: Delete chain once if chain is exist
 */
int exfat_reload_directory(uint32_t from, uint32_t to)
{
	return exfat_traverse_directories(from, to);
}

/**
 * exfat_traverse_one_directory - function to traverse one directory
 * @index:         index of the cluster want to check
 *
 * @return        0 (success)
 *               -1 (failed to read)
 */
int exfat_traverse_one_directory(uint32_t index)
{
	int i, j, name_len;
	uint8_t scount;
	uint16_t uniname[MAX_NAME_LENGTH] = {0};
	uint64_t len;
	size_t dindex = exfat_get_dirindex(index);
	struct exfat_dirinfo *dinfo = (struct exfat_dirinfo *)info.root[dindex]->data;
	size_t size = info.cluster_size;
	size_t entries = size / sizeof(struct exfat_dentry);
	void *clu, *clu_tmp;
	struct exfat_dentry d, next, name;

	if (!(dinfo->attr & EXFAT_DIR_NEW)) {
		pr_debug("Directory %s was already traversed.\n", dinfo->name);
		return 0;
	}

	clu = malloc(size);
	get_cluster(clu, index);

	do {
		for (i = 0; i < entries; i++){
			d = ((struct exfat_dentry *)clu)[i];

			switch (d.EntryType) {
				case DENTRY_UNUSED:
					goto out;
				case DENTRY_BITMAP:
					pr_debug("Get: allocation table: cluster %x, size: %lx\n",
							d.dentry.bitmap.FirstCluster,
							d.dentry.bitmap.DataLength);
					info.chain_head = init_node();
					clu_tmp = malloc(info.cluster_size);
					get_cluster(clu_tmp, d.dentry.bitmap.FirstCluster);
					exfat_create_allocation_chain(clu_tmp);
					free(clu_tmp);

					pr_info("Allocation Bitmap (#%u):\n", d.dentry.bitmap.FirstCluster);
					exfat_print_allocation_bitmap();
					break;
				case DENTRY_UPCASE:
					info.upcase_size = d.dentry.upcase.DataLength;
					len = (info.cluster_size / info.upcase_size) + 1;
					info.upcase_table = malloc(info.cluster_size * len);
					pr_debug("Get: Up-case table: cluster %x, size: %x\n",
							d.dentry.upcase.FirstCluster,
							d.dentry.upcase.DataLength);
					get_clusters(info.upcase_table, d.dentry.upcase.FirstCluster, len);
					break;
				case DENTRY_VOLUME:
					pr_info("volume Label: ");
					name_len = d.dentry.vol.CharacterCount;
					memcpy(uniname, d.dentry.vol.VolumeLabel, sizeof(uint16_t) * name_len);
					exfat_print_volume_label(uniname, name_len);
					break;
				case DENTRY_FILE:
					scount = d.dentry.file.SecondaryCount;
					if (i + scount >= entries) {
						index = exfat_concat_cluster(index, &clu, size);
						size += info.cluster_size;
						entries = size / sizeof(struct exfat_dentry);
					}

					next = ((struct exfat_dentry *)clu)[i + 1];
					name = ((struct exfat_dentry *)clu)[i + 2];
					if (next.EntryType != DENTRY_STREAM || name.EntryType != DENTRY_NAME) {
						pr_warn("File should have stream/name entry, but This don't have.\n");
						return -1;
					}
					name_len = next.dentry.stream.NameLength;
					for (j = 0; j < scount - 1; j++) {
						name_len = MIN(ENTRY_NAME_MAX, next.dentry.stream.NameLength - j * ENTRY_NAME_MAX);
						memcpy(uniname + j * ENTRY_NAME_MAX, (((struct exfat_dentry *)clu)[i + 2 + j]).dentry.name.FileName, name_len * sizeof(uint16_t));
					}
					exfat_create_fileinfo(info.root[dindex], index,
							&d, &next, uniname);
					i += scount;
					break;
				case DENTRY_STREAM:
					pr_warn("Stream needs be File entry, but This is not.\n");
					break;
			}
		}
		index = exfat_concat_cluster(index, &clu, size);
		if (!index)
			break;

		size += info.cluster_size;
		entries = size / sizeof(struct exfat_dentry);
	} while (1);
out:
	((struct exfat_dirinfo *)(info.root[dindex]->data))->attr &= ~EXFAT_DIR_NEW;
	free(clu);
	return 0;
}

/**
 * exfat_traverse_directories - function to traverse directories
 * @from            cluster index to start search
 * @to              cluster index to end search
 *
 * @return        0 (success)
 *               -1 (failed to read)
 */
int exfat_traverse_directories(uint32_t from, uint32_t to)
{
	int i;

	for (i = 0; info.root[i] && i < info.root_size; i++) {
		if ((info.root[i]->index >= from) && (info.root[i]->index <= to))
			exfat_traverse_one_directory(info.root[from++]->index);
	}
	return 0;
}

/**
 * exfat_check_filesystem - Whether or not exFAT filesystem
 * @boot:      boot sector pointer
 * @ops:       filesystem operations
 *
 * @return:     1 (Image is exFAT filesystem)
 *              0 (Image isn't exFAT filesystem)
 */
int exfat_check_filesystem(struct pseudo_bootsec *boot, struct operations *ops)
{
	int ret = 0;
	struct exfat_bootsec *b;
	struct exfat_dirinfo *dinfo;

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
		dinfo = malloc(sizeof(struct exfat_dirinfo));
		dinfo->name = malloc(sizeof(unsigned char *) * (strlen("/") + 1));
		strncpy((char *)dinfo->name, "/", strlen("/") + 1);
		dinfo->pindex = info.root_offset;
		dinfo->entries = 0;
		dinfo->attr = EXFAT_DIR_NEW;
		dinfo->hash = 0;
		info.root[0] = init_node2(info.root_offset, dinfo);

		ops->statfs = exfat_print_boot_sec;
		ops->lookup =  exfat_lookup;
		ops->readdir = exfat_readdir;
		ops->reload = exfat_reload_directory;
		ops->convert = exfat_convert_character;
		ops->clean = exfat_clean_directory_chain;
		ops->print_cluster = exfat_print_cluster;
		ret = 1;
	}

	return ret;
}

/**
 * exfat_check_allocation_cluster - Whether or not cluster is allocated
 * @index:         index of the cluster want to check
 *
 * @return         true  (allocated cluster)
 *                 false (not allocated)
 */
static bool exfat_check_allocation_cluster(uint32_t index)
{
	node_t *node = search_node(info.chain_head, index);
	if (node)
		return true;
	return false;
}

/**
 * exfat_check_fatentry - Whether or not cluster is continuous
 * @index:         index of the cluster want to check
 *
 * @retrun:        next cluster (@index has next cluster)
 *                 0            (@index doesn't have next cluster)
 */
static uint32_t exfat_check_fatentry(uint32_t index)
{
	uint32_t ret;
	size_t entry_per_sector = info.sector_size / sizeof(uint32_t);
	uint32_t fat_index = (info.fat_offset +  index / entry_per_sector) * info.sector_size;
	uint32_t *fat;
	uint32_t entry_off = (index) % entry_per_sector;

	fat = malloc(info.sector_size);
	get_sector(fat, fat_index, 1);
	/* validate index */
	if (index == EXFAT_BADCLUSTER) {
		ret = 0;
		pr_err("cluster: %u is bad cluster.\n", index);
	} else if (index == EXFAT_LASTCLUSTER) {
		ret = 0;
		pr_debug("cluster: %u is the last cluster of cluster chain.\n", index);
	} else if (index < EXFAT_FIRST_CLUSTER || index > info.cluster_count + 1) {
		ret = 0;
		pr_debug("cluster: %u is invalid.\n", index);
	} else {
		ret = fat[entry_off];
		pr_debug("cluster: %u has chain. next is %u.\n", index, fat[entry_off]);
	}

	free(fat);
	return ret;
}

/**
 * exfat_check_exist_directory - check whether @index has already loaded
 * @index:         index of the cluster
 *
 * @retrun:        1 (@index has loaded)
 *                 0 (@index hasn't loaded)
 */
static int exfat_check_exist_directory(uint32_t index)
{
	int i;
	for (i = 0; info.root[i] && i < info.root_size; i++) {
		if (info.root[i]->index == index)
			return 1;
	}
	return 0;
}

/**
 * exfat_concat_cluster - Contatenate cluster @data with next_cluster
 * @index:         index of the cluster
 * @data:          The cluster
 * @size:          allocated size to store cluster data
 *
 * @retrun:        next cluster (@index has next cluster)
 *                 0            (@index doesn't have next cluster, or failed to realloc)
 */
static uint32_t exfat_concat_cluster(uint32_t index, void **data, size_t size)
{
	int i;
	uint32_t ret;
	void *clu_tmp;
	ret = exfat_check_fatentry(index);

	if (ret) {
		clu_tmp = realloc(*data, size + info.cluster_size);
		if (clu_tmp) {
			*data = clu_tmp;
			get_cluster(clu_tmp + size, ret);
			pr_msg("Concatenate cluster #%u with #%u\n", index, ret);
		} else {
			pr_err("Failed to Get new memory.\n");
			ret = 0;
		}
	}
	return ret;
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
 * exfat_create_file_entry - Create file infomarion
 * @dir:        Directory chain head
 * @index:      parent Directory cluster index
 * @file:       file dentry
 * @stream:     stream Extension dentry
 * @uniname:    File Name dentry
 */
static void exfat_create_fileinfo(node2_t *dir, uint32_t index,
		struct exfat_dentry *file, struct exfat_dentry *stream, uint16_t *uniname)
{
	int i, next_index = stream->dentry.stream.FirstCluster;
	struct exfat_fileinfo *finfo;
	size_t namelen = stream->dentry.stream.NameLength;

	finfo = malloc(sizeof(struct exfat_fileinfo));
	finfo->name = malloc(namelen * UTF8_MAX_CHARSIZE + 1);
	memset(finfo->name, '\0', namelen * UTF8_MAX_CHARSIZE + 1);

	exfat_load_filename(uniname, namelen, finfo->name);
	finfo->namelen = namelen;
	finfo->datalen = stream->dentry.stream.DataLength;
	finfo->attr = file->dentry.file.FileAttributes;
	finfo->hash = stream->dentry.stream.NameHash;

	exfat_load_timestamp(&finfo->ctime, "Create", file->dentry.file.CreateTimestamp,
			file->dentry.file.Create10msIncrement,
			file->dentry.file.CreateUtcOffset);
	exfat_load_timestamp(&finfo->mtime, "Modify", file->dentry.file.LastModifiedTimestamp,
			file->dentry.file.LastModified10msIncrement,
			file->dentry.file.LastModifiedUtcOffset);
	exfat_load_timestamp(&finfo->atime, "Access", file->dentry.file.LastAccessedTimestamp,
			0,
			file->dentry.file.LastAccessdUtcOffset);
	append_node2(dir, next_index, finfo);
	((struct exfat_dirinfo *)(dir->data))->entries++;

	/* If this entry is Directory, prepare to create next chain */
	if ((finfo->attr & ATTR_DIRECTORY) && (!exfat_check_exist_directory(next_index))) {
		struct exfat_dirinfo *dinfo = malloc(sizeof(struct exfat_dirinfo));
		dinfo->name = malloc(finfo->namelen + 1);
		strncpy((char *)dinfo->name, (char *)finfo->name, finfo->namelen + 1);
		dinfo->pindex = index;
		dinfo->entries = 0;
		dinfo->attr = EXFAT_DIR_NEW;
		dinfo->hash = finfo->hash;

		i = exfat_get_dirindex(next_index);
		info.root[i] = init_node2(next_index, dinfo);
	}
}
