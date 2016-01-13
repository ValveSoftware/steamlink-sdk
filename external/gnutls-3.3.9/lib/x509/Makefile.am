## Process this file with automake to produce Makefile.in
# Copyright (C) 2003-2012 Free Software Foundation, Inc.
#
# This file is part of GnuTLS.
#
# The GnuTLS is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 3 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>

AM_CFLAGS = $(WERROR_CFLAGS) $(WSTACK_CFLAGS) $(WARN_CFLAGS) $(NETTLE_CFLAGS)
AM_CPPFLAGS = \
	-I$(srcdir)/../../gl		\
	-I$(builddir)/../../gl		\
	-I$(srcdir)/../includes		\
	-I$(builddir)/../includes	\
	-I$(srcdir)/..			\
	$(LIBOPENCDK_CFLAGS)

if ENABLE_MINITASN1
AM_CPPFLAGS += -I$(srcdir)/../minitasn1
endif

noinst_LTLIBRARIES = libgnutls_x509.la

libgnutls_x509_la_SOURCES =	\
	common.c key_encode.c	\
	common.h key_decode.c	\
	crl.c			\
	crl_write.c		\
	crq.c			\
	dn.c			\
	extensions.c		\
	mpi.c			\
	output.c		\
	pbkdf2-sha1.c		\
	pbkdf2-sha1.h		\
	pkcs12.c		\
	pkcs12_bag.c		\
	pkcs12_encr.c		\
	pkcs7.c			\
	privkey.c		\
	privkey_pkcs8.c		\
	privkey_openssl.c	\
	rfc2818_hostname.c	\
	sign.c			\
	verify.c		\
	x509.c x509_dn.c	\
	x509_int.h		\
	x509_write.c		\
	name_constraints.c	\
	verify-high.c		\
	verify-high2.c		\
	verify-high.h		\
	x509_ext.c

if ENABLE_OCSP
libgnutls_x509_la_SOURCES += ocsp.c ocsp_output.c
endif
