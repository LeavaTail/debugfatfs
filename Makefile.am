bin_PROGRAMS = debugfatfs
debugfatfs_SOURCES = src/main.c \
                     src/nls.c \
                     src/shell.c \
                     src/fat.c \
                     src/exfat.c

TESTS = \
        tests/01_simple_option_check.sh \
        tests/02_simple_inetractive_check.sh \
        tests/03_simple_command_check.sh \
        tests/04_simple_invalid_check.sh \
        tests/05_show_dentry_check.sh \
        tests/11_alloc_free_check.sh

EXTRA_DIST = include

AM_CPPFLAGS = -I$(top_srcdir)/include
if DEBUG
AM_CPPFLAGS += -O0 -g3 -Wall -DDEBUGFATFS_DEBUG
else
AM_CPPFLAGS += -O2
endif

if GCOV
AM_CPPFLAGS += -O0 --no-inline -fprofile-arcs -ftest-coverage
LDFLAGS +=  -fprofile-generate
endif

if HELP2MAN
man_MANS = debugfatfs.8

debugfatfs.8: debugfatfs$(EXEEXT)
	help2man --no-discard-stderr --section=8 -N -o debugfatfs.8 ./debugfatfs
endif
