// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/mock_file_stream.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"

namespace net {

namespace testing {

MockFileStream::MockFileStream(
    const scoped_refptr<base::TaskRunner>& task_runner)
    : net::FileStream(task_runner),
      forced_error_(net::OK),
      async_error_(false),
      throttled_(false),
      weak_factory_(this) {
}

MockFileStream::MockFileStream(
    base::File file,
    const scoped_refptr<base::TaskRunner>& task_runner)
    : net::FileStream(file.Pass(), task_runner),
      forced_error_(net::OK),
      async_error_(false),
      throttled_(false),
      weak_factory_(this) {
}

MockFileStream::~MockFileStream() {
}

int MockFileStream::Seek(Whence whence, int64 offset,
                         const Int64CompletionCallback& callback) {
  Int64CompletionCallback wrapped_callback =
      base::Bind(&MockFileStream::DoCallback64,
                 weak_factory_.GetWeakPtr(), callback);
  if (forced_error_ == net::OK)
    return FileStream::Seek(whence, offset, wrapped_callback);
  return ErrorCallback64(wrapped_callback);
}

int MockFileStream::Read(IOBuffer* buf,
                         int buf_len,
                         const CompletionCallback& callback) {
  CompletionCallback wrapped_callback = base::Bind(&MockFileStream::DoCallback,
                                                   weak_factory_.GetWeakPtr(),
                                                   callback);
  if (forced_error_ == net::OK)
    return FileStream::Read(buf, buf_len, wrapped_callback);
  return ErrorCallback(wrapped_callback);
}

int MockFileStream::Write(IOBuffer* buf,
                          int buf_len,
                          const CompletionCallback& callback) {
  CompletionCallback wrapped_callback = base::Bind(&MockFileStream::DoCallback,
                                                   weak_factory_.GetWeakPtr(),
                                                   callback);
  if (forced_error_ == net::OK)
    return FileStream::Write(buf, buf_len, wrapped_callback);
  return ErrorCallback(wrapped_callback);
}

int MockFileStream::Flush(const CompletionCallback& callback) {
  CompletionCallback wrapped_callback = base::Bind(&MockFileStream::DoCallback,
                                                   weak_factory_.GetWeakPtr(),
                                                   callback);
  if (forced_error_ == net::OK)
    return FileStream::Flush(wrapped_callback);
  return ErrorCallback(wrapped_callback);
}

void MockFileStream::ThrottleCallbacks() {
  CHECK(!throttled_);
  throttled_ = true;
}

void MockFileStream::ReleaseCallbacks() {
  CHECK(throttled_);
  throttled_ = false;

  if (!throttled_task_.is_null()) {
    base::Closure throttled_task = throttled_task_;
    throttled_task_.Reset();
    base::MessageLoop::current()->PostTask(FROM_HERE, throttled_task);
  }
}

void MockFileStream::DoCallback(const CompletionCallback& callback,
                                int result) {
  if (!throttled_) {
    callback.Run(result);
    return;
  }
  CHECK(throttled_task_.is_null());
  throttled_task_ = base::Bind(callback, result);
}

void MockFileStream::DoCallback64(const Int64CompletionCallback& callback,
                                  int64 result) {
  if (!throttled_) {
    callback.Run(result);
    return;
  }
  CHECK(throttled_task_.is_null());
  throttled_task_ = base::Bind(callback, result);
}

int MockFileStream::ErrorCallback(const CompletionCallback& callback) {
  CHECK_NE(net::OK, forced_error_);
  if (async_error_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, forced_error_));
    clear_forced_error();
    return net::ERR_IO_PENDING;
  }
  int ret = forced_error_;
  clear_forced_error();
  return ret;
}

int64 MockFileStream::ErrorCallback64(const Int64CompletionCallback& callback) {
  CHECK_NE(net::OK, forced_error_);
  if (async_error_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, forced_error_));
    clear_forced_error();
    return net::ERR_IO_PENDING;
  }
  int64 ret = forced_error_;
  clear_forced_error();
  return ret;
}

}  // namespace testing

}  // namespace net
