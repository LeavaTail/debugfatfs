// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2021 LeavaTail
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell.h"
#include "debugfatfs.h"

static uint32_t cluster = 0;

static int format_path(char *, size_t, char *, char **);

static int set_env(char **, char *, char *);
static int get_env(char **, char *, char *);

static int cmd_ls(int, char **, char **);
static int cmd_cd(int, char **, char **);
static int cmd_cluster(int, char **, char **);
static int cmd_entry(int, char **, char **);
static int cmd_alloc(int, char **, char **);
static int cmd_release(int, char **, char **);
static int cmd_fat(int, char **, char **);
static int cmd_create(int, char **, char **);
static int cmd_remove(int, char **, char **);
static int cmd_trim(int, char **, char **);
static int cmd_fill(int, char **, char **);
static int cmd_tail(int, char **, char **);
static int cmd_help(int, char **, char **);
static int cmd_exit(int, char **, char **);

/**
 * command list
 */
struct command cmd[] = {
	{"ls", cmd_ls},
	{"cd", cmd_cd},
	{"cluster", cmd_cluster},
	{"entry", cmd_entry},
	{"alloc", cmd_alloc},
	{"release", cmd_release},
	{"fat", cmd_fat},
	{"create", cmd_create},
	{"remove", cmd_remove},
	{"trim", cmd_trim},
	{"fill", cmd_fill},
	{"tail", cmd_tail},
	{"help", cmd_help},
	{"exit", cmd_exit},
};

/**
 * cmd_ls - List directory contests.
 * @argc:   argument count
 * @argv:   argument vetor
 * @envp:   environment pointer
 *
 * @return  0 (success)
 */
static int cmd_ls(int argc, char **argv, char **envp)
{
	int i = 0, ret = 0;
	char ro = 'R', hidden = 'H', sys = 'S', dir = 'D', arch = 'A';
	char len[16] = {}, time[64] = {};
	struct tm t;
	struct directory *dirs = NULL, *dirs_tmp = NULL;

	dirs = malloc(sizeof(struct directory) * DIRECTORY_FILES);
	ret = info.ops->readdir(dirs, DIRECTORY_FILES, cluster);
	if (ret < 0) {
		/* Only once, expand dirs structure and execute readdir */
		ret = abs(ret) + 1;
		dirs_tmp = realloc(dirs, sizeof(struct directory) * (DIRECTORY_FILES + ret));
		if (dirs_tmp) {
			dirs = dirs_tmp;
			ret = info.ops->readdir(dirs, DIRECTORY_FILES + ret, cluster);
		} else {
			fprintf(stdout, "ls: failed to load firectory.\n");
			return 1;
		}
	}

	for (i = 0; i < ret; i++) {
		t = dirs[i].ctime;
		snprintf(len, sizeof(len), "%8zu", dirs[i].datalen);
		snprintf(time, sizeof(time), "%d-%02d-%02d %02d:%02d:%02d",
			1980 + t.tm_year, t.tm_mon, t.tm_mday,
			t.tm_hour, t.tm_min, t.tm_sec);
		fprintf(stdout, "%c", (dirs[i].attr & ATTR_READ_ONLY) ? ro : '-');
		fprintf(stdout, "%c", (dirs[i].attr & ATTR_HIDDEN) ? hidden : '-');
		fprintf(stdout, "%c", (dirs[i].attr & ATTR_SYSTEM) ? sys : '-');
		fprintf(stdout, "%c", (dirs[i].attr & ATTR_DIRECTORY) ? dir : '-');
		fprintf(stdout, "%c", (dirs[i].attr & ATTR_ARCHIVE) ? arch : '-');
		fprintf(stdout, " %s", len);
		fprintf(stdout, " %s", time);
		fprintf(stdout, " %s ", dirs[i].name);
		fprintf(stdout, "\n");
	}

	fprintf(stdout, "\n");
	return 0;
}

/**
 * cmd_cd - Change the directory.
 * @argc:   argument count
 * @argv:   argument vetor
 * @envp:   environment pointer
 *
 * @return  0 (success)
 */
static int cmd_cd(int argc, char **argv, char **envp)
{
	int dir = 0;
	char *path = "/";
	char buf[ARG_MAXLEN] = {};
	char pwd[ARG_MAXLEN] = {};

	switch (argc) {
		case 1:
			dir = info.root_offset;
			snprintf(pwd, ARG_MAXLEN, "/");
			break;
		case 2:
			format_path(buf, ARG_MAXLEN, argv[1], envp);
			dir = info.ops->lookup(cluster, buf);
			snprintf(pwd, ARG_MAXLEN, "%s", buf);
			break;
		default:
			fprintf(stdout, "%s: too many arguments.\n", argv[0]);
			break;
	}

	if (strcmp(path, "/"))
		snprintf(pwd, ARG_MAXLEN, "/");

	if (dir >= 0 && *pwd) {
		cluster = dir;
		set_env(envp, "PWD", pwd);
	}

	return 0;
}

/**
 * cmd_cluster - Get cluster raw data.
 * @argc:        argument count
 * @argv:        argument vetor
 * @envp:        environment pointer
 *
 * @return       0 (success)
 */
static int cmd_cluster(int argc, char **argv, char **envp)
{
	switch (argc) {
		case 1:
			fprintf(stdout, "%s: too few arguments.\n", argv[0]);
			break;
		case 2:
			print_cluster(strtoul(argv[1], NULL, 10));
			break;
		default:
			fprintf(stdout, "%s: too many arguments.\n", argv[0]);
			break;
	}

	return 0;
}

/**
 * cmd_entry - Print entry in current directory
 * @argc:      argument count
 * @argv:      argument vetor
 * @envp:      environment pointer
 *
 * @return     0 (success)
 */
static int cmd_entry(int argc, char **argv, char **envp)
{
	size_t index = 0;

	switch (argc) {
		case 1:
			fprintf(stdout, "%s: too few arguments.\n", argv[0]);
			break;
		case 2:
			index = strtoul(argv[1], NULL, 10);
			info.ops->dentry(cluster, index);
			break;
		default:
			fprintf(stdout, "%s: too many arguments.\n", argv[0]);
			break;
	}

	return 0;
}

/**
 * cmd_alloc - Allocate cluster in bitmap.
 * @argc:      argument count
 * @argv:      argument vetor
 * @envp:      environment pointer
 *
 * @return     0 (success)
 */
static int cmd_alloc(int argc, char **argv, char **envp)
{
	unsigned int index = 0;

	switch (argc) {
		case 1:
			fprintf(stdout, "%s: too few arguments.\n", argv[0]);
			break;
		case 2:
			index = strtoul(argv[1], NULL, 10);
			info.ops->alloc(index);
			fprintf(stdout, "Alloc: cluster %u.\n", index);
			break;
		default:
			fprintf(stdout, "%s: too many arguments.\n", argv[0]);
			break;
	}
	return 0;
}

/**
 * cmd_release - Release cluster in bitmap.
 * @argc:        argument count
 * @argv:        argument vetor
 * @envp:        environment pointer
 *
 * @return       0 (success)
 */
static int cmd_release(int argc, char **argv, char **envp)
{
	unsigned int index = 0;

	switch (argc) {
		case 1:
			fprintf(stdout, "%s: too few arguments.\n", argv[0]);
			break;
		case 2:
			index = strtoul(argv[1], NULL, 10);
			info.ops->release(index);
			fprintf(stdout, "Release: cluster %u.\n", index);
			break;
		default:
			fprintf(stdout, "%s: too many arguments.\n", argv[0]);
			break;
	}
	return 0;
}

/**
 * cmd_fat - Set/Get FAT entry.
 * @argc:    argument count
 * @argv:    argument vetor
 * @envp:    environment pointer
 *
 * @return   0 (success)
 */
static int cmd_fat(int argc, char **argv, char **envp)
{
	unsigned int index = 0, entry = 0;

	switch (argc) {
		case 1:
			fprintf(stdout, "%s: too few arguments.\n", argv[0]);
			break;
		case 2:
			index = strtoul(argv[1], NULL, 10);
			info.ops->getfat(index, &entry);
			fprintf(stdout, "Get: Cluster %u is FAT entry %08x\n", index, entry);
			break;
		case 3:
			index = strtoul(argv[1], NULL, 10);
			entry = strtoul(argv[2], NULL, 16);
			info.ops->setfat(index, entry);
			fprintf(stdout, "Set: Cluster %u is FAT entry %08x\n", index, entry);
			break;
		default:
			fprintf(stdout, "%s: too many arguments.\n", argv[0]);
			break;
	}
	return 0;
}

/**
 * cmd_create - Create file or Directory.
 * @argc:       argument count
 * @argv:       argument vetor
 * @envp:       environment pointer
 *
 * @return      0 (success)
 */
static int cmd_create(int argc, char **argv, char **envp)
{
	int opt, create_option = 0;
	char buf[ARG_MAXLEN] = {};
	char *filename;

	/* To restart scanning a new argument vector */
	optind = 1;

	while ((opt = getopt(argc, argv, "d")) != -1) {
		switch (opt) {
			case 'd':
				create_option = CREATE_DIRECTORY;
				break;
			default:
				fprintf(stderr,"Usage: %s [-d] FILE\n", argv[0]);
				fprintf(stderr, "\n");
				fprintf(stderr, "  -d\tCreate directory\n");
				return 0;
		}
	}

	switch (argc - optind) {
		case 0:
			fprintf(stdout, "%s: too few arguments.\n", argv[0]);
			break;
		case 1:
			filename = strtok_dir(argv[optind]);
			if (filename != argv[optind]) {
				pr_warn("Create doesn't support Absolute path.\n");
				break;
			}
			info.ops->create(filename, cluster, create_option);
			info.ops->reload(cluster);
			break;
		default:
			fprintf(stdout, "%s: too many arguments.\n", argv[0]);
			break;
	}
	return 0;
}

/**
 * cmd_remove - Remove file or Directory.
 * @argc:       argument count
 * @argv:       argument vetor
 * @envp:       environment pointer
 *
 * @return      0 (success)
 */
static int cmd_remove(int argc, char **argv, char **envp)
{
	char buf[ARG_MAXLEN] = {};
	char *filename;

	switch (argc) {
		case 1:
			fprintf(stdout, "%s: too few arguments.\n", argv[0]);
			break;
		case 2:
			filename = strtok_dir(argv[1]);
			if (filename != argv[1]) {
				pr_warn("Create doesn't support Absolute path.\n");
				break;
			}
			info.ops->remove(filename, cluster, 0);
			info.ops->reload(cluster);
			break;
		default:
			fprintf(stdout, "%s: too many arguments.\n", argv[0]);
			break;
	}
	return 0;
}

/**
 * cmd_trim - Trim unsed dentry
 * @argc:     argument count
 * @argv:     argument vetor
 * @envp:     environment pointer
 *
 * @return    0 (success)
 */
static int cmd_trim(int argc, char **argv, char **envp)
{
	switch (argc) {
		case 1:
			info.ops->trim(cluster);
			break;
		default:
			fprintf(stdout, "%s: too many arguments.\n", argv[0]);
			break;
	}
	return 0;
}

/**
 * cmd_fill - fill in directory
 * @argc:     argument count
 * @argv:     argument vetor
 * @envp:     environment pointer
 *
 * @return    0 (success)
 */
static int cmd_fill(int argc, char **argv, char **envp)
{
	unsigned int count = 0;

	switch (argc) {
		case 1:
			info.ops->fill(cluster, info.cluster_size / sizeof(struct exfat_dentry));
			break;
		case 2:
			count = strtoul(argv[1], NULL, 10);
			info.ops->fill(cluster, count);
			break;
		default:
			fprintf(stdout, "%s: too many arguments.\n", argv[0]);
			break;
	}
	info.ops->reload(cluster);

	return 0;
}

/**
 * cmd_tail - Display file contents.
 * @argc:       argument count
 * @argv:       argument vetor
 * @envp:       environment pointer
 *
 * @return      0 (success)
 */
static int cmd_tail(int argc, char **argv, char **envp)
{
	uint32_t clu = 0;

	switch (argc) {
		case 1:
			fprintf(stdout, "%s: too few arguments.\n", argv[0]);
			break;
		case 2:
			info.ops->contents(argv[1], cluster, 0);
			break;
		default:
			fprintf(stdout, "%s: too many arguments.\n", argv[0]);
			break;
	}
	return 0;
}

/**
 * cmd_help - display help
 * @argc:     argument count
 * @argv:     argument vector
 * @envp:     environment pointer
 *
 * @return    0 (success)
 */
static int cmd_help(int argc, char **argv, char **envp)
{
	fprintf(stderr, "ls         list current directory contents.\n");
	fprintf(stderr, "cd         change directory.\n");
	fprintf(stderr, "cluster    print cluster raw-data.\n");
	fprintf(stderr, "entry      print directory entry.\n");
	fprintf(stderr, "alloc      allocate cluster.\n");
	fprintf(stderr, "release    release cluster.\n");
	fprintf(stderr, "fat        change File Allocation Table entry\n");
	fprintf(stderr, "create     create directory entry.\n");
	fprintf(stderr, "remove     remove directory entry.\n");
	fprintf(stderr, "trim       trim deleted dentry.\n");
	fprintf(stderr, "fill       fill in directory.\n");
	fprintf(stderr, "tail       output the last part of files.\n");
	fprintf(stderr, "help       display this help.\n");
	fprintf(stderr, "\n");
	return 0;
}

/**
 * cmd_exit - Cause the shell to exit.
 * @argc:     argument count
 * @argv:     argument vetor
 * @envp:     environment pointer
 *
 * @return    1
 */
static int cmd_exit(int argc, char **argv, char **envp)
{
	fprintf(stdout, "Goodbye!\n");
	return 1;
}

/**
 * format_path - format pathname
 * @dist:        formatted file path (Output)
 * @len:         filepath length
 * @str          filepath
 *
 * @return       0
 */
static int format_path(char *dist, size_t len, char *str, char **envp)
{
	int i = 1;
	char *saveptr = NULL;
	char *token;
	char *buf;

	if ((token = strtok_r(str, "/", &saveptr)) == NULL) {
		snprintf(dist, strlen("/") + 1, "/");
		return 0;
	}

	buf = calloc(ARG_MAXLEN, sizeof(char));
	/* Create full path */
	get_env(envp, "PWD", buf);

	/* If str is "/A/B/C" or "/", then "/" else concatenate PWD */
	if ((str[0] == '/') || (!strncmp(buf, "/", strlen("/") + 1)))
		snprintf(dist, strlen("/") + 1, "/");
	else 
		snprintf(dist, len, "%s/", buf);

	/* Remove redundant "/" */
	snprintf(buf, len, "%s%s", dist, token);
	strncpy(dist, buf, len);

	while ((token = strtok_r(NULL, "/", &saveptr)) != NULL) {
		snprintf(buf, len, "%s/%s", dist, token);
		strncpy(dist, buf, len);
	}

	free(buf);

	return 0;
}

/**
 * execute_cmd - Execute registerd command
 * @argc:        argument count
 * @argv:        argument vetor
 * @envp:        environment pointer
 *
 * @return       0 (continue shell)
 *               1 (exit shell)
 */
static int execute_cmd(int argc, char **argv, char **envp)
{
	int i;

	if (!argc)
		return 0;

	for (i = 0; i < (sizeof(cmd) / sizeof(struct command)); i++) {
		if(!strcmp(argv[0], cmd[i].name))
			return cmd[i].func(argc, argv, envp);
	}

	fprintf(stdout, "%s: command not found\n", argv[0]);
	return 0;
}

/**
 * decode_cmd - Interpret strings as commands
 * @str         Input string
 * @argv:       argument vetor (Output)
 * @envp:       environment pointer
 *
 * @return      argument count
 */
static int decode_cmd(char *str, char **argv, char **envp)
{
	int argc = 0;
	char *saveptr = NULL;
	char *token;

	token = strtok_r(str, CMD_DELIM, &saveptr);
	while ((token != NULL) && (argc < ARG_MAXLEN)) {
		snprintf(argv[argc++], ARG_MAXLEN, "%s", token);
		token = strtok_r(NULL, CMD_DELIM, &saveptr);
	}
	return argc;
}

/**
 * read_cmd - Prompt for a string
 * @buf:      string (Output)
 *
 * @return    0 (success)
 *            1 (failed)
 */
static int read_cmd(char *buf)
{
	if (fgets(buf, CMD_MAXLEN, stdin) == NULL) {
		return 1;
	}
	return 0;
}

/**
 * set_env - Set environment
 * @envp:    environment pointer
 * @env:     environment
 * @value:   parameter
 *
 * @return   0
 */
static int set_env(char **envp, char *env, char *value)
{
	int i;
	char str[ARG_MAXLEN] = {0};
	char *saveptr = NULL;
	char *token;

	for (i = 0; i < ENV_MAXNUM - 1; i++) {
		strncpy(str, envp[i], ARG_MAXLEN);
		token = strtok_r(str, "=", &saveptr);
		if (token && !strcmp(token, env))
			break;
	}
	snprintf(envp[i], CMD_MAXLEN, "%s=%s", env, value);

	return 0;
}

/**
 * get_env - Get environment
 * @envp:    environment pointer
 * @env:     environment
 * @value:   parameter (Output)
 *
 * @return   0 (Found)
 *           1 (Not found)
 */
static int get_env(char **envp, char *env, char *value)
{
	int i;
	char *tp;
	char str[ARG_MAXLEN] = {0};
	char *saveptr = NULL;

	for (i = 0; envp[i]; i++) {
		strncpy(str, envp[i], ARG_MAXLEN);
		tp = strtok_r(str, "=", &saveptr);
		if (tp && !strcmp(tp, env)) {
			tp = strtok_r(NULL, "=", &saveptr);
			strncpy(value, tp, ARG_MAXLEN);
			return 0;
		}
	}
	return 1;
}

/**
 * init_env - Initialize environment
 * @envp:     environment pointer
 *
 * @return    0
 */
static int init_env(char **envp)
{
	cluster = info.root_offset;
	set_env(envp, "PWD", "/");
	return 0;
}

/**
 * shell  - Interactive main function
 *
 * @return  0
 */
int shell(void)
{
	int i, argc = 0;
	char buf[CMD_MAXLEN] = {};
	char **argv = calloc(ARG_MAXNUM, sizeof(char *));
	char **envp = calloc(ENV_MAXNUM, sizeof(char *));

	for (i = 0; i < ARG_MAXNUM; i++)
		argv[i] = calloc(ARG_MAXLEN, sizeof(char));
	for (i = 0; i < ENV_MAXNUM; i++)
		envp[i] = calloc(ARG_MAXLEN, sizeof(char));

	fprintf(stdout, "Welcome to %s %s (Interactive Mode)\n\n", PROGRAM_NAME, PROGRAM_VERSION);
	init_env(envp);
	srand(time(NULL));
	info.ops->readdir(NULL, 0, cluster);
	while (1) {
		get_env(envp, "PWD", buf);
		fprintf(stdout, "%s> ", buf);
		fflush(stdout);
		if (read_cmd(buf))
			break;
		argc = decode_cmd(buf, argv, envp);
		if (execute_cmd(argc, argv, envp))
			break;
	}

	for (i = 0; i < ENV_MAXNUM; i++)
		free(envp[i]);
	for (i = 0; i < ARG_MAXNUM; i++)
		free(argv[i]);

	free(argv);
	free(envp);
	return 0;
}
