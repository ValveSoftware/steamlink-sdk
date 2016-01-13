// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icu_util.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <string>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "third_party/icu/source/common/unicode/putil.h"
#include "third_party/icu/source/common/unicode/udata.h"

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#endif

#define ICU_UTIL_DATA_FILE   0
#define ICU_UTIL_DATA_SHARED 1
#define ICU_UTIL_DATA_STATIC 2

#if ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE
// Use an unversioned file name to simplify a icu version update down the road.
// No need to change the filename in multiple places (gyp files, windows
// build pkg configurations, etc). 'l' stands for Little Endian.
#define ICU_UTIL_DATA_FILE_NAME "icudtl.dat"
#elif ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_SHARED
#define ICU_UTIL_DATA_SYMBOL "icudt" U_ICU_VERSION_SHORT "_dat"
#if defined(OS_WIN)
#define ICU_UTIL_DATA_SHARED_MODULE_NAME "icudt.dll"
#endif
#endif

namespace base {
namespace i18n {

namespace {

#if !defined(NDEBUG)
// Assert that we are not called more than once.  Even though calling this
// function isn't harmful (ICU can handle it), being called twice probably
// indicates a programming error.
bool g_called_once = false;
bool g_check_called_once = true;
#endif
}


#if defined(OS_ANDROID)
bool InitializeICUWithFileDescriptor(int data_fd) {
#if !defined(NDEBUG)
  DCHECK(!g_check_called_once || !g_called_once);
  g_called_once = true;
#endif

#if (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_STATIC)
  // The ICU data is statically linked.
  return true;
#elif (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)
  CR_DEFINE_STATIC_LOCAL(base::MemoryMappedFile, mapped_file, ());
  if (!mapped_file.IsValid()) {
    if (!mapped_file.Initialize(base::File(data_fd))) {
      LOG(ERROR) << "Couldn't mmap icu data file";
      return false;
    }
  }
  UErrorCode err = U_ZERO_ERROR;
  udata_setCommonData(const_cast<uint8*>(mapped_file.data()), &err);
  return err == U_ZERO_ERROR;
#endif // ICU_UTIL_DATA_FILE
}
#endif


bool InitializeICU() {
#if !defined(NDEBUG)
  DCHECK(!g_check_called_once || !g_called_once);
  g_called_once = true;
#endif

#if (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_SHARED)
  // We expect to find the ICU data module alongside the current module.
  FilePath data_path;
  PathService::Get(base::DIR_MODULE, &data_path);
  data_path = data_path.AppendASCII(ICU_UTIL_DATA_SHARED_MODULE_NAME);

  HMODULE module = LoadLibrary(data_path.value().c_str());
  if (!module) {
    LOG(ERROR) << "Failed to load " << ICU_UTIL_DATA_SHARED_MODULE_NAME;
    return false;
  }

  FARPROC addr = GetProcAddress(module, ICU_UTIL_DATA_SYMBOL);
  if (!addr) {
    LOG(ERROR) << ICU_UTIL_DATA_SYMBOL << ": not found in "
               << ICU_UTIL_DATA_SHARED_MODULE_NAME;
    return false;
  }

  UErrorCode err = U_ZERO_ERROR;
  udata_setCommonData(reinterpret_cast<void*>(addr), &err);
  return err == U_ZERO_ERROR;
#elif (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_STATIC)
  // The ICU data is statically linked.
  return true;
#elif (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)
  // If the ICU data directory is set, ICU won't actually load the data until
  // it is needed.  This can fail if the process is sandboxed at that time.
  // Instead, we map the file in and hand off the data so the sandbox won't
  // cause any problems.

  // Chrome doesn't normally shut down ICU, so the mapped data shouldn't ever
  // be released.
  CR_DEFINE_STATIC_LOCAL(base::MemoryMappedFile, mapped_file, ());
  if (!mapped_file.IsValid()) {
#if defined(TOOLKIT_QT)
    FilePath data_path;
    bool path_ok = PathService::Get(base::DIR_QT_LIBRARY_DATA, &data_path);
    DCHECK(path_ok);
    data_path = data_path.AppendASCII(ICU_UTIL_DATA_FILE_NAME);
#elif !defined(OS_MACOSX)
    FilePath data_path;
#if defined(OS_WIN)
    // The data file will be in the same directory as the current module.
    bool path_ok = PathService::Get(base::DIR_MODULE, &data_path);
#elif defined(OS_ANDROID)
    bool path_ok = PathService::Get(base::DIR_ANDROID_APP_DATA, &data_path);
#else
    // For now, expect the data file to be alongside the executable.
    // This is sufficient while we work on unit tests, but will eventually
    // likely live in a data directory.
    bool path_ok = PathService::Get(base::DIR_EXE, &data_path);
#endif
    DCHECK(path_ok);
    data_path = data_path.AppendASCII(ICU_UTIL_DATA_FILE_NAME);
#else
    // Assume it is in the framework bundle's Resources directory.
    FilePath data_path =
      base::mac::PathForFrameworkBundleResource(CFSTR(ICU_UTIL_DATA_FILE_NAME));
    if (data_path.empty()) {
      LOG(ERROR) << ICU_UTIL_DATA_FILE_NAME << " not found in bundle";
      return false;
    }
#endif  // OS check
    if (!mapped_file.Initialize(data_path)) {
      LOG(ERROR) << "Couldn't mmap " << data_path.AsUTF8Unsafe();
      return false;
    }
  }
  UErrorCode err = U_ZERO_ERROR;
  udata_setCommonData(const_cast<uint8*>(mapped_file.data()), &err);
  return err == U_ZERO_ERROR;
#endif
}

void AllowMultipleInitializeCallsForTesting() {
#if !defined(NDEBUG)
  g_check_called_once = false;
#endif
}

}  // namespace i18n
}  // namespace base
