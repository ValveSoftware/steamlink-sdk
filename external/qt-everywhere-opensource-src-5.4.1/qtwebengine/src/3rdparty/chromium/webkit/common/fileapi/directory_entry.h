// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMMON_FILEAPI_DIRECTORY_ENTRY_H_
#define WEBKIT_COMMON_FILEAPI_DIRECTORY_ENTRY_H_

#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "webkit/common/webkit_storage_common_export.h"

namespace fileapi {

// Holds metadata for file or directory entry.
struct WEBKIT_STORAGE_COMMON_EXPORT DirectoryEntry {
  enum DirectoryEntryType {
    FILE,
    DIRECTORY,
  };

  DirectoryEntry();
  DirectoryEntry(const std::string& name,
                 DirectoryEntryType type,
                 int64 size,
                 const base::Time& last_modified_time);

  base::FilePath::StringType name;
  bool is_directory;
  int64 size;
  base::Time last_modified_time;
};

}  // namespace fileapi

#endif  // WEBKIT_COMMON_FILEAPI_DIRECTORY_ENTRY_H_
