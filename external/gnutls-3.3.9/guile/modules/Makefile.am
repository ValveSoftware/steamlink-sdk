#  GnuTLS --- Guile bindings for GnuTLS.
#  Copyright (C) 2007-2012 Free Software Foundation, Inc.
#
#  GnuTLS is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2.1 of the License, or (at your option) any later version.
#
#  GnuTLS is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with GnuTLS; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

guilemoduledir = $(GUILE_SITE)

nobase_nodist_guilemodule_DATA = gnutls.scm
nobase_dist_guilemodule_DATA   = gnutls/extra.scm

documentation_modules = system/documentation/README		\
		        system/documentation/c-snarf.scm	\
                        system/documentation/output.scm

EXTRA_DIST = gnutls/build/enums.scm gnutls/build/smobs.scm	\
	     gnutls/build/utils.scm gnutls/build/priorities.scm	\
	     gnutls/build/tests.scm				\
	     gnutls.in						\
             $(documentation_modules)

CLEANFILES = gnutls.scm

.in.scm:
	cat "$^" | $(SED) -e's|[@]guileextensiondir[@]|$(guileextensiondir)|g' \
	  > "$@.tmp"
	mv "$@.tmp" "$@"
