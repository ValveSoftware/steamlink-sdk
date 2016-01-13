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

EXTRA_DIST = README key-ca.pem key-user.pem \
	ca-gnutls-keyid.pem ca-no-keyid.pem ca-weird-keyid.pem \
	key-ca-1234.p8 key-ca-empty.p8 key-ca-null.p8 \
	openssl-key-ecc.p8 key-ecc.p8 key-ecc.pem

dist_check_SCRIPTS = key-id pkcs8

TESTS = key-id pkcs8

TESTS_ENVIRONMENT = EXEEXT=$(EXEEXT) \
	LC_ALL="C"						\
	top_builddir="$(top_builddir)"				\
	srcdir="$(srcdir)"
