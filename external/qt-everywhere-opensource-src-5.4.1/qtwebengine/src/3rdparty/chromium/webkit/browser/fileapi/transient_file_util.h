// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_TRANSIENT_FILE_UTIL_H_
#define WEBKIT_BROWSER_FILEAPI_TRANSIENT_FILE_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "webkit/browser/fileapi/local_file_util.h"
#include "webkit/browser/webkit_storage_browser_export.h"

namespace fileapi {

class FileSystemOperationContext;

class WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE TransientFileUtil
    : public LocalFileUtil {
 public:
  TransientFileUtil() {}
  virtual ~TransientFileUtil() {}

  // LocalFileUtil overrides.
  virtual webkit_blob::ScopedFile CreateSnapshotFile(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      base::File::Error* error,
      base::File::Info* file_info,
      base::FilePath* platform_path) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TransientFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_TRANSIENT_FILE_UTIL_H_
