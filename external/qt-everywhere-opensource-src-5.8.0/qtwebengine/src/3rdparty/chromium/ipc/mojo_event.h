// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_MOJO_EVENT_H_
#define IPC_MOJO_EVENT_H_

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace IPC {

// A MojoEvent is a simple wrapper around a Mojo message pipe which supports
// common WaitableEvent-like methods of Signal() and Reset(). This class exists
// to support the transition from legacy IPC to Mojo IPC and is not intended for
// general use outside of src/ipc. Unlike base::WaitableEvent, all MojoEvents
// must be manually reset.
class MojoEvent {
 public:
  // Constructs a new MojoEvent that is initially not signaled.
  MojoEvent();

  ~MojoEvent();

  // Gets a Handle that can be waited on for this MojoEvent. When the Event is
  // signaled, this handle will have |MOJO_HANDLE_SIGNAL_READABLE| satisfied.
  const mojo::Handle& GetHandle() const { return wait_handle_.get(); }

  void Signal();
  void Reset();

 private:
  mojo::ScopedMessagePipeHandle signal_handle_;
  mojo::ScopedMessagePipeHandle wait_handle_;

  base::Lock lock_;
  bool is_signaled_ = false;

  DISALLOW_COPY_AND_ASSIGN(MojoEvent);
};

}  // namespace IPC

#endif  // IPC_MOJO_EVENT_H_
