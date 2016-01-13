#  GnuTLS --- Guile bindings for GnuTLS.
#  Copyright (C) 2007-2013 Free Software Foundation, Inc.
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

TESTS = anonymous-auth.scm session-record-port.scm	\
        pkcs-import-export.scm				\
        errors.scm					\
	x509-certificates.scm x509-auth.scm		\
	priorities.scm

if ENABLE_OPENPGP
TESTS += openpgp-keys.scm openpgp-keyring.scm openpgp-auth.scm
endif

if ENABLE_SRP
TESTS += srp-base64.scm
endif

TESTS_ENVIRONMENT =					\
  GUILE_AUTO_COMPILE=0					\
  GUILE_WARN_DEPRECATED=detailed

LOG_COMPILER = $(top_builddir)/guile/pre-inst-guile -L $(srcdir)


EXTRA_DIST = $(TESTS) openpgp-pub.asc openpgp-sec.asc	\
		openpgp-elg-pub.asc openpgp-elg-sec.asc \
             openpgp-keyring.gpg openpgp-keyring.asc	\
	     rsa-parameters.pem dh-parameters.pem	\
	     x509-certificate.pem x509-key.pem
