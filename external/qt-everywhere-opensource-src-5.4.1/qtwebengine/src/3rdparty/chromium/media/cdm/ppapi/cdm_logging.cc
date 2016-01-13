// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Only compile this file in debug build. This gives us one more level of
// protection that if the linker tries to link in strings/symbols appended to
// "DLOG() <<" in release build (which it shouldn't), we'll get "undefined
// reference" errors.
#if !defined(NDEBUG)

#include "media/cdm/ppapi/cdm_logging.h"

#include "base/basictypes.h"

#if defined(OS_WIN)
#include <io.h>
#include <windows.h>
#elif defined(OS_MACOSX)
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach-o/dyld.h>
#elif defined(OS_POSIX)
#include <sys/syscall.h>
#include <time.h>
#endif

#if defined(OS_POSIX)
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#endif

#include <iomanip>
#include <string>

namespace media {

namespace {

// Helper functions to wrap platform differences.

int32 CurrentProcessId() {
#if defined(OS_WIN)
  return GetCurrentProcessId();
#elif defined(OS_POSIX)
  return getpid();
#endif
}

int32 CurrentThreadId() {
  // Pthreads doesn't have the concept of a thread ID, so we have to reach down
  // into the kernel.
#if defined(OS_LINUX)
  return syscall(__NR_gettid);
#elif defined(OS_ANDROID)
  return gettid();
#elif defined(OS_SOLARIS)
  return pthread_self();
#elif defined(OS_POSIX)
  return reinterpret_cast<int64>(pthread_self());
#elif defined(OS_WIN)
  return static_cast<int32>(::GetCurrentThreadId());
#endif
}

uint64 TickCount() {
#if defined(OS_WIN)
  return GetTickCount();
#elif defined(OS_MACOSX)
  return mach_absolute_time();
#elif defined(OS_POSIX)
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  uint64 absolute_micro =
    static_cast<int64>(ts.tv_sec) * 1000000 +
    static_cast<int64>(ts.tv_nsec) / 1000;

  return absolute_micro;
#endif
}

}  // namespace

CdmLogMessage::CdmLogMessage(const char* file, int line) {
  std::string filename(file);
  size_t last_slash_pos = filename.find_last_of("\\/");
  if (last_slash_pos != std::string::npos)
    filename = filename.substr(last_slash_pos + 1);

  stream_ << '[';

  // Process and thread ID.
  stream_ << CurrentProcessId() << ':';
  stream_ << CurrentThreadId() << ':';

  // Time and tick count.
  time_t t = time(NULL);
  struct tm local_time = {0};
#if _MSC_VER >= 1400
  localtime_s(&local_time, &t);
#else
  localtime_r(&t, &local_time);
#endif
  struct tm* tm_time = &local_time;
  stream_ << std::setfill('0')
          << std::setw(2) << 1 + tm_time->tm_mon
          << std::setw(2) << tm_time->tm_mday
          << '/'
          << std::setw(2) << tm_time->tm_hour
          << std::setw(2) << tm_time->tm_min
          << std::setw(2) << tm_time->tm_sec
          << ':';
  stream_ << TickCount() << ':';

  // File name.
  stream_ << filename << "(" << line << ")] ";
}

CdmLogMessage::~CdmLogMessage() {
  // Use std::cout explicitly for the line break. This limits the use of this
  // class only to the definition of DLOG() (which also uses std::cout).
  //
  // This appends "std::endl" after all other messages appended to DLOG(),
  // which relies on the C++ standard ISO/IEC 14882:1998(E) $12.2.3:
  // "Temporary objects are destroyed as the last step in evaluating the
  // full-expression (1.9) that (lexically) contains the point where they were
  // created."
  std::cout << std::endl;
}

}  // namespace media

#endif  // !defined(NDEBUG)
