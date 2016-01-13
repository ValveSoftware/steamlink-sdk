// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/message_pipe_endpoint.h"

#include "base/logging.h"
#include "mojo/system/channel.h"

namespace mojo {
namespace system {

void MessagePipeEndpoint::Close() {
  NOTREACHED();
}

void MessagePipeEndpoint::CancelAllWaiters() {
  NOTREACHED();
}

MojoResult MessagePipeEndpoint::ReadMessage(void* /*bytes*/,
                                            uint32_t* /*num_bytes*/,
                                            DispatcherVector* /*dispatchers*/,
                                            uint32_t* /*num_dispatchers*/,
                                            MojoReadMessageFlags /*flags*/) {
  NOTREACHED();
  return MOJO_RESULT_INTERNAL;
}

MojoResult MessagePipeEndpoint::AddWaiter(Waiter* /*waiter*/,
                                          MojoHandleSignals /*signals*/,
                                          uint32_t /*context*/) {
  NOTREACHED();
  return MOJO_RESULT_INTERNAL;
}

void MessagePipeEndpoint::RemoveWaiter(Waiter* /*waiter*/) {
  NOTREACHED();
}

void MessagePipeEndpoint::Attach(scoped_refptr<Channel> /*channel*/,
                                 MessageInTransit::EndpointId /*local_id*/) {
  NOTREACHED();
}

bool MessagePipeEndpoint::Run(MessageInTransit::EndpointId /*remote_id*/) {
  NOTREACHED();
  return true;
}

void MessagePipeEndpoint::OnRemove() {
  NOTREACHED();
}

}  // namespace system
}  // namespace mojo
