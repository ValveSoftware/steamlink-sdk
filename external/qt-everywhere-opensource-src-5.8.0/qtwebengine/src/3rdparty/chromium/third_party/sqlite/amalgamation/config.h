/* On Windows and OSX, SQLite uses preprocessor macros to configure itself.  On
 * Linux, it expects config.h from autoconf.  autoconf generates config.h by
 * compiling a series of probe programs, and Chromium's build system has no
 * "configure" phase to put such generation in.  This file is a workaround for
 * this issue.
 */
/* TODO(shess): Expand this to OSX and Windows? */
/* TODO(shess): Consider config_linux.h, config_mac.h, config_win.h? */

/* NOTE(shess): This file is included by sqlite3.c, be very careful about adding
 * #include lines.
 */
/* TODO(shess): Consider using build/build_config.h for OS_ macros. */
/* TODO(shess): build_config.h uses unistd.h, perhaps for portability reasons,
 * but AFAICT there are no current portability concerns here.  limits.h is
 * another alternative.
 */
#include <features.h>

/* SQLite wants to track malloc sizes.  On OSX it uses malloc_size(), on
 * Windows _msize(), elsewhere it handles it manually by enlarging the malloc
 * and injecting a field.  Enable malloc_usable_size() for Linux.
 *
 * malloc_usable_size() is not exported by the Android NDK.  It is not
 * implemented by uclibc.
 */
#if defined(__linux__) && !defined(__UCLIBC__)
#define HAVE_MALLOC_H 1
#define HAVE_MALLOC_USABLE_SIZE 1
#endif

/* TODO(shess): Eat other config options from gn and gyp? */
