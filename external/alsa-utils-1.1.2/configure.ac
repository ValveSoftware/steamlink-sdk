dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.59)
AC_INIT(alsa-utils, 1.1.2)
AC_CONFIG_SRCDIR([aplay/aplay.c])
AC_PREFIX_DEFAULT(/usr)
AM_INIT_AUTOMAKE

AM_MAINTAINER_MODE([enable])

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.15])

dnl Checks for programs.

AC_PROG_CC
dnl AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MKDIR_P
AC_PROG_LN_S
AC_PROG_SED
PKG_PROG_PKG_CONFIG
AM_PATH_ALSA(1.0.27)
if test "x$enable_alsatest" = "xyes"; then
AC_CHECK_FUNC([snd_ctl_elem_add_enumerated],
	      , [AC_ERROR([No user enum control support in alsa-lib])])
fi

dnl Check components
AC_CHECK_HEADERS([alsa/pcm.h], [have_pcm="yes"], [have_pcm="no"],
  [#include <alsa/asoundlib.h>])
AC_CHECK_HEADERS([alsa/mixer.h], [have_mixer="yes"], [have_mixer="no"],
  [#include <alsa/asoundlib.h>])
AC_CHECK_HEADERS([alsa/rawmidi.h], [have_rawmidi="yes"], [have_rawmidi="no"],
  [#include <alsa/asoundlib.h>])
AC_CHECK_HEADERS([alsa/seq.h], [have_seq="yes"], [have_seq="no"],
  [#include <alsa/asoundlib.h>])
AC_CHECK_HEADERS([alsa/use-case.h], [have_ucm="yes"], [have_ucm="no"],
  [#include <alsa/asoundlib.h>])
AC_CHECK_HEADERS([alsa/topology.h], [have_topology="yes"], [have_topology="no"],
  [#include <alsa/asoundlib.h>])
AC_CHECK_HEADERS([samplerate.h], [have_samplerate="yes"], [have_samplerate="no"],
  [#include <samplerate.h>])

AC_CHECK_LIB([asound], [snd_seq_client_info_get_card], [HAVE_SEQ_CLIENT_INFO_GET_CARD="yes"])
if test "$HAVE_SEQ_CLIENT_INFO_GET_CARD" = "yes" ; then
    AC_DEFINE([HAVE_SEQ_CLIENT_INFO_GET_CARD], 1, [alsa-lib supports snd_seq_client_info_get_card])
fi
AC_CHECK_LIB([asound], [snd_seq_client_info_get_pid], [HAVE_SEQ_CLIENT_INFO_GET_PID="yes"])
if test "$HAVE_SEQ_CLIENT_INFO_GET_PID" = "yes" ; then
    AC_DEFINE([HAVE_SEQ_CLIENT_INFO_GET_PID], 1, [alsa-lib supports snd_seq_client_info_get_pid])
fi

AM_CONDITIONAL(HAVE_PCM, test "$have_pcm" = "yes")
AM_CONDITIONAL(HAVE_MIXER, test "$have_mixer" = "yes")
AM_CONDITIONAL(HAVE_RAWMIDI, test "$have_rawmidi" = "yes")
AM_CONDITIONAL(HAVE_SEQ, test "$have_seq" = "yes")
AM_CONDITIONAL(HAVE_UCM, test "$have_ucm" = "yes")
AM_CONDITIONAL(HAVE_TOPOLOGY, test "$have_topology" = "yes")
AM_CONDITIONAL(HAVE_SAMPLERATE, test "$have_samplerate" = "yes")

dnl Use tinyalsa
alsabat_backend_tiny=
AC_ARG_ENABLE(alsabat_backend_tiny,
    AS_HELP_STRING([--enable-alsabat-backend-tiny], [Use tinyalsa for alsabat backend]),
    [case "${enableval}" in
      yes) alsabat_backend_tiny=true ;;
      no) alsabat_backend_tiny=false ;;
      *) AC_MSG_ERROR(bad value ${enableval} for --enable-alsabat-backend-tiny) ;;
    esac],[alsabat_backend_tiny=false])

dnl Disable bat
bat=
if test "$have_pcm" = "yes"; then
AC_ARG_ENABLE(bat,
     AS_HELP_STRING([--disable-bat], [Disable bat compilation]),
     [case "${enableval}" in
       yes) bat=true ;;
       no)  bat=false ;;
       *) AC_MSG_ERROR(bad value ${enableval} for --enable-bat) ;;
     esac],[bat=true])
fi
AM_CONDITIONAL(BAT, test x$bat = xtrue)

if test x$bat = xtrue; then

  saved_CFLAGS="$CFLAGS"
  saved_LDFLAGS="$LDFLAGS"
  saved_LIBS="$LIBS"
  FFTW_INC=""
  FFTW_LIB=""
  FFTW_CFLAGS=""
  dnl Check for libfftw3
  have_libfftw3="yes"
  AC_CHECK_LIB([fftw3f], [fftwf_malloc], , [have_libfftw3="no"])
  dnl Check for libtinyalsa
  have_libtinyalsa=
  if test x$alsabat_backend_tiny = xtrue; then
    have_libtinyalsa="yes"
    AC_CHECK_LIB([tinyalsa], [pcm_open], , [have_libtinyalsa="no"])
  fi
  AC_CHECK_LIB([m], [sqrtf], , [AC_MSG_ERROR([Error: Need sqrtf])])
  AC_CHECK_LIB([pthread], [pthread_create], , [AC_MSG_ERROR([Error: need PTHREAD library])])
  FFTW_CFLAGS="$CFLAGS"
  FFTW_LIB="$LIBS"
  CFLAGS="$saved_CFLAGS"
  LDFLAGS="$saved_LDFLAGS"
  LIBS="$saved_LIBS"
  AC_SUBST(FFTW_INC)
  AC_SUBST(FFTW_LIB)
  AC_SUBST(FFTW_CFLAGS)

fi
AM_CONDITIONAL(HAVE_LIBFFTW3, test "$have_libfftw3" = "yes")
AM_CONDITIONAL(HAVE_LIBTINYALSA, test "$have_libtinyalsa" = "yes")

dnl Check for librt
LIBRT=""
AC_MSG_CHECKING(for librt)
AC_ARG_WITH(librt,
  AS_HELP_STRING([--with-librt], [Use librt for monotonic clock (default = yes)]),
  [ have_librt="$withval" ], [ have_librt="yes" ])
if test "$have_librt" = "yes"; then
  AC_CHECK_LIB([rt], [clock_gettime], [HAVE_LIBRT="yes"])
  if test "$HAVE_LIBRT" = "yes" ; then
    LIBRT="-lrt"
    AC_DEFINE([HAVE_LIBRT], 1, [Have librt])
    AC_DEFINE([HAVE_CLOCK_GETTIME], 1, [Have clock gettime])
  fi
else
  AC_MSG_RESULT(no)
fi

dnl Disable alsamixer
CURSESINC=""
CURSESLIB=""
CURSES_CFLAGS=""
alsamixer=
if test "$have_mixer" = "yes"; then
AC_ARG_ENABLE(alsamixer,
     AS_HELP_STRING([--disable-alsamixer], [Disable alsamixer compilation]),
     [case "${enableval}" in
       yes) alsamixer=true ;;
       no)  alsamixer=false ;;
       *) AC_MSG_ERROR(bad value ${enableval} for --enable-alsamixer) ;;
     esac],[alsamixer=true])
fi
AM_CONDITIONAL(ALSAMIXER, test x$alsamixer = xtrue)

dnl Disable alsaconf
AC_ARG_ENABLE(alsaconf,
     AS_HELP_STRING([--disable-alsaconf], [Disable alsaconf packaging]),
     [case "${enableval}" in
       yes) alsaconf=true ;;
       no)  alsaconf=false ;;
       *) AC_MSG_ERROR(bad value ${enableval} for --enable-alsaconf) ;;
     esac],[alsaconf=true])
AM_CONDITIONAL(ALSACONF, test x$alsaconf = xtrue)

dnl Disable alsaloop
AC_ARG_ENABLE(alsaloop,
     AS_HELP_STRING([--disable-alsaloop], [Disable alsaloop packaging]),
     [case "${enableval}" in
       yes) alsaloop=true ;;
       no)  alsaloop=false ;;
       *) AC_MSG_ERROR(bad value ${enableval} for --enable-alsaloop) ;;
     esac],[alsaloop=true])
AM_CONDITIONAL(ALSALOOP, test x$alsaloop = xtrue)

xmlto=""
AC_ARG_ENABLE(xmlto,
 AS_HELP_STRING([--disable-xmlto], [Disable man page creation via xmlto]),
 xmlto="$enableval", xmlto="yes")
if test "$xmlto" = "yes"; then
  AC_CHECK_PROG([xmlto], [xmlto], [yes])
fi
AM_CONDITIONAL(USE_XMLTO, test x"$xmlto" = xyes)

AC_ARG_WITH(
        [udev-rules-dir],
        AS_HELP_STRING([--with-udev-rules-dir=DIR],[Directory where to install udev rules to (default=auto)]),
        [udevrulesdir="$withval"],
        [udevdir=$($PKG_CONFIG udev --variable=udevdir)
        if test "x$udevdir" = "x"; then
            udevrulesdir="/lib/udev/rules.d"
        else
            udevrulesdir="$udevdir/rules.d"
        fi])
AC_SUBST(udevrulesdir)

dnl Checks for header files.
AC_HEADER_STDC
if test x$alsamixer = xtrue; then
  AC_ARG_WITH(curses,
    AS_HELP_STRING([--with-curses=libname], [Specify the curses library to use (default=auto)]),
    curseslib="$withval",
    curseslib="auto")
  CURSESLIBDIR=""
  NCURSESLIBSUFFIX=""
  CURSES_NLS="no"
  if test "$curseslib" = "ncursesw" -o \( "$curseslib" = "auto" -a "$USE_NLS" = "yes" \); then
    dnl First try out pkg-config, then fall back to old config scripts.
    PKG_CHECK_MODULES([NCURSESW], [ncursesw], [
        CURSESINC="<ncurses.h>"
        CURSESLIB="${NCURSESW_LIBS}"
        CURSESLIBDIR=
        CURSES_CFLAGS="${NCURSESW_CFLAGS}"
        curseslib="ncursesw"
      ], [
        AC_CHECK_PROG([ncursesw5_config], [ncursesw5-config], [yes])
        if test "$ncursesw5_config" = "yes"; then
          CURSESINC="<ncurses.h>"
          CURSESLIB=`ncursesw5-config --libs`
          CURSESLIBDIR=`ncursesw5-config --libdir`
          CURSES_CFLAGS=`ncursesw5-config --cflags`
          curseslib="ncursesw"
        else
          AC_CHECK_LIB(ncursesw, initscr,
                     [ CURSESINC='<ncurses.h>'; CURSESLIB='-lncursesw'; curseslib="ncursesw"])
        fi
      ])
    if test -n "$CURSESINC"; then
      NCURSESLIBSUFFIX="w"
      CURSES_NLS="yes"
    fi
  fi
  if test "$curseslib" = "ncurses" -o "$curseslib" = "auto"; then
    dnl First try out pkg-config, then fall back to old config scripts.
    PKG_CHECK_MODULES([NCURSES], [ncurses], [
        CURSESINC="<ncurses.h>"
        CURSESLIB="${NCURSES_LIBS}"
        CURSESLIBDIR=
        CURSES_CFLAGS="${NCURSES_CFLAGS}"
        curseslib="ncurses"
      ], [
        AC_CHECK_PROG([ncurses5_config], [ncurses5-config], [yes])
        if test "$ncurses5_config" = "yes"; then
          CURSESINC="<ncurses.h>"
          CURSESLIB=`ncurses5-config --libs`
          CURSESLIBDIR=`ncurses5-config --libdir`
          CURSES_CFLAGS=`ncurses5-config --cflags`
          curseslib="ncurses"
        else
          AC_CHECK_LIB(ncurses, initscr,
                     [ CURSESINC='<ncurses.h>'; CURSESLIB='-lncurses'; curseslib="ncurses"])
        fi
      ])
  fi
  if test "$curseslib" = "curses" -o "$curseslib" = "auto"; then
    AC_CHECK_LIB(curses, initscr,
                 [ CURSESINC='<curses.h>'; CURSESLIB='-lcurses'; curseslib="curses"])
  fi
  if test -z "$CURSESINC"; then
     AC_MSG_ERROR(this packages requires a curses library)
  fi

  AC_MSG_CHECKING([for curses library])
  AC_MSG_RESULT([$curseslib])
  AC_MSG_CHECKING([for curses header name])
  AC_MSG_RESULT([$CURSESINC])
  AC_MSG_CHECKING([for curses compiler flags])
  AC_MSG_RESULT([$CURSES_CFLAGS])

  dnl CURSESLIBS might have the library path at the beginning.  If so, we cut it
  dnl off so that we can insert the other curses libraries before the ncurses
  dnl library but after the library path (which is later again prepended below).
  if test -n "$CURSESLIBDIR"; then
    if test "-L$CURSESLIBDIR " = "$(echo $CURSESLIB | cut -c-$((${#CURSESLIBDIR}+3)) )"; then
      CURSESLIB="$(echo $CURSESLIB | cut -c$((${#CURSESLIBDIR}+4))-)"
    fi
  fi

  saved_CFLAGS="$CFLAGS"
  saved_LDFLAGS="$LDFLAGS"
  saved_LIBS="$LIBS"
  CFLAGS="$CFLAGS $CURSES_CFLAGS"
  if test -n "$CURSESLIBDIR"; then
    LDFLAGS="$LDFLAGS -L$CURSESLIBDIR"
  fi
  LIBS="$CURSESLIB $LIBS"

  AC_TRY_LINK([#include <panel.h>], [set_escdelay(100);],[HAVE_CURSES_ESCDELAY="yes"])
  if test "$HAVE_CURSES_ESCDELAY" = "yes"; then
    AC_DEFINE([HAVE_CURSES_ESCDELAY], 1, [Have curses set_escdelay])
  fi

  if test "$USE_NLS" = "yes"; then
    AC_MSG_CHECKING([for curses NLS support])
    dnl In theory, a single-byte curses works just fine in ISO 8859-* locales.
    dnl In practice, however, everybody uses UTF-8 nowadays, so we'd better
    dnl check for wide-character support.
    dnl For ncurses/ncursesw, CURSES_NLS was already set above.
    if test "$curseslib" = "curses"; then
      AC_TRY_LINK([
          #define _XOPEN_SOURCE 1
          #define _XOPEN_SOURCE_EXTENDED 1
          #include <curses.h>
        ], [
          cchar_t wc;
          setcchar(&wc, L"x", A_NORMAL, 0, 0);
        ],
        [CURSES_NLS="yes"])
    fi
    AC_MSG_RESULT([$CURSES_NLS])
    if test "$CURSES_NLS" = "yes"; then
      AC_DEFINE([ENABLE_NLS_IN_CURSES], [1],
                [Define if curses-based programs can show translated messages.])
    fi
  fi

  AC_CHECK_HEADERS([panel.h menu.h form.h], [],
                   [AC_MSG_ERROR([required curses helper header not found])])
  AC_CHECK_LIB([panel$NCURSESLIBSUFFIX], [new_panel],
               [CURSESLIB="-lpanel$NCURSESLIBSUFFIX $CURSESLIB"],
               [AC_MSG_ERROR([panel$NCURSESLIBSUFFIX library not found])])
  AC_CHECK_LIB([menu$NCURSESLIBSUFFIX], [new_menu],
               [CURSESLIB="-lmenu$NCURSESLIBSUFFIX $CURSESLIB"],
               [AC_MSG_ERROR([menu$NCURSESLIBSUFFIX library not found])])
  AC_CHECK_LIB([form$NCURSESLIBSUFFIX], [new_form],
               [CURSESLIB="-lform$NCURSESLIBSUFFIX $CURSESLIB"],
               [AC_MSG_ERROR([form$NCURSESLIBSUFFIX library not found])])

  CFLAGS="$saved_CFLAGS"
  LDFLAGS="$saved_LDFLAGS"
  LIBS="$saved_LIBS"

  if test -n "$CURSESLIBDIR"; then
    CURSESLIB="-L$CURSESLIBDIR $CURSESLIB"
  fi

  AC_MSG_CHECKING([for curses linker flags])
  AC_MSG_RESULT([$CURSESLIB])
fi

AC_SUBST(CURSESINC)
AC_SUBST(CURSESLIB)
AC_SUBST(CURSES_CFLAGS)

test "x$prefix" = xNONE && prefix=$ac_default_prefix

eval dir="$datadir"
case "$dir" in
/*) ;;
*) dir="$prefix/share"
esac

soundsdir="$dir/sounds/alsa"
AC_DEFINE_UNQUOTED(SOUNDSDIR, "$soundsdir", [directory containing sample data])

mydatadir="$dir/alsa"
AC_DEFINE_UNQUOTED(DATADIR, "$mydatadir", [directory containing alsa configuration])
AC_SUBST(mydatadir)

AC_ARG_WITH(testsound,
  AS_HELP_STRING([--with-testsound=file], [give the path of test sound file for alsaconf]),
  TESTSOUND="$withval",
  TESTSOUND="$dir/test.wav")
AC_SUBST(TESTSOUND)

AC_CONFIG_HEADERS(include/aconfig.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_HEADER_TIME

dnl Checks for library functions.
AC_PROG_GCC_TRADITIONAL

dnl Enable largefile support
AC_SYS_LARGEFILE

SAVE_UTIL_VERSION

AC_SUBST(LIBRT)

dnl Check for systemd
AC_ARG_WITH([systemdsystemunitdir],
        AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
        [], [with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)])
if test "x$with_systemdsystemunitdir" != xno; then
        AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
fi
AM_CONDITIONAL(HAVE_SYSTEMD, [test -n "$with_systemdsystemunitdir" -a "x$with_systemdsystemunitdir" != xno ])

AC_ARG_WITH([asound-state-dir],
        AS_HELP_STRING([--with-asound-state-dir=DIR], [Directory to place asound.state file in]),
        [ASOUND_STATE_DIR="$withval"],
        [ASOUND_STATE_DIR="/var/lib/alsa"])
AC_SUBST(ASOUND_STATE_DIR)

AC_ARG_WITH([alsactl-lock-dir],
        AS_HELP_STRING([--with-alsactl-lock-dir=DIR], [Directory to place lock files in]),
        [ASOUND_LOCK_DIR="$withval"],
        [ASOUND_LOCK_DIR="/var/lock"])
AC_SUBST(ASOUND_LOCK_DIR)

AC_ARG_WITH([alsactl-pidfile-dir],
        AS_HELP_STRING([--with-alsactl-pidfile-dir=DIR], [Directory to place alsactl.pid file in]),
        [ALSACTL_PIDFILE_DIR="$withval"],
        [ALSACTL_PIDFILE_DIR="/var/run"])
AC_SUBST(ALSACTL_PIDFILE_DIR)

AC_ARG_WITH([alsactl-daemonswitch],
        AS_HELP_STRING([--with-alsactl-daemonswitch=FILE], [File to test for the daemon mode]),
        [ALSACTL_DAEMONSWITCH="$withval"],
        [ALSACTL_DAEMONSWITCH="/etc/alsa/state-daemon.conf"])
AC_SUBST(ALSACTL_DAEMONSWITCH)

AC_OUTPUT(Makefile alsactl/Makefile alsactl/init/Makefile \
	  alsamixer/Makefile amidi/Makefile amixer/Makefile \
	  m4/Makefile po/Makefile.in \
	  alsaconf/alsaconf alsaconf/Makefile \
	  alsaconf/po/Makefile \
	  alsaucm/Makefile topology/Makefile bat/Makefile \
	  aplay/Makefile include/Makefile iecset/Makefile utils/Makefile \
	  utils/alsa-utils.spec seq/Makefile seq/aconnect/Makefile \
	  seq/aplaymidi/Makefile seq/aseqdump/Makefile seq/aseqnet/Makefile \
	  speaker-test/Makefile speaker-test/samples/Makefile \
	  alsaloop/Makefile alsa-info/Makefile)
