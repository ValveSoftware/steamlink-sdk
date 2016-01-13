## Process this file with automake to produce Makefile.in
# Copyright (C) 2004-2012 Free Software Foundation, Inc.
#
# Author: Simon Josefsson
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

SUBDIRS = . rsa-md5-collision pkcs1-padding pkcs8-decode pkcs12-decode	\
	userid cert-tests key-tests sha2 safe-renegotiation dsa scripts ecdsa \
	slow dtls srp

if ENABLE_OPENPGP
SUBDIRS += openpgp-certs
endif

if WANT_TEST_SUITE
SUBDIRS += suite
endif

EXTRA_DIST = suppressions.valgrind eagain-common.h test-chains.h \
	certs/ca-cert-ecc.pem  certs/cert-ecc256.pem  certs/cert-ecc521.pem \
	certs/cert-rsa-2432.pem certs/ecc384.pem certs/ecc.pem \
	certs/ca-ecc.pem certs/cert-ecc384.pem certs/cert-ecc.pem certs/ecc256.pem \
	certs/ecc521.pem certs/rsa-2432.pem x509cert-dir/ca.pem

AM_CFLAGS = $(WARN_CFLAGS) $(WERROR_CFLAGS)
AM_CPPFLAGS = \
	-I$(top_srcdir)/gl			\
	-I$(top_builddir)/gl			\
	-I$(top_srcdir)/lib/includes		\
	-I$(top_builddir)/lib/includes		\
	-I$(top_srcdir)/libdane/includes	\
	-I$(top_builddir)/libdane/includes	\
	-I$(top_srcdir)/extra/includes		\
	-I$(top_builddir)/extra/includes	\
	-I$(top_srcdir)/lib			\
	-I$(top_srcdir)/doc/examples

AM_LDFLAGS = -no-install
LDADD = ../lib/libgnutls.la \
	../gl/libgnu.la \
	libutils.la \
	$(LTLIBGCRYPT)	\
	$(LIBSOCKET) $(INET_NTOP_LIB) $(INET_PTON_LIB)

dane_LDADD = $(LDADD) ../libdane/libgnutls-dane.la

if ENABLE_MINITASN1
AM_CPPFLAGS += -I$(srcdir)/../lib/minitasn1
endif

noinst_LTLIBRARIES = libutils.la
libutils_la_SOURCES = utils.h utils.c
libutils_la_LIBADD = ../gl/libgnu.la

ctests = mini-record-2 simple gc set_pkcs12_cred certder certuniqueid	\
	 mpi certificate_set_x509_crl dn parse_ca moredn record-sizes \
	 hostname-check cve-2008-4989 pkcs12_s2k chainverify record-sizes-range \
	 crq_key_id x509sign-verify cve-2009-1415 cve-2009-1416		\
	 crq_apis init_roundtrip pkcs12_s2k_pem dn2 mini-eagain		\
	 nul-in-x509-names x509_altname pkcs12_encode mini-x509		\
	 mini-rehandshake rng-fork mini-eagain-dtls resume-dtls \
	 x509cert x509cert-tl infoaccess mini-dtls-hello-verify \
	 mini-tdb mini-dtls-rehandshake mini-alpn mini-dtls-large \
	 mini-termination mini-x509-cas mini-x509-2 pkcs12_simple \
	 mini-emsgsize-dtls chainverify-unsorted mini-overhead \
	 mini-dtls-heartbeat mini-x509-callbacks key-openssl priorities	\
	 mini-dtls-srtp rsa-encrypt-decrypt mini-loss-time \
	 mini-record mini-dtls-record mini-handshake-timeout mini-record-range \
	 mini-cert-status mini-rsa-psk global-init sec-params \
	 fips-test mini-global-load name-constraints x509-extensions \
	 long-session-id mini-x509-callbacks-intr \
	 crlverify

if ENABLE_OCSP
ctests += ocsp
endif

if ENABLE_DANE
ctests += dane
endif

if ENABLE_OPENSSL
if !ENABLE_FIPS140
ctests +=  openssl
openssl_LDADD = ../extra/libgnutls-openssl.la $(LDADD)
endif
endif

if ENABLE_OPENPGP
ctests += openpgp-auth openpgp-auth2 openpgp-keyring pgps2kgnu
endif

if HAVE_FORK
ctests += x509self x509dn anonself pskself dhepskself	\
	resume setcredcrash

if ENABLE_OPENPGP
ctests += openpgpself 
endif

endif

check_PROGRAMS = $(ctests)
dist_check_SCRIPTS = rfc2253-escape-test

TESTS = $(ctests) $(dist_check_SCRIPTS)

TESTS_ENVIRONMENT =						\
	LC_ALL="C"						\
	CAFILE=$(srcdir)/cert-tests/ca-certs.pem		\
	PKCS12_MANY_CERTS_FILE=$(srcdir)/pkcs12-decode/pkcs12_5certs.p12	\
	PKCS12FILE=$(srcdir)/pkcs12-decode/client.p12		\
	PKCS12PASSWORD=foobar					\
	PKCS12FILE_2=$(srcdir)/pkcs12-decode/pkcs12_2certs.p12	\
	PKCS12PASSWORD_2=""					\
	PKCS12PATH=$(srcdir)/pkcs12-decode/			\
	X509CERTDIR=$(srcdir)/x509cert-dir/			\
	EXEEXT=$(EXEEXT)					\
	top_builddir="$(top_builddir)"				\
	srcdir="$(srcdir)"

if WANT_TEST_SUITE
LOG_COMPILER = $(VALGRIND)
endif
