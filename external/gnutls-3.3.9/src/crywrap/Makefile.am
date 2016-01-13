## Process this file with automake to produce Makefile.in
# Copyright (C) 2011-2012 Free Software Foundation, Inc.
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

AM_CFLAGS = $(WARN_CFLAGS) $(LIBIDN_CFLAGS)
AM_CPPFLAGS = \
	-I$(builddir)/../../lib/includes		\
	-I$(srcdir)/../../lib/includes \
	-DSYSCONFDIR=\"${sysconfdir}\" \
	-DCRYWRAP_PATCHLEVEL=\"${CRYWRAP_PATCHLEVEL}\"

#	-I$(srcdir)/../../gl			

EXTRA_DIST = README
bin_PROGRAMS = crywrap

crywrap_SOURCES = crywrap.c primes.h crywrap.h
crywrap_LDADD = ../../lib/libgnutls.la $(LIBIDN_LIBS) #../../gl/libgnu.la 
crywrap_LDADD += $(LIB_CLOCK_GETTIME)
