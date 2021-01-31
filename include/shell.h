// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2021 LeavaTail
 */
#ifndef _SHELL_H
#define _SHELL_H

#define CMD_MAXLEN 255
#define CMD_DELIM " \t\r\n\a"

struct command {
	char *name;
	int (*func)(int, char **, char **);
};

int shell(void);

#endif /*_SHELL_H */
