AC_PREREQ(2.60)
AC_INIT(sbc, 1.3)

AM_INIT_AUTOMAKE([foreign subdir-objects color-tests])
AC_CONFIG_HEADERS(config.h)

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AM_MAINTAINER_MODE

AC_PREFIX_DEFAULT(/usr/local)

COMPILER_FLAGS

AC_LANG_C

AC_PROG_CC
AC_PROG_CC_PIE
AC_PROG_INSTALL

LT_INIT

AC_ARG_ENABLE(optimization, AC_HELP_STRING([--disable-optimization],
			[disable code optimization through compiler]), [
	if (test "${enableval}" = "no"); then
		CFLAGS="$CFLAGS -O0"
	fi
])

AC_ARG_ENABLE(debug, AC_HELP_STRING([--enable-debug],
			[enable compiling with debugging information]), [
	if (test "${enableval}" = "yes" &&
				test "${ac_cv_prog_cc_g}" = "yes"); then
		CFLAGS="$CFLAGS -g"
	fi
])

AC_ARG_ENABLE(pie, AC_HELP_STRING([--enable-pie],
			[enable position independent executables flag]), [
	if (test "${enableval}" = "yes" &&
				test "${ac_cv_prog_cc_pie}" = "yes"); then
		CFLAGS="$CFLAGS -fPIE"
		LDFLAGS="$LDFLAGS -pie"
	fi
])

AC_ARG_ENABLE(high-precision, AC_HELP_STRING([--enable-high-precision],
					[enable SBC high precision support]),
					[enable_high_precision=${enableval}])
if (test "${enable_high_precision}" = "yes"); then
	AC_DEFINE(SBC_HIGH_PRECISION, 1,
		[Define to 1 to enable high precision build of SBC encoder])
fi

AC_ARG_ENABLE(tools, AC_HELP_STRING([--disable-tools],
		[disable SBC tools]), [enable_tools=${enableval}])
AM_CONDITIONAL(TOOLS, test "${enable_tools}" != "no")

AC_ARG_ENABLE(tester, AC_HELP_STRING([--disable-tester],
		[disable SBC tester]), [enable_tester=${enableval}])
if (test "${enable_tester}" != "no"); then
	PKG_CHECK_MODULES(SNDFILE, sndfile, dummy=yes,
				AC_MSG_ERROR(sndfile library is required))
	AC_SUBST(SNDFILE_CFLAGS)
	AC_SUBST(SNDFILE_LIBS)
fi
AM_CONDITIONAL(TESTER, test "${enable_tester}" != "no")

AC_OUTPUT(Makefile sbc/sbc.pc)
