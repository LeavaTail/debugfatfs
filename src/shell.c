#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shell.h"

static int execute_cmd(int argc, char **argv, char **envp)
{
	return 1;
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
		copy = (char*)malloc(sizeof(char) * (len + 1));
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
	char *str = (char*)malloc(sizeof(char) * (CMD_MAXLEN + 1));

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
	set_env(envp, "PWD", "/");
	return 0;
}

int shell(void)
{
	int argc = 0;
	char buf[CMD_MAXLEN + 1] = {};
	char **argv = (char**)malloc(sizeof(char*) * (CMD_MAXLEN + 1));
	char **envp = (char**)malloc(sizeof(char*) * 16);

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
