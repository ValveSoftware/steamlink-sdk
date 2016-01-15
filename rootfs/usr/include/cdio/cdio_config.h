#ifndef __CDIO_CONFIG_H__
#define __CDIO_CONFIG_H__
/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* Define 1 if you are compiling using cygwin */
/* #undef CYGWIN */

/* what to put between the brackets for empty arrays */
#define EMPTY_ARRAY_SIZE 

/* Define 1 if you have BSDI-type CD-ROM support */
/* #undef HAVE_BSDI_CDROM */

/* Define to 1 if you have the `bzero' function. */
#define HAVE_BZERO 1

/* Define this if you have libcddb installed */
/* #undef HAVE_CDDB */

/* Define to 1 if you have the `chdir' function. */
#define HAVE_CHDIR 1

/* Define to 1 if you have the <CoreFoundation/CFBase.h> header file. */
/* #undef HAVE_COREFOUNDATION_CFBASE_H */

/* Define to 1 if you have the <curses.h> header file. */
#define HAVE_CURSES_H 1

/* Define 1 if you have Darwin OS X-type CD-ROM support */
/* #undef HAVE_DARWIN_CDROM */

/* Define if time.h defines extern long timezone and int daylight vars. */
#define HAVE_DAYLIGHT 1

/* Define to 1 if you have the Apple DiskArbitration framework */
/* #undef HAVE_DISKARBITRATION */

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the `drand48' function. */
#define HAVE_DRAND48 1

/* Define to 1 if you have the <dvd.h> header file. */
/* #undef HAVE_DVD_H */

/* Define to 1 if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define 1 if you have FreeBSD CD-ROM support */
/* #undef HAVE_FREEBSD_CDROM */

/* Define to 1 if fseeko (and presumably ftello) exists and is declared. */
/* #undef HAVE_FSEEKO */

/* Define to 1 if you have the `ftruncate' function. */
#define HAVE_FTRUNCATE 1

/* Define to 1 if you have the `geteuid' function. */
#define HAVE_GETEUID 1

/* Define to 1 if you have the `getgid' function. */
#define HAVE_GETGID 1

/* Define to 1 if you have the <getopt.h> header file. */
#define HAVE_GETOPT_H 1

/* Define to 1 if you have the `getpwuid' function. */
#define HAVE_GETPWUID 1

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the `getuid' function. */
#define HAVE_GETUID 1

/* Define to 1 if you have the <glob.h> header file. */
#define HAVE_GLOB_H 1

/* Define to 1 if you have the `gmtime_r' function. */
#define HAVE_GMTIME_R 1

/* Define if you have the iconv() function and it works. */
#define HAVE_ICONV 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <IOKit/IOKitLib.h> header file. */
/* #undef HAVE_IOKIT_IOKITLIB_H */

/* Supports ISO _Pragma() macro */
#define HAVE_ISOC99_PRAGMA /**/

/* Define 1 if you want ISO-9660 Joliet extension support. You must have also
   libiconv installed to get Joliet extension support. */
#define HAVE_JOLIET 1

/* Define this if your libcurses has keypad */
#define HAVE_KEYPAD /**/

/* Define if you have <langinfo.h> and nl_langinfo(CODESET). */
#define HAVE_LANGINFO_CODESET 1

/* Define to 1 if you have the `nsl' library (-lnsl). */
/* #undef HAVE_LIBNSL */

/* Define to 1 if you have the `socket' library (-lsocket). */
/* #undef HAVE_LIBSOCKET */

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define 1 if you have Linux-type CD-ROM support */
#define HAVE_LINUX_CDROM 1

/* Define to 1 if you have the <linux/cdrom.h> header file. */
#define HAVE_LINUX_CDROM_H 1

/* Define 1 if timeout is in cdrom_generic_command struct */
/* #undef HAVE_LINUX_CDROM_TIMEOUT */

/* Define to 1 if you have the <linux/major.h> header file. */
#define HAVE_LINUX_MAJOR_H 1

/* Define to 1 if you have the <linux/version.h> header file. */
#define HAVE_LINUX_VERSION_H 1

/* Define to 1 if you have the `localtime_r' function. */
#define HAVE_LOCALTIME_R 1

/* Define to 1 if you have the `lstat' function. */
#define HAVE_LSTAT 1

/* Define to 1 if you have the `memcpy' function. */
#define HAVE_MEMCPY 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `memset' function. */
#define HAVE_MEMSET 1

/* Define to 1 if you have the <ncurses.h> header file. */
#define HAVE_NCURSES_H 1

/* Define 1 if you have NetBSD CD-ROM support */
/* #undef HAVE_NETBSD_CDROM */

/* Define 1 if you have OS/2 CD-ROM support */
/* #undef HAVE_OS2_CDROM */

/* Define to 1 if you have the <pwd.h> header file. */
#define HAVE_PWD_H 1

/* Define to 1 if you have the `rand' function. */
#define HAVE_RAND 1

/* Define to 1 if you have the `readlink' function. */
#define HAVE_READLINK 1

/* Define to 1 if you have the `realpath' function. */
#define HAVE_REALPATH 1

/* Define 1 if you want ISO-9660 Rock-Ridge extension support. */
/* #undef HAVE_ROCK */

/* Define to 1 if you have the `setegid' function. */
#define HAVE_SETEGID 1

/* Define to 1 if you have the `setenv' function. */
#define HAVE_SETENV 1

/* Define to 1 if you have the `seteuid' function. */
#define HAVE_SETEUID 1

/* Define to 1 if you have the `sleep' function. */
#define HAVE_SLEEP 1

/* Define to 1 if you have the `snprintf' function. */
#define HAVE_SNPRINTF 1

/* Define 1 if you have Solaris CD-ROM support */
/* #undef HAVE_SOLARIS_CDROM */

/* Define to 1 if you have the <stdarg.h> header file. */
#define HAVE_STDARG_H 1

/* Define to 1 if you have the <stdbool.h> header file. */
#define HAVE_STDBOOL_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define this if you have struct timespec */
#define HAVE_STRUCT_TIMESPEC /**/

/* Define to 1 if you have the <sys/cdio.h> header file. */
/* #undef HAVE_SYS_CDIO_H */

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/timeb.h> header file. */
#define HAVE_SYS_TIMEB_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/utsname.h> header file. */
#define HAVE_SYS_UTSNAME_H 1

/* Define this <sys/stat.h> defines S_ISLNK() */
#define HAVE_S_ISLNK /**/

/* Define this <sys/stat.h> defines S_ISSOCK() */
#define HAVE_S_ISSOCK /**/

/* Define to 1 if timegm is available */
#define HAVE_TIMEGM 1

/* Define if you have an extern long timenzone variable. */
#define HAVE_TIMEZONE_VAR 1

/* Define if struct tm has the tm_gmtoff member. */
#define HAVE_TM_GMTOFF 1

/* Define if time.h defines extern extern char *tzname[2] variable */
#define HAVE_TZNAME 1

/* Define to 1 if you have the `tzset' function. */
#define HAVE_TZSET 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `unsetenv' function. */
#define HAVE_UNSETENV 1

/* Define to 1 if you have the `usleep' function. */
#define HAVE_USLEEP 1

/* Define this if you have libvcdinfo installed */
/* #undef HAVE_VCDINFO */

/* Define to 1 if you have the `vsnprintf' function. */
#define HAVE_VSNPRINTF 1

/* Define 1 if you have MinGW CD-ROM support */
/* #undef HAVE_WIN32_CDROM */

/* Define to 1 if you have the <windows.h> header file. */
/* #undef HAVE_WINDOWS_H */

/* Define as const if the declaration of iconv() needs const. */
#define ICONV_CONST 

/* Is set when libcdio's config.h has been included. Applications wishing to
   sue their own config.h values (such as set by the application's configure
   script can define this before including any of libcdio's headers. */
#define LIBCDIO_CONFIG_H 1

/* Full path to libcdio top_sourcedir. */
#define LIBCDIO_SOURCE_PATH "/build/buildd-libcdio_0.83-4-armhf-Mygvxj/libcdio-0.83"

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Define 1 if you are compiling using MinGW */
/* #undef MINGW32 */

/* Define 1 if you need timezone defined to get timzone defined as a variable.
   In cygwin it is a function too */
/* #undef NEED_TIMEZONEVAR */

/* Define to 1 if your C compiler doesn't accept -c and -o together. */
/* #undef NO_MINUS_C_MINUS_O */

/* Name of package */
#define PACKAGE "libcdio"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "libcdio-help@gnu.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "libcdio"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "libcdio 0.83"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "libcdio"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.83"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Enable extensions on AIX 3, Interix.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable threading extensions on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif


/* Version number of package */
#define VERSION "0.83"

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
#define _FILE_OFFSET_BITS 64

/* Define to 1 to make fseeko visible on some hosts (e.g. glibc 2.2). */
/* #undef _LARGEFILE_SOURCE */

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

/* Define to 1 if on MINIX. */
/* #undef _MINIX */

/* Define to 2 if the system does not provide POSIX.1 features except with
   this defined. */
/* #undef _POSIX_1_SOURCE */

/* Define to 1 if you need to in order for `stat' and other things to work. */
/* #undef _POSIX_SOURCE */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif
#endif /* #ifndef CDIO_CONFIG_H */
