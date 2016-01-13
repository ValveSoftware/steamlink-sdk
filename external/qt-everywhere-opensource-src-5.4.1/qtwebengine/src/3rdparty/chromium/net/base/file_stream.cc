// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream.h"

#include "net/base/file_stream_context.h"
#include "net/base/net_errors.h"

namespace net {

FileStream::FileStream(const scoped_refptr<base::TaskRunner>& task_runner)
    : context_(new Context(task_runner)) {
}

FileStream::FileStream(base::File file,
                       const scoped_refptr<base::TaskRunner>& task_runner)
    : context_(new Context(file.Pass(), task_runner)) {
}

FileStream::~FileStream() {
  context_.release()->Orphan();
}

int FileStream::Open(const base::FilePath& path, int open_flags,
                     const CompletionCallback& callback) {
  if (IsOpen()) {
    DLOG(FATAL) << "File is already open!";
    return ERR_UNEXPECTED;
  }

  DCHECK(open_flags & base::File::FLAG_ASYNC);
  context_->OpenAsync(path, open_flags, callback);
  return ERR_IO_PENDING;
}

int FileStream::Close(const CompletionCallback& callback) {
  context_->CloseAsync(callback);
  return ERR_IO_PENDING;
}

bool FileStream::IsOpen() const {
  return context_->file().IsValid();
}

int FileStream::Seek(Whence whence,
                     int64 offset,
                     const Int64CompletionCallback& callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  context_->SeekAsync(whence, offset, callback);
  return ERR_IO_PENDING;
}

int FileStream::Read(IOBuffer* buf,
                     int buf_len,
                     const CompletionCallback& callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  // read(..., 0) will return 0, which indicates end-of-file.
  DCHECK_GT(buf_len, 0);

  return context_->ReadAsync(buf, buf_len, callback);
}

int FileStream::Write(IOBuffer* buf,
                      int buf_len,
                      const CompletionCallback& callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  // write(..., 0) will return 0, which indicates end-of-file.
  DCHECK_GT(buf_len, 0);

  return context_->WriteAsync(buf, buf_len, callback);
}

int FileStream::Flush(const CompletionCallback& callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  context_->FlushAsync(callback);
  return ERR_IO_PENDING;
}

const base::File& FileStream::GetFileForTesting() const {
  return context_->file();
}

}  // namespace net
