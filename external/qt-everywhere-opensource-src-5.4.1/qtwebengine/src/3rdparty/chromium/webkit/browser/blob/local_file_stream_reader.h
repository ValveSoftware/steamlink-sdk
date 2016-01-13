// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_BLOB_LOCAL_FILE_STREAM_READER_H_
#define WEBKIT_BROWSER_BLOB_LOCAL_FILE_STREAM_READER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/webkit_storage_browser_export.h"

namespace base {
class TaskRunner;
}

namespace content {
class LocalFileStreamReaderTest;
}

namespace net {
class FileStream;
}

namespace webkit_blob {

// A thin wrapper of net::FileStream with range support for sliced file
// handling.
class WEBKIT_STORAGE_BROWSER_EXPORT LocalFileStreamReader
    : public NON_EXPORTED_BASE(FileStreamReader) {
 public:
  virtual ~LocalFileStreamReader();

  // FileStreamReader overrides.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) OVERRIDE;
  virtual int64 GetLength(
      const net::Int64CompletionCallback& callback) OVERRIDE;

 private:
  friend class FileStreamReader;
  friend class content::LocalFileStreamReaderTest;

  LocalFileStreamReader(base::TaskRunner* task_runner,
                        const base::FilePath& file_path,
                        int64 initial_offset,
                        const base::Time& expected_modification_time);
  int Open(const net::CompletionCallback& callback);

  // Callbacks that are chained from Open for Read.
  void DidVerifyForOpen(const net::CompletionCallback& callback,
                        int64 get_length_result);
  void DidOpenFileStream(const net::CompletionCallback& callback,
                         int result);
  void DidSeekFileStream(const net::CompletionCallback& callback,
                         int64 seek_result);
  void DidOpenForRead(net::IOBuffer* buf,
                      int buf_len,
                      const net::CompletionCallback& callback,
                      int open_result);

  void DidGetFileInfoForGetLength(const net::Int64CompletionCallback& callback,
                                  base::File::Error error,
                                  const base::File::Info& file_info);

  scoped_refptr<base::TaskRunner> task_runner_;
  scoped_ptr<net::FileStream> stream_impl_;
  const base::FilePath file_path_;
  const int64 initial_offset_;
  const base::Time expected_modification_time_;
  bool has_pending_open_;
  base::WeakPtrFactory<LocalFileStreamReader> weak_factory_;
};

}  // namespace webkit_blob

#endif  // WEBKIT_BROWSER_BLOB_LOCAL_FILE_STREAM_READER_H_
