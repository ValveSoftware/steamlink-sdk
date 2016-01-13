// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_LOCAL_FILE_STREAM_WRITER_H_
#define WEBKIT_BROWSER_FILEAPI_LOCAL_FILE_STREAM_WRITER_H_

#include <utility>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task_runner.h"
#include "webkit/browser/fileapi/file_stream_writer.h"
#include "webkit/browser/webkit_storage_browser_export.h"

namespace content {
class LocalFileStreamWriterTest;
}

namespace net {
class FileStream;
}

namespace fileapi {

// This class is a thin wrapper around net::FileStream for writing local files.
class WEBKIT_STORAGE_BROWSER_EXPORT LocalFileStreamWriter
    : public NON_EXPORTED_BASE(FileStreamWriter) {
 public:
  virtual ~LocalFileStreamWriter();

  // FileStreamWriter overrides.
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback) OVERRIDE;
  virtual int Cancel(const net::CompletionCallback& callback) OVERRIDE;
  virtual int Flush(const net::CompletionCallback& callback) OVERRIDE;

 private:
  friend class content::LocalFileStreamWriterTest;
  friend class FileStreamWriter;
  LocalFileStreamWriter(base::TaskRunner* task_runner,
                        const base::FilePath& file_path,
                        int64 initial_offset,
                        OpenOrCreate open_or_create);

  // Opens |file_path_| and if it succeeds, proceeds to InitiateSeek().
  // If failed, the error code is returned by calling |error_callback|.
  int InitiateOpen(const net::CompletionCallback& error_callback,
                   const base::Closure& main_operation);
  void DidOpen(const net::CompletionCallback& error_callback,
               const base::Closure& main_operation,
               int result);

  // Seeks to |initial_offset_| and proceeds to |main_operation| if it succeeds.
  // If failed, the error code is returned by calling |error_callback|.
  void InitiateSeek(const net::CompletionCallback& error_callback,
                    const base::Closure& main_operation);
  void DidSeek(const net::CompletionCallback& error_callback,
               const base::Closure& main_operation,
               int64 result);

  // Passed as the |main_operation| of InitiateOpen() function.
  void ReadyToWrite(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback);

  // Writes asynchronously to the file.
  int InitiateWrite(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback);
  void DidWrite(const net::CompletionCallback& callback, int result);

  // Flushes asynchronously to the file.
  int InitiateFlush(const net::CompletionCallback& callback);
  void DidFlush(const net::CompletionCallback& callback, int result);

  // Stops the in-flight operation and calls |cancel_callback_| if it has been
  // set by Cancel() for the current operation.
  bool CancelIfRequested();

  // Initialization parameters.
  const base::FilePath file_path_;
  OpenOrCreate open_or_create_;
  const int64 initial_offset_;
  scoped_refptr<base::TaskRunner> task_runner_;

  // Current states of the operation.
  bool has_pending_operation_;
  scoped_ptr<net::FileStream> stream_impl_;
  net::CompletionCallback cancel_callback_;

  base::WeakPtrFactory<LocalFileStreamWriter> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(LocalFileStreamWriter);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_LOCAL_FILE_STREAM_WRITER_H_
