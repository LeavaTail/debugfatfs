bin_PROGRAMS = debugfatfs
debugfatfs_SOURCES = src/main.c \
                     src/nls.c \
                     src/shell.c \
                     src/fat.c \
                     src/exfat.c

man_MANS = debugfatfs.8
TESTS = tests/00_init.sh \
        tests/01_simple_option_check.sh \
        tests/02_simple_inetractive_check.sh \
        tests/03_simple_check_other.sh

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

debugfatfs.8: debugfatfs$(EXEEXT)
	help2man --no-discard-stderr --section=8 -N -o debugfatfs.8 ./debugfatfs
