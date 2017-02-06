// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/common/fileapi/directory_entry.h"

namespace storage {

DirectoryEntry::DirectoryEntry() : is_directory(false) {}

DirectoryEntry::DirectoryEntry(const std::string& name, DirectoryEntryType type)
    : name(base::FilePath::FromUTF8Unsafe(name).value()),
      is_directory(type == DIRECTORY) {}

}  // namespace storage
