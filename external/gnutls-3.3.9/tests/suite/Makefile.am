## Process this file with automake to produce Makefile.in
# Copyright (C) 2004-2012 Free Software Foundation, Inc.
#
# This file is part of GnuTLS.
#
# This file is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This file is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this file; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

AM_CFLAGS = $(WARN_CFLAGS)
AM_CPPFLAGS = \
	-I$(top_srcdir)/gl			\
	-I$(top_builddir)/gl			\
	-I$(top_srcdir)/lib/includes		\
	-I$(top_builddir)/lib/includes		\
	-I$(top_srcdir)/extra/includes	\
	-I$(top_builddir)/extra/includes	\
	-I$(top_srcdir)/lib			\
	-I$(top_srcdir)/doc/examples	\
	-I$(top_srcdir)/tests/suite/ecore/src/include \
	-I$(top_srcdir)/tests/suite/ecore/src/lib

EXTRA_DIST = crl/long.crl crl/long.pem

check_PROGRAMS = rng
AM_LDFLAGS = -no-install
LDADD = ../../lib/libgnutls.la \
	../../gl/libgnu.la \
	../libutils.la \
	$(LTLIBGCRYPT)	\
	$(LIBSOCKET) $(INET_NTOP_LIB) $(INET_PTON_LIB)


libecore_la_CPPFLAGS = -I$(top_srcdir)/tests/suite/ecore/		\
	-I$(top_srcdir)/tests/suite/ecore/src/include			\
	-I$(top_srcdir)/tests/suite/ecore/src/lib -DHAVE_CONFIG_H	\
	-D__UNUSED__= -DVMAJ=1 -D VMIC=0 -DVMIN=0 -DVREV=0		\
	-DEFL_HAVE_POSIX_THREADS=1 -DEFL_HAVE_POSIX_THREADS_SPINLOCK=1	\
	-DEFL_HAVE_THREADS=1 -DEINA_BUILD_CHAINED_POOL=1		\
	-DEINA_STATIC_BUILD_CHAINED_POOL=1 -DHAVE_CLOCK_GETTIME		\
	-DHAVE_GETTIMEOFDAY -DPACKAGE_LIB_DIR=\"/usr/lib\"		\
	-DMODULE_ARCH=\"unix\" -DSHARED_LIB_SUFFIX=\".so\"

libecore_la_CFLAGS = -w
nodist_libecore_la_SOURCES = ecore/src/lib/ecore_anim.c			\
	ecore/src/lib/ecore_app.c ecore/src/lib/ecore.c			\
	ecore/src/lib/ecore_events.c ecore/src/lib/ecore_exe.c		\
	ecore/src/lib/ecore_getopt.c ecore/src/lib/ecore_glib.c		\
	ecore/src/lib/ecore_idle_enterer.c				\
	ecore/src/lib/ecore_idle_exiter.c ecore/src/lib/ecore_idler.c	\
	ecore/src/lib/ecore_job.c ecore/src/lib/ecore_main.c		\
	ecore/src/lib/ecore_pipe.c ecore/src/lib/ecore_poll.c		\
	ecore/src/lib/ecore_signal.c ecore/src/lib/ecore_thread.c	\
	ecore/src/lib/ecore_time.c ecore/src/lib/ecore_timer.c		\
	ecore/src/lib/eina_accessor.c ecore/src/lib/eina_array.c	\
	ecore/src/lib/eina_benchmark.c ecore/src/lib/eina_binshare.c	\
	ecore/src/lib/eina_chained_mempool.c				\
	ecore/src/lib/eina_convert.c ecore/src/lib/eina_counter.c	\
	ecore/src/lib/eina_cpu.c ecore/src/lib/eina_error.c		\
	ecore/src/lib/eina_file.c ecore/src/lib/eina_fp.c		\
	ecore/src/lib/eina_hamster.c ecore/src/lib/eina_hash.c		\
	ecore/src/lib/eina_inlist.c ecore/src/lib/eina_iterator.c	\
	ecore/src/lib/eina_lalloc.c ecore/src/lib/eina_list.c		\
	ecore/src/lib/eina_log.c ecore/src/lib/eina_magic.c		\
	ecore/src/lib/eina_main.c ecore/src/lib/eina_matrixsparse.c	\
	ecore/src/lib/eina_mempool.c ecore/src/lib/eina_module.c	\
	ecore/src/lib/eina_quadtree.c ecore/src/lib/eina_rbtree.c	\
	ecore/src/lib/eina_rectangle.c					\
	ecore/src/lib/eina_safety_checks.c ecore/src/lib/eina_sched.c	\
	ecore/src/lib/eina_share_common.c ecore/src/lib/eina_strbuf.c	\
	ecore/src/lib/eina_strbuf_common.c ecore/src/lib/eina_str.c	\
	ecore/src/lib/eina_stringshare.c ecore/src/lib/eina_tiler.c	\
	ecore/src/lib/eina_unicode.c ecore/src/lib/eina_ustrbuf.c	\
	ecore/src/lib/eina_ustringshare.c ecore/src/lib/eina_value.c



nodist_check_SCRIPTS = eagain testsrn testcompat chain invalid-cert testrandom \
	testpkcs11 testpkcs11.pkcs15 testpkcs11.softhsm testpkcs11.sc-hsm \
	testrng

TESTS = ciphersuite/test-ciphersuites.sh eagain testsrn testcompat chain invalid-cert \
	testpkcs11 testrng

if ENABLE_PKCS11
check_PROGRAMS += pkcs11-chainverify pkcs11-get-issuer pkcs11-is-known pkcs11-combo
TESTS += pkcs11-chainverify pkcs11-get-issuer crl-test pkcs11-is-known pkcs11-combo
endif

TESTS_ENVIRONMENT = EXEEXT=$(EXEEXT) \
	LC_ALL="C"					\
	VALGRIND="$(VALGRIND)"				\
	LIBTOOL="$(LIBTOOL)"				\
	top_builddir="$(top_builddir)"			\
	srcdir="$(srcdir)"

if ENABLE_DANE
TESTS += testdane
endif

if !MACOSX
if !WINDOWS
noinst_LTLIBRARIES = libecore.la

mini_record_timing_LDADD = -lrt $(LDADD)
eagain_cli_LDADD = libecore.la -lrt -lm -ldl -lpthread $(LDADD)
nodist_eagain_cli_SOURCES = mini-eagain2.c

noinst_PROGRAMS = eagain-cli mini-record-timing

TESTS += eagain
endif
endif

#pkcs11-chainverify cannot run under valgrind (or hell breaks loose)

#if WANT_TEST_SUITE
#LOG_COMPILER = $(VALGRIND)
#endif
