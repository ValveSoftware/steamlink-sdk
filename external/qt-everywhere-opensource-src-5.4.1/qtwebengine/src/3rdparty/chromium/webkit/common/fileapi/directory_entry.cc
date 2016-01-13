// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/fileapi/directory_entry.h"

namespace fileapi {

DirectoryEntry::DirectoryEntry() : is_directory(false), size(0) {}

DirectoryEntry::DirectoryEntry(const std::string& name,
                               DirectoryEntryType type,
                               int64 size,
                               const base::Time& last_modified_time)
    : name(base::FilePath::FromUTF8Unsafe(name).value()),
      is_directory(type == DIRECTORY),
      size(size),
      last_modified_time(last_modified_time) {
}

}  // namespace fileapi
