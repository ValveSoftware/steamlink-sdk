EXTRA_LTLIBRARIES = libtopology.la

libtopology_la_SOURCES =\
	parser.c \
	builder.c \
	ctl.c \
	dapm.c \
	pcm.c \
	data.c \
	text.c \
	channel.c \
	ops.c \
	elem.c

noinst_HEADERS = tplg_local.h

all: libtopology.la

AM_CPPFLAGS=-I$(top_srcdir)/include
