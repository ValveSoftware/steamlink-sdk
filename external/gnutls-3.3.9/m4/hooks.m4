# Copyright (C) 2000-2012 Free Software Foundation, Inc.
#
# Author: Nikos Mavrogiannopoulos, Simon Josefsson
#
# This file is part of GnuTLS.
#
# The GnuTLS is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2.1 of
# the License, or (at your option) any later version.
#
# The GnuTLS is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with GnuTLS; if not, write to the Free
# Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA 02110-1301, USA

AC_DEFUN([LIBGNUTLS_EXTRA_HOOKS],
[
  AC_MSG_CHECKING([whether to build OpenSSL compatibility layer])
  AC_ARG_ENABLE(openssl-compatibility,
    AS_HELP_STRING([--disable-openssl-compatibility],
                   [disable the OpenSSL compatibility support]),
    enable_openssl=$enableval, enable_openssl=yes)
  AC_MSG_RESULT($enable_openssl)
  AM_CONDITIONAL(ENABLE_OPENSSL, test "$enable_openssl" = "yes")

  # We link to ../lib's gnulib, which needs -lws2_32 via LIBSOCKET in Makefile.am.
  gl_SOCKETS
])

AC_DEFUN([LIBGNUTLS_HOOKS],
[
  # Library code modified:                              REVISION++
  # Interfaces changed/added/removed:   CURRENT++       REVISION=0
  # Interfaces added:                             AGE++
  # Interfaces removed:                           AGE=0 (+bump all symbol versions in .map)
  AC_SUBST(LT_CURRENT, 69)
  AC_SUBST(LT_REVISION, 1)
  AC_SUBST(LT_AGE, 41)

  AC_SUBST(LT_SSL_CURRENT, 27)
  AC_SUBST(LT_SSL_REVISION, 2)
  AC_SUBST(LT_SSL_AGE, 0)

  AC_SUBST(LT_DANE_CURRENT, 5)
  AC_SUBST(LT_DANE_REVISION, 0)
  AC_SUBST(LT_DANE_AGE, 5)

  AC_SUBST(CXX_LT_CURRENT, 29)
  AC_SUBST(CXX_LT_REVISION, 0)
  AC_SUBST(CXX_LT_AGE, 1)

  AC_SUBST(CRYWRAP_PATCHLEVEL, 3)

  # Used when creating the Windows libgnutls-XX.def files.
  DLL_VERSION=`expr ${LT_CURRENT} - ${LT_AGE}`
  AC_SUBST(DLL_VERSION)

  PKG_CHECK_MODULES(NETTLE, [nettle >= 2.7 nettle < 3.0], [cryptolib="nettle"], [
AC_MSG_ERROR([[
  *** 
  *** Libnettle 2.7.1 was not found. Note that this version of gnutls doesn't support nettle 3.0.
]])
  ])
  PKG_CHECK_MODULES(HOGWEED, [hogweed >= 2.7], [], [
AC_MSG_ERROR([[
  *** 
  *** Libhogweed (nettle's companion library) was not found. Note that you must compile nettle with gmp support.
]])
  ])
  AM_CONDITIONAL(ENABLE_NETTLE, test "$cryptolib" = "nettle")
  AC_DEFINE([HAVE_LIBNETTLE], 1, [nettle is enabled])

  GNUTLS_REQUIRES_PRIVATE="Requires.private: nettle, hogweed"

  AC_ARG_WITH(nettle-mini,
    AS_HELP_STRING([--with-nettle-mini], [Link against a mini-nettle (that includes mini-gmp)]),
      mini_nettle=$withval,
      mini_nettle=no)

  AC_ARG_VAR(GMP_CFLAGS, [C compiler flags for gmp])
  AC_ARG_VAR(GMP_LIBS, [linker flags for gmp])
  if test "$mini_nettle" != no;then
    GMP_CLFLAGS=""
    GMP_LIBS=""
  else
    if test x$GMP_LIBS = x; then
    	AC_CHECK_LIB(gmp, __gmpz_cmp, [GMP_LIBS="-lgmp"], [AC_MSG_ERROR([[
***
*** gmp was not found.
]])])
    fi
  fi
  AC_SUBST(GMP_CFLAGS)
  AC_SUBST(GMP_LIBS)

  AC_ARG_WITH(included-libtasn1,
    AS_HELP_STRING([--with-included-libtasn1], [use the included libtasn1]),
      included_libtasn1=$withval,
      included_libtasn1=no)
  if test "$included_libtasn1" = "no"; then
    PKG_CHECK_MODULES(LIBTASN1, [libtasn1 >= 3.9], [], [included_libtasn1=yes])
    if test "$included_libtasn1" = yes; then
      AC_MSG_WARN([[
  *** 
  *** Libtasn1 was not found. Will use the included one.
  ]])
    fi
  fi
  AC_MSG_CHECKING([whether to use the included minitasn1])
  AC_MSG_RESULT($included_libtasn1)
  AM_CONDITIONAL(ENABLE_MINITASN1, test "$included_libtasn1" = "yes")

  if test "$included_libtasn1" = "no"; then
    GNUTLS_REQUIRES_PRIVATE="${GNUTLS_REQUIRES_PRIVATE}, libtasn1"
  fi

  AC_MSG_CHECKING([whether C99 macros are supported])
  AC_TRY_COMPILE(,
  [
    #define test_mac(...) 
    int z,y,x;
    test_mac(x,y,z);
    return 0;
  ], [
    AC_DEFINE([C99_MACROS], 1, [C99 macros are supported])
    AC_MSG_RESULT(yes)
  ], [
    AC_MSG_RESULT(no)
    AC_MSG_WARN([C99 macros not supported. This may affect compiling.])
  ])

  ac_enable_srtp=yes
  AC_MSG_CHECKING([whether to disable DTLS-SRTP extension])
  AC_ARG_ENABLE(dtls-srtp-support,
    AS_HELP_STRING([--disable-dtls-srtp-support],
                   [disable support for the DTLS-SRTP extension]),
    ac_enable_srtp=$enableval)
  if test x$ac_enable_srtp != xno; then
   AC_MSG_RESULT(no)
   AC_DEFINE([ENABLE_DTLS_SRTP], 1, [enable DTLS-SRTP support])
  else
   ac_full=0
   AC_MSG_RESULT(yes)
  fi
  AM_CONDITIONAL(ENABLE_DTLS_SRTP, test "$ac_enable_srtp" != "no")

  AC_MSG_CHECKING([whether to disable ALPN extension])
  AC_ARG_ENABLE(alpn-support,
    AS_HELP_STRING([--disable-alpn-support],
                   [disable support for the Application Layer Protocol Negotiation (ALPN) extension]),
    ac_enable_alpn=$enableval,ac_enable_alpn=yes)
  if test x$ac_enable_alpn != xno; then
   AC_MSG_RESULT(no)
   AC_DEFINE([ENABLE_ALPN], 1, [enable ALPN support])
  else
   ac_full=0
   AC_MSG_RESULT(yes)
  fi
  AM_CONDITIONAL(ENABLE_ALPN, test "$ac_enable_alpn" != "no")

  AC_MSG_CHECKING([whether to disable RSA-EXPORT support])
  AC_ARG_ENABLE(rsa-export,
    AS_HELP_STRING([--disable-rsa-export],
                   [disable the RSA-EXPORT support]),
    ac_enable_rsa_export=$enableval, ac_enable_rsa_export=yes)
  if test x$ac_enable_rsa_export != xno; then
   AC_MSG_RESULT(no)
   AC_DEFINE([ENABLE_RSA_EXPORT], 1, [enable RSA-EXPORT])
  else
   ac_full=0
   AC_MSG_RESULT(yes)
  fi
  AM_CONDITIONAL(ENABLE_RSA_EXPORT, test "$ac_enable_rsa_export" != "no")

  ac_enable_heartbeat=yes
  AC_MSG_CHECKING([whether to disable TLS heartbeat support])
  AC_ARG_ENABLE(heartbeat-support,
    AS_HELP_STRING([--disable-heartbeat-support],
                   [disable support for the heartbeat extension]),
    ac_enable_heartbeat=$enableval)
  if test x$ac_enable_heartbeat != xno; then
   AC_MSG_RESULT(yes)
   AC_DEFINE([ENABLE_HEARTBEAT], 1, [enable heartbeat support])
  else
   AC_MSG_RESULT(no)
  fi
  AM_CONDITIONAL(ENABLE_HEARTBEAT, test "$ac_enable_heartbeat" != "no")

  ac_enable_srp=yes
  AC_MSG_CHECKING([whether to disable SRP authentication support])
  AC_ARG_ENABLE(srp-authentication,
    AS_HELP_STRING([--disable-srp-authentication],
                   [disable the SRP authentication support]),
    ac_enable_srp=$enableval)
  if test x$ac_enable_srp != xno; then
   AC_MSG_RESULT(no)
   AC_DEFINE([ENABLE_SRP], 1, [enable SRP authentication])
  else
   ac_full=0
   AC_MSG_RESULT(yes)
  fi
  AM_CONDITIONAL(ENABLE_SRP, test "$ac_enable_srp" != "no")
  
  ac_enable_psk=yes
  AC_MSG_CHECKING([whether to disable PSK authentication support])
  AC_ARG_ENABLE(psk-authentication,
    AS_HELP_STRING([--disable-psk-authentication],
                   [disable the PSK authentication support]),
    ac_enable_psk=$enableval)
  if test x$ac_enable_psk != xno; then
   AC_MSG_RESULT(no)
   AC_DEFINE([ENABLE_PSK], 1, [enable PSK authentication])
  else
   ac_full=0
   AC_MSG_RESULT(yes)
  fi
  AM_CONDITIONAL(ENABLE_PSK, test "$ac_enable_psk" != "no")
  
  ac_enable_anon=yes
  AC_MSG_CHECKING([whether to disable anonymous authentication support])
  AC_ARG_ENABLE(anon-authentication,
    AS_HELP_STRING([--disable-anon-authentication],
                   [disable the anonymous authentication support]),
    ac_enable_anon=$enableval)
  if test x$ac_enable_anon != xno; then
   AC_MSG_RESULT(no)
   AC_DEFINE([ENABLE_ANON], 1, [enable anonymous authentication])
  else
   ac_full=0
   AC_MSG_RESULT(yes)
  fi
  AM_CONDITIONAL(ENABLE_ANON, test "$ac_enable_anon" != "no")

  AC_MSG_CHECKING([whether to disable DHE support])
  AC_ARG_ENABLE(dhe,
    AS_HELP_STRING([--disable-dhe],
                   [disable the DHE support]),
    ac_enable_dhe=$enableval, ac_enable_dhe=yes)
  if test x$ac_enable_dhe != xno; then
   AC_MSG_RESULT(no)
   AC_DEFINE([ENABLE_DHE], 1, [enable DHE])
  else
   ac_full=0
   AC_MSG_RESULT(yes)
  fi
  AM_CONDITIONAL(ENABLE_DHE, test "$ac_enable_dhe" != "no")

  AC_MSG_CHECKING([whether to disable ECDHE support])
  AC_ARG_ENABLE(ecdhe,
    AS_HELP_STRING([--disable-ecdhe],
                   [disable the ECDHE support]),
    ac_enable_ecdhe=$enableval, ac_enable_ecdhe=yes)
  if test x$ac_enable_ecdhe != xno; then
   AC_MSG_RESULT(no)
   AC_DEFINE([ENABLE_ECDHE], 1, [enable DHE])
  else
   ac_full=0
   AC_MSG_RESULT(yes)
  fi
  AM_CONDITIONAL(ENABLE_ECDHE, test "$ac_enable_ecdhe" != "no")

  ac_enable_openpgp=yes
  AC_MSG_CHECKING([whether to disable OpenPGP Certificate authentication support])
  AC_ARG_ENABLE(openpgp-authentication,
    AS_HELP_STRING([--disable-openpgp-authentication],
                   [disable the OpenPGP authentication support]),
    ac_enable_openpgp=$enableval)
  if test x$ac_enable_openpgp = xno; then
   AC_MSG_RESULT(yes)
   ac_full=0
  else
   AC_DEFINE([ENABLE_OPENPGP], 1, [use openpgp authentication])
   AC_MSG_RESULT(no)
  fi
  AM_CONDITIONAL(ENABLE_OPENPGP, test "$ac_enable_openpgp" = "yes")

  # For cryptodev
  AC_MSG_CHECKING([whether to add cryptodev support])
  AC_ARG_ENABLE(cryptodev,
    AS_HELP_STRING([--enable-cryptodev], [enable cryptodev support]),
  enable_cryptodev=$enableval,enable_cryptodev=no)
  AC_MSG_RESULT($enable_cryptodev)

  if test "$enable_cryptodev" = "yes"; then
    AC_DEFINE([ENABLE_CRYPTODEV], 1, [Enable cryptodev support])
  fi

  AC_MSG_CHECKING([whether to disable OCSP support])
  AC_ARG_ENABLE(ocsp,
    AS_HELP_STRING([--disable-ocsp],
                   [disable OCSP support]),
    ac_enable_ocsp=$enableval,ac_enable_ocsp=yes)
  if test x$ac_enable_ocsp != xno; then
   ac_enable_ocsp=yes
   AC_MSG_RESULT(no)
   AC_DEFINE([ENABLE_OCSP], 1, [enable OCSP support])
  else
   ac_full=0
   AC_MSG_RESULT(yes)
  fi
  AM_CONDITIONAL(ENABLE_OCSP, test "$ac_enable_ocsp" != "no")


  AC_MSG_CHECKING([whether to disable session tickets support])
  AC_ARG_ENABLE(session-tickets,
    AS_HELP_STRING([--disable-session-tickets],
                   [disable session tickets support]),
    ac_enable_session_tickets=$enableval,ac_enable_session_tickets=yes)
  if test x$ac_enable_session_tickets != xno; then
   ac_enable_session_tickets=yes
   AC_MSG_RESULT(no)
   AC_DEFINE([ENABLE_SESSION_TICKETS], 1, [enable session tickets support])
  else
   ac_full=0
   AC_MSG_RESULT(yes)
  fi
  AM_CONDITIONAL(ENABLE_SESSION_TICKETS, test "$ac_enable_session_tickets" != "no")

  # For storing integers in pointers without warnings
  # http://developer.gnome.org/doc/API/2.0/glib/glib-Type-Conversion-Macros.html#desc
  AC_CHECK_SIZEOF(void *)
  AC_CHECK_SIZEOF(long long)
  AC_CHECK_SIZEOF(long)
  AC_CHECK_SIZEOF(int)
  if test x$ac_cv_sizeof_void_p = x$ac_cv_sizeof_long;then
      AC_DEFINE([GNUTLS_POINTER_TO_INT_CAST], [(long)],
                [Additional cast to bring void* to a type castable to int.])
  elif test x$ac_cv_sizeof_void_p = x$ac_cv_sizeof_long_long;then
      AC_DEFINE([GNUTLS_POINTER_TO_INT_CAST], [(long long)],
                [Additional cast to bring void* to a type castable to int.])
   else
      AC_DEFINE([GNUTLS_POINTER_TO_INT_CAST], [])
   fi

dnl this is called from somewhere else
dnl #AM_ICONV
dnl m4_ifdef([gl_ICONV_MODULE_INDICATOR],
dnl  [gl_ICONV_MODULE_INDICATOR([iconv])])
])
