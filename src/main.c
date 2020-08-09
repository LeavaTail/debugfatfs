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
#include <sys/types.h>
#include <sys/stat.h>

#include "dumpexfat.h"
FILE *output = NULL;
unsigned int print_level = PRINT_WARNING;
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
	{"cluster",required_argument, NULL, 'c'},
	{"force",no_argument, NULL, 'f'},
	{"output",required_argument, NULL, 'o'},
	{"sector",required_argument, NULL, 's'},
	{"upper",required_argument, NULL, 'u'},
	{"verbose",no_argument, NULL, 'v'},
	{"help",no_argument, NULL, GETOPT_HELP_CHAR},
	{"version",no_argument, NULL, GETOPT_VERSION_CHAR},
	{0,0,0,0}
};

/**
 * usage - print out usage.
 * @status: Status code
 */
static void usage()
{
	fprintf(stderr, "Usage: %s [OPTION]... FILE\n", PROGRAM_NAME);
	fprintf(stderr, "dump FAT/exFAT filesystem information.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "  -c, --cluster=index\tdump the cluster index after dump filesystem information.\n");
	fprintf(stderr, "  -f, --force\tdump the cluster forcibly in spite of the non-allocated.\n");
	fprintf(stderr, "  -o, --output=file\tsend output to file rather than stdout.\n");
	fprintf(stderr, "  -s, --sector=index\tdump the sector index after dump filesystem information.\n");
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
 * version - print out program version.
 * @command_name: command name
 * @version:      program version
 * @author:       program authoer
 */
static void version(const char *command_name, const char *version,
		const char *author)
{
	fprintf(stdout, "%s %s\n", command_name, version);
	fprintf(stdout, "\n");
	fprintf(stdout, "Written by %s.\n", author);
}

/**
 * get_sector - Get Raw-Data from any sector.
 * @info:       Target device information
 * @index:      Start bytes
 * @count:      The number of sectors
 */
int get_sector(struct device_info *info, void *data, off_t index, size_t count)
{
	size_t sector_size = info->sector_size;

	pr_debug("Get: Sector from %lx to %lx\n", index , index + (count * sector_size) - 1);
	if ((pread(info->fd, data, count * sector_size, index)) < 0) {
		pr_err("can't read %s.", info->name);
		return -1;
	}
	return 0;
}

/**
 * get_cluster - Get Raw-Data from any cluster.
 * @info:       Target device information
 * @index:      Start cluster index
 */
int get_cluster(struct device_info *info, void *data, off_t index)
{
	return get_clusters(info, data,index, 1);
}

/**
 * get_clusters - Get Raw-Data from any cluster.
 * @info:       Target device information
 * @index:      Start cluster index
 * @num:        The number of clusters
 */
int get_clusters(struct device_info *info, void *data, off_t index, size_t num)
{
	size_t clu_per_sec = info->cluster_size / info->sector_size;
	off_t heap_start = info->heap_offset * info->sector_size;

	if (index < 2 || index + num > info->cluster_count) {
		pr_err("invalid cluster index %lu.", index);
		return -1;
	}

	return get_sector(info,
			data,
			heap_start + ((index - 2) * info->cluster_size),
			clu_per_sec * num);
}

/**
 * hexdump - Hex dump of a given data
 * @out:        Output stream
 * @data:       Input data
 * @index:      Input data size
 */
void hexdump(FILE *out, void *data, size_t size)
{
	size_t line, byte = 0;
	size_t count = size / 0x10;

	for (line = 0; line < count; line++) {
		pr_msg("%08lX:  ", line * 0x10);
		for (byte = 0; byte < 0x10; byte++) {
			pr_msg("%02X ", ((unsigned char *)data)[line * 0x10 + byte]);
		}
		putchar(' ');
		for (byte = 0; byte < 0x10; byte++) {
			char ch = ((unsigned char *)data)[line * 0x10 + byte];
			pr_msg("%c", isprint(ch)? ch: '.');
		}
		pr_msg("\n");
	}
}

/**
 * init_device_info - Initialize member in struct device_info
 * @info:      structure to be Initialized
 */
static void init_device_info(struct device_info *info)
{
	info->fd = -1;
	info->attr = 0;
	info->total_size = 0;
	info->sector_size = 0;
	info->cluster_size = 0;
	info->cluster_count = 0;
	info->fstype = 0;
	info->flags = 0;
	info->fat_offset = 0;
	info->fat_length = 0;
	info->heap_offset = 0;
	info->root_offset = 0;
	info->chain_head = NULL;
	info->upcase_table = NULL;
	info->upcase_size = 0;
	info->root = NULL;
	info->root_size = 0;
	info->root_maxsize = DENTRY_LISTSIZE;
}

/**
 * get_device_info - get device name and store in device_info
 * @info:      structure to be stored device name
 */
static int get_device_info(struct device_info *info)
{
	int fd;
	struct stat s;

	if ((fd = open(info->name, O_RDONLY)) < 0) {
		pr_err("can't open %s.", info->name);
		return -1;
	}

	if (fstat(fd, &s) < 0) {
		pr_err("can't get stat %s.", info->name);
		close(fd);
		return -1;
	}

	info->fd = fd;
	info->total_size = s.st_size;
	return 0;
}

/**
 * free_dentry_list - release list2_t
 * @info:      free to be stored device name
 *
 * TODO: Check for memory leaks
 */
static int free_dentry_list(struct device_info *info)
{
	int i;
	for(i = 0; i < info->root_size; i++) {
		/* FIXME: There may be areas that have not been released. */
		free_list2(info->root[i]);
	}
	free(info->root);

	return 0;
}

/**
 * pseudo_show_boot_sec - virtual function to show boot sector
 * @info:      structure to be shown device_info
 * @boot:      boot sector pointer
 *
 * TODO: implement function in FAT12/16/32
 */
static int pseudo_show_boot_sec(struct device_info *info, struct pseudo_bootsector *boot)
{
	size_t count = 0;

	count = pread(info->fd, boot, SECSIZE, 0);
	if(count < 0){
		pr_err("can't read %s.", info->name);
		return -1;
	}
	if (!strncmp((char *)boot->FileSystemName, "EXFAT   ", 8)) {
		/* exFAT */
		info->fstype = EXFAT_FILESYSTEM;
		exfat_show_boot_sec(info, (struct exfat_bootsec*)boot);
	} else {
		/* FAT or invalid */
		fat_show_boot_sec(info, (struct fat_bootsec*)boot);
	}
	return 0;
}

/**
 * pseudo_get_cluster_chain - virtual function to get cluster chain
 * @info:      structure to be get device_info
 *
 * TODO: implement function in FAT12/16/32
 */
static int pseudo_get_cluster_chain(struct device_info *info)
{
	switch (info->fstype) {
		case EXFAT_FILESYSTEM:
			exfat_traverse_directories(info, info->root_offset);
			break;
		case FAT12_FILESYSTEM:
			/* FALLTHROUGH */
		case FAT16_FILESYSTEM:
			/* FIXME: Unimplemented*/
			break;
		case FAT32_FILESYSTEM:
			/* FIXME: Unimplemented*/
			break;
		default:
			pr_err("invalid filesystem image.");
			return -1;
	}

	return 0;
}

/**
 * pseudo_convert_char - convert character for each filesystem
 * @info:      structure to be get device_info
 * @character: target character
 */
static int pseudo_convert_character(struct device_info *info, const char *character)
{
	char out[MAX_NAME_LENGTH + 1] = {};
	switch (info->fstype) {
		case EXFAT_FILESYSTEM:
			exfat_convert_character(info, character, strlen(character), out);
			pr_msg("Convert: %s -> %s\n", character, out);
			break;
		case FAT12_FILESYSTEM:
			/* FALLTHROUGH */
		case FAT16_FILESYSTEM:
			/* FALLTHROUGH */
		case FAT32_FILESYSTEM:
			/* TODO: implement function */
			break;
		default:
			pr_err("invalid filesystem image.");
			return -1;
	}

	return 0;
}

/**
 * pseudo_print_cluster - virtual function to print any cluster
 * @info:      structure to be get device_info
 * @cluster:   cluster index to display
 */
static int pseudo_print_cluster(struct device_info *info, uint32_t cluster)
{
	switch (info->fstype) {
		case EXFAT_FILESYSTEM:
			exfat_print_cluster(info, cluster);
			break;
		case FAT12_FILESYSTEM:
			/* FALLTHROUGH */
		case FAT16_FILESYSTEM:
			/* FALLTHROUGH */
		case FAT32_FILESYSTEM:
			fat_print_cluster(info, cluster);
			break;
		default:
			pr_err("invalid filesystem image.");
			return -1;
	}

	return 0;
}

/**
 * pseudo_print_sector - virtual function to print any sector
 * @info:      structure to be get device_info
 * @sector:    sector index to display
 */
static int pseudo_print_sector(struct device_info *info, uint32_t sector)
{
	void *data;

	data = (char *)malloc(info->sector_size);
	if (get_sector(info, data, sector, 1)) {
		pr_msg("Sector #%u:\n", sector);
		hexdump(output, data, info->sector_size);
	}
	free(data);
	return 0;
}

/**
 * main - main function
 * @argc:      argument count
 * @argv:      argument vector
 */
int main(int argc, char *argv[])
{
	int opt;
	int longindex;
	int ret = 0;
	uint8_t attr = 0;
	bool cflag = false;
	uint32_t cluster = 0;
	bool uflag = false;
	bool sflag = false;
	uint32_t sector = 0;
	bool outflag = false;
	char *outfile = NULL;
	char *input = NULL;
	struct device_info info;
	struct pseudo_bootsector bootsec;

	while ((opt = getopt_long(argc, argv,
					"c:fo:s:u:v",
					longopts, &longindex)) != -1) {
		switch (opt) {
			case 'c':
				cflag = true;
				cluster = strtoul(optarg, NULL, 0);
				break;
			case 'f':
				attr |= FORCE_ATTR;
				break;
			case 'o':
				outflag = true;
				outfile = optarg;
				break;
			case 's':
				sflag = true;
				sector = strtoul(optarg, NULL, 0);
				break;
			case 'u':
				uflag = true;
				input = optarg;
				break;
			case 'v':
				print_level = PRINT_INFO;
				break;
			case GETOPT_HELP_CHAR:
				usage(EXIT_SUCCESS);
				exit(EXIT_SUCCESS);
			case GETOPT_VERSION_CHAR:
				version(PROGRAM_NAME, PROGRAM_VERSION, PROGRAM_AUTHOR);
				exit(EXIT_SUCCESS);
			default:
				usage();
				exit(EXIT_FAILURE);
		}
	}

#ifdef DUMPEXFAT_DEBUG
	print_level = PRINT_DEBUG;
#endif

	if (optind != argc - 1) {
		usage();
		exit(EXIT_FAILURE);
	}

	init_device_info(&info);
	output = stdout;
	if(outflag) {
		if ((output = fopen(outfile, "w")) == NULL) {
			pr_err("can't open %s.", optarg);
			exit(EXIT_FAILURE);
		}
	}
	info.attr = attr;

	memcpy(info.name, argv[optind], 255);
	ret = get_device_info(&info);
	if (ret < 0)
		goto out;
	info.chain_head = init_node();

	ret = pseudo_show_boot_sec(&info, &bootsec);
	if (ret < 0)
		goto out;

	ret = pseudo_get_cluster_chain(&info);
	if (ret < 0)
		goto file_err;

	if (uflag) {
		ret = pseudo_convert_character(&info, input);
		if(ret < 0)
			goto file_err;
	}

	if (sflag) {
		ret = pseudo_print_sector(&info, sector);
		if (ret < 0)
			goto file_err;
	}

	if (cflag) {
		ret = pseudo_print_cluster(&info, cluster);
		if (ret < 0)
			goto file_err;
	}

file_err:
	close(info.fd);

out:
	free_list(info.chain_head);
	free(info.upcase_table);
	free_dentry_list(&info);
	if(outflag)
		fclose(output);

	return ret;
}
