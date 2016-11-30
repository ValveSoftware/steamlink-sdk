AM_CPPFLAGS = -I$(top_srcdir)/include
LDADD = -lm
# LDFLAGS = -static
# CFLAGS += -g -Wall

bin_PROGRAMS = amixer
amixer_SOURCES = amixer.c ../alsamixer/volume_mapping.c
noinst_HEADERS = amixer.h
man_MANS = amixer.1
EXTRA_DIST = amixer.1
