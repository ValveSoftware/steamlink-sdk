// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_test_util.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "net/disk_cache/simple/simple_util.h"

namespace disk_cache {
namespace simple_util {

bool CreateCorruptFileForTests(const std::string& key,
                               const base::FilePath& cache_path) {
  base::FilePath entry_file_path = cache_path.AppendASCII(
      disk_cache::simple_util::GetFilenameFromKeyAndFileIndex(key, 0));
  int flags = base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE;
  base::File entry_file(entry_file_path, flags);

  if (!entry_file.IsValid())
    return false;

  return entry_file.Write(0, "dummy", 1) == 1;
}

}  // namespace simple_util
}  // namespace disk_cache
