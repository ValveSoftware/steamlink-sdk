// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo_event.h"

namespace IPC {

MojoEvent::MojoEvent() {
  mojo::MessagePipe pipe;
  signal_handle_ = std::move(pipe.handle0);
  wait_handle_ = std::move(pipe.handle1);
}

MojoEvent::~MojoEvent() {}

void MojoEvent::Signal() {
  base::AutoLock lock(lock_);
  if (is_signaled_)
    return;
  is_signaled_ = true;
  MojoResult rv = mojo::WriteMessageRaw(
      signal_handle_.get(), nullptr, 0, nullptr, 0,
      MOJO_WRITE_MESSAGE_FLAG_NONE);
  CHECK_EQ(rv, MOJO_RESULT_OK);
}

void MojoEvent::Reset() {
  base::AutoLock lock(lock_);
  if (!is_signaled_)
    return;
  is_signaled_ = false;
  MojoResult rv = mojo::ReadMessageRaw(
      wait_handle_.get(), nullptr, nullptr, nullptr, nullptr,
      MOJO_READ_MESSAGE_FLAG_NONE);
  CHECK_EQ(rv, MOJO_RESULT_OK);
}

}  // namespace IPC
