## Process this file with automake to produce Makefile.in
# Copyright (C) 2000-2012 Free Software Foundation, Inc.
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

SUBDIRS = gl

BUILT_SOURCES = srptool-args.c srptool-args.h \
	psktool-args.c psktool-args.h ocsptool-args.h ocsptool-args.c \
	serv-args.c serv-args.h cli-args.c cli-args.h \
	cli-debug-args.c cli-debug-args.h certtool-args.c certtool-args.h \
	danetool-args.c danetool-args.h p11tool-args.c p11tool-args.h \
	tpmtool-args.c tpmtool-args.h

if ENABLE_CRYWRAP
SUBDIRS += crywrap
endif

if NEED_LIBOPTS
LIBOPTS = libopts/libopts.la
SUBDIRS += libopts
else
LIBOPTS = $(LIBOPTS_LDADD)
endif

EXTRA_DIST = args-std.def

AM_CFLAGS = $(WARN_CFLAGS) $(WERROR_CFLAGS)
AM_CPPFLAGS = \
	-I$(srcdir)/gl				\
	-I$(srcdir)/../gl			\
	-I$(builddir)/gl			\
	-I$(builddir)/../gl			\
	-I$(builddir)/../lib/includes		\
	-I$(srcdir)/../lib/includes		\
	-I$(srcdir)/../libdane/includes		\
	-I$(srcdir)/../extra/includes	\
	$(LIBOPTS_CFLAGS)

bin_PROGRAMS = psktool gnutls-cli-debug certtool
if ENABLE_SRP
bin_PROGRAMS += srptool
endif

if ENABLE_OCSP
bin_PROGRAMS += ocsptool gnutls-serv 
if ENABLE_ANON
bin_PROGRAMS += gnutls-cli
endif
endif

bin_PROGRAMS += danetool

if ENABLE_TROUSERS
bin_PROGRAMS += tpmtool
endif

if ENABLE_PKCS11
bin_PROGRAMS += p11tool
endif

noinst_LTLIBRARIES =


if ENABLE_SRP
srptool_SOURCES = srptool.c
srptool_LDADD = ../lib/libgnutls.la libcmd-srp.la $(LIBOPTS) ../gl/libgnu.la 
srptool_LDADD += $(LTLIBINTL) gl/libgnu_gpl.la
noinst_LTLIBRARIES += libcmd-srp.la
libcmd_srp_la_SOURCES = srptool-args.def srptool-args.c srptool-args.h
endif

psktool_SOURCES = psk.c
psktool_LDADD = ../lib/libgnutls.la libcmd-psk.la $(LIBOPTS) ../gl/libgnu.la 
psktool_LDADD += $(LTLIBINTL) gl/libgnu_gpl.la
noinst_LTLIBRARIES += libcmd-psk.la
libcmd_psk_la_SOURCES = psktool-args.def psktool-args.c psktool-args.h


if ENABLE_OCSP

ocsptool_SOURCES = ocsptool.c ocsptool-common.h ocsptool-common.c \
	socket.c socket.h
ocsptool_LDADD = ../lib/libgnutls.la libcmd-ocsp.la $(LIBOPTS) ../gl/libgnu.la 
ocsptool_LDADD += $(LTLIBINTL) $(GETADDRINFO_LIB) gl/libgnu_gpl.la
noinst_LTLIBRARIES += libcmd-ocsp.la
libcmd_ocsp_la_SOURCES = ocsptool-args.def ocsptool-args.h ocsptool-args.c

gnutls_serv_SOURCES =		\
  list.h serv.c			\
  udp-serv.c udp-serv.h		\
  socket.c socket.h		\
  common.h common.c		\
  certtool-common.h 
gnutls_serv_LDADD = ../lib/libgnutls.la
gnutls_serv_LDADD += libcmd-serv.la $(LIBOPTS) ../gl/libgnu.la
gnutls_serv_LDADD += $(LTLIBINTL) gl/libgnu_gpl.la
gnutls_serv_LDADD += $(LIBSOCKET) $(GETADDRINFO_LIB)
noinst_LTLIBRARIES += libcmd-serv.la
libcmd_serv_la_SOURCES = serv-args.def serv-args.c serv-args.h

if ENABLE_ANON

BENCHMARK_SRCS = benchmark-cipher.c benchmark.c benchmark.h benchmark-tls.c

gnutls_cli_SOURCES = cli.c common.h common.c \
	socket.c socket.h ocsptool-common.c inline_cmds.h \
	$(BENCHMARK_SRCS)
gnutls_cli_LDADD = ../lib/libgnutls.la
if ENABLE_DANE
gnutls_cli_LDADD += ../libdane/libgnutls-dane.la
endif
gnutls_cli_LDADD += libcmd-cli.la $(LIBOPTS) ../gl/libgnu.la $(LTLIBINTL)
gnutls_cli_LDADD += $(LIBSOCKET) $(GETADDRINFO_LIB) $(LIB_CLOCK_GETTIME) \
	 $(SERVENT_LIB) gl/libgnu_gpl.la
noinst_LTLIBRARIES += libcmd-cli.la
libcmd_cli_la_SOURCES = cli-args.def cli-args.c cli-args.h
endif
endif

gnutls_cli_debug_SOURCES = cli-debug.c tests.h tests.c \
		socket.c socket.h common.h common.c
gnutls_cli_debug_LDADD = ../lib/libgnutls.la libcmd-cli-debug.la 
gnutls_cli_debug_LDADD += $(LIBOPTS) ../gl/libgnu.la gl/libgnu_gpl.la
gnutls_cli_debug_LDADD += $(LTLIBINTL)
gnutls_cli_debug_LDADD += $(LIBSOCKET) $(GETADDRINFO_LIB)
noinst_LTLIBRARIES += libcmd-cli-debug.la
libcmd_cli_debug_la_SOURCES = cli-debug-args.def cli-debug-args.c cli-debug-args.h

#certtool

certtool_SOURCES = certtool.c certtool-common.c certtool-extras.c common.c
certtool_LDADD = ../lib/libgnutls.la 
certtool_LDADD += libcmd-certtool.la ../gl/libgnu.la gl/libgnu_gpl.la

noinst_LTLIBRARIES += libcmd-certtool.la
libcmd_certtool_la_SOURCES = certtool-args.c certtool-args.def certtool-args.h	\
	certtool-cfg.h certtool-cfg.c
libcmd_certtool_la_LIBADD = ../lib/libgnutls.la gl/libgnu_gpl.la ../gl/libgnu.la 
libcmd_certtool_la_LIBADD += $(LIBOPTS) $(LTLIBINTL)
libcmd_certtool_la_LIBADD += $(LTLIBREADLINE) gl/libgnu_gpl.la
libcmd_certtool_la_LIBADD += $(INET_PTON_LIB) $(LIB_CLOCK_GETTIME)

danetool_SOURCES = danetool.c certtool-common.c certtool-extras.c common.c socket.c
danetool_LDADD = ../lib/libgnutls.la 
danetool_LDADD += libcmd-danetool.la ../gl/libgnu.la gl/libgnu_gpl.la
if ENABLE_DANE
danetool_LDADD += ../libdane/libgnutls-dane.la
endif

noinst_LTLIBRARIES += libcmd-danetool.la
libcmd_danetool_la_SOURCES = danetool-args.c danetool-args.def danetool-args.h \
	certtool-cfg.h certtool-cfg.c
libcmd_danetool_la_LIBADD = ../lib/libgnutls.la gl/libgnu_gpl.la ../gl/libgnu.la 
libcmd_danetool_la_LIBADD += $(LIBOPTS)
libcmd_danetool_la_LIBADD += $(LTLIBINTL)
libcmd_danetool_la_LIBADD += $(LTLIBREADLINE)
libcmd_danetool_la_LIBADD += $(INET_PTON_LIB) $(LIB_CLOCK_GETTIME)

# p11 tool
if ENABLE_PKCS11

p11tool_SOURCES = p11tool-args.def p11tool.c pkcs11.c certtool-common.c \
	certtool-extras.c p11tool.h common.c
p11tool_LDADD = ../lib/libgnutls.la
p11tool_LDADD += libcmd-p11tool.la $(LIBOPTS) ../gl/libgnu.la gl/libgnu_gpl.la
p11tool_LDADD += $(LTLIBINTL)

noinst_LTLIBRARIES += libcmd-p11tool.la
libcmd_p11tool_la_SOURCES = p11tool-args.def p11tool-args.c p11tool-args.h \
	certtool-cfg.h certtool-cfg.c
libcmd_p11tool_la_LIBADD = ../lib/libgnutls.la gl/libgnu_gpl.la ../gl/libgnu.la 
libcmd_p11tool_la_LIBADD += $(LTLIBREADLINE) $(INET_PTON_LIB) $(LIB_CLOCK_GETTIME)

endif # ENABLE_PKCS11

if ENABLE_TROUSERS

tpmtool_SOURCES = tpmtool-args.def tpmtool.c certtool-common.c certtool-extras.c common.c
tpmtool_LDADD = ../lib/libgnutls.la
tpmtool_LDADD += libcmd-tpmtool.la $(LIBOPTS) ../gl/libgnu.la gl/libgnu_gpl.la
tpmtool_LDADD += $(LTLIBINTL)

noinst_LTLIBRARIES += libcmd-tpmtool.la
libcmd_tpmtool_la_SOURCES = tpmtool-args.def tpmtool-args.c tpmtool-args.h \
	certtool-cfg.h certtool-cfg.c
libcmd_tpmtool_la_LIBADD = ../lib/libgnutls.la gl/libgnu_gpl.la ../gl/libgnu.la 
libcmd_tpmtool_la_LIBADD += $(LTLIBREADLINE) $(INET_PTON_LIB) $(LIB_CLOCK_GETTIME)

endif # ENABLE_TROUSERS

danetool-args.h: danetool-args.c
danetool-args.c: $(srcdir)/danetool-args.def $(srcdir)/args-std.def
	-$(AUTOGEN) $<
ocsptool-args.h: ocsptool-args.c
ocsptool-args.c: $(srcdir)/ocsptool-args.def $(srcdir)/args-std.def
	-$(AUTOGEN) $<
tpmtool-args.h: tpmtool-args.c
tpmtool-args.c: $(srcdir)/tpmtool-args.def $(srcdir)/args-std.def
	-$(AUTOGEN) $<
p11tool-args.h: p11tool-args.c
p11tool-args.c: $(srcdir)/p11tool-args.def $(srcdir)/args-std.def
	-$(AUTOGEN) $<
psktool-args.h: psktool-args.c
psktool-args.c: $(srcdir)/psktool-args.def $(srcdir)/args-std.def
	-$(AUTOGEN) $<
cli-debug-args.h: cli-debug-args.c
cli-debug-args.c: $(srcdir)/cli-debug-args.def $(srcdir)/args-std.def
	-$(AUTOGEN) $<
cli-args.h: cli-args.c
cli-args.c cli-args.h: $(srcdir)/cli-args.def $(srcdir)/args-std.def
	-$(AUTOGEN) $<
serv-args.h: serv-args.c
serv-args.c: $(srcdir)/serv-args.def $(srcdir)/args-std.def
	-$(AUTOGEN) $<
srptool-args.h: srptool-args.c
srptool-args.c: $(srcdir)/srptool-args.def $(srcdir)/args-std.def
	-$(AUTOGEN) $<
certtool-args.h: certtool-args.c
certtool-args.c: $(srcdir)/certtool-args.def $(srcdir)/args-std.def
	-$(AUTOGEN) $<
