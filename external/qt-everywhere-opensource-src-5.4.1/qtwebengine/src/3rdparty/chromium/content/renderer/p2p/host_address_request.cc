// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/host_address_request.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/common/p2p_messages.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "jingle/glue/utils.h"

namespace content {

P2PAsyncAddressResolver::P2PAsyncAddressResolver(
    P2PSocketDispatcher* dispatcher)
    : dispatcher_(dispatcher),
      ipc_message_loop_(dispatcher->message_loop()),
      delegate_message_loop_(base::MessageLoopProxy::current()),
      state_(STATE_CREATED),
      request_id_(0),
      registered_(false) {
  AddRef();  // Balanced in Destroy().
}

P2PAsyncAddressResolver::~P2PAsyncAddressResolver() {
  DCHECK(state_ == STATE_CREATED || state_ == STATE_FINISHED);
  DCHECK(!registered_);
}

void P2PAsyncAddressResolver::Start(const talk_base::SocketAddress& host_name,
                                    const DoneCallback& done_callback) {
  DCHECK(delegate_message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(STATE_CREATED, state_);

  state_ = STATE_SENT;
  ipc_message_loop_->PostTask(FROM_HERE, base::Bind(
      &P2PAsyncAddressResolver::DoSendRequest, this, host_name, done_callback));
}

void P2PAsyncAddressResolver::Cancel() {
  DCHECK(delegate_message_loop_->BelongsToCurrentThread());

  if (state_ != STATE_FINISHED) {
    state_ = STATE_FINISHED;
    ipc_message_loop_->PostTask(FROM_HERE, base::Bind(
        &P2PAsyncAddressResolver::DoUnregister, this));
  }
  done_callback_.Reset();
}

void P2PAsyncAddressResolver::DoSendRequest(
    const talk_base::SocketAddress& host_name,
    const DoneCallback& done_callback) {
  DCHECK(ipc_message_loop_->BelongsToCurrentThread());

  done_callback_ = done_callback;
  request_id_ = dispatcher_->RegisterHostAddressRequest(this);
  registered_ = true;
  dispatcher_->SendP2PMessage(
      new P2PHostMsg_GetHostAddress(host_name.hostname(), request_id_));
}

void P2PAsyncAddressResolver::DoUnregister() {
  DCHECK(ipc_message_loop_->BelongsToCurrentThread());
  if (registered_) {
    dispatcher_->UnregisterHostAddressRequest(request_id_);
    registered_ = false;
  }
}

void P2PAsyncAddressResolver::OnResponse(const net::IPAddressList& addresses) {
  DCHECK(ipc_message_loop_->BelongsToCurrentThread());
  DCHECK(registered_);

  dispatcher_->UnregisterHostAddressRequest(request_id_);
  registered_ = false;

  delegate_message_loop_->PostTask(FROM_HERE, base::Bind(
      &P2PAsyncAddressResolver::DeliverResponse, this, addresses));
}

void P2PAsyncAddressResolver::DeliverResponse(
    const net::IPAddressList& addresses) {
  DCHECK(delegate_message_loop_->BelongsToCurrentThread());
  if (state_ == STATE_SENT) {
    state_ = STATE_FINISHED;
    base::ResetAndReturn(&done_callback_).Run(addresses);
  }
}

}  // namespace content
