dnl Process this file with autoconf to produce a configure script.
AC_INIT([ortp],[0.27.0])
AC_CANONICAL_SYSTEM

case $INSTALL in
        *ginstall*)
                INSTALL="$INSTALL -C"
        ;;
esac

dnl Source packaging numbers
ORTP_MAJOR_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f1)
ORTP_MINOR_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f2)
ORTP_MICRO_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f3)
ORTP_EXTRA_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f4)

LIBORTP_SO_CURRENT=11 dnl increment this number when you add/change/remove an interface
LIBORTP_SO_REVISION=0 dnl increment this number when you change source code, without changing interfaces; set to 0 when incrementing CURRENT
LIBORTP_SO_AGE=0 dnl increment this number when you add an interface, set to 0 if you remove an interface

LIBORTP_SO_VERSION=$LIBORTP_SO_CURRENT:$LIBORTP_SO_REVISION:$LIBORTP_SO_AGE
ORTP_VERSION=${ORTP_MAJOR_VERSION}.${ORTP_MINOR_VERSION}.${ORTP_MICRO_VERSION}

if test -n "$ORTP_EXTRA_VERSION" ; then
	ORTP_VERSION="${ORTP_VERSION}.${ORTP_EXTRA_VERSION}"
fi

ORTP_PKGCONFIG_VERSION=${ORTP_VERSION}

AC_SUBST(LIBORTP_SO_CURRENT, $LIBORTP_SO_CURRENT)
AC_SUBST(LIBORTP_SO_VERSION)
AC_SUBST(ORTP_VERSION)
AC_SUBST(ORTP_PKGCONFIG_VERSION)


PACKAGE=ortp

AM_INIT_AUTOMAKE([tar-ustar])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])],)
AC_SUBST([docdir], [${datadir}/doc])
AC_CONFIG_HEADERS(ortp-config.h)
AC_CONFIG_MACRO_DIR([m4])
AC_DEFINE_UNQUOTED(ORTP_MAJOR_VERSION,$ORTP_MAJOR_VERSION, [major version])
AC_DEFINE_UNQUOTED(ORTP_MINOR_VERSION,$ORTP_MINOR_VERSION, [minor version])
AC_DEFINE_UNQUOTED(ORTP_MICRO_VERSION,$ORTP_MICRO_VERSION, [micro version])
AC_DEFINE_UNQUOTED(ORTP_VERSION,"$ORTP_VERSION",[ortp version number])

dnl Checks for programs.
AC_PROG_CC
LT_INIT([win32-dll shared disable-static])

gl_LD_OUTPUT_DEF

AC_MSG_CHECKING([warning make an error on compilation])

AC_ARG_ENABLE(strict,
	AC_HELP_STRING([--enable-strict], [Build with stricter options @<:@yes@:>@]),
	[strictness="${enableval}"],
	[strictness=yes]
)

STRICT_OPTIONS="-Wall -Wuninitialized"
STRICT_OPTIONS_CC="-Wdeclaration-after-statement -Wstrict-prototypes"
STRICT_OPTIONS_CXX=""

#for clang

case $CC in
	*clang*)
		STRICT_OPTIONS="$STRICT_OPTIONS -Qunused-arguments "
	;;
esac

# because Darwin's gcc is actually clang, we need to check it...
case "$target_os" in
	*darwin*)
	STRICT_OPTIONS="$STRICT_OPTIONS -Wno-error=unknown-warning-option -Qunused-arguments -Wno-tautological-compare -Wno-unused-function "
	#disabled due to wrong optimization false positive with small string
	#(cf. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=35903)
	STRICT_OPTIONS="$STRICT_OPTIONS -Wno-array-bounds "
	;;
esac

if test "$strictness" = "yes" ; then
	STRICT_OPTIONS="$STRICT_OPTIONS -Werror -Wextra -Wno-unused-parameter -Wno-missing-field-initializers"
	CFLAGS="$CFLAGS -fno-strict-aliasing"
fi

AC_SUBST(STRICT_OPTIONS)
AC_SUBST(STRICT_OPTIONS_CC)
AC_SUBST(STRICT_OPTIONS_CXX)

AC_ARG_ENABLE(perf,
	[AS_HELP_STRING([--enable-perf], [Disable costly features to reduce cpu consumtion (default=no)])],
	[perf=$enableval],
	[perf=no]
)

ORTP_DEFS=

AC_DEFINE(__APPLE_USE_RFC_3542, 1, [ Apple wants you to declare what behavior you want by defining either __APPLE_USE_RFC_3542])


dnl enable timestamp support
AC_ARG_ENABLE(ntp-timestamp,
	[AS_HELP_STRING([--enable-ntp-timestamp], [Turn on NTP timestamping on received packet (default=no)])],
	[case "${enableval}" in
		yes)	ntptimestamp=true;;
		no)	ntptimestamp=false;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-ntp-timestamp) ;;
	esac],
	[ntptimestamp=false]
)
if test x$ntptimestamp = xtrue ; then
	ORTP_DEFS="$ORTP_DEFS -DORTP_TIMESTAMP"
fi

AC_ARG_ENABLE(mode64bit,
	[AS_HELP_STRING([--enable-mode64bit], [Produce a 64-bit library (default=no)])],
	[case "${enableval}" in
		yes)	mode64bit_enabled=yes;;
		no)	mode64bit_enabled=no;;
		*)	AC_MSG_ERROR("Bad value for --enable-mode64bit");;
	esac],
	[mode64bit_enabled=no]
)

AC_ARG_ENABLE(debug,
	[AS_HELP_STRING([--enable-debug], [Enable the display of traces showing the execution of the library (default=yes)])],
	[case "${enableval}" in
		yes)	debug_enabled=yes;;
		no)	debug_enabled=no;;
		*)	AC_MSG_ERROR("Bad value for --enable-debug");;
	esac],
	[debug_enabled=no]
)

hpux_host=no
posixtimer_interval=10000
PTHREAD_LDFLAGS=

case "$target_os" in
	*hpux*)
		hpux_host=yes
		AC_DEFINE(NOCONNECT,1,[Defined if we should not use connect() on udp sockets])
		CFLAGS="$CFLAGS -D_HPUX_SOURCE -D_XOPEN_SOURCE_EXTENDED -D_XOPEN_SOURCE=500 -D_POSIX_C_SOURCE=199506L"
		LIBS="$LIBS -lxnet"
	;;
	*freebsd*)
		AC_DEFINE(NOCONNECT,1,[Defined if we should not use connect() on udp sockets])
		PTHREAD_LDFLAGS="-pthread"
	;;
	*mingw32ce)
		CFLAGS="$CFLAGS -D_WIN32_WCE -D_WIN32_WINNT=0x0501 -DORTP_STATIC"
		LIBS="$LIBS -lws2 -liphlpapi"
		mingw_found=yes
	;;
	*mingw*)
		CFLAGS="$CFLAGS -D_WIN32_WINNT=0x0501 -DORTP_STATIC"
		LIBS="$LIBS -lws2_32 -liphlpapi -lwinmm"
		mingw_found=yes
	;;
esac

AM_CONDITIONAL(BUILD_WIN32, test "$mingw_found" = "yes")
AC_CONFIG_COMMANDS([libtool-hacking],
	[if test "$mingw_found" = "yes" ; then
		echo "Hacking libtool to work with mingw..."
		sed -e 's/\*\" \$a_deplib \"\*/\*/' < ./libtool > libtool.tmp
		cp -f ./libtool.tmp ./libtool
		rm -f ./libtool.tmp
	fi],
	[mingw_found=$mingw_found]
)

if test "$GCC" != "yes" ; then
	if test "$hpux_host" = "yes" ; then
		dnl we are probably using HPUX cc compiler, so add a +O2 to CFLAGS
			CFLAGS="$CFLAGS +O2 -g "
		if test x$mode64bit_enabled = xyes ; then
			CFLAGS="$CFLAGS +DA2.0W +DS2.0"
		fi
	fi
else
	CFLAGS="$CFLAGS -Wall"
fi

build_scheduler=yes

dnl Check if we have seteuid system call
AC_CHECK_FUNCS(seteuid)

dnl Check if we have arc4random family routines available
AC_CHECK_FUNCS(arc4random)


dnl check if we can use the pthread_library
AC_CHECK_LIB(pthread, pthread_mutex_init, [pthread_enabled=yes], [pthread_enabled=no])
if test $pthread_enabled = "no" ; then
	build_scheduler=no
else
	PTHREAD_LIBS="-lpthread"
	PTHREAD_CFLAGS="-D_REENTRANT"
	AC_SUBST(PTHREAD_CFLAGS)
	AC_SUBST(PTHREAD_LIBS)
	AC_SUBST(PTHREAD_LDFLAGS)
fi
AC_ARG_WITH(thread-stack-size,
	AC_HELP_STRING([--with-thread-stack-size=SIZE-IN-BYTES],[Set thread stack size [[default=os-default]]]),
	[thread_stack_size=$withval],
	[thread_stack_size=0]
)
AC_DEFINE_UNQUOTED(ORTP_DEFAULT_THREAD_STACK_SIZE, $thread_stack_size, [Default thread stack size (0 = let uperating system decide)])


dnl check if we can use the rt library
AC_CHECK_LIB(rt, clock_gettime, [rt_enabled=yes])
if test "$rt_enabled" = "yes" ; then
	RT_LIBS="-lrt"
	AC_SUBST(RT_LIBS)
fi


if test $debug_enabled = "yes"; then
	ORTP_DEFS="$ORTP_DEFS -DORTP_DEBUG_MODE"
	CFLAGS=`echo $CFLAGS | sed 's/-O.//'`
	CFLAGS="$CFLAGS -g"
fi


AC_ARG_ENABLE(memcheck,
	[AS_HELP_STRING([--enable-memcheck], [Enable memory leak detection (HPUX only)])],
	[case "${enableval}" in
		yes)	memcheck_enabled=yes;;
		no)	memcheck_enabled=no;;
		*)	AC_MSG_ERROR("Bad value for --enable-memcheck");;
	esac],
	[memcheck_enabled=no]
)

if test "$memcheck_enabled" = "yes" ; then
	if test "$hpux_host" = "yes" ; then
		AC_DEFINE(ENABLE_MEMCHECK,1,[Defined when memory leak checking if enabled])
	else
		echo "WARNING ************ : the memory check option is only available for HPUX."
	fi
fi


dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(poll.h sys/poll.h sys/uio.h fcntl.h sys/time.h unistd.h sys/audio.h linux/soundcard.h sys/shm.h stdatomic.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_HEADER_TIME
AC_WORDS_BIGENDIAN
if test x$ac_cv_c_bigendian = xyes ; then
	ORTP_DEFS="$ORTP_DEFS -DORTP_BIGENDIAN"
fi

dnl Checks for library functions.
AC_CHECK_FUNCS(select socket strerror)

if test $hpux_host = "yes" ; then
dnl it seems 10 ms is too fast on hpux and it causes trouble
		posixtimer_interval=20000
fi

AC_DEFINE_UNQUOTED(POSIXTIMER_INTERVAL,$posixtimer_interval,[Defines the periodicity of the rtp scheduler in microseconds])

if test "$perf" = "yes" ; then
	CFLAGS="$CFLAGS -DPERF"
fi


PKG_CHECK_MODULES(BCTOOLBOX, bctoolbox, [found_bctoolbox=yes],[found_bctoolbox=no])
if test "x$found_bctoolbox" != "xyes" ; then
	AC_MSG_ERROR(["Could not find bctoolbox (required dependency)"])
fi


ORTPDEPS_LIBS="$ORTPDEPS_LIBS $PTHREAD_LDFLAGS $BCTOOLBOX_LIBS"
ORTPDEPS_CFLAGS="$ORTPDEPS_CFLAGS $PTHREAD_CFLAGS $ORTP_DEFS $BCTOOLBOX_CFLAGS"
CFLAGS="$CFLAGS $ORTP_DEFS"
echo "$ORTPDEPS_CFLAGS" > ortp.defs


AC_ARG_ENABLE(tests,
	[AS_HELP_STRING([--disable-tests], [Disable compilation of tests])],
	[case "${enableval}" in
		yes)	tests_enabled=true ;;
		no)	tests_enabled=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --disable-tests) ;;
	esac],
	[tests_enabled=false]
)
AM_CONDITIONAL(ENABLE_TESTS, test x$tests_enabled = xtrue)

case "$target_os" in
	*linux*)
		# Eliminate -lstdc++ addition to postdeps for cross compiles.
		postdeps_CXX=`echo " $postdeps_CXX " | sed 's, -lstdc++ ,,g'`
	;;
esac

dnl ##################################################
dnl # Check for doxygen
dnl ##################################################
AC_ARG_ENABLE(documentation,
	[AS_HELP_STRING([--enable-documentation], [Documentation generation using doxygen (default=yes)])],
	[case "${enableval}" in
		yes)	documentation_enabled=yes;;
		no)		documentation_enabled=no;;
		*)		AC_MSG_ERROR("Bad value for --enable-documentation");;
	esac],
	[documentation_enabled=yes]
)
if test "$documentation_enabled" = "yes" ; then
	AC_CHECK_PROG(DOXYGEN,doxygen,doxygen,false)
else
	DOXYGEN=false
fi
AM_CONDITIONAL(HAVE_DOXYGEN, test "$DOXYGEN" != "false")
AC_CHECK_PROG(DOXYGEN,doxygen,doxygen,false)
AM_CONDITIONAL(HAVE_DOXYGEN, test "$DOXYGEN" != "false")


dnl ##################################################
dnl # Check for ESP Packager
dnl ##################################################

AC_PATH_PROG(EPM,epm,false)
AC_PATH_PROG(MKEPMLIST,mkepmlist,false)
AC_PATH_PROG(EPMINSTALL,epminstall,false)
AM_CONDITIONAL(WITH_EPM,test $EPM != false && test $MKEPMLIST != false && test $EPMINSTALL != false)

# Preferred packaging system, as per EPM terminology
case $target in
	*-*-linux*)
		if test -f /etc/debian_version ; then
			EPM_PKG_EXT=deb
		else
			EPM_PKG_EXT=rpm
		fi
	;;
	*-hp-hpux*)
		EPM_PKG_EXT=depot.gz
	;;
	*-dec-osf*)
		EPM_PKG_EXT=setld
	;;
esac
AC_SUBST(EPM_PKG_EXT)

# System software User & Group names
case $target in
	*-*-linux*)
		SYS_USER=root
		SYS_GROUP=root
	;;
	*-*-hpux*|*-dec-osf*)
		SYS_USER=bin
		SYS_GROUP=bin
	;;
esac
AC_SUBST(SYS_USER)
AC_SUBST(SYS_GROUP)

# CPU Architecture
case $target_cpu in
	i?86)
		ARCH=i386
	;;
	*)	ARCH=$target_cpu
	;;
esac
AC_SUBST(ARCH)

# Various other packaging variables, that can be over-ridden ad `make
# package' time
SUMMARY="An LGPL implementation of RTP - RFC3550"
AC_SUBST(SUMMARY)
PACKAGER=anonymous
AC_SUBST(PACKAGER)
LICENSE=LGPL
AC_SUBST(LICENSE)
VENDOR=Linphone
AC_SUBST(VENDOR)
RELEASE=1
AC_SUBST(RELEASE)

AC_SUBST(ORTPDEPS_CFLAGS)
AC_SUBST(ORTPDEPS_LIBS)
AC_SUBST(ORTPDEPS_LDFLAGS)

AC_OUTPUT(
	Makefile
	include/Makefile
	include/ortp/Makefile
	m4/Makefile
	src/Makefile
	src/tests/Makefile
	src/tests/win_receiver/Makefile
	src/tests/win_sender/Makefile
	build/Makefile
	ortp.pc
	ortp.spec
	ortp.doxygen
)

