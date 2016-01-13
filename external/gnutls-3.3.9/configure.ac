dnl Process this file with autoconf to produce a configure script.
# Copyright (C) 2000-2012 Free Software Foundation, Inc.
#
# Author: Nikos Mavrogiannopoulos, Simon Josefsson
#
# This file is part of GnuTLS.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301
# USA

AC_PREREQ(2.61)
AC_INIT([GnuTLS], [3.3.9], [bugs@gnutls.org])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([1.12.2 subdir-objects no-dist-gzip dist-xz dist-lzip -Wall -Wno-override])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_CONFIG_HEADERS([config.h])

AC_MSG_RESULT([***
*** Checking for compilation programs...
])

dnl Checks for programs.
PKG_PROG_PKG_CONFIG
AC_PROG_CC
gl_EARLY
ggl_EARLY
AM_PROG_AS
AM_PROG_AR
AC_PROG_CXX
AM_PROG_CC_C_O
AC_PROG_YACC
AC_PROG_SED
AC_CHECK_PROG([AUTOGEN], [autogen], [autogen], [/bin/true])

if test x"$AUTOGEN" = "x/bin/true"; then
  AC_MSG_WARN([[
***
*** autogen not found. Will not link against libopts.
*** ]])
enable_local_libopts=yes
fi

# For includes/gnutls/gnutls.h.in.
AC_SUBST(MAJOR_VERSION, `echo $PACKAGE_VERSION | sed 's/\(.*\)\..*\..*/\1/g'`)
AC_SUBST(MINOR_VERSION, `echo $PACKAGE_VERSION | sed 's/.*\.\(.*\)\..*/\1/g'`)
AC_SUBST(PATCH_VERSION, [[`echo $PACKAGE_VERSION | sed 's/.*\..*\.\([0-9]*\).*/\1/g'`]])
AC_SUBST(NUMBER_VERSION, `printf "0x%02x%02x%02x" $MAJOR_VERSION $MINOR_VERSION $PATCH_VERSION`)

touch suppressions.valgrind
dnl C and C++ capabilities
AC_C_INLINE
AC_HEADER_STDC

# For the C++ code
AC_ARG_ENABLE(cxx,
  AS_HELP_STRING([--disable-cxx], [unconditionally disable the C++ library]),
    use_cxx=$enableval, use_cxx=yes)
if test "$use_cxx" != "no"; then
  AC_LANG_PUSH(C++)
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[])], use_cxx=yes, use_cxx=no)
  AC_LANG_POP(C++)
fi
AM_CONDITIONAL(ENABLE_CXX, test "$use_cxx" != "no")
AM_CONDITIONAL(WANT_TEST_SUITE, [test -f tests/suite/mini-eagain2.c])

dnl Detect windows build
case "$host" in
  *mingw32* | *mingw64*)
    have_win=yes
  ;;
  *darwin*)
    have_macosx=yes
  ;;
  *)
    have_elf=yes
  ;;
esac

AM_CONDITIONAL(WINDOWS, test "$have_win" = yes)
AM_CONDITIONAL(MACOSX, test "$have_macosx" = yes)
AM_CONDITIONAL(ELF, test "$have_elf" = yes)

dnl Hardware Acceleration
AC_ARG_ENABLE(hardware-acceleration,
  AS_HELP_STRING([--disable-hardware-acceleration], [unconditionally disable hardware acceleration]),
    use_accel=$enableval, use_accel=yes)
hw_accel=none


if test "$use_accel" != "no"; then
case $host_cpu in
  i?86 | x86_64 | amd64)
      AC_CHECK_HEADERS(cpuid.h)
      if test "$host_cpu" = "x86_64" || test "$host_cpu" = "amd64"; then
        hw_accel="x86-64"
      else
        hw_accel="x86"
      fi
  ;;
  *)
  ;;
esac

fi

AC_ARG_ENABLE(padlock,
  AS_HELP_STRING([--disable-padlock], [unconditionally disable padlock acceleration]),
    use_padlock=$enableval, use_padlock=yes)

if test "$use_padlock" != "no"; then
    AC_DEFINE([ENABLE_PADLOCK], 1, [Enable padlock acceleration])
    AC_SUBST([ENABLE_PADLOCK])
fi
AM_CONDITIONAL(ENABLE_PADLOCK, test "$use_padlock" = "yes")
AM_CONDITIONAL(ASM_X86_64, test x"$hw_accel" = x"x86-64")
AM_CONDITIONAL(ASM_X86_32, test x"$hw_accel" = x"x86")
AM_CONDITIONAL(ASM_X86, test x"$hw_accel" = x"x86" || test x"$hw_accel" = x"x86-64")
AM_CONDITIONAL(HAVE_GCC_GNU89_INLINE_OPTION, test "$gnu89_inline" = "yes"])
AM_CONDITIONAL(HAVE_GCC, test "$GCC" = "yes")


dnl Try the hooks.m4
LIBGNUTLS_HOOKS
LIBGNUTLS_EXTRA_HOOKS

AC_ARG_ENABLE(doc,
  AS_HELP_STRING([--disable-doc], [don't generate any documentation]),
    enable_doc=$enableval, enable_doc=yes)
AM_CONDITIONAL(ENABLE_DOC, test "$enable_doc" != "no")

AC_ARG_ENABLE(tests,
  AS_HELP_STRING([--disable-tests], [don't compile or run any tests]),
    enable_tests=$enableval, enable_tests=yes)
AM_CONDITIONAL(ENABLE_TESTS, test "$enable_tests" != "no")

GTK_DOC_CHECK(1.1)
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.18])

AC_C_BIGENDIAN

dnl No fork on MinGW, disable some self-tests until we fix them.
dnl Check clock_gettime and pthread_mutex_lock in libc (avoid linking to other libs)
AC_CHECK_FUNCS([fork inet_ntop inet_pton getrusage getpwuid_r nanosleep daemon getpid clock_gettime iconv localtime vasprintf],,)
if test "$ac_cv_func_vasprintf" != "yes";then
  AC_MSG_CHECKING([for va_copy])
  AC_LINK_IFELSE([AC_LANG_PROGRAM([
	#include <stdarg.h>
	va_list a;],[
	va_list b;
	va_copy(b,a);
	va_end(b);])],
	[AC_DEFINE(HAVE_VA_COPY, 1, [Have va_copy()])
	AC_MSG_RESULT(va_copy)],
	[AC_LINK_IFELSE([AC_LANG_PROGRAM([
		#include <stdarg.h>
		va_list a;],[
		va_list b;
		__va_copy(b,a);
		va_end(b);])],
		[AC_DEFINE(HAVE___VA_COPY, 1, [Have __va_copy()])
		AC_MSG_RESULT(__va_copy)],
		[AC_MSG_RESULT(no)
		AC_MSG_ERROR([Your system lacks vasprintf() and va_copy()])])
	])
fi

AM_CONDITIONAL(HAVE_FORK, test "$ac_cv_func_fork" != "no")

AC_LIB_HAVE_LINKFLAGS(rt,, [#include <time.h>
#include <signal.h>
], [timer_create (0,0,0);])

if test "$have_win" != "yes";then
  AC_CHECK_FUNCS([pthread_mutex_lock],,)
  if test "$ac_cv_func_pthread_mutex_lock" != "yes";then
    AC_LIB_HAVE_LINKFLAGS(pthread,, [#include <pthread.h>], [pthread_mutex_lock (0);])
  fi
fi

if test "$ac_cv_func_nanosleep" != "yes";then
  AC_LIB_HAVE_LINKFLAGS(rt,, [#include <time.h>], [nanosleep (0, 0);])
  gnutls_needs_librt=yes
fi

if test "$ac_cv_func_clock_gettime" != "yes";then
  AC_LIB_HAVE_LINKFLAGS(rt,, [#include <time.h>], [clock_gettime (0, 0);])
  gnutls_needs_librt=yes
fi

ac_have_unicode=no
if test "$ac_cv_func_iconv" != "yes";then
  AC_LIB_HAVE_LINKFLAGS(iconv,, [#include <iconv.h>], [iconv (0, 0, 0, 0, 0);])
  if test "$HAVE_LIBICONV" = "yes";then
    ac_have_unicode=yes
  fi
else
  ac_have_unicode=yes
fi

if test "$ac_have_unicode" != "yes";then
  if test "$have_win" = "yes";then
    ac_have_unicode=yes
  fi
fi

dnl Note that g*l_INIT are run after we check for library capabilities,
dnl to prevent issues from caching lib dependencies. See discussion
dnl in https://bugs.gentoo.org/show_bug.cgi?id=494940 and
dnl http://gnu-autoconf.7623.n7.nabble.com/Correct-way-to-check-for-clock-gettime-td12276.html
gl_INIT
ggl_INIT

dnl GCC warnings to enable

AC_ARG_ENABLE([gcc-warnings],
  [AS_HELP_STRING([--enable-gcc-warnings],
		  [turn on lots of GCC warnings (for developers)])],
  [case $enableval in
     yes|no) ;;
     *)      AC_MSG_ERROR([bad value $enableval for gcc-warnings option]) ;;
   esac
   gl_gcc_warnings=$enableval],
  [gl_gcc_warnings=no]
)

if test "$gl_gcc_warnings" = yes; then
  gl_WARN_ADD([-Wframe-larger-than=1024], [WSTACK_CFLAGS])

  nw="$nw -Wsystem-headers"         # Don't let system headers trigger warnings
  nw="$nw -Wc++-compat"             # We don't care about C++ compilers
  nw="$nw -Wundef"                  # Warns on '#if GNULIB_FOO' etc in gnulib
  nw="$nw -Wtraditional"            # Warns on #elif which we use often
  nw="$nw -Wlogical-op"             # Too many false positives
  nw="$nw -Wold-style-definition"   # 
  nw="$nw -Wpadded"                 # Our structs are not padded
  nw="$nw -Wunreachable-code"       # Too many false positives
  nw="$nw -Wtraditional-conversion" # Too many warnings for now
  nw="$nw -Wcast-qual"              # Too many warnings for now
  nw="$nw -Waggregate-return"       # Too many warnings for now
  nw="$nw -Wshadow"                 # Too many warnings for now
  nw="$nw -Wswitch-default"         # Too many warnings for now
  nw="$nw -Wswitch-enum"            # Too many warnings for now
  nw="$nw -Wconversion"             # Too many warnings for now
  nw="$nw -Wsign-conversion"        # Too many warnings for now
  nw="$nw -Wformat-y2k"             # Too many warnings for now
  nw="$nw -Woverlength-strings"     # We use some in tests/
  nw="$nw -Wvla"                    # There is no point to avoid C99 variable length arrays
  nw="$nw -Wformat-nonliteral"      # Incompatible with gettext _()
  nw="$nw -Wunsafe-loop-optimizations"
  nw="$nw -Wstrict-overflow"
  nw="$nw -Wmissing-noreturn"
  nw="$nw -Winline"                 # Too compiler dependent
  nw="$nw -Wsuggest-attribute=pure" # Is it worth using attributes?
  nw="$nw -Wsuggest-attribute=const" # Is it worth using attributes?
  nw="$nw -Wsuggest-attribute=noreturn" # Is it worth using attributes?
  nw="$nw -Wstack-protector"        # Some functions cannot be protected
  nw="$nw -Wredundant-decls"        # Some files cannot be compiled with that (gl_fd_to_handle)

  gl_MANYWARN_ALL_GCC([ws])
  gl_MANYWARN_COMPLEMENT(ws, [$ws], [$nw])
  for w in $ws; do
    gl_WARN_ADD([$w])
  done

  gl_WARN_ADD([-Wno-missing-field-initializers]) # We need this one
  gl_WARN_ADD([-Wno-format-y2k])     # Too many warnings for now
  gl_WARN_ADD([-Wno-unused-value]) # warnings for things we don't want to get
  gl_WARN_ADD([-Wno-unused-result]) # warnings for things we don't want to get
  gl_WARN_ADD([-Wno-unused-parameter]) # Too many warnings for now
  gl_WARN_ADD([-Wno-stack-protector])  # Some functions cannot be protected
  gl_WARN_ADD([-Wno-int-to-pointer-cast])  # Some files cannot be compiled with that (gl_fd_to_handle)
  gl_WARN_ADD([-fdiagnostics-show-option])
fi

AC_SUBST([WERROR_CFLAGS])
AC_SUBST([WSTACK_CFLAGS])
AC_SUBST([WARN_CFLAGS])

dnl Programs for compilation or development
AC_PROG_LN_S
LT_INIT([disable-static,win32-dll,shared])



AC_ARG_ENABLE(self-checks,
  AS_HELP_STRING([--enable-self-checks], [enable self checking functionality]),
    enable_self_checks=$enableval, enable_self_checks=no)

AC_ARG_ENABLE(fips140-mode,
  AS_HELP_STRING([--enable-fips140-mode], [enable FIPS140-2 mode (implies self checks)]),
    enable_fips=$enableval, enable_fips=no)
AM_CONDITIONAL(ENABLE_FIPS140, test "$enable_fips" = "yes")
if [ test "$enable_fips" = "yes" ];then
  AC_LIB_HAVE_LINKFLAGS(dl,, [#include <dlfcn.h>], [dladdr (0, 0);])
  if test "x$HAVE_LIBDL" = "xyes";then
    enable_self_checks=yes

    AC_DEFINE([ENABLE_FIPS140], 1, [Enable FIPS140-2 mode])

    AC_SUBST([FIPS140_LIBS], $LIBDL)
  else
    enable_fips=no
    AC_MSG_WARN([[
*** 
*** This system is not supported in FIPS140 mode.
*** libdl and dladdr() are required.
*** ]])
  fi
fi

AC_ARG_ENABLE(non-suiteb-curves,
  AS_HELP_STRING([--disable-non-suiteb-curves], [disable curves not in SuiteB]),
    enable_non_suiteb=$enableval, enable_non_suiteb=yes)
if [ test "$enable_non_suiteb" = "yes" ];then
  AC_DEFINE([ENABLE_NON_SUITEB_CURVES], 1, [Enable all curves])
fi

AM_CONDITIONAL(ENABLE_SELF_CHECKS, test "$enable_self_checks" = "yes")
if [ test "$enable_self_checks" = "yes" ];then
   AC_DEFINE([ENABLE_SELF_CHECKS], 1, [Self checks are included in the library])
fi

AC_MSG_CHECKING([whether to build libdane])
AC_ARG_ENABLE(libdane,
    AS_HELP_STRING([--disable-libdane],
                   [disable the built of libdane]),
    enable_dane=$enableval, enable_dane=yes)
AC_MSG_RESULT($enable_dane)

if test "$enable_dane" != "no"; then
    LIBS="$oldlibs -lunbound"
    AC_MSG_CHECKING([for unbound library])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([
		   #include <unbound.h>],[
		   struct ub_ctx* ctx;
		   ctx = ub_ctx_create();])],
		  [AC_MSG_RESULT(yes)
		   AC_SUBST([UNBOUND_LIBS], [-lunbound])
		   AC_SUBST([UNBOUND_CFLAGS], [])
		   AC_DEFINE([HAVE_DANE], 1, [Enable the DANE library])
		   enable_dane=yes],
		  [AC_MSG_RESULT(no)
   	           AC_MSG_WARN([[
*** 
*** libunbound was not found. Libdane will not be built.
*** ]])
		  enable_dane=no])
    LIBS="$oldlibs"
fi

AM_CONDITIONAL(ENABLE_DANE, test "$enable_dane" = "yes")

AC_ARG_WITH(unbound-root-key-file, AS_HELP_STRING([--with-unbound-root-key-file],
                                 [specify the unbound root key file]),
            unbound_root_key_file="$withval", 
if test "$have_win" = yes; then
    unbound_root_key_file="C:\\Program Files\\Unbound\\root.key"
else
    if test -f /var/lib/unbound/root.key;then
    	unbound_root_key_file="/var/lib/unbound/root.key"
    else
        unbound_root_key_file="/etc/unbound/root.key"
    fi
fi
)

AC_DEFINE_UNQUOTED([UNBOUND_ROOT_KEY_FILE],
  ["$unbound_root_key_file"], [The DNSSEC root key file])

AC_ARG_WITH(system-priority-file, AS_HELP_STRING([--with-system-priority-file],
                                 [specify the system priority file]),
            system_priority_file="$withval", 
system_priority_file="/etc/gnutls/default-priorities"
)

AC_DEFINE_UNQUOTED([SYSTEM_PRIORITY_FILE],
  ["$system_priority_file"], [The system priority file])


dnl Check for p11-kit
P11_KIT_MINIMUM=0.20.7
AC_ARG_WITH(p11-kit,
	AS_HELP_STRING([--without-p11-kit],
		[Build without p11-kit and PKCS#11 support]))
if test "$with_p11_kit" != "no"; then
	PKG_CHECK_MODULES(P11_KIT, [p11-kit-1 >= $P11_KIT_MINIMUM], [with_p11_kit=yes], [with_p11_kit=no])
	if test "$with_p11_kit" != "no";then
		AC_DEFINE([ENABLE_PKCS11], 1, [Build PKCS#11 support])
		if test "x$GNUTLS_REQUIRES_PRIVATE" = "x"; then
			GNUTLS_REQUIRES_PRIVATE="Requires.private: p11-kit-1"
		else
			GNUTLS_REQUIRES_PRIVATE="${GNUTLS_REQUIRES_PRIVATE}, p11-kit-1"
		fi
	else
	        with_p11_kit=no
	        AC_MSG_WARN([[
*** 
*** p11-kit >= $P11_KIT_MINIMUM was not found. PKCS #11 support will be disabled.
*** You may get it from http://p11-glue.freedesktop.org/p11-kit.html
*** ]])
	fi
fi

AM_CONDITIONAL(ENABLE_PKCS11, test "$with_p11_kit" != "no")

AC_ARG_WITH(tpm,
	AS_HELP_STRING([--without-tpm],
		[Disable TPM (trousers) support.]),
		[with_tpm=$withval], [with_tpm=yes])
if test "$with_tpm" != "no"; then
    LIBS="$oldlibs -ltspi"
    AC_MSG_CHECKING([for tss library])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([
		   #include <trousers/tss.h>
		   #include <trousers/trousers.h>],[
		   int err = Tspi_Context_Create((void *)0);
		   Trspi_Error_String(err);])],
		  [AC_MSG_RESULT(yes)
		   AC_SUBST([TSS_LIBS], [-ltspi])
		   AC_SUBST([TSS_CFLAGS], [])
		   AC_DEFINE([HAVE_TROUSERS], 1, [Enable TPM])
		   with_tpm=yes],
		  [AC_MSG_RESULT(no)
   	           AC_MSG_WARN([[
*** 
*** trousers was not found. TPM support will be disabled.
*** ]])
		  with_tpm=no])
    LIBS="$oldlibs"
fi

AM_CONDITIONAL(ENABLE_TROUSERS, test "$with_tpm" != "no")

LIBOPTS_CHECK([src/libopts])
if test "$NEED_LIBOPTS_DIR" = "true";then
	dnl replace libopts-generated files with distributed backups, if present
	missing_baks=
	for i in ${srcdir}/src/*-args.c.bak ${srcdir}/src/*-args.h.bak; do
		nam=`echo $i|sed 's/.bak//g'`
		if test -f $i;then
			cp -f $i $nam
		else
			missing_baks=true
		fi
	done
	if test -z "$missing_baks"; then
		AC_SUBST([AUTOGEN], [/bin/true])
	fi
	enable_local_libopts=yes
else
	enable_local_libopts=no
fi
AM_CONDITIONAL(NEED_LIBOPTS, test "$enable_local_libopts" = "yes")

AC_CHECK_TYPE(ssize_t,
  [
    DEFINE_SSIZE_T="#include <sys/types.h>"
    AC_SUBST(DEFINE_SSIZE_T)
  ], [
    AC_DEFINE([NO_SSIZE_T], 1, [no ssize_t type was found])
    DEFINE_SSIZE_T="typedef int ssize_t;"
    AC_SUBST(DEFINE_SSIZE_T)
  ], [
    #include <sys/types.h>
  ])

# For minitasn1.
AC_CHECK_SIZEOF(unsigned long int, 4)
AC_CHECK_SIZEOF(unsigned int, 4)

AC_ARG_WITH(zlib, AS_HELP_STRING([--without-zlib],
                                 [disable zlib compression support]),
            ac_zlib=$withval, ac_zlib=yes)
AC_MSG_CHECKING([whether to include zlib compression support])
if test x$ac_zlib != xno; then
 AC_MSG_RESULT(yes)
 AC_LIB_HAVE_LINKFLAGS(z,, [#include <zlib.h>], [compress (0, 0, 0, 0);])
 if test x$ac_cv_libz != xyes; then
   AC_MSG_WARN(
*** 
*** ZLIB was not found. You will not be able to use ZLIB compression.)
 fi
else
 AC_MSG_RESULT(no)
fi

PKG_CHECK_EXISTS(zlib, ZLIB_HAS_PKGCONFIG=y, ZLIB_HAS_PKGCONFIG=n)

if test x$ac_zlib != xno; then
  if test "$ZLIB_HAS_PKGCONFIG" = "y" ; then
    if test "x$GNUTLS_REQUIRES_PRIVATE" = x; then
      GNUTLS_REQUIRES_PRIVATE="Requires.private: zlib"
    else
      GNUTLS_REQUIRES_PRIVATE="$GNUTLS_REQUIRES_PRIVATE, zlib"
    fi
  fi
fi
AC_SUBST(GNUTLS_REQUIRES_PRIVATE)



AC_ARG_WITH([default-trust-store-pkcs11],
  [AS_HELP_STRING([--with-default-trust-store-pkcs11=URI],
    [use the given pkcs11 uri as default trust store])])

if test "x$with_default_trust_store_pkcs11" != x; then
  if test "x$with_p11_kit" = xno; then
    AC_MSG_ERROR([cannot use pkcs11 store without p11-kit])
  fi
  AC_DEFINE_UNQUOTED([DEFAULT_TRUST_STORE_PKCS11],
    ["$with_default_trust_store_pkcs11"], [use the given pkcs11 uri as default trust store])
fi

AC_ARG_WITH([default-trust-store-dir],
  [AS_HELP_STRING([--with-default-trust-store-dir=DIR],
    [use the given directory as default trust store])])

if test "x$with_default_trust_store_dir" != x; then
  AC_DEFINE_UNQUOTED([DEFAULT_TRUST_STORE_DIR],
    ["$with_default_trust_store_dir"], [use the given directory as default trust store])
fi

dnl auto detect http://lists.gnu.org/archive/html/help-gnutls/2012-05/msg00004.html
AC_ARG_WITH([default-trust-store-file],
  [AS_HELP_STRING([--with-default-trust-store-file=FILE],
    [use the given file default trust store])], with_default_trust_store_file="$withval",
  [if test "$build" = "$host" && test x$with_default_trust_store_pkcs11 = x && test x$with_default_trust_store_dir = x;then
  for i in \
    /etc/ssl/ca-bundle.pem \
    /etc/ssl/certs/ca-certificates.crt \
    /etc/pki/tls/cert.pem \
    /usr/local/share/certs/ca-root-nss.crt \
    /etc/ssl/cert.pem
    do
    if test -e "$i"; then
      with_default_trust_store_file="$i"
      break
    fi
  done
  fi]
)

if test "$with_default_trust_store_file" = "no";then
  with_default_trust_store_file=""
fi

AC_ARG_WITH([default-crl-file],
  [AS_HELP_STRING([--with-default-crl-file=FILE],
    [use the given CRL file as default])])

AC_ARG_WITH([default-blacklist-file],
  [AS_HELP_STRING([--with-default-blacklist-file=FILE],
    [use the given certificate blacklist file as default])])

if test "x$with_default_trust_store_file" != x; then
  AC_DEFINE_UNQUOTED([DEFAULT_TRUST_STORE_FILE],
    ["$with_default_trust_store_file"], [use the given file default trust store])
fi

if test "x$with_default_crl_file" != x; then
  AC_DEFINE_UNQUOTED([DEFAULT_CRL_FILE],
    ["$with_default_crl_file"], [use the given CRL file])
fi

if test "x$with_default_blacklist_file" != x; then
  AC_DEFINE_UNQUOTED([DEFAULT_BLACKLIST_FILE],
    ["$with_default_blacklist_file"], [use the given certificate blacklist file])
fi

dnl Guile bindings.
opt_guile_bindings=yes
AC_MSG_CHECKING([whether building Guile bindings])
AC_ARG_ENABLE(guile,
	AS_HELP_STRING([--enable-guile], [build GNU Guile bindings]),
opt_guile_bindings=$enableval)
AC_MSG_RESULT($opt_guile_bindings)

AC_ARG_WITH([guile-site-dir],
  [AS_HELP_STRING([--with-guile-site-dir],
     [use the given directory as the Guile site (use with care)])])

if test "$opt_guile_bindings" = "yes"; then
   AC_MSG_RESULT([***
*** Detecting GNU Guile...
])

   AC_PATH_PROG([guile_snarf], [guile-snarf])
   if test "x$guile_snarf" = "x"; then
      AC_MSG_WARN([`guile-snarf' from Guile 1.8 not found.  Guile bindings not built.])
      opt_guile_bindings=no
   else
      GUILE_PROGS
      GUILE_FLAGS

      save_CFLAGS="$CFLAGS"
      save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $GUILE_CFLAGS"
      LIBS="$LIBS $GUILE_LDFLAGS"
      AC_MSG_CHECKING([whether GNU Guile is recent enough])
      AC_LINK_IFELSE([AC_LANG_PROGRAM([], [scm_from_locale_string ("")])],
        [], [opt_guile_bindings=no])
      CFLAGS="$save_CFLAGS"
      LIBS="$save_LIBS"

      if test "$opt_guile_bindings" = "yes"; then
        AC_MSG_RESULT([yes])
	case "x$with_guile_site_dir" in 
	     xno)
		# Use the default $(GUILE_SITE).
		GUILE_SITE_DIR
		;;
	     x|xyes)
		# Automatically derive $(GUILE_SITE) from $(pkgdatadir).  This
		# hack is used to allow `distcheck' to work (see
		# `DISTCHECK_CONFIGURE_FLAGS' in the top-level `Makefile.am').
		GUILE_SITE="\$(datadir)/guile/site"
		AC_SUBST(GUILE_SITE)
		;;
	     *)
		# Use the user-specified directory as $(GUILE_SITE).
		GUILE_SITE="$with_guile_site_dir"
		AC_SUBST(GUILE_SITE)
		;;
	esac
        AC_MSG_CHECKING([whether gcc supports -fgnu89-inline])
        _gcc_cflags_save="$CFLAGS"
        CFLAGS="${CFLAGS} -fgnu89-inline"
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])],
                          gnu89_inline=yes, gnu89_inline=no)
        AC_MSG_RESULT($gnu89_inline)
        CFLAGS="$_gcc_cflags_save"

	# Optional Guile functions.
	save_CFLAGS="$CFLAGS"
	save_LIBS="$LIBS"
	CFLAGS="$CFLAGS $GUILE_CFLAGS"
	LIBS="$LIBS $GUILE_LDFLAGS"
	AC_CHECK_FUNCS([scm_gc_malloc_pointerless])
	CFLAGS="$save_CFLAGS"
	LIBS="$save_LIBS"

	# The place where guile-gnutls.la will go.
	AC_MSG_CHECKING([the Guile effective version])
	guile_effective_version="`$GUILE -c '(display (effective-version))'`"
	AC_MSG_RESULT([$guile_effective_version])
	guileextensiondir="$libdir/guile/$guile_effective_version"
	AC_SUBST([guileextensiondir])
      else
        AC_MSG_RESULT([no])
        AC_MSG_WARN([A sufficiently recent GNU Guile not found.  Guile bindings not built.])
        opt_guile_bindings=no
      fi
   fi
fi
AM_CONDITIONAL(HAVE_GUILE, test "$opt_guile_bindings" = "yes")

LIBGNUTLS_LIBS="-L${libdir} -lgnutls $LIBS"
LIBGNUTLS_CFLAGS="-I${includedir}"
AC_SUBST(LIBGNUTLS_LIBS)
AC_SUBST(LIBGNUTLS_CFLAGS)

AM_CONDITIONAL(NEEDS_LIBRT, test "$gnutls_needs_librt" = "yes")

AC_DEFINE([GNUTLS_COMPAT_H], 1, [Make sure we don't use old features in code.])
AC_DEFINE([GNUTLS_INTERNAL_BUILD], 1, [We allow temporarily usage of deprecated functions - until they are removed.])

AC_DEFINE([fread_file], [_gnutls_fread_file], [static lib rename])
AC_DEFINE([read_file], [_gnutls_read_file], [static lib rename])
AC_DEFINE([read_binary_file], [_gnutls_read_binary_file], [static lib rename])

dnl Crywrap dependencies
   AC_MSG_RESULT([***
*** Checking dependencies for crywrap...
])

crywrap=no

if test "$have_win" != "yes"; then

AC_CHECK_HEADERS([arpa/inet.h netinet/in.h sys/select.h sys/types.h sys/wait.h])

dnl **********************
dnl * Typedefs & co
dnl **********************
AC_CACHE_CHECK([return type of signal handlers],[ac_cv_type_signal],[AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM([#include <sys/types.h>
#include <signal.h>
],
		 [return *(signal (0, 0)) (0) == 1;])],
		   [ac_cv_type_signal=int],
		   [ac_cv_type_signal=void])])
AC_DEFINE_UNQUOTED([RETSIGTYPE],[$ac_cv_type_signal],[Define as the return type of signal handlers
		    (`int' or `void').])

AC_FUNC_SELECT_ARGTYPES
AC_CHECK_FUNCS([alarm atexit dup2 epoll_create kqueue memchr memset munmap \
		putenv regcomp scandir select socket strcasecmp strchr \
		strdup strerror strncasecmp strrchr strstr strtoul uname])

AC_ARG_ENABLE(crywrap,
	AS_HELP_STRING([--disable-crywrap], [unconditionally disable the crywrap TLS proxy service]))

 libidn=no

 if test "x$enable_crywrap" != "xno" ; then
	PKG_CHECK_MODULES(LIBIDN, libidn >= 0.0.0, [libidn=yes], [libidn=no])
	AC_CHECK_FUNCS([argp_usage],[ac_argp=yes],[ac_argp=no])
 fi

 if test "x$libidn" != "xno" && test "$ac_cv_func_daemon" != "no" && test "$ac_argp" != "no";then
  crywrap=yes
 fi

fi

AM_CONDITIONAL(ENABLE_CRYWRAP, test "x$crywrap" != "xno")

dnl end of crywrap requirements

dnl Some variables needed in makefiles
YEAR=`date +%Y`
AC_SUBST([YEAR], $YEAR)

AC_CONFIG_FILES([guile/pre-inst-guile], [chmod +x guile/pre-inst-guile])
AC_CONFIG_FILES([
  Makefile
  doc/Makefile
  doc/credentials/Makefile
  doc/credentials/openpgp/Makefile
  doc/credentials/srp/Makefile
  doc/credentials/x509/Makefile
  doc/cyclo/Makefile
  doc/doxygen/Doxyfile
  doc/examples/Makefile
  doc/latex/Makefile
  doc/manpages/Makefile
  doc/reference/Makefile
  doc/reference/version.xml
  doc/scripts/Makefile
  extra/Makefile
  extra/includes/Makefile
  libdane/Makefile
  libdane/includes/Makefile
  libdane/gnutls-dane.pc
  gl/Makefile
  gl/tests/Makefile
  guile/Makefile
  guile/modules/Makefile
  guile/src/Makefile
  guile/tests/Makefile
  lib/Makefile
  lib/accelerated/Makefile
  lib/accelerated/x86/Makefile
  lib/algorithms/Makefile
  lib/auth/Makefile
  lib/ext/Makefile
  lib/extras/Makefile
  lib/gnutls.pc
  lib/includes/Makefile
  lib/includes/gnutls/gnutls.h
  lib/minitasn1/Makefile
  lib/nettle/Makefile
  lib/opencdk/Makefile
  lib/openpgp/Makefile
  lib/x509/Makefile
  po/Makefile.in
  src/Makefile
  src/crywrap/Makefile
  src/gl/Makefile
  tests/Makefile
  tests/cert-tests/Makefile
  tests/dsa/Makefile
  tests/dtls/Makefile
  tests/srp/Makefile
  tests/ecdsa/Makefile
  tests/key-tests/Makefile
  tests/openpgp-certs/Makefile
  tests/pkcs1-padding/Makefile
  tests/pkcs12-decode/Makefile
  tests/pkcs8-decode/Makefile
  tests/rsa-md5-collision/Makefile
  tests/safe-renegotiation/Makefile
  tests/scripts/Makefile
  tests/sha2/Makefile
  tests/slow/Makefile
  tests/suite/Makefile
  tests/userid/Makefile
])

AC_OUTPUT

dnl  Warning flags:        errors: ${WERROR_CFLAGS} warnings: ${WARN_CFLAGS}
dnl  Valgrind:             $opt_valgrind_tests ${VALGRIND}
AC_MSG_NOTICE([summary of build options:

  version:              ${VERSION} shared $LT_CURRENT:$LT_REVISION:$LT_AGE
  Host/Target system:   ${host}
  Build system:         ${build}
  Install prefix:       ${prefix}
  Compiler:             ${CC}
  CFlags:               ${CFLAGS}
  Library types:        Shared=${enable_shared}, Static=${enable_static}
  Local libopts:        ${enable_local_libopts}
  Local libtasn1:       ${included_libtasn1}
  Use nettle-mini:      ${mini_nettle}
])

AC_MSG_NOTICE([External hardware support:

  /dev/crypto:          $enable_cryptodev
  Hardware accel:       $hw_accel
  Padlock accel:        $use_padlock
  PKCS#11 support:      $with_p11_kit
  TPM support:          $with_tpm
])

AC_MSG_NOTICE([Optional features:
(note that included applications might not compile properly
if features are disabled)

  DTLS-SRTP support:    $ac_enable_srtp
  ALPN support:         $ac_enable_alpn
  OCSP support:         $ac_enable_ocsp
  Ses. ticket support:  $ac_enable_session_tickets
  OpenPGP support:      $ac_enable_openpgp
  SRP support:          $ac_enable_srp
  PSK support:          $ac_enable_psk
  DHE support:          $ac_enable_dhe
  ECDHE support:        $ac_enable_ecdhe
  RSA-EXPORT support:   $ac_enable_rsa_export
  Anon auth support:    $ac_enable_anon
  Heartbeat support:    $ac_enable_heartbeat
  Unicode support:      $ac_have_unicode
  Self checks:          $enable_self_checks
  Non-SuiteB curves:    $enable_non_suiteb
  FIPS140 mode:         $enable_fips
])

AC_MSG_NOTICE([Optional applications:

  crywrap app:          $libidn
])

AC_MSG_NOTICE([Optional libraries:

  Guile wrappers:       $opt_guile_bindings
  C++ library:          $use_cxx
  DANE library:         $enable_dane
  OpenSSL compat:       $enable_openssl
])

AC_MSG_NOTICE([System files:

  Trust store pkcs11:   $with_default_trust_store_pkcs11
  Trust store dir:      $with_default_trust_store_dir
  Trust store file:     $with_default_trust_store_file
  Blacklist file:       $with_default_blacklist_file
  CRL file:             $with_default_crl_file
  Priority file:        $system_priority_file
  DNSSEC root key file: $unbound_root_key_file
])

if test ! -f "$unbound_root_key_file"; then
AC_MSG_WARN([[
*** 
*** The DNSSEC root key file in $unbound_root_key_file was not found. 
*** This file is needed for the verification of DNSSEC responses.
*** Use the command: unbound-anchor -a "$unbound_root_key_file"
*** to generate or update it.
*** ]])
fi
