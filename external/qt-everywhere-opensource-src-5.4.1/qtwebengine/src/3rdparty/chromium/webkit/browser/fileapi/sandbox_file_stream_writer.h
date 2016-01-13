// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_SANDBOX_FILE_STREAM_WRITER_H_
#define WEBKIT_BROWSER_FILEAPI_SANDBOX_FILE_STREAM_WRITER_H_

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_stream_writer.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/task_runner_bound_observer_list.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/blob/shareable_file_reference.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/quota/quota_types.h"

namespace fileapi {

class FileSystemContext;
class FileSystemQuotaUtil;
class FileStreamWriter;

class WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE SandboxFileStreamWriter
    : public NON_EXPORTED_BASE(FileStreamWriter) {
 public:
  SandboxFileStreamWriter(FileSystemContext* file_system_context,
                          const FileSystemURL& url,
                          int64 initial_offset,
                          const UpdateObserverList& observers);
  virtual ~SandboxFileStreamWriter();

  // FileStreamWriter overrides.
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback) OVERRIDE;
  virtual int Cancel(const net::CompletionCallback& callback) OVERRIDE;
  virtual int Flush(const net::CompletionCallback& callback) OVERRIDE;

  // Used only by tests.
  void set_default_quota(int64 quota) {
    default_quota_ = quota;
  }

 private:
  // Performs quota calculation and calls local_file_writer_->Write().
  int WriteInternal(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback);

  // Callbacks that are chained for the first write.  This eventually calls
  // WriteInternal.
  void DidCreateSnapshotFile(
      const net::CompletionCallback& callback,
      base::File::Error file_error,
      const base::File::Info& file_info,
      const base::FilePath& platform_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref);
  void DidGetUsageAndQuota(const net::CompletionCallback& callback,
                           quota::QuotaStatusCode status,
                           int64 usage, int64 quota);
  void DidInitializeForWrite(net::IOBuffer* buf, int buf_len,
                             const net::CompletionCallback& callback,
                             int init_status);

  void DidWrite(const net::CompletionCallback& callback, int write_response);

  // Stops the in-flight operation, calls |cancel_callback_| and returns true
  // if there's a pending cancel request.
  bool CancelIfRequested();

  scoped_refptr<FileSystemContext> file_system_context_;
  FileSystemURL url_;
  int64 initial_offset_;
  scoped_ptr<FileStreamWriter> local_file_writer_;
  net::CompletionCallback cancel_callback_;

  UpdateObserverList observers_;

  base::FilePath file_path_;
  int64 file_size_;
  int64 total_bytes_written_;
  int64 allowed_bytes_to_write_;
  bool has_pending_operation_;

  int64 default_quota_;

  base::WeakPtrFactory<SandboxFileStreamWriter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SandboxFileStreamWriter);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_SANDBOX_FILE_STREAM_WRITER_H_
