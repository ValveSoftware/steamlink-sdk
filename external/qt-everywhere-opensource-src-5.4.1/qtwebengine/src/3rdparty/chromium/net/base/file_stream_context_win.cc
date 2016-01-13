// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream_context.h"

#include <windows.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/task_runner.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

// Ensure that we can just use our Whence values directly.
COMPILE_ASSERT(FROM_BEGIN == FILE_BEGIN, bad_whence_begin);
COMPILE_ASSERT(FROM_CURRENT == FILE_CURRENT, bad_whence_current);
COMPILE_ASSERT(FROM_END == FILE_END, bad_whence_end);

namespace {

void SetOffset(OVERLAPPED* overlapped, const LARGE_INTEGER& offset) {
  overlapped->Offset = offset.LowPart;
  overlapped->OffsetHigh = offset.HighPart;
}

void IncrementOffset(OVERLAPPED* overlapped, DWORD count) {
  LARGE_INTEGER offset;
  offset.LowPart = overlapped->Offset;
  offset.HighPart = overlapped->OffsetHigh;
  offset.QuadPart += static_cast<LONGLONG>(count);
  SetOffset(overlapped, offset);
}

}  // namespace

FileStream::Context::Context(const scoped_refptr<base::TaskRunner>& task_runner)
    : io_context_(),
      async_in_progress_(false),
      orphaned_(false),
      task_runner_(task_runner) {
  io_context_.handler = this;
  memset(&io_context_.overlapped, 0, sizeof(io_context_.overlapped));
}

FileStream::Context::Context(base::File file,
                             const scoped_refptr<base::TaskRunner>& task_runner)
    : io_context_(),
      file_(file.Pass()),
      async_in_progress_(false),
      orphaned_(false),
      task_runner_(task_runner) {
  io_context_.handler = this;
  memset(&io_context_.overlapped, 0, sizeof(io_context_.overlapped));
  if (file_.IsValid()) {
    // TODO(hashimoto): Check that file_ is async.
    OnAsyncFileOpened();
  }
}

FileStream::Context::~Context() {
}

int FileStream::Context::ReadAsync(IOBuffer* buf,
                                   int buf_len,
                                   const CompletionCallback& callback) {
  DCHECK(!async_in_progress_);

  DWORD bytes_read;
  if (!ReadFile(file_.GetPlatformFile(), buf->data(), buf_len,
                &bytes_read, &io_context_.overlapped)) {
    IOResult error = IOResult::FromOSError(GetLastError());
    if (error.os_error == ERROR_IO_PENDING) {
      IOCompletionIsPending(callback, buf);
    } else if (error.os_error == ERROR_HANDLE_EOF) {
      return 0;  // Report EOF by returning 0 bytes read.
    } else {
      LOG(WARNING) << "ReadFile failed: " << error.os_error;
    }
    return error.result;
  }

  IOCompletionIsPending(callback, buf);
  return ERR_IO_PENDING;
}

int FileStream::Context::WriteAsync(IOBuffer* buf,
                                    int buf_len,
                                    const CompletionCallback& callback) {
  DWORD bytes_written = 0;
  if (!WriteFile(file_.GetPlatformFile(), buf->data(), buf_len,
                 &bytes_written, &io_context_.overlapped)) {
    IOResult error = IOResult::FromOSError(GetLastError());
    if (error.os_error == ERROR_IO_PENDING) {
      IOCompletionIsPending(callback, buf);
    } else {
      LOG(WARNING) << "WriteFile failed: " << error.os_error;
    }
    return error.result;
  }

  IOCompletionIsPending(callback, buf);
  return ERR_IO_PENDING;
}

void FileStream::Context::OnAsyncFileOpened() {
  base::MessageLoopForIO::current()->RegisterIOHandler(file_.GetPlatformFile(),
                                                       this);
}

FileStream::Context::IOResult FileStream::Context::SeekFileImpl(Whence whence,
                                                                int64 offset) {
  LARGE_INTEGER distance, res;
  distance.QuadPart = offset;
  DWORD move_method = static_cast<DWORD>(whence);
  if (SetFilePointerEx(file_.GetPlatformFile(), distance, &res, move_method)) {
    SetOffset(&io_context_.overlapped, res);
    return IOResult(res.QuadPart, 0);
  }

  return IOResult::FromOSError(GetLastError());
}

FileStream::Context::IOResult FileStream::Context::FlushFileImpl() {
  if (FlushFileBuffers(file_.GetPlatformFile()))
    return IOResult(OK, 0);

  return IOResult::FromOSError(GetLastError());
}

void FileStream::Context::IOCompletionIsPending(
    const CompletionCallback& callback,
    IOBuffer* buf) {
  DCHECK(callback_.is_null());
  callback_ = callback;
  in_flight_buf_ = buf;  // Hold until the async operation ends.
  async_in_progress_ = true;
}

void FileStream::Context::OnIOCompleted(
    base::MessageLoopForIO::IOContext* context,
    DWORD bytes_read,
    DWORD error) {
  DCHECK_EQ(&io_context_, context);
  DCHECK(!callback_.is_null());
  DCHECK(async_in_progress_);

  async_in_progress_ = false;
  if (orphaned_) {
    callback_.Reset();
    in_flight_buf_ = NULL;
    CloseAndDelete();
    return;
  }

  int result;
  if (error == ERROR_HANDLE_EOF) {
    result = 0;
  } else if (error) {
    IOResult error_result = IOResult::FromOSError(error);
    result = error_result.result;
  } else {
    result = bytes_read;
    IncrementOffset(&io_context_.overlapped, bytes_read);
  }

  CompletionCallback temp_callback = callback_;
  callback_.Reset();
  scoped_refptr<IOBuffer> temp_buf = in_flight_buf_;
  in_flight_buf_ = NULL;
  temp_callback.Run(result);
}

}  // namespace net
