// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/stability_debugging_win.h"

#include <string>

#include "base/metrics/persistent_memory_allocator.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"

namespace browser_watcher {

namespace {

bool GetCreationTime(const base::Process& process, base::Time* time) {
  DCHECK(time);

  FILETIME creation_time = {};
  FILETIME ignore1 = {};
  FILETIME ignore2 = {};
  FILETIME ignore3 = {};
  if (!::GetProcessTimes(process.Handle(), &creation_time, &ignore1, &ignore2,
                         &ignore3)) {
    return false;
  }
  *time = base::Time::FromFileTime(creation_time);
  return true;
}

}  // namespace

base::FilePath GetStabilityDir(const base::FilePath& user_data_dir) {
  return user_data_dir.AppendASCII("Stability");
}

bool GetStabilityFileForProcess(const base::Process& process,
                                const base::FilePath& user_data_dir,
                                base::FilePath* file_path) {
  DCHECK(file_path);
  base::FilePath stability_dir = GetStabilityDir(user_data_dir);

  // Build the name using the pid and creation time. On windows, this is unique
  // even after the process exits.
  base::Time creation_time;
  if (!GetCreationTime(process, &creation_time))
    return false;

  std::string file_name =
      base::StringPrintf("%u-%llu", process.Pid(), creation_time.ToJavaTime());
  *file_path = stability_dir.AppendASCII(file_name).AddExtension(
      base::PersistentMemoryAllocator::kFileExtension);
  return true;
}

base::FilePath::StringType GetStabilityFilePattern() {
  return base::FilePath::StringType(FILE_PATH_LITERAL("*-*")) +
         base::PersistentMemoryAllocator::kFileExtension;
}

}  // namespace browser_watcher
