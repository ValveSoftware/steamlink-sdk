// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/test_completion_callback.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "net/base/io_buffer.h"

namespace net {

namespace internal {

void TestCompletionCallbackBaseInternal::DidSetResult() {
  have_result_ = true;
  if (waiting_for_result_)
    base::MessageLoop::current()->Quit();
}

void TestCompletionCallbackBaseInternal::WaitForResult() {
  DCHECK(!waiting_for_result_);
  while (!have_result_) {
    waiting_for_result_ = true;
    base::MessageLoop::current()->Run();
    waiting_for_result_ = false;
  }
  have_result_ = false;  // Auto-reset for next callback.
}

TestCompletionCallbackBaseInternal::TestCompletionCallbackBaseInternal()
    : have_result_(false),
      waiting_for_result_(false) {
}

}  // namespace internal

TestCompletionCallback::TestCompletionCallback()
    : callback_(base::Bind(&TestCompletionCallback::SetResult,
                base::Unretained(this))) {
}

TestCompletionCallback::~TestCompletionCallback() {
}

TestInt64CompletionCallback::TestInt64CompletionCallback()
    : callback_(base::Bind(&TestInt64CompletionCallback::SetResult,
                base::Unretained(this))) {
}

TestInt64CompletionCallback::~TestInt64CompletionCallback() {
}

ReleaseBufferCompletionCallback::ReleaseBufferCompletionCallback(
    IOBuffer* buffer) : buffer_(buffer) {
}

ReleaseBufferCompletionCallback::~ReleaseBufferCompletionCallback() {
}

void ReleaseBufferCompletionCallback::SetResult(int result) {
  if (!buffer_->HasOneRef())
    result = net::ERR_FAILED;
  TestCompletionCallback::SetResult(result);
}

}  // namespace net
