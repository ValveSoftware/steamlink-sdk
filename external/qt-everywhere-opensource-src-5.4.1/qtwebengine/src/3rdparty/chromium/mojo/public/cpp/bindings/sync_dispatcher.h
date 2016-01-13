// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SYNC_DISPATCHER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SYNC_DISPATCHER_H_

#include "mojo/public/cpp/bindings/lib/filter_chain.h"
#include "mojo/public/cpp/bindings/lib/message_header_validator.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {

class MessageReceiver;

// Waits for one message to arrive on the message pipe, and dispatch it to the
// receiver. Returns true on success, false on failure.
//
// NOTE: The message hasn't been validated and may be malformed!
bool WaitForMessageAndDispatch(MessagePipeHandle handle,
                               mojo::MessageReceiver* receiver);

template<typename Interface> class SyncDispatcher {
 public:
  SyncDispatcher(ScopedMessagePipeHandle message_pipe, Interface* sink)
      : message_pipe_(message_pipe.Pass()) {
    stub_.set_sink(sink);

    filters_.Append<internal::MessageHeaderValidator>();
    filters_.Append<typename Interface::RequestValidator_>();
    filters_.SetSink(&stub_);
  }

  bool WaitAndDispatchOneMessage() {
    return WaitForMessageAndDispatch(message_pipe_.get(),
                                     filters_.GetHead());
  }

 private:
  ScopedMessagePipeHandle message_pipe_;
  typename Interface::Stub_ stub_;
  internal::FilterChain filters_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SYNC_DISPATCHER_H_
