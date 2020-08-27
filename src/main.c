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
	{"interactive", no_argument, NULL, 'i'},
	{"load", required_argument, NULL, 'l'},
	{"output", required_argument, NULL, 'o'},
	{"ro", no_argument, NULL, 'r'},
	{"save", required_argument, NULL, 's'},
	{"upper", required_argument, NULL, 'u'},
	{"verbose", no_argument, NULL, 'v'},
	{"help", no_argument, NULL, GETOPT_HELP_CHAR},
	{"version", no_argument, NULL, GETOPT_VERSION_CHAR},
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

	fprintf(stderr, "  -a, --all\tTrverse all directories.\n");
	fprintf(stderr, "  -b, --byte=offset\tdump the any byte after dump filesystem information.\n");
	fprintf(stderr, "  -c, --cluster=index\tdump the cluster index after dump filesystem information.\n");
	fprintf(stderr, "  -i, --interactive\tprompt the user operate filesystem.\n");
	fprintf(stderr, "  -l, --load=file\tLoad Main boot region and FAT region from file.\n");
	fprintf(stderr, "  -o, --output=file\tsend output to file rather than stdout.\n");
	fprintf(stderr, "  -r, --ro\tread only mode. \n");
	fprintf(stderr, "  -s, --save=file\tSave Main boot region and FAT region in file.\n");
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
 * @data:       Sector raw data (Output)
 * @index:      Start bytes
 * @count:      The number of sectors
 *
 * @return        0 (success)
 *               -1 (failed to read)
 *
 * NOTE: Need to allocate @data before call it.
 */
int get_sector(void *data, off_t index, size_t count)
{
	size_t sector_size = info.sector_size;

	pr_debug("Get: Sector from %lx to %lx\n", index , index + (count * sector_size) - 1);
	if ((pread(info.fd, data, count * sector_size, index)) < 0) {
		pr_err("can't read %s.", info.name);
		return -1;
	}
	return 0;
}

/**
 * set_sector - Set Raw-Data from any sector.
 * @data:       Sector raw data
 * @index:      Start bytes
 * @count:      The number of sectors
 *
 * @return        0 (success)
 *               -1 (failed to read)
 *
 * NOTE: Need to allocate @data before call it.
 */
int set_sector(void *data, off_t index, size_t count)
{
	size_t sector_size = info.sector_size;

	pr_debug("Set: Sector from %lx to %lx\n", index , index + (count * sector_size) - 1);
	if ((pwrite(info.fd, data, count * sector_size, index)) < 0) {
		pr_err("can't write %s.", info.name);
		return -1;
	}
	return 0;
}

/**
 * get_cluster - Get Raw-Data from any cluster.
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
 * set_cluster - Set Raw-Data from any cluster.
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
 * get_clusters - Get Raw-Data from any cluster.
 * @data:        cluster raw data (Output)
 * @index:      Start cluster index
 * @num:        The number of clusters
 *
 * @return        0 (success)
 *               -1 (failed to read)
 *
 * NOTE: Need to allocate @data before call it.
 */
int get_clusters(void *data, off_t index, size_t num)
{
	size_t clu_per_sec = info.cluster_size / info.sector_size;
	off_t heap_start = info.heap_offset * info.sector_size;

	if (index < 2 || index + num > info.cluster_count) {
		pr_err("invalid cluster index %lu.", index);
		return -1;
	}

	return get_sector(data,
			heap_start + ((index - 2) * info.cluster_size),
			clu_per_sec * num);
}

/**
 * set_clusters - Set Raw-Data from any cluster.
 * @data:        cluster raw data
 * @index:       Start cluster index
 * @num:         The number of clusters
 *
 * @return        0 (success)
 *               -1 (failed to read)
 *
 * NOTE: Need to allocate @data before call it.
 */
int set_clusters(void *data, off_t index, size_t num)
{
	size_t clu_per_sec = info.cluster_size / info.sector_size;
	off_t heap_start = info.heap_offset * info.sector_size;

	if (index < 2 || index + num > info.cluster_count) {
		pr_err("invalid cluster index %lu.", index);
		return -1;
	}

	return set_sector(data,
			heap_start + ((index - 2) * info.cluster_size),
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
 * @return        0 (success)
 *               -1 (failed to open)
 */
static int get_device_info(uint32_t attr)
{
	int fd;
	struct stat s;

	if ((fd = open(info.name, attr & OPTION_READONLY ? O_RDONLY : O_RDWR)) < 0) {
		pr_err("can't open %s.", info.name);
		return -1;
	}

	if (fstat(fd, &s) < 0) {
		pr_err("can't get stat %s.", info.name);
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
 * @return        Number of lists freed
 *
 * TODO: Check for memory leaks
 */
static int free_dentry_list(void)
{
	int i;
	for(i = 0; i < info.root_size && info.root[i]; i++) {
		/* FIXME: There may be areas that have not been released. */
		info.ops->clean(i);
	}
	free((*(info.root))->data);
	free(*(info.root));
	free(info.root);

	return i;
}

/**
 * pseudo_check_filesystem - virtual function to check filesystem
 * @boot:      boot sector pointer
 *
 * return:     0  (succeeded in obtaining filesystem)
 *             -1 (failed)
 */
static int pseudo_check_filesystem(struct pseudo_bootsec *boot)
{
	size_t count = 0;

	count = pread(info.fd, boot, SECSIZE, 0);
	if (count < 0){
		pr_err("can't read %s.", info.name);
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
 * return:     0  (succeeded in obtaining filesystem)
 */
static int print_sector(uint32_t sector)
{
	void *data;

	data = malloc(info.sector_size);
	if (!get_sector(data, sector, 1)) {
		pr_msg("Sector #%u:\n", sector);
		hexdump(output, data, info.sector_size);
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
		hexdump(output, data, info.cluster_size);
	}
	free(data);
	return 0;
}

/**
 * query_param - prompt user from parameter
 * @q:           query paramater
 * @param:       parameter (output)
 * @def:         default parameter
 * @size:        byte size in @param
 *
 * @return        0 (success)
 *               -1 (failed)
 */
int query_param(const struct query q, void *param, unsigned int def, size_t size)
{
	int i;
	char buf[QUERY_BUFFER_SIZE] = {};

	pr_msg("%s\n", q.name);
	for (i = 0; i < q.len; i++)
		pr_msg("%s\n", q.select[i]);
	pr_msg("Select (Default %0x): ", def);
	fflush(stdout);

	if (!fgets(buf, QUERY_BUFFER_SIZE, stdin))
		return -1;

	pr_msg("\n");
	switch (size) {
		case 1:
			if (buf[0] == '\n')
				*(uint8_t *)param = def;
			else 
				sscanf(buf, "%02hhx", (uint8_t *)param);
			break;
		case 2:
			if (buf[0] == '\n')
				*(uint16_t *)param = def;
			else 
				sscanf(buf, "%04hx", (uint16_t *)param);
			break;
		case 4:
			if (buf[0] == '\n')
				*(uint32_t *)param = def;
			else 
				sscanf(buf, "%08x", (uint32_t *)param);
			break;
		case 8:
			if (buf[0] == '\n')
				*(uint64_t *)param = def;
			else 
				sscanf(buf, "%016lx", (uint64_t *)param);
			break;
		default:
			pr_warn("size should be param length.\n");
			return -1;
	}
	
	return 0;
}

/**
 * main - main function
 * @argc:      argument count
 * @argv:      argument vector
 */
int main(int argc, char *argv[])
{
	int i;
	int opt;
	int longindex;
	int ret = 0;
	int entries = 0;
	uint32_t attr = 0;
	uint32_t cluster = 0;
	uint32_t sector = 0;
	char *outfile = NULL;
	char *backup = NULL;
	char *input = NULL;
	char out[MAX_NAME_LENGTH + 1] = {};
	char *s;
	FILE *bfile = NULL;
	struct pseudo_bootsec bootsec;
	struct directory *dirs = NULL, *dirs_tmp = NULL;

	while ((opt = getopt_long(argc, argv,
					"ab:c:il:o:rs:u:v",
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
			case 'i':
				attr |= OPTION_INTERACTIVE;
				break;
			case 'l':
				attr |= OPTION_LOAD;
				backup = optarg;
				break;
			case 'o':
				attr |= OPTION_OUTPUT;
				outfile = optarg;
				break;
			case 'r':
				attr |= OPTION_READONLY;
				break;
			case 's':
				attr |= OPTION_SAVE;
				backup = optarg;
				break;
			case 'u':
				attr |= OPTION_UPPER;
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

#ifdef DEBUGFATFS_DEBUG
	print_level = PRINT_DEBUG;
#endif

	if (optind != argc - 1) {
		usage();
		exit(EXIT_FAILURE);
	}

	init_device_info();
	info.attr = attr;

	output = stdout;
	if (attr & OPTION_OUTPUT) {
		if ((output = fopen(outfile, "w")) == NULL) {
			pr_err("can't open %s.", optarg);
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

	/* Command line: -s, -l option */
	if ((attr & OPTION_SAVE) || (attr & OPTION_LOAD)) {
		if ((bfile = fopen(backup, "ab+")) == NULL) {
			pr_err("can't open %s.", optarg);
			goto device_close;
		}
		s = malloc(sizeof(char) * info.sector_size);
		if (attr & OPTION_SAVE) {
			/* Save Phase */
			for (i = 0; i < info.heap_offset; i++) {
				get_sector(s, i * info.sector_size, 1);
				fwrite(s, info.sector_size, 1, bfile);
			}
		} else {
			/* load Phase */
			for (i = 0; i < info.heap_offset; i++) {
				fwrite(s, info.sector_size, 1, bfile);
				set_sector(s, i * info.sector_size, 1);
			}
		}
		free(s);
		fclose(bfile);
		goto device_close;
	}

	ret = info.ops->statfs();
	if (ret < 0)
		goto device_close;

	dirs = malloc(sizeof(struct directory) * DIRECTORY_FILES);
	ret = info.ops->readdir(dirs, DIRECTORY_FILES, info.root_offset);
	if (ret < 0) {
		/* Only once, expand dirs structure and execute readdir */
		ret = abs(ret) + 1;
		dirs_tmp = realloc(dirs, sizeof(struct directory) * (DIRECTORY_FILES + ret));
		if (dirs_tmp) {
			dirs = dirs_tmp;
			ret = info.ops->readdir(dirs, DIRECTORY_FILES + ret, info.root_offset);
			if (ret < 0)
				goto out;
		} else {
			pr_err("Can't load Root directory because of failed to allocate space.\n");
			goto out;
		}
	}

	entries = ret;
	pr_msg("Read \"/\" Directory (%d entries).\n", entries);
	for (i = 0; i < entries; i++)
		pr_msg("%s ", dirs[i].name);

	pr_msg("\n");
	ret = 0;

	/* Command line: -a option */
	if (attr & OPTION_ALL) {
		ret = info.ops->info();
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

out:
	for (i = 0; i < entries; i++)
		free(dirs[i].name);
	free(dirs);
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
