## Process this file with automake to produce Makefile.in
# Copyright (C) 2000-2012 Free Software Foundation, Inc.
#
# Author: Nikos Mavrogiannopoulos
#
# This file is part of GnuTLS.
#
# The GnuTLS is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 3 of
# the License, or (at your option) any later version.
#
# The GnuTLS is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>

ACLOCAL_AMFLAGS = -I ../m4 -I ../gl/m4

EXTRA_DIST = priority_options.gperf
BUILT_SOURCES = pkix_asn1_tab.c gnutls_asn1_tab.c priority_options.h

SUBDIRS = includes x509 auth ext algorithms extras accelerated

if ENABLE_MINITASN1
SUBDIRS += minitasn1
endif

localedir = $(datadir)/locale

AM_CFLAGS = $(WERROR_CFLAGS) $(WSTACK_CFLAGS) $(WARN_CFLAGS) $(NETTLE_CFLAGS)
AM_CPPFLAGS = \
	-DLOCALEDIR=\"$(localedir)\"		\
	-I$(srcdir)/../gl			\
	-I$(builddir)/../gl			\
	-I$(builddir)/includes			\
	-I$(builddir)/x509			\
	-I$(srcdir)/includes			\
	-I$(builddir)/includes			\
	-I$(srcdir)/x509			\
	$(LIBTASN1_CFLAGS)			\
	$(P11_KIT_CFLAGS)

if ENABLE_OPENPGP
SUBDIRS += opencdk
SUBDIRS += openpgp
AM_CPPFLAGS += -I$(srcdir)/opencdk
endif

if ENABLE_MINITASN1
AM_CPPFLAGS += -I$(srcdir)/minitasn1
endif

# Pkg-config script.
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gnutls.pc
DISTCLEANFILES = $(pkgconfig_DATA)

lib_LTLIBRARIES = libgnutls.la

SRP_COBJECTS = gnutls_srp.c

PSK_COBJECTS = gnutls_psk.c

COBJECTS = gnutls_range.c gnutls_record.c \
	gnutls_compress.c debug.c gnutls_cipher.c	\
	gnutls_mbuffers.c gnutls_buffers.c gnutls_handshake.c		\
	gnutls_num.c gnutls_errors.c gnutls_dh.c gnutls_kx.c		\
	gnutls_priority.c gnutls_hash_int.c gnutls_cipher_int.c		\
	gnutls_session.c gnutls_db.c x509_b64.c gnutls_extensions.c	\
	gnutls_auth.c gnutls_v2_compat.c gnutls_datum.c			\
	gnutls_session_pack.c gnutls_mpi.c gnutls_pk.c gnutls_cert.c	\
	gnutls_global.c gnutls_constate.c gnutls_anon_cred.c		\
	pkix_asn1_tab.c gnutls_asn1_tab.c				\
	gnutls_mem.c gnutls_ui.c vasprintf.c vasprintf.h		\
	gnutls_sig.c gnutls_ecc.c gnutls_alert.c gnutls_privkey_raw.c	\
	system.c gnutls_str.c gnutls_state.c gnutls_x509.c		\
	gnutls_rsa_export.c gnutls_helper.c gnutls_supplemental.c	\
	random.c crypto-api.c gnutls_privkey.c gnutls_pcert.c		\
	gnutls_pubkey.c locks.c gnutls_dtls.c system_override.c	\
	crypto-backend.c verify-tofu.c pin.c tpm.c fips.c \
	safe-memset.c inet_pton.c

if ENABLE_SELF_CHECKS
COBJECTS += crypto-selftests.c crypto-selftests-pk.c
endif

if ENABLE_PKCS11
COBJECTS += pkcs11.c pkcs11x.c pkcs11_privkey.c pkcs11_write.c pkcs11_secret.c \
	pkcs11_int.c
endif

if ENABLE_NETTLE
SUBDIRS += nettle
endif

HFILES = abstract_int.h debug.h gnutls_compress.h gnutls_cipher.h	\
	gnutls_buffers.h gnutls_errors.h gnutls_int.h gnutls_dtls.h	\
	gnutls_handshake.h gnutls_num.h algorithms.h		\
	gnutls_dh.h gnutls_kx.h gnutls_hash_int.h gnutls_cipher_int.h	\
	gnutls_db.h gnutls_auth.h gnutls_extensions.h	\
	x509_b64.h gnutls_v2_compat.h gnutls_datum.h \
	gnutls_mpi.h gnutls_pk.h gnutls_record.h		\
	gnutls_constate.h gnutls_global.h gnutls_sig.h gnutls_mem.h	\
	gnutls_session_pack.h gnutls_str.h gnutls_str_array.h		\
	gnutls_state.h gnutls_x509.h crypto-backend.h			\
	gnutls_srp.h auth/srp.h auth/srp_passwd.h	\
	gnutls_helper.h gnutls_supplemental.h crypto.h random.h system.h\
	locks.h gnutls_mbuffers.h gnutls_ecc.h pin.h fips.h \
	priority_options.h

if ENABLE_PKCS11
HFILES += pkcs11_int.h pkcs11x.h
endif

# Separate so we can create the documentation

libgnutls_la_SOURCES = $(HFILES) $(COBJECTS) $(SRP_COBJECTS)	\
	$(PSK_COBJECTS) \
	gnutls.asn pkix.asn libgnutls.map

libgnutls_la_LDFLAGS = -no-undefined \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

libgnutls_la_LIBADD = ../gl/libgnu.la x509/libgnutls_x509.la \
	ext/libgnutls_ext.la \
	auth/libgnutls_auth.la algorithms/libgnutls_alg.la \
	extras/libgnutls_extras.la
thirdparty_libadd = $(LTLIBZ) $(LTLIBINTL) $(LIBSOCKET) \
	$(LTLIBICONV) $(P11_KIT_LIBS) $(LIB_SELECT) $(TSS_LIBS)

if ENABLE_NETTLE
libgnutls_la_LIBADD += accelerated/libaccelerated.la
endif

if !WINDOWS
thirdparty_libadd += $(LTLIBPTHREAD)
endif

if NEEDS_LIBRT
thirdparty_libadd += $(LTLIBRT)
endif

if ENABLE_FIPS140
thirdparty_libadd += $(FIPS140_LIBS)
endif

if ENABLE_OPENPGP
libgnutls_la_LIBADD += openpgp/libgnutls_openpgp.la
libgnutls_la_LIBADD += opencdk/libminiopencdk.la
endif

if HAVE_LD_VERSION_SCRIPT
libgnutls_la_LDFLAGS += -Wl,--version-script=$(srcdir)/libgnutls.map
else
libgnutls_la_LDFLAGS += -export-symbols-regex '^(gnutls|_gnutls).*'
endif

if ENABLE_MINITASN1
libgnutls_la_LIBADD += minitasn1/libminitasn1.la
else
thirdparty_libadd += $(LIBTASN1_LIBS)
endif

if ENABLE_NETTLE
thirdparty_libadd += $(NETTLE_LIBS) $(HOGWEED_LIBS) $(GMP_LIBS)
libgnutls_la_LIBADD += nettle/libcrypto.la
endif

if HAVE_LD_OUTPUT_DEF
libgnutls_la_LDFLAGS += -Wl,--output-def,libgnutls-$(DLL_VERSION).def
libgnutls-$(DLL_VERSION).def: libgnutls.la
defexecdir = $(bindir)
defexec_DATA = libgnutls-$(DLL_VERSION).def
DISTCLEANFILES += $(defexec_DATA)
endif

if WINDOWS
thirdparty_libadd += -lcrypt32
endif

libgnutls_la_LIBADD += $(thirdparty_libadd)

# C++ library

if ENABLE_CXX
libgnutlsxx_la_CPPFLAGS = -I$(top_srcdir)/includes -I$(top_builddir)/includes -I$(srcdir)/includes

AM_CXXFLAGS = \
	-I$(srcdir)/includes	\
	-I$(builddir)/includes	

lib_LTLIBRARIES += libgnutlsxx.la

libgnutlsxx_la_SOURCES = gnutlsxx.cpp

libgnutlsxx_la_LDFLAGS = -no-undefined \
	-version-info $(CXX_LT_CURRENT):$(CXX_LT_REVISION):$(CXX_LT_AGE)

libgnutlsxx_la_LIBADD = libgnutls.la

endif

pkix_asn1_tab.c: $(srcdir)/pkix.asn
	-asn1Parser $(srcdir)/pkix.asn pkix_asn1_tab.c

gnutls_asn1_tab.c: $(srcdir)/gnutls.asn
	-asn1Parser $(srcdir)/gnutls.asn gnutls_asn1_tab.c

priority_options.h: $(srcdir)/priority_options.gperf
	-gperf -t $^ > $@-tmp && mv $@-tmp $@

