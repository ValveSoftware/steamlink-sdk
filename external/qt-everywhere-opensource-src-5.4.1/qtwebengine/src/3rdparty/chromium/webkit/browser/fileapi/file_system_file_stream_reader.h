// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_FILE_STREAM_READER_H_
#define WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_FILE_STREAM_READER_H_

#include "base/bind.h"
#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/blob/shareable_file_reference.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace content {
class FileSystemFileStreamReaderTest;
}

namespace fileapi {

class FileSystemContext;

// Generic FileStreamReader implementation for FileSystem files.
// Note: This generic implementation would work for any filesystems but
// remote filesystem should implement its own reader rather than relying
// on FileSystemOperation::GetSnapshotFile() which may force downloading
// the entire contents for remote files.
class WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE FileSystemFileStreamReader
    : public NON_EXPORTED_BASE(webkit_blob::FileStreamReader) {
 public:
  virtual ~FileSystemFileStreamReader();

  // FileStreamReader overrides.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) OVERRIDE;
  virtual int64 GetLength(
      const net::Int64CompletionCallback& callback) OVERRIDE;

 private:
  friend class webkit_blob::FileStreamReader;
  friend class content::FileSystemFileStreamReaderTest;

  FileSystemFileStreamReader(FileSystemContext* file_system_context,
                             const FileSystemURL& url,
                             int64 initial_offset,
                             const base::Time& expected_modification_time);

  int CreateSnapshot(const base::Closure& callback,
                     const net::CompletionCallback& error_callback);
  void DidCreateSnapshot(
      const base::Closure& callback,
      const net::CompletionCallback& error_callback,
      base::File::Error file_error,
      const base::File::Info& file_info,
      const base::FilePath& platform_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref);

  scoped_refptr<FileSystemContext> file_system_context_;
  FileSystemURL url_;
  const int64 initial_offset_;
  const base::Time expected_modification_time_;
  scoped_ptr<webkit_blob::FileStreamReader> local_file_reader_;
  scoped_refptr<webkit_blob::ShareableFileReference> snapshot_ref_;
  bool has_pending_create_snapshot_;
  base::WeakPtrFactory<FileSystemFileStreamReader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemFileStreamReader);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_FILE_STREAM_READER_H_
