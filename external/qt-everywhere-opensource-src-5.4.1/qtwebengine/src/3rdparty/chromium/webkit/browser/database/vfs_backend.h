// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_DATABASE_VFS_BACKEND_H_
#define WEBKIT_BROWSER_DATABASE_VFS_BACKEND_H_

#include "base/files/file.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "webkit/browser/webkit_storage_browser_export.h"

namespace base {
class FilePath;
}

namespace webkit_database {

class WEBKIT_STORAGE_BROWSER_EXPORT VfsBackend {
 public:
   static base::File OpenFile(const base::FilePath& file_path,
                              int desired_flags);

  static base::File OpenTempFileInDirectory(const base::FilePath& dir_path,
                                            int desired_flags);

  static int DeleteFile(const base::FilePath& file_path, bool sync_dir);

  static uint32 GetFileAttributes(const base::FilePath& file_path);

  static int64 GetFileSize(const base::FilePath& file_path);

  // Used to make decisions in the DatabaseDispatcherHost.
  static bool OpenTypeIsReadWrite(int desired_flags);

 private:
  static bool OpenFileFlagsAreConsistent(int desired_flags);
};

} // namespace webkit_database

#endif  // WEBKIT_BROWSER_DATABASE_VFS_BACKEND_H_
