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
unsigned int print_level = DUMP_WARNING;
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
	{"output",required_argument, NULL, 'o'},
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
	fprintf(stderr, "Usage: %s [OPTION] FILE\n", PROGRAM_NAME);
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
	fprintf(stdout, "Written by %s.\n", author);
}

/**
 * get_sector - Get Raw-Data from any sector.
 * @info:       Target device information
 * @index:      Start bytes
 * @count:      The number of sectors
 */
void* get_sector(struct device_info *info, off_t index, size_t count)
{
	void *data;
	size_t sector_size = info->sector_size;

	dump_debug("Get: Sector from %lx to %lx\n", index , index + (count * sector_size) - 1);
	data = (char *)malloc(sector_size * count);
	if ((pread(info->fd, data, count * sector_size, index)) < 0) {
		dump_err("can't read %s.", info->name);
		return NULL;
	}
	return data;
}

/**
 * get_cluster - Get Raw-Data from any cluster.
 * @info:       Target device information
 * @index:      Start cluster index
 */
void *get_cluster(struct device_info *info, off_t index)
{
	void *data;
	size_t sector_size = info->sector_size;
	off_t heap_start = info->heap_offset * sector_size;

	if (index < 2 || index > info->cluster_count + 1) {
		dump_err("invalid cluster index %lu.", index);
		return NULL;
	}

	data = get_sector(info,
			heap_start + ((index - 2) * (1 << info->cluster_shift) * sector_size),
			(1 << info->cluster_shift));
	return data;
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
		fprintf(out, "%08lX:  ", line * 0x10);
		for (byte = 0; byte < 0x10; byte++) {
			fprintf(out, "%02X ", ((unsigned char *)data)[line * 0x10 + byte]);
		}
		putchar(' ');
		for (byte = 0; byte < 0x10; byte++) {
			char ch = ((unsigned char *)data)[line * 0x10 + byte];
			fprintf(out, "%c", isprint(ch)? ch: '.');
		}
		fprintf(out, "\n");
	}
}

static int get_device_info(struct device_info *info)
{
	int fd;
	struct stat s;

	if ((fd = open(info->name, O_RDONLY)) < 0) {
		dump_err("can't open %s.", info->name);
		return -1;
	}

	if (fstat(fd, &s) < 0) {
		dump_err("can't get stat %s.", info->name);
		close(fd);
		return -1;
	}

	info->fd = fd;
	info->total_size = s.st_size;
	return 0;
}

static int pseudo_show_boot_sec(struct device_info *info, struct pseudo_bootsector *boot)
{
	size_t count = 0;

	count = pread(info->fd, boot, SECSIZE, 0);
	if(count < 0){
		dump_err("can't read %s.", info->name);
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

static int pseudo_get_cluster_chain(struct device_info *info)
{
	switch (info->fstype) {
		case EXFAT_FILESYSTEM:
			{
				void *root = get_cluster(info, info->root_offset);
				exfat_get_allocation_bitmap(info, root);
				free(root);
				break;
			}
		case FAT12_FILESYSTEM:
			/* FALLTHROUGH */
		case FAT16_FILESYSTEM:
			/* FIXME: Unimplemented*/
			break;
		case FAT32_FILESYSTEM:
			/* FIXME: Unimplemented*/
			break;
		default:
			dump_err("invalid filesystem image.");
			return -1;
	}

	return 0;
}

static int pseudo_print_cluster(struct device_info *info, uint32_t cluster)
{
	switch (info->fstype) {
		case EXFAT_FILESYSTEM:
			{
				void *data = get_cluster(info, cluster);
				if (data) {
					fprintf(info->out, "Cluster #%u:\n", cluster);
					exfat_print_cluster(info, data);
					free(data);
				}
				break;
			}
		case FAT12_FILESYSTEM:
			/* FALLTHROUGH */
		case FAT16_FILESYSTEM:
			/* FIXME: Unimplemented*/
			break;
		case FAT32_FILESYSTEM:
			/* FIXME: Unimplemented*/
			break;
		default:
			dump_err("invalid filesystem image.");
			return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int opt;
	int longindex;
	int ret = 0;
	bool cflag = false;
	uint32_t cluster = 0;
	bool outflag = false;
	char *outfile = NULL;
	struct device_info info;
	struct pseudo_bootsector bootsec;

	while ((opt = getopt_long(argc, argv,
					"c:o:v",
					longopts, &longindex)) != -1) {
		switch (opt) {
			case 'c':
				cflag = true;
				cluster = strtoul(optarg, NULL, 0);
				break;
			case 'o':
				outflag = true;
				outfile = optarg;
				break;
			case 'v':
				print_level = DUMP_INFO;
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
	print_level = DUMP_DEBUG;
#endif

	if (optind != argc - 1) {
		usage();
		exit(EXIT_FAILURE);
	}

	if(outflag) {
		if ((info.out = fopen(outfile, "w")) == NULL) {
			dump_err("can't open %s.", optarg);
			exit(EXIT_FAILURE);
		}
	} else {
		info.out = stdout;
	}

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

	if (cflag) {
		ret = pseudo_print_cluster(&info, cluster);
		if (ret < 0)
			goto file_err;
	}

file_err:
	close(info.fd);

out:
	free_list(info.chain_head);
	if(outflag)
		fclose(info.out);

	return ret;
}
