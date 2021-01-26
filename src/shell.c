// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2020 LeavaTail
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell.h"
#include "debugfatfs.h"

static uint32_t cluster = 0;

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
static int cmd_update(int, char **, char **);
static int cmd_trim(int, char **, char **);
static int cmd_fill(int, char **, char **);
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
	{"update", cmd_update},
	{"trim", cmd_trim},
	{"fill", cmd_fill},
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
		sprintf(len, "%8lu", dirs[i].datalen);
		sprintf(time, "%d-%02d-%02d %02d:%02d:%02d",
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
	char pwd[CMD_MAXLEN + 1] = {};

	switch (argc) {
		case 1:
			dir = info.root_offset;
			break;
		case 2:
			get_env(envp, "PWD", pwd);
			dir = info.ops->lookup(cluster, argv[1]);
			if (argv[1][0] == '/')
				path = argv[1];
			else
				path = strcat(pwd, argv[1]);
			break;
		default:
			fprintf(stdout, "%s: too many arguments.\n", argv[0]);
			break;
	}

	if (strcmp(path, "/"))
		path = strcat(path, "/");

	if (dir >= 0) {
		cluster = dir;
		set_env(envp, "PWD", path);
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
			info.ops->create(argv[optind], cluster, create_option);
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
	switch (argc) {
		case 1:
			fprintf(stdout, "%s: too few arguments.\n", argv[0]);
			break;
		case 2:
			info.ops->remove(argv[1], cluster, 0);
			info.ops->reload(cluster);
			break;
		default:
			fprintf(stdout, "%s: too many arguments.\n", argv[0]);
			break;
	}
	return 0;
}

/**
 * cmd_update - Update directory entry
 * @argc:       argument count
 * @argv:       argument vetor
 * @envp:       environment pointer
 *
 * @return      0 (success)
 */
static int cmd_update(int argc, char **argv, char **envp)
{
	int index;

	switch (argc) {
		case 1:
			fprintf(stdout, "%s: too few arguments.\n", argv[0]);
			break;
		case 2:
			index = strtoul(argv[1], NULL, 10);
			info.ops->update(cluster, index);
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
	fprintf(stderr, "update     update directory entry.\n");
	fprintf(stderr, "trim       trim deleted dentry.\n");
	fprintf(stderr, "fill       fill in directory.\n");
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
	char *token;
	char *copy;
	size_t len;

	token = strtok(str, CMD_DELIM);
	while (token != NULL) {
		len = strlen(token);
		copy = malloc(sizeof(char) * (len + 1));
		strcpy(copy, token);
		argv[argc++] = copy;
		token = strtok(NULL, CMD_DELIM);
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
	char *str = malloc(sizeof(char) * (CMD_MAXLEN + 1));

	for (i = 0; envp[i]; i++) {
		if (!strcmp(strtok(envp[i], "="), env)) {
			free(envp[i]);
			break;
		}
	}
	snprintf(str, CMD_MAXLEN, "%s=%s", env, value);
	envp[i] = str;

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
	char str[CMD_MAXLEN + 1] = {};

	for (i = 0; envp[i]; i++) {
		strncpy(str, envp[i], CMD_MAXLEN);
		tp = strtok(str, "=");
		if (!strcmp(tp, env)) {
			tp = strtok(NULL, "=");
			strncpy(value, tp, CMD_MAXLEN);
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
	int argc = 0;
	char buf[CMD_MAXLEN + 1] = {};
	char **argv = malloc(sizeof(char *) * (CMD_MAXLEN + 1));
	char **envp = malloc(sizeof(char *) * 16);

	fprintf(stdout, "Welcome to %s %s (Interactive Mode)\n\n", PROGRAM_NAME, PROGRAM_VERSION);
	init_env(envp);
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

	free(argv);
	free(envp);
	return 0;
}
