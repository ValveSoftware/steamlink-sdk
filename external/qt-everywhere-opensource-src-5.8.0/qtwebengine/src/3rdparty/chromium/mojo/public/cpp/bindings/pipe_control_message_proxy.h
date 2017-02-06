// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_PIPE_CONTROL_MESSAGE_PROXY_H_
#define MOJO_PUBLIC_CPP_BINDINGS_PIPE_CONTROL_MESSAGE_PROXY_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/lib/serialization_context.h"

namespace mojo {

class MessageReceiver;

// Proxy for request messages defined in pipe_control_messages.mojom.
class PipeControlMessageProxy {
 public:
  // Doesn't take ownership of |receiver|. It must outlive this object.
  explicit PipeControlMessageProxy(MessageReceiver* receiver);

  void NotifyPeerEndpointClosed(InterfaceId id);
  void NotifyEndpointClosedBeforeSent(InterfaceId id);

 private:
  // Not owned.
  MessageReceiver* receiver_;
  internal::SerializationContext context_;

  DISALLOW_COPY_AND_ASSIGN(PipeControlMessageProxy);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_PIPE_CONTROL_MESSAGE_PROXY_H_
