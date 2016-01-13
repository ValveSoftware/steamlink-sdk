// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_ISOLATED_FILE_SYSTEM_BACKEND_H_
#define WEBKIT_BROWSER_FILEAPI_ISOLATED_FILE_SYSTEM_BACKEND_H_

#include "base/memory/scoped_ptr.h"
#include "webkit/browser/fileapi/file_system_backend.h"

namespace fileapi {

class AsyncFileUtilAdapter;

class IsolatedFileSystemBackend : public FileSystemBackend {
 public:
  IsolatedFileSystemBackend();
  virtual ~IsolatedFileSystemBackend();

  // FileSystemBackend implementation.
  virtual bool CanHandleType(FileSystemType type) const OVERRIDE;
  virtual void Initialize(FileSystemContext* context) OVERRIDE;
  virtual void ResolveURL(const FileSystemURL& url,
                          OpenFileSystemMode mode,
                          const OpenFileSystemCallback& callback) OVERRIDE;
  virtual AsyncFileUtil* GetAsyncFileUtil(FileSystemType type) OVERRIDE;
  virtual CopyOrMoveFileValidatorFactory* GetCopyOrMoveFileValidatorFactory(
      FileSystemType type,
      base::File::Error* error_code) OVERRIDE;
  virtual FileSystemOperation* CreateFileSystemOperation(
      const FileSystemURL& url,
      FileSystemContext* context,
      base::File::Error* error_code) const OVERRIDE;
  virtual bool SupportsStreaming(const FileSystemURL& url) const OVERRIDE;
  virtual scoped_ptr<webkit_blob::FileStreamReader> CreateFileStreamReader(
      const FileSystemURL& url,
      int64 offset,
      const base::Time& expected_modification_time,
      FileSystemContext* context) const OVERRIDE;
  virtual scoped_ptr<FileStreamWriter> CreateFileStreamWriter(
      const FileSystemURL& url,
      int64 offset,
      FileSystemContext* context) const OVERRIDE;
  virtual FileSystemQuotaUtil* GetQuotaUtil() OVERRIDE;

 private:
  scoped_ptr<AsyncFileUtilAdapter> isolated_file_util_;
  scoped_ptr<AsyncFileUtilAdapter> dragged_file_util_;
  scoped_ptr<AsyncFileUtilAdapter> transient_file_util_;
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_ISOLATED_FILE_SYSTEM_BACKEND_H_
