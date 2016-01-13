// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FILE_DESCRIPTOR_INFO_H_
#define CONTENT_PUBLIC_BROWSER_FILE_DESCRIPTOR_INFO_H_

#include "base/file_descriptor_posix.h"

namespace content {

// This struct is used when passing files that should be mapped in a process
// that is been created and allows to associate that file with a specific ID.
// It also provides a way to know if the actual file descriptor should be
// closed with the FileDescriptor.auto_close field.

struct FileDescriptorInfo {
  FileDescriptorInfo(int id, const base::FileDescriptor& file_descriptor)
      : id(id),
        fd(file_descriptor) {
  }

  int id;
  base::FileDescriptor fd;
};

}

#endif  // CONTENT_PUBLIC_BROWSER_FILE_DESCRIPTOR_INFO_H_
