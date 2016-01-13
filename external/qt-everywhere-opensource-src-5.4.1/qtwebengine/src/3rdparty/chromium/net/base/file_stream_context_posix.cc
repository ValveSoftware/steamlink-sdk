// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For 64-bit file access (off_t = off64_t, lseek64, etc).
#define _FILE_OFFSET_BITS 64

#include "net/base/file_stream_context.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

#if defined(OS_ANDROID)
// Android's bionic libc only supports the LFS transitional API.
#define off_t off64_t
#define lseek lseek64
#define stat stat64
#define fstat fstat64
#endif

namespace net {

// We cast back and forth, so make sure it's the size we're expecting.
COMPILE_ASSERT(sizeof(int64) == sizeof(off_t), off_t_64_bit);

// Make sure our Whence mappings match the system headers.
COMPILE_ASSERT(FROM_BEGIN   == SEEK_SET &&
               FROM_CURRENT == SEEK_CUR &&
               FROM_END     == SEEK_END, whence_matches_system);

FileStream::Context::Context(const scoped_refptr<base::TaskRunner>& task_runner)
    : async_in_progress_(false),
      orphaned_(false),
      task_runner_(task_runner) {
}

FileStream::Context::Context(base::File file,
                             const scoped_refptr<base::TaskRunner>& task_runner)
    : file_(file.Pass()),
      async_in_progress_(false),
      orphaned_(false),
      task_runner_(task_runner) {
}

FileStream::Context::~Context() {
}

int FileStream::Context::ReadAsync(IOBuffer* in_buf,
                                   int buf_len,
                                   const CompletionCallback& callback) {
  DCHECK(!async_in_progress_);

  scoped_refptr<IOBuffer> buf = in_buf;
  const bool posted = base::PostTaskAndReplyWithResult(
      task_runner_.get(),
      FROM_HERE,
      base::Bind(&Context::ReadFileImpl, base::Unretained(this), buf, buf_len),
      base::Bind(&Context::OnAsyncCompleted,
                 base::Unretained(this),
                 IntToInt64(callback)));
  DCHECK(posted);

  async_in_progress_ = true;
  return ERR_IO_PENDING;
}

int FileStream::Context::WriteAsync(IOBuffer* in_buf,
                                    int buf_len,
                                    const CompletionCallback& callback) {
  DCHECK(!async_in_progress_);

  scoped_refptr<IOBuffer> buf = in_buf;
  const bool posted = base::PostTaskAndReplyWithResult(
      task_runner_.get(),
      FROM_HERE,
      base::Bind(&Context::WriteFileImpl, base::Unretained(this), buf, buf_len),
      base::Bind(&Context::OnAsyncCompleted,
                 base::Unretained(this),
                 IntToInt64(callback)));
  DCHECK(posted);

  async_in_progress_ = true;
  return ERR_IO_PENDING;
}

FileStream::Context::IOResult FileStream::Context::SeekFileImpl(Whence whence,
                                                                int64 offset) {
  off_t res = lseek(file_.GetPlatformFile(), static_cast<off_t>(offset),
                    static_cast<int>(whence));
  if (res == static_cast<off_t>(-1))
    return IOResult::FromOSError(errno);

  return IOResult(res, 0);
}

FileStream::Context::IOResult FileStream::Context::FlushFileImpl() {
  ssize_t res = HANDLE_EINTR(fsync(file_.GetPlatformFile()));
  if (res == -1)
    return IOResult::FromOSError(errno);

  return IOResult(res, 0);
}

FileStream::Context::IOResult FileStream::Context::ReadFileImpl(
    scoped_refptr<IOBuffer> buf,
    int buf_len) {
  // Loop in the case of getting interrupted by a signal.
  ssize_t res = HANDLE_EINTR(read(file_.GetPlatformFile(), buf->data(),
                                  static_cast<size_t>(buf_len)));
  if (res == -1)
    return IOResult::FromOSError(errno);

  return IOResult(res, 0);
}

FileStream::Context::IOResult FileStream::Context::WriteFileImpl(
    scoped_refptr<IOBuffer> buf,
    int buf_len) {
  ssize_t res = HANDLE_EINTR(write(file_.GetPlatformFile(), buf->data(),
                                   buf_len));
  if (res == -1)
    return IOResult::FromOSError(errno);

  return IOResult(res, 0);
}

}  // namespace net
