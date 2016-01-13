// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/fileapi/file_system_info.h"

namespace fileapi {

FileSystemInfo::FileSystemInfo()
    : mount_type(fileapi::kFileSystemTypeTemporary) {
}

FileSystemInfo::FileSystemInfo(const std::string& name,
                               const GURL& root_url,
                               fileapi::FileSystemType mount_type)
    : name(name),
      root_url(root_url),
      mount_type(mount_type) {
}

FileSystemInfo::~FileSystemInfo() {
}

}  // namespace fileapi
