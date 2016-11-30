AM_CPPFLAGS = -I$(top_srcdir)/include
LDADD = -lm
AM_CFLAGS = -D_GNU_SOURCE
if HAVE_SAMPLERATE
LDADD += -lsamplerate
endif
# LDFLAGS = -static
# CFLAGS += -g -Wall

bin_PROGRAMS = alsaloop
alsaloop_SOURCES = alsaloop.c pcmjob.c control.c
noinst_HEADERS = alsaloop.h
man_MANS = alsaloop.1
EXTRA_DIST = alsaloop.1
