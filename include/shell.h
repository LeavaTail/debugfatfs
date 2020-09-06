#ifndef _SHELL_H
#define _SHELL_H

#define CMD_MAXLEN 255
#define CMD_DELIM " \t\r\n\a"

#define INTERACTIVE_COMMAND 1

struct command {
	char *name;
	int (*func)(int, char **, char **);
};

int shell(void);

#endif /*_SHELL_H */
