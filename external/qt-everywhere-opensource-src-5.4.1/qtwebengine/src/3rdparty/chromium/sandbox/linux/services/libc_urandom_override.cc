// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/libc_urandom_override.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/rand_util.h"

// Note: this file is used by the zygote and nacl_helper.

#if !defined(HAVE_XSTAT) && defined(LIBC_GLIBC)
#define HAVE_XSTAT 1
#endif

namespace sandbox {

static bool g_override_urandom = false;

// TODO(sergeyu): Currently InitLibcUrandomOverrides() doesn't work properly
// under ASan or MSan - it crashes content_unittests. Make sure it works
// properly and enable it here. http://crbug.com/123263
#if !(defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER))
static void InitLibcFileIOFunctions();
static pthread_once_t g_libc_file_io_funcs_guard = PTHREAD_ONCE_INIT;
#endif

void InitLibcUrandomOverrides() {
  // Make sure /dev/urandom is open.
  base::GetUrandomFD();
  g_override_urandom = true;

#if !(defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER))
  CHECK_EQ(0, pthread_once(&g_libc_file_io_funcs_guard,
                           InitLibcFileIOFunctions));
#endif
}

#if !(defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER))

static const char kUrandomDevPath[] = "/dev/urandom";

typedef FILE* (*FopenFunction)(const char* path, const char* mode);

static FopenFunction g_libc_fopen = NULL;
static FopenFunction g_libc_fopen64 = NULL;

#if HAVE_XSTAT
typedef int (*XstatFunction)(int version, const char *path, struct stat *buf);
typedef int (*Xstat64Function)(int version, const char *path,
                               struct stat64 *buf);

static XstatFunction g_libc_xstat = NULL;
static Xstat64Function g_libc_xstat64 = NULL;
#else
typedef int (*StatFunction)(const char *path, struct stat *buf);
typedef int (*Stat64Function)(const char *path, struct stat64 *buf);

static StatFunction g_libc_stat = NULL;
static Stat64Function g_libc_stat64 = NULL;
#endif  // HAVE_XSTAT

// Find the libc's real fopen* and *stat* functions. This should only be
// called once, and should be guarded by g_libc_file_io_funcs_guard.
static void InitLibcFileIOFunctions() {
  g_libc_fopen = reinterpret_cast<FopenFunction>(
      dlsym(RTLD_NEXT, "fopen"));
  g_libc_fopen64 = reinterpret_cast<FopenFunction>(
      dlsym(RTLD_NEXT, "fopen64"));

  if (!g_libc_fopen) {
    LOG(FATAL) << "Failed to get fopen() from libc.";
  } else if (!g_libc_fopen64) {
#if !defined(OS_OPENBSD) && !defined(OS_FREEBSD)
    LOG(WARNING) << "Failed to get fopen64() from libc. Using fopen() instead.";
#endif  // !defined(OS_OPENBSD) && !defined(OS_FREEBSD)
    g_libc_fopen64 = g_libc_fopen;
  }

#if HAVE_XSTAT
  g_libc_xstat = reinterpret_cast<XstatFunction>(
      dlsym(RTLD_NEXT, "__xstat"));
  g_libc_xstat64 = reinterpret_cast<Xstat64Function>(
      dlsym(RTLD_NEXT, "__xstat64"));

  if (!g_libc_xstat) {
    LOG(FATAL) << "Failed to get __xstat() from libc.";
  }
  if (!g_libc_xstat64) {
    LOG(FATAL) << "Failed to get __xstat64() from libc.";
  }
#else
  g_libc_stat = reinterpret_cast<StatFunction>(
      dlsym(RTLD_NEXT, "stat"));
  g_libc_stat64 = reinterpret_cast<Stat64Function>(
      dlsym(RTLD_NEXT, "stat64"));

  if (!g_libc_stat) {
    LOG(FATAL) << "Failed to get stat() from libc.";
  }
  if (!g_libc_stat64) {
    LOG(FATAL) << "Failed to get stat64() from libc.";
  }
#endif  // HAVE_XSTAT
}

// fopen() and fopen64() are intercepted here so that NSS can open
// /dev/urandom to seed its random number generator. NSS is used by
// remoting in the sendbox.

// fopen() call may be redirected to fopen64() in stdio.h using
// __REDIRECT(), which sets asm name for fopen() to "fopen64". This
// means that we cannot override fopen() directly here. Instead the
// the code below defines fopen_override() function with asm name
// "fopen", so that all references to fopen() will resolve to this
// function.

__attribute__ ((__visibility__("default")))
FILE* fopen_override(const char* path, const char* mode) {
  if (g_override_urandom && strcmp(path, kUrandomDevPath) == 0) {
    int fd = HANDLE_EINTR(dup(base::GetUrandomFD()));
    if (fd < 0) {
      PLOG(ERROR) << "dup() failed.";
      return NULL;
    }
    return fdopen(fd, mode);
  } else {
    CHECK_EQ(0, pthread_once(&g_libc_file_io_funcs_guard,
                             InitLibcFileIOFunctions));
    return g_libc_fopen(path, mode);
  }
}

__attribute__ ((__visibility__("default")))
FILE* fopen64_override(const char* path, const char* mode) {
  if (g_override_urandom && strcmp(path, kUrandomDevPath) == 0) {
    int fd = HANDLE_EINTR(dup(base::GetUrandomFD()));
    if (fd < 0) {
      PLOG(ERROR) << "dup() failed.";
      return NULL;
    }
    return fdopen(fd, mode);
  } else {
    CHECK_EQ(0, pthread_once(&g_libc_file_io_funcs_guard,
                             InitLibcFileIOFunctions));
    return g_libc_fopen64(path, mode);
  }
}

// The stat() family of functions are subject to the same problem as
// fopen(), so we have to use the same trick to override them.

#if HAVE_XSTAT

__attribute__ ((__visibility__("default")))
int xstat_override(int version, const char *path, struct stat *buf) {
  if (g_override_urandom && strcmp(path, kUrandomDevPath) == 0) {
    int result = __fxstat(version, base::GetUrandomFD(), buf);
    return result;
  } else {
    CHECK_EQ(0, pthread_once(&g_libc_file_io_funcs_guard,
                             InitLibcFileIOFunctions));
    return g_libc_xstat(version, path, buf);
  }
}

__attribute__ ((__visibility__("default")))
int xstat64_override(int version, const char *path, struct stat64 *buf) {
  if (g_override_urandom && strcmp(path, kUrandomDevPath) == 0) {
    int result = __fxstat64(version, base::GetUrandomFD(), buf);
    return result;
  } else {
    CHECK_EQ(0, pthread_once(&g_libc_file_io_funcs_guard,
                             InitLibcFileIOFunctions));
    return g_libc_xstat64(version, path, buf);
  }
}

#else

__attribute__ ((__visibility__("default")))
int stat_override(const char *path, struct stat *buf) {
  if (g_override_urandom && strcmp(path, kUrandomDevPath) == 0) {
    int result = fstat(base::GetUrandomFD(), buf);
    return result;
  } else {
    CHECK_EQ(0, pthread_once(&g_libc_file_io_funcs_guard,
                             InitLibcFileIOFunctions));
    return g_libc_stat(path, buf);
  }
}

__attribute__ ((__visibility__("default")))
int stat64_override(const char *path, struct stat64 *buf) {
  if (g_override_urandom && strcmp(path, kUrandomDevPath) == 0) {
    int result = fstat64(base::GetUrandomFD(), buf);
    return result;
  } else {
    CHECK_EQ(0, pthread_once(&g_libc_file_io_funcs_guard,
                             InitLibcFileIOFunctions));
    return g_libc_stat64(path, buf);
  }
}

#endif  // HAVE_XSTAT

#endif  // !(defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER))

}  // namespace content
