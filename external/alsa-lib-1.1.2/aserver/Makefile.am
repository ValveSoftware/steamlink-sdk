bin_PROGRAMS = aserver
aserver_SOURCES = aserver.c
# aserver_LDADD = -lasound
aserver_LDADD = ../src/libasound.la

all: aserver

AM_CPPFLAGS=-I$(top_srcdir)/include -I$(top_srcdir)/src/pcm

../src/libasound.la:
	$(MAKE) -C ../src libasound.la

