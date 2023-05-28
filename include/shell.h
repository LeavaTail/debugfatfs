// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2021 LeavaTail
 */
#ifndef _SHELL_H
#define _SHELL_H

#define CMD_MAXLEN 4096
#define ARG_MAXNUM 3
#define ARG_MAXLEN 1024
#define ENV_MAXNUM 16
#define CMD_DELIM " \t\r\n\a"

struct command {
	char *name;
	int (*func)(int, char **, char **);
};

int shell(void);

#endif /*_SHELL_H */
