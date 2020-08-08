bin_PROGRAMS = dumpexfat
dumpexfat_SOURCES = src/main.c src/nls.c src/fat.c src/exfat.c
man_MANS = dumpexfat.1
EXTRA_DIST = include

AM_CPPFLAGS = -I$(top_srcdir)/include
if DEBUG
AM_CPPFLAGS += -O0 -g3 -Wall -DDUMPEXFAT_DEBUG
else
AM_CPPFLAGS += -O2
endif

dumpexfat.1: dumpexfat$(EXEEXT)
	help2man --no-discard-stderr -N -o dumpexfat.1 ./dumpexfat
