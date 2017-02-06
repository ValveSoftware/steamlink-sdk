// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/websocket_dispatcher_host.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/websocket_host.h"
#include "content/common/websocket_messages.h"

namespace content {

namespace {

// Many methods defined in this file return a WebSocketHostState enum
// value. Make WebSocketHostState visible at file scope so it doesn't have to be
// fully-qualified every time.
typedef WebSocketDispatcherHost::WebSocketHostState WebSocketHostState;

// Max number of pending connections per WebSocketDispatcherHost
// used for per-renderer WebSocket throttling.
const int kMaxPendingWebSocketConnections = 255;

}  // namespace

WebSocketDispatcherHost::WebSocketDispatcherHost(
    int process_id,
    const GetRequestContextCallback& get_context_callback,
    ChromeBlobStorageContext* blob_storage_context,
    StoragePartition* storage_partition)
    : BrowserMessageFilter(WebSocketMsgStart),
      process_id_(process_id),
      get_context_callback_(get_context_callback),
      websocket_host_factory_(
          base::Bind(&WebSocketDispatcherHost::CreateWebSocketHost,
                     base::Unretained(this))),
      num_pending_connections_(0),
      num_current_succeeded_connections_(0),
      num_previous_succeeded_connections_(0),
      num_current_failed_connections_(0),
      num_previous_failed_connections_(0),
      blob_storage_context_(blob_storage_context),
      storage_partition_(storage_partition) {}

WebSocketDispatcherHost::WebSocketDispatcherHost(
    int process_id,
    const GetRequestContextCallback& get_context_callback,
    const WebSocketHostFactory& websocket_host_factory)
    : BrowserMessageFilter(WebSocketMsgStart),
      process_id_(process_id),
      get_context_callback_(get_context_callback),
      websocket_host_factory_(websocket_host_factory),
      num_pending_connections_(0),
      num_current_succeeded_connections_(0),
      num_previous_succeeded_connections_(0),
      num_current_failed_connections_(0),
      num_previous_failed_connections_(0),
      storage_partition_(nullptr) {}

WebSocketHost* WebSocketDispatcherHost::CreateWebSocketHost(
    int routing_id,
    base::TimeDelta delay) {
  return new WebSocketHost(
      routing_id, this, get_context_callback_.Run(), delay);
}

bool WebSocketDispatcherHost::OnMessageReceived(const IPC::Message& message) {
  switch (message.type()) {
    case WebSocketHostMsg_AddChannelRequest::ID:
    case WebSocketHostMsg_SendBlob::ID:
    case WebSocketMsg_SendFrame::ID:
    case WebSocketMsg_FlowControl::ID:
    case WebSocketMsg_DropChannel::ID:
      break;

    default:
      // Every message that has not been handled by a previous filter passes
      // through here, so it is good to pass them on as efficiently as possible.
      return false;
  }

  int routing_id = message.routing_id();
  WebSocketHost* host = GetHost(routing_id);
  if (message.type() == WebSocketHostMsg_AddChannelRequest::ID) {
    if (host) {
      DVLOG(1) << "routing_id=" << routing_id << " already in use.";
      // The websocket multiplexing spec says to should drop the physical
      // connection in this case, but there isn't a real physical connection
      // to the renderer, and killing the renderer for this would seem to be a
      // little extreme. So for now just ignore the bogus request.
      return true;  // We handled the message (by ignoring it).
    }
    if (num_pending_connections_ >= kMaxPendingWebSocketConnections) {
      if (!Send(new WebSocketMsg_NotifyFailure(
              routing_id,
              "Error in connection establishment: "
              "net::ERR_INSUFFICIENT_RESOURCES"))) {
        DVLOG(1) << "Sending of message type "
                 << "WebSocketMsg_NotifyFailure failed.";
      }
      return true;
    }
    host = websocket_host_factory_.Run(routing_id, CalculateDelay());
    hosts_.insert(WebSocketHostTable::value_type(routing_id, host));
    ++num_pending_connections_;
    if (!throttling_period_timer_.IsRunning())
      throttling_period_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMinutes(2),
          this,
          &WebSocketDispatcherHost::ThrottlingPeriodTimerCallback);
  }
  if (!host) {
    DVLOG(1) << "Received invalid routing ID " << routing_id
             << " from renderer.";
    return true;  // We handled the message (by ignoring it).
  }
  return host->OnMessageReceived(message);
}

bool WebSocketDispatcherHost::CanReadRawCookies() const {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  return policy->CanReadRawCookies(process_id_);
}

storage::BlobStorageContext* WebSocketDispatcherHost::blob_storage_context()
    const {
  DCHECK(blob_storage_context_);
  return blob_storage_context_->context();
}

WebSocketHost* WebSocketDispatcherHost::GetHost(int routing_id) const {
  WebSocketHostTable::const_iterator it = hosts_.find(routing_id);
  return it == hosts_.end() ? NULL : it->second;
}

WebSocketHostState WebSocketDispatcherHost::SendOrDrop(IPC::Message* message) {
  const uint32_t message_type = message->type();
  const int32_t message_routing_id = message->routing_id();
  if (!Send(message)) {
    message = NULL;
    DVLOG(1) << "Sending of message type " << message_type
             << " failed. Dropping channel.";
    DeleteWebSocketHost(message_routing_id);
    return WEBSOCKET_HOST_DELETED;
  }
  return WEBSOCKET_HOST_ALIVE;
}

WebSocketHostState WebSocketDispatcherHost::SendAddChannelResponse(
    int routing_id,
    const std::string& selected_protocol,
    const std::string& extensions) {
  // Update throttling counters (success).
  WebSocketHost* host = GetHost(routing_id);
  DCHECK(host);
  host->OnHandshakeSucceeded();
  --num_pending_connections_;
  DCHECK_GE(num_pending_connections_, 0);
  ++num_current_succeeded_connections_;

  return SendOrDrop(new WebSocketMsg_AddChannelResponse(
      routing_id, selected_protocol, extensions));
}

WebSocketHostState WebSocketDispatcherHost::SendFrame(
    int routing_id,
    bool fin,
    WebSocketMessageType type,
    const std::vector<char>& data) {
  return SendOrDrop(new WebSocketMsg_SendFrame(routing_id, fin, type, data));
}

WebSocketHostState WebSocketDispatcherHost::SendFlowControl(int routing_id,
                                                            int64_t quota) {
  return SendOrDrop(new WebSocketMsg_FlowControl(routing_id, quota));
}

WebSocketHostState WebSocketDispatcherHost::NotifyClosingHandshake(
    int routing_id) {
  return SendOrDrop(new WebSocketMsg_NotifyClosing(routing_id));
}

WebSocketHostState WebSocketDispatcherHost::NotifyStartOpeningHandshake(
    int routing_id, const WebSocketHandshakeRequest& request) {
  return SendOrDrop(new WebSocketMsg_NotifyStartOpeningHandshake(
      routing_id, request));
}

WebSocketHostState WebSocketDispatcherHost::NotifyFinishOpeningHandshake(
    int routing_id, const WebSocketHandshakeResponse& response) {
  return SendOrDrop(new WebSocketMsg_NotifyFinishOpeningHandshake(
      routing_id, response));
}

WebSocketHostState WebSocketDispatcherHost::NotifyFailure(
    int routing_id,
    const std::string& message) {
  if (SendOrDrop(new WebSocketMsg_NotifyFailure(
          routing_id, message)) == WEBSOCKET_HOST_DELETED) {
    return WEBSOCKET_HOST_DELETED;
  }
  DeleteWebSocketHost(routing_id);
  return WEBSOCKET_HOST_DELETED;
}

WebSocketHostState WebSocketDispatcherHost::BlobSendComplete(int routing_id) {
  return SendOrDrop(new WebSocketMsg_BlobSendComplete(routing_id));
}

WebSocketHostState WebSocketDispatcherHost::DoDropChannel(
    int routing_id,
    bool was_clean,
    uint16_t code,
    const std::string& reason) {
  if (SendOrDrop(
          new WebSocketMsg_DropChannel(routing_id, was_clean, code, reason)) ==
      WEBSOCKET_HOST_DELETED)
    return WEBSOCKET_HOST_DELETED;
  DeleteWebSocketHost(routing_id);
  return WEBSOCKET_HOST_DELETED;
}

WebSocketDispatcherHost::~WebSocketDispatcherHost() {
  std::vector<WebSocketHost*> hosts;
  for (base::hash_map<int, WebSocketHost*>::const_iterator i = hosts_.begin();
       i != hosts_.end(); ++i) {
    // In order to avoid changing the container while iterating, we copy
    // the hosts.
    hosts.push_back(i->second);
  }

  for (size_t i = 0; i < hosts.size(); ++i) {
    // Note that some calls to GoAway could fail. In that case hosts[i] will be
    // deleted and removed from |hosts_| in |DoDropChannel|.
    hosts[i]->GoAway();
    hosts[i] = NULL;
  }

  STLDeleteContainerPairSecondPointers(hosts_.begin(), hosts_.end());
}

void WebSocketDispatcherHost::DeleteWebSocketHost(int routing_id) {
  WebSocketHostTable::iterator it = hosts_.find(routing_id);
  DCHECK(it != hosts_.end());
  DCHECK(it->second);
  if (!it->second->handshake_succeeded()) {
    // Update throttling counters (failure).
    --num_pending_connections_;
    DCHECK_GE(num_pending_connections_, 0);
    ++num_current_failed_connections_;
  }

  delete it->second;
  hosts_.erase(it);

  DCHECK_LE(base::checked_cast<size_t>(num_pending_connections_),
            hosts_.size());
}

int64_t WebSocketDispatcherHost::num_failed_connections() const {
  return num_previous_failed_connections_ +
         num_current_failed_connections_;
}

int64_t WebSocketDispatcherHost::num_succeeded_connections() const {
  return num_previous_succeeded_connections_ +
         num_current_succeeded_connections_;
}

// Calculate delay as described in
// the per-renderer WebSocket throttling design doc:
// https://docs.google.com/document/d/1aw2oN5PKfk-1gLnBrlv1OwLA8K3-ykM2ckwX2lubTg4/edit?usp=sharing
base::TimeDelta WebSocketDispatcherHost::CalculateDelay() const {
  int64_t f = num_failed_connections();
  int64_t s = num_succeeded_connections();
  int p = num_pending_connections();
  return base::TimeDelta::FromMilliseconds(
      base::RandInt(1000, 5000) *
      (1 << std::min(p + f / (s + 1), INT64_C(16))) / 65536);
}

void WebSocketDispatcherHost::ThrottlingPeriodTimerCallback() {
  num_previous_failed_connections_ = num_current_failed_connections_;
  num_current_failed_connections_ = 0;

  num_previous_succeeded_connections_ = num_current_succeeded_connections_;
  num_current_succeeded_connections_ = 0;

  if (num_pending_connections_ == 0 &&
      num_previous_failed_connections_ == 0 &&
      num_previous_succeeded_connections_ == 0) {
    throttling_period_timer_.Stop();
  }
}

}  // namespace content
