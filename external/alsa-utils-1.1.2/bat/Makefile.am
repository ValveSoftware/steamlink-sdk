bin_PROGRAMS = alsabat
man_MANS = alsabat.1
EXTRA_DIST = alsabat.1 alsabat-test.sh
sbin_SCRIPTS = alsabat-test.sh

alsabat_SOURCES = \
	bat.c \
	common.c \
	signal.c \
	latencytest.c \
	convert.c

noinst_HEADERS = \
	common.h \
	bat-signal.h \
	latencytest.h \
	convert.h

if HAVE_LIBFFTW3
alsabat_SOURCES += analyze.c
noinst_HEADERS += analyze.h
endif

if HAVE_LIBTINYALSA
alsabat_SOURCES += tinyalsa.c
noinst_HEADERS += tinyalsa.h
else
alsabat_SOURCES += alsa.c
noinst_HEADERS += alsa.h
endif

AM_CPPFLAGS = \
	      -Wall -I$(top_srcdir)/include

alsabat_LDADD = @FFTW_LIB@
