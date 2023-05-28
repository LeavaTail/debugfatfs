// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2021 LeavaTail
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <mntent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "debugfatfs.h"
FILE *output = NULL;
unsigned int print_level = PRINT_WARNING;
struct device_info info;
/**
 * Special Option(no short option)
 */
enum
{
	GETOPT_HELP_CHAR = (CHAR_MIN - 2),
	GETOPT_VERSION_CHAR = (CHAR_MIN - 3)
};

/* option data {"long name", needs argument, flags, "short name"} */
static struct option const longopts[] =
{
	{"all", no_argument, NULL, 'a'},
	{"byte", required_argument, NULL, 'b'},
	{"cluster", required_argument, NULL, 'c'},
	{"directory", required_argument, NULL, 'd'},
	{"fat", required_argument, NULL, 'f'},
	{"interactive", no_argument, NULL, 'i'},
	{"output", required_argument, NULL, 'o'},
	{"quiet", no_argument, NULL, 'q'},
	{"ro", no_argument, NULL, 'r'},
	{"upper", required_argument, NULL, 'u'},
	{"verbose", no_argument, NULL, 'v'},
	{"help", no_argument, NULL, GETOPT_HELP_CHAR},
	{"version", no_argument, NULL, GETOPT_VERSION_CHAR},
	{0,0,0,0}
};

/**
 * usage - print out usage
 */
static void usage(void)
{
	fprintf(stderr, "Usage: %s [OPTION]... FILE\n", PROGRAM_NAME);
	fprintf(stderr, "dump FAT/exFAT filesystem information.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "  -a, --all\tTrverse all directories.\n");
	fprintf(stderr, "  -b, --byte=offset\tdump the any byte after dump filesystem information.\n");
	fprintf(stderr, "  -c, --cluster=index\tdump the cluster index after dump filesystem information.\n");
	fprintf(stderr, "  -d, --direcotry=path\tread directory entry from path.\n");
	fprintf(stderr, "  -f, --fource\twrite foucibly even if filesystem image has already mounted.\n");
	fprintf(stderr, "  -i, --interactive\tprompt the user operate filesystem.\n");
	fprintf(stderr, "  -o, --output=file\tsend output to file rather than stdout.\n");
	fprintf(stderr, "  -q, --quiet\tSuppress message about Main boot Sector.\n");
	fprintf(stderr, "  -r, --ro\tread only mode. \n");
	fprintf(stderr, "  -u, --upper\tconvert into uppercase latter by up-case Table.\n");
	fprintf(stderr, "  -v, --verbose\tVersion mode.\n");
	fprintf(stderr, "  --help\tdisplay this help and exit.\n");
	fprintf(stderr, "  --version\toutput version information and exit.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "Examples:\n");
	fprintf(stderr, "  %s /dev/sda\tdump FAT/exFAT filesystem information.\n", PROGRAM_NAME);
	fprintf(stderr, "  %s -c 2 /dev/sda\tdump FAT/exFAT filesystem information and cluster #2.\n", PROGRAM_NAME);
	fprintf(stderr, "\n");
}

/**
 * version        - print out program version
 * @command_name:   command name
 * @version:        program version
 * @author:         program authoer
 */
static void version(const char *command_name, const char *version, const char *author)
{
	fprintf(stdout, "%s %s\n", command_name, version);
	fprintf(stdout, "\n");
	fprintf(stdout, "Written by %s.\n", author);
}

/**
 * get_sector - Get Raw-Data from any sector
 * @data:       Sector raw data (Output)
 * @index:      Start bytes
 * @count:      The number of sectors
 *
 * @return       0 (success)
 *              -1 (failed to read)
 *
 * NOTE: Need to allocate @data before call it.
 */
int get_sector(void *data, off_t index, size_t count)
{
	size_t sector_size = info.sector_size;

	pr_debug("Get: Sector from 0x%lx to 0x%lx\n", index , index + (count * sector_size) - 1);
	if ((pread(info.fd, data, count * sector_size, index)) < 0) {
		pr_err("read: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

/**
 * set_sector - Set Raw-Data from any sector
 * @data:       Sector raw data
 * @index:      Start bytes
 * @count:      The number of sectors
 *
 * @return       0 (success)
 *              -1 (failed to read)
 *
 * NOTE: Need to allocate @data before call it.
 */
int set_sector(void *data, off_t index, size_t count)
{
	size_t sector_size = info.sector_size;

	pr_debug("Set: Sector from 0x%lx to 0x%lx\n", index, index + (count * sector_size) - 1);
	if ((pwrite(info.fd, data, count * sector_size, index)) < 0) {
		pr_err("write: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

/**
 * get_cluster - Get Raw-Data from any cluster
 * @data:        cluster raw data (Output)
 * @index:       Start cluster index
 *
 * @return        0 (success)
 *               -1 (failed to read)
 *
 * NOTE: Need to allocate @data before call it.
 */
int get_cluster(void *data, off_t index)
{
	return get_clusters(data, index, 1);
}

/**
 * set_cluster - Set Raw-Data from any cluster
 * @data:        cluster raw data
 * @index:       Start cluster index
 *
 * @return        0 (success)
 *               -1 (failed to read)
 *
 * NOTE: Need to allocate @data before call it.
 */
int set_cluster(void *data, off_t index)
{
	return set_clusters(data, index, 1);
}

/**
 * get_clusters - Get Raw-Data from any cluster
 * @data:         cluster raw data (Output)
 * @index:        Start cluster index
 * @num:          The number of clusters
 *
 * @return         0 (success)
 *                -1 (failed to read)
 *
 * NOTE: Need to allocate @data before call it.
 */
int get_clusters(void *data, off_t index, size_t num)
{
	size_t clu_per_sec = info.cluster_size / info.sector_size;
	off_t heap_start = info.heap_offset * info.sector_size;

	if (index < 2 || index + num > info.cluster_count) {
		pr_err("invalid cluster index %lu.\n", index);
		return -1;
	}

	return get_sector(data,
			heap_start + ((index - 2) * info.cluster_size),
			clu_per_sec * num);
}

/**
 * set_clusters - Set Raw-Data from any cluster
 * @data:         cluster raw data
 * @index:        Start cluster index
 * @num:          The number of clusters
 *
 * @return         0 (success)
 *                -1 (failed to read)
 *
 * NOTE: Need to allocate @data before call it.
 */
int set_clusters(void *data, off_t index, size_t num)
{
	size_t clu_per_sec = info.cluster_size / info.sector_size;
	off_t heap_start = info.heap_offset * info.sector_size;

	if (index < 2 || index + num > info.cluster_count) {
		pr_err("invalid cluster index %lu.\n", index);
		return -1;
	}

	return set_sector(data,
			heap_start + ((index - 2) * info.cluster_size),
			clu_per_sec * num);
}

/**
 * hexdump - Hex dump of a given data
 * @data:    Input data
 * @size:    Input data size
 */
void hexdump(void *data, size_t size)
{
	size_t skip = 0;
	size_t line, byte = 0;
	size_t count = size / 0x10;
	const char zero[0x10] = {0};

	for (line = 0; line < count; line++) {
		if ((line != count - 1) && (!memcmp(data + line * 0x10, zero, 0x10))) {
			switch (skip++) {
				case 0:
					break;
				case 1:
					pr_msg("*\n");
					/* FALLTHROUGH */
				default:
					continue;
			}
		} else {
			skip = 0;
		}

		pr_msg("%08zX:  ", line * 0x10);
		for (byte = 0; byte < 0x10; byte++) {
			pr_msg("%02X ", ((unsigned char *)data)[line * 0x10 + byte]);
		}
		putchar(' ');
		for (byte = 0; byte < 0x10; byte++) {
			char ch = ((unsigned char *)data)[line * 0x10 + byte];
			pr_msg("%c", isprint(ch) ? ch : '.');
		}
		pr_msg("\n");
	}
}

/**
 * gen_rand - generate random string of any characters
 * @data:     Output data (Output)
 * @len:      data length
 */
void gen_rand(char *data, size_t len)
{
	int i;
	const char strset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	for (i = 0; i < len; i++)
		data[i] = strset[rand() % (sizeof(strset) - 1)];
	data[i] = '\0';
}

/**
 * check_mounted_filesystem - check if the image has mounted
 *
 * @return                    0 (not mount)
 *                            1 (already mounted)
 */
static int check_mounted_filesystem(void)
{
	FILE *fstab = setmntent("/etc/mtab", "r");
	struct mntent *e = NULL;

	while ((e = getmntent(fstab)) != NULL) {
		if (!strcmp(e->mnt_fsname, info.name))
			return 1;
	}
	return 0;
}

/**
 * init_device_info - Initialize member in struct device_info
 */
static void init_device_info(void)
{
	info.fd = -1;
	info.attr = 0;
	info.total_size = 0;
	info.sector_size = 0;
	info.cluster_size = 0;
	info.cluster_count = 0;
	info.fstype = 0;
	info.flags = 0;
	info.fat_offset = 0;
	info.fat_length = 0;
	info.heap_offset = 0;
	info.root_offset = 0;
	info.root_length = 0;
	info.alloc_table = NULL;
	info.upcase_table = NULL;
	info.upcase_size = 0;
	info.vol_label = NULL;
	info.vol_length = 0;
	info.root_size = DENTRY_LISTSIZE;
	info.root = calloc(info.root_size, sizeof(node2_t *));
}

/**
 * get_device_info - get device name and store in device_info
 * @attr:            command line options
 *
 * @return            0 (success)
 *                   -1 (failed to open)
 */
static int get_device_info(uint32_t attr)
{
	int fd;
	struct stat s;

	if (check_mounted_filesystem() &&
			!(attr & OPTION_READONLY)) {
		pr_err("Error has occurred becasue %s has already mounted.\n", info.name);
		return -1;
	}

	if ((fd = open(info.name, attr & OPTION_READONLY ? O_RDONLY : O_RDWR)) < 0) {
		pr_err("open: %s\n", strerror(errno));
		return -1;
	}

	if (fstat(fd, &s) < 0) {
		pr_err("stat: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	info.fd = fd;
	info.total_size = s.st_size;
	return 0;
}

/**
 * free_dentry_list - release list2_t
 *
 * @return            Number of lists freed
 */
static int free_dentry_list(void)
{
	int i;

	for(i = 0; i < info.root_size && info.root[i]; i++) {
		info.ops->clean(i);
	}
	free(info.root);
	return i;
}

/**
 * pseudo_check_filesystem - virtual function to check filesystem
 * @boot:                    boot sector pointer
 *
 * return:                    0 (succeeded in obtaining filesystem)
 *                           -1 (failed)
 */
static int pseudo_check_filesystem(struct pseudo_bootsec *boot)
{
	size_t count = 0;

	count = pread(info.fd, boot, SECSIZE, 0);
	if (count < 0) {
		pr_err("read: %s\n", strerror(errno));
		return -1;
	}

	if (exfat_check_filesystem(boot))
		return 0;
	if (fat_check_filesystem(boot))
		return 0;

	pr_err("%s can't support this image.\n", PROGRAM_NAME);
	return -1;
}

/**
 * print_sector - print any sector
 * @sector:       sector index to display
 *
 * return:        0 (succeeded in obtaining filesystem)
 */
static int print_sector(uint32_t sector)
{
	void *data;

	data = malloc(info.sector_size);
	if (!get_sector(data, sector, 1)) {
		pr_msg("Sector #%u:\n", sector);
		hexdump(data, info.sector_size);
	}
	free(data);
	return 0;
}

/**
 * print_cluster - print any cluster
 * @index:         cluster index to display
 *
 * @return        0 (success)
 */
int print_cluster(uint32_t index)
{
	void *data;

	data = malloc(info.cluster_size);
	if (!get_cluster(data, index)) {
		pr_msg("Cluster #%u:\n", index);
		hexdump(data, info.cluster_size);
	}
	free(data);
	return 0;
}

/**
 * main   - main function
 * @argc:   argument count
 * @argv:   argument vector
 */
int main(int argc, char *argv[])
{
	int i;
	int opt;
	int longindex;
	int ret = 0;
	int offset = 0;
	int entries = 0;
	uint32_t attr = 0;
	uint32_t cluster = 0;
	uint32_t fatent = 0;
	uint32_t value = 0;
	uint32_t sector = 0;
	char *filepath = NULL;
	char *outfile = NULL;
	char *dir = NULL;
	char *input = NULL;
	char out[MAX_NAME_LENGTH + 1] = {};
	struct pseudo_bootsec bootsec;
	struct directory *dirs = NULL, *dirs_tmp = NULL;

	while ((opt = getopt_long(argc, argv,
					"ab:c:d:f:il:o:qrs:u:v",
					longopts, &longindex)) != -1) {
		switch (opt) {
			case 'a':
				attr |= OPTION_ALL;
				break;
			case 'b':
				attr |= OPTION_SECTOR;
				sector = strtoul(optarg, NULL, 0);
				break;
			case 'c':
				attr |= OPTION_CLUSTER;
				cluster = strtoul(optarg, NULL, 0);
				break;
			case 'd':
				attr |= OPTION_DIRECTORY;
				dir = optarg;
				break;
			case 'f':
				attr |= OPTION_FATENT;
				fatent = strtoul(optarg, NULL, 0);
				break;
			case 'i':
				attr |= OPTION_INTERACTIVE;
				break;
			case 'o':
				attr |= OPTION_OUTPUT;
				outfile = optarg;
				break;
			case 'r':
				attr |= OPTION_READONLY;
				break;
			case 'q':
				print_level = PRINT_ERR;
				break;
			case 'u':
				attr |= OPTION_UPPER;
				input = optarg;
				break;
			case 'v':
				print_level = PRINT_INFO;
				break;
			case GETOPT_HELP_CHAR:
				usage();
				exit(EXIT_SUCCESS);
			case GETOPT_VERSION_CHAR:
				version(PROGRAM_NAME, PROGRAM_VERSION, PROGRAM_AUTHOR);
				exit(EXIT_SUCCESS);
			default:
				usage();
				exit(EXIT_FAILURE);
		}
	}

#ifdef DEBUGFATFS_DEBUG
	print_level = PRINT_DEBUG;
#endif

	switch (argc - optind) {
		case 1:
			break;
		case 2:
			filepath = argv[optind + 1];
			break;
		default:
			usage();
			exit(EXIT_FAILURE);
			break;
	}

	init_device_info();
	info.attr = attr;

	output = stdout;
	if (attr & OPTION_OUTPUT) {
		if ((output = fopen(outfile, "w")) == NULL) {
			pr_err("open: %s\n", strerror(errno));
			goto info_release;
		}
	}

	memcpy(info.name, argv[optind], 255);
	ret = get_device_info(attr);
	if (ret < 0)
		goto output_close;

	ret = pseudo_check_filesystem(&bootsec);
	if (ret < 0)
		goto device_close;

	/* Interactive Mode: -i option */
	if (attr & OPTION_INTERACTIVE) {
		shell();
		goto device_close;
	}

	/* Filesystem statistic: default or -a option */
	if (!attr || (attr & OPTION_ALL)) {
		ret = info.ops->statfs();
		if (ret < 0)
			goto device_close;
	}

	offset = info.root_offset;

	/* Command line: -d option */
	if (attr & OPTION_DIRECTORY) {
		dirs = calloc(DIRECTORY_FILES, sizeof(struct directory));
		entries = DIRECTORY_FILES;
		offset = info.ops->lookup(info.root_offset, dir);
		if (offset < 0)
			goto out;
	}
	ret = info.ops->readdir(dirs, entries, offset);
	if (attr & OPTION_DIRECTORY) {
		if (ret < 0) {
			/* Only once, expand dirs structure and execute readdir */
			ret = abs(ret) + 1;
			dirs_tmp = realloc(dirs, sizeof(struct directory) * (DIRECTORY_FILES + ret));
			if (dirs_tmp) {
				dirs = dirs_tmp;
				ret = info.ops->readdir(dirs, DIRECTORY_FILES + ret, offset);
				if (ret < 0)
					goto out;
			} else {
				pr_err("Can't load directory because of failed to allocate space.\n");
				goto out;
			}
		}

		entries = ret;
		pr_msg("Read \"/\" Directory (%d entries).\n", entries);
		for (i = 0; i < entries; i++)
			pr_msg("%s ", dirs[i].name);

		pr_msg("\n");
	}
	ret = 0;

	/* Command line: -a option */
	if (attr & OPTION_ALL) {
		ret = info.ops->info();
		if (ret < 0)
			goto device_close;
	}

	/* Command line: -f option */
	if (attr & OPTION_FATENT) {
		ret = info.ops->getfat(fatent, &value);
		pr_msg("Get: Cluster %u is FAT entry %08x\n", fatent, value);
		if (ret < 0)
			goto device_close;
	}

	/* Command line: -u option */
	if (attr & OPTION_UPPER) {
		ret = info.ops->convert(input, strlen(input), out);
		if(ret < 0)
			goto out;
		pr_msg("Convert: %s -> %s\n", input, out);
	}

	/* Command line: -c, -s option */
	if ((attr & OPTION_SECTOR) || (attr & OPTION_CLUSTER)) {
		if (attr & OPTION_CLUSTER)
			ret = print_cluster(cluster);
		else
			ret = print_sector(sector);

		if (ret < 0)
			goto out;
	}

	/* file argument */
	if (filepath) {
		uint32_t p_clu;
		char *tmp;

		tmp = calloc(strlen(filepath), sizeof(char));

		strncpy(tmp, filepath, strlen(filepath));
		filepath = strtok_dir(tmp);
		/* FIXME: workaround to search non-AbsolutePath */
		if (filepath == tmp)
			snprintf(tmp, sizeof("/") + 1, "/");

		p_clu = info.ops->lookup(info.root_offset, tmp);
		ret = info.ops->stat(filepath, p_clu);

		free(tmp);
	}

out:
	if (attr & OPTION_DIRECTORY) {
		for (i = 0; i < entries; i++)
			free(dirs[i].name);
		free(dirs);
	}
	free(info.vol_label);
	free(info.upcase_table);
	free(info.alloc_table);

device_close:
	close(info.fd);

output_close:
	if (attr & OPTION_OUTPUT)
		fclose(output);

info_release:
	free_dentry_list();

	return ret;
}
