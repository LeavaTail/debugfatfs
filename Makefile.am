bin_PROGRAMS = dumpexfat
dumpexfat_SOURCES = src/main.c src/fat.c src/exfat.c

AM_CPPFLAGS = -I$(top_srcdir)/include
if DEBUG
AM_CPPFLAGS += -O0 -g3 -Wall -DDUMPEXFAT_DEBUG
else
AM_CPPFLAGS += -O2
endif
