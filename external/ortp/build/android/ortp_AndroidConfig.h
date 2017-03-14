/* ortp-config.h.  Generated from ortp-config.h.in by configure.  */
/* ortp-config.h.in.  Generated from configure.ac by autoheader.  */

/* Defined when memory leak checking if enabled */
/* #undef ENABLE_MEMCHECK */

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <linux/soundcard.h> header file. */
#define HAVE_LINUX_SOUNDCARD_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <poll.h> header file. */
#define HAVE_POLL_H 1

/* Define to 1 if you have the `select' function. */
#define HAVE_SELECT 1

/* Define to 1 if you have the `seteuid' function. */
#define HAVE_SETEUID 1

/* Define to 1 if you have the `socket' function. */
#define HAVE_SOCKET 1

/* Defined when srtp support is compiled */
/*#define HAVE_SRTP 1*/

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/audio.h> header file. */
/* #undef HAVE_SYS_AUDIO_H */

/* Define to 1 if you have the <sys/poll.h> header file. */
#define HAVE_SYS_POLL_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Defined if we should not use connect() on udp sockets */
/* #undef NOCONNECT */

/* Default thread stack size (0 = let operating system decide) */
#define ORTP_DEFAULT_THREAD_STACK_SIZE 0

/* major version */
#define ORTP_MAJOR_VERSION 0

/* micro version */
#define ORTP_MICRO_VERSION 0

/* minor version */
#define ORTP_MINOR_VERSION 15

/* ortp version number */
#define ORTP_VERSION "0.15.0"

/* Name of package */
#define PACKAGE "ortp"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "ortp"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "ortp 0.15.0"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "ortp"

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.15.0"

/* Defines the periodicity of the rtp scheduler in microseconds */
#define POSIXTIMER_INTERVAL 10000

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Version number of package */
#define VERSION "0.15.0"

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
/* #undef WORDS_BIGENDIAN */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* /dev/random cannot be guessed at ./configure time in case or
 * cross-compilation */
#define HAVE_DEV_RANDOM 1
