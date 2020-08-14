#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shell.h"
#include "dumpexfat.h"

static uint32_t cluster = 0;

static int set_env(char **, char *, char *);
static int get_env(char **, char *, char *);

static int cmd_ls(int, char **, char **);
static int cmd_cd(int, char **, char **);
static int cmd_exit(int, char **, char **);

/**
 * command list
 */
struct command cmd[] = {
	{"ls", cmd_ls},
	{"cd", cmd_cd},
	{"exit", cmd_exit},
};

/**
 * cmd_ls     - List directory contests.
 * @argc:       argument count
 * @argv:       argument vetor
 * @envp:       environment pointer
 *
 * @return        0 (success)
 *
 * TODO: Use 10msIncrement and shift timestamp
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
 * cmd_cd     - Change the directory.
 * @argc:       argument count
 * @argv:       argument vetor
 * @envp:       environment pointer
 *
 * @return        0 (success)
 *
 * TODO: Check whether pathname is a directory or file
 */
static int cmd_cd(int argc, char **argv, char **envp)
{
	int dir = 0;
	char *path = "/";

	switch (argc) {
		case 1:
			dir = info.root_offset;
			break;
		case 2:
			dir = info.ops->lookup(cluster, argv[1]);
			path = argv[1];
			break;
		default:
			fprintf(stdout, "%s: too many arguments.\n", argv[0]);
			break;
	}

	if (dir) {
		cluster = dir;
		set_env(envp, "PWD", path);
	}

	return 0;
}

/**
 * cmd_exit   - Cause the shell to exit.
 * @argc:       argument count
 * @argv:       argument vetor
 * @envp:       environment pointer
 *
 * @return      1
 */
static int cmd_exit(int argc, char **argv, char **envp)
{
	fprintf(stdout, "Goodbye!\n");
	return 1;
}

/**
 * execute_cmd    - Execute registerd command
 * @argc:           argument count
 * @argv:           argument vetor
 * @envp:           environment pointer
 *
 * @return      0 (continue shell)
 *              1 (exit shell)
 */
static int execute_cmd(int argc, char **argv, char **envp)
{
	int i, ret;
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
 * decode_cmd     - Interpret strings as commands
 * @str             Input string
 * @argv:           argument vetor (Output)
 * @envp:           environment pointer
 *
 * @return          argument count
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
 * read_cmd       - Prompt for a string
 * @buf:            string (Output)
 *
 * @return          0 (success)
 *                  1 (failed)
 */
static int read_cmd(char *buf)
{
	if (fgets(buf, CMD_MAXLEN, stdin) == NULL) {
		return 1;
	}
	return 0;
}

/**
 * set_env        - Set environment
 * @envp:           environment pointer
 * @env:            environment
 * @value:          parameter
 *
 * @return          0
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
 * get_env        - Get environment
 * @envp:           environment pointer
 * @env:            environment
 * @value:          parameter (Output)
 *
 * @return          0 (Found)
 *                  1 (Not found)
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
 * init_env       - Initialize environment
 * @envp:           environment pointer
 *
 * @return          0
 */
static int init_env(char **envp)
{
	cluster = info.root_offset;
	set_env(envp, "PWD", "/");
	return 0;
}

/**
 * shell          - Interactive main function
 *
 * @return          0
 */
int shell(void)
{
	int argc = 0;
	char buf[CMD_MAXLEN + 1] = {};
	char **argv = malloc(sizeof(char *) * (CMD_MAXLEN + 1));
	char **envp = malloc(sizeof(char *) * 16);

	fprintf(stdout, "Welcome to %s %s (Interactive Mode)\n\n", PROGRAM_NAME, PROGRAM_VERSION);
	init_env(envp);
	while (1) {
		get_env(envp, "PWD", buf);
		fprintf(stdout, "%s> ", buf);
		fflush(stdout);
		read_cmd(buf);
		argc = decode_cmd(buf, argv, envp);
		if (execute_cmd(argc, argv, envp))
			break;
	}

	free(argv);
	free(envp);
	return 0;
}
