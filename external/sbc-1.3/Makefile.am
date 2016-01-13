
AM_MAKEFLAGS = --no-print-directory

# Library code modified:	REVISION++
# Interfaces changed:		CURRENT++ REVISION=0
# Interfaces added:		CURRENT++ REVISION=0 AGE++
# Interfaces removed:		CURRENT++ REVISION=0 AGE=0

SBC_CURRENT = 3
SBC_REVISION = 1
SBC_AGE = 2

sbc_headers = sbc/sbc.h

sbc_sources = sbc/sbc.c sbc/sbc_private.h sbc/sbc_math.h sbc/sbc_tables.h \
		sbc/sbc_primitives.h sbc/sbc_primitives.c \
		sbc/sbc_primitives_mmx.h sbc/sbc_primitives_mmx.c \
		sbc/sbc_primitives_iwmmxt.h sbc/sbc_primitives_iwmmxt.c \
		sbc/sbc_primitives_neon.h sbc/sbc_primitives_neon.c \
		sbc/sbc_primitives_armv6.h sbc/sbc_primitives_armv6.c

pkginclude_HEADERS = $(sbc_headers)

lib_LTLIBRARIES = sbc/libsbc.la

sbc_libsbc_la_SOURCES = $(sbc_headers) $(sbc_sources)

sbc_libsbc_la_LDFLAGS = -no-undefined \
		-version-info $(SBC_CURRENT):$(SBC_REVISION):$(SBC_AGE) \
		-Wl,--version-script=$(top_srcdir)/sbc/sbc.sym

sbc_libsbc_la_DEPENDENCIES = sbc/sbc.sym

if TOOLS
noinst_LTLIBRARIES = sbc/libsbc-private.la

sbc_libsbc_private_la_SOURCES = $(sbc_headers) $(sbc_sources)

bin_PROGRAMS = src/sbcinfo src/sbcdec src/sbcenc

src_sbcdec_SOURCES = src/sbcdec.c src/formats.h
src_sbcdec_LDADD = sbc/libsbc-private.la

src_sbcenc_SOURCES = src/sbcenc.c src/formats.h
src_sbcenc_LDADD = sbc/libsbc-private.la
endif

if TESTER
noinst_PROGRAMS = src/sbctester

src_sbctester_LDADD = @SNDFILE_LIBS@ -lm
endif

AM_CFLAGS = @SNDFILE_CFLAGS@ -fvisibility=hidden \
				-finline-functions \
				-fgcse-after-reload \
				-funswitch-loops \
				-funroll-loops

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA = sbc/sbc.pc

EXTRA_DIST = sbc/sbc.sym

MAINTAINERCLEANFILES = Makefile.in \
	aclocal.m4 configure config.h.in config.sub config.guess \
	ltmain.sh depcomp compile missing install-sh mkinstalldirs
