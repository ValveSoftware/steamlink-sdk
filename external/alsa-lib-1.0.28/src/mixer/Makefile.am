EXTRA_LTLIBRARIES=libmixer.la

libmixer_la_SOURCES = bag.c mixer.c simple.c simple_none.c

if BUILD_MODULES
libmixer_la_SOURCES += simple_abst.c
endif

noinst_HEADERS = mixer_local.h mixer_simple.h

all: libmixer.la


AM_CPPFLAGS=-I$(top_srcdir)/include
