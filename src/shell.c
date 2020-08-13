#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shell.h"
#include "dumpexfat.h"

static uint32_t cluster = 0;

static int cmd_ls(int, char **, char **);
static int cmd_exit(int, char **, char **);

struct command cmd[] = {
	{"ls", cmd_ls},
	{"exit", cmd_exit},
};

static int cmd_ls(int argc, char **argv, char **envp)
{
	int i = 0, ret = 0;
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

	for (i = 0; i < ret; i++)
		fprintf(stdout, "%s ", dirs[i].name);

	fprintf(stdout, "\n");
	return 0;
}

static int cmd_exit(int argc, char **argv, char **envp)
{
	fprintf(stdout, "Goodbye!\n");
	return 1;
}

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

static int read_cmd(char *buf)
{
	if (fgets(buf, CMD_MAXLEN, stdin) == NULL) {
		return 1;
	}
	return 0;
}

static int set_env(char **envp, char *env, char *value)
{
	int i;
	char *str = malloc(sizeof(char) * (CMD_MAXLEN + 1));

	for (i = 0; envp[i]; i++) {
		if (!strcmp(strtok(envp[i], "="), env))
			break;
	}
	snprintf(str, CMD_MAXLEN, "%s=%s", env, value);
	envp[i] = str;

	return 0;
}

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

static int init_env(char **envp)
{
	cluster = info.root_offset;
	set_env(envp, "PWD", "/");
	return 0;
}

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
