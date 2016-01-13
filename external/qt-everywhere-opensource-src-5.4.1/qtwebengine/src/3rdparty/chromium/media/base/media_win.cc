// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media.h"

#include <windows.h>
#if defined(_WIN32_WINNT_WIN8)
// The Windows 8 SDK defines FACILITY_VISUALCPP in winerror.h.
#undef FACILITY_VISUALCPP
#endif
#include <delayimp.h>

#include "base/files/file_path.h"

#pragma comment(lib, "delayimp.lib")

namespace media {
namespace internal {

bool InitializeMediaLibraryInternal(const base::FilePath& module_dir) {
  // LoadLibraryEx(..., LOAD_WITH_ALTERED_SEARCH_PATH) cannot handle
  // relative path.
  if (!module_dir.IsAbsolute())
    return false;

  // Use alternate DLL search path so we don't load dependencies from the
  // system path.  Refer to http://crbug.com/35857
  static const char kFFmpegDLL[] = "ffmpegsumo.dll";
  HMODULE lib = ::LoadLibraryEx(
      module_dir.AppendASCII(kFFmpegDLL).value().c_str(), NULL,
      LOAD_WITH_ALTERED_SEARCH_PATH);

  // Check that we loaded the library successfully.
  return lib != NULL;
}

}  // namespace internal
}  // namespace media
