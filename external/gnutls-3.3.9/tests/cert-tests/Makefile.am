## Process this file with automake to produce Makefile.in
# Copyright (C) 2007-2008, 2010, 2012 Free Software Foundation, Inc.
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

EXTRA_DIST = ca-no-pathlen.pem no-ca-or-pathlen.pem aki-cert.pem \
	template-test.key template-test.pem template-test.tmpl \
	funny-spacing.pem ca-certs.pem dane-test.rr cert-ecc256.pem \
	bmpstring.pem template-utf8.pem template-utf8.tmpl \
	template-dn.tmpl template-dn.pem complex-cert.pem template-overflow.pem \
	template-overflow.tmpl template-overflow2.pem template-overflow2.tmpl \
	template-date.tmpl template-date.pem template-dn-err.tmpl \
	template-nc.tmpl template-nc.pem xmpp-othername.pem \
	suppressions.valgrind

dist_check_SCRIPTS = pathlen aki template-test pem-decoding dane

TESTS = pathlen aki pem-decoding

if !WINDOWS
TESTS += template-test
endif

if ENABLE_DANE
TESTS += dane
endif

TESTS_ENVIRONMENT = EXEEXT=$(EXEEXT) \
	LC_ALL="C"				\
	VALGRIND="$(VALGRIND)"			\
	LIBTOOL="$(LIBTOOL)"			\
	top_builddir="$(top_builddir)"		\
	srcdir="$(srcdir)"

