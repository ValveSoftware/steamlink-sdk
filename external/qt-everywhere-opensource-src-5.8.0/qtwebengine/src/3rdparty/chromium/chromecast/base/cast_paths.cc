// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/cast_paths.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"

namespace chromecast {

bool PathProvider(int key, base::FilePath* result) {
  switch (key) {
    case DIR_CAST_HOME: {
      base::FilePath home = base::GetHomeDir();
#if defined(ARCH_CPU_ARMEL)
      // When running on the actual device, $HOME is set to the user's
      // directory under the data partition.
      *result = home;
#else
      // When running a development instance as a regular user, use
      // a data directory under $HOME (similar to Chrome).
      *result = home.Append(".config/cast_shell");
#endif
      return true;
    }
#if defined(OS_ANDROID)
    case FILE_CAST_ANDROID_LOG: {
      base::FilePath base_dir;
      CHECK(PathService::Get(base::DIR_ANDROID_APP_DATA, &base_dir));
      *result = base_dir.AppendASCII("cast_shell.log");
      return true;
    }
#endif  // defined(OS_ANDROID)
    case FILE_CAST_CONFIG: {
      base::FilePath data_dir;
#if defined(OS_ANDROID)
      CHECK(PathService::Get(base::DIR_ANDROID_APP_DATA, &data_dir));
      *result = data_dir.Append("cast_shell.conf");
#else
      CHECK(PathService::Get(DIR_CAST_HOME, &data_dir));
      *result = data_dir.Append(".eureka.conf");
#endif  // defined(OS_ANDROID)
      return true;
    }
    case FILE_CAST_PAK: {
      base::FilePath base_dir;
#if defined(OS_ANDROID)
      CHECK(PathService::Get(base::DIR_ANDROID_APP_DATA, &base_dir));
      *result = base_dir.Append("paks/cast_shell.pak");
#else
      CHECK(PathService::Get(base::DIR_MODULE, &base_dir));
      *result = base_dir.Append("assets/cast_shell.pak");
#endif  // defined(OS_ANDROID)
      return true;
    }
  }
  return false;
}

void RegisterPathProvider() {
  PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace chromecast
