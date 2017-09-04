// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/websockets/websocket_manager.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/storage_partition.h"
#include "services/service_manager/public/cpp/interface_registry.h"

namespace content {

namespace {

const char kWebSocketManagerKeyName[] = "web_socket_manager";

// Max number of pending connections per WebSocketManager used for per-renderer
// WebSocket throttling.
const int kMaxPendingWebSocketConnections = 255;

}  // namespace

class WebSocketManager::Handle : public base::SupportsUserData::Data,
                                 public RenderProcessHostObserver {
 public:
  explicit Handle(WebSocketManager* manager) : manager_(manager) {}

  ~Handle() override {
    DCHECK(!manager_) << "Should have received RenderProcessHostDestroyed";
  }

  WebSocketManager* manager() const { return manager_; }

  // The network stack could be shutdown after this notification, so be sure to
  // stop using it before then.
  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, manager_);
    manager_ = nullptr;
  }

 private:
  WebSocketManager* manager_;
};

// static
void WebSocketManager::CreateWebSocket(int process_id, int frame_id,
                                       blink::mojom::WebSocketRequest request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RenderProcessHost* host = RenderProcessHost::FromID(process_id);
  DCHECK(host);

  // Maintain a WebSocketManager per RenderProcessHost. While the instance of
  // WebSocketManager is allocated on the UI thread, it must only be used and
  // deleted from the IO thread.

  Handle* handle =
      static_cast<Handle*>(host->GetUserData(kWebSocketManagerKeyName));
  if (!handle) {
    handle = new Handle(
        new WebSocketManager(process_id, host->GetStoragePartition()));
    host->SetUserData(kWebSocketManagerKeyName, handle);
    host->AddObserver(handle);
  } else {
    DCHECK(handle->manager());
  }

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&WebSocketManager::DoCreateWebSocket,
                 base::Unretained(handle->manager()),
                 frame_id,
                 base::Passed(&request)));
}

WebSocketManager::WebSocketManager(int process_id,
                                   StoragePartition* storage_partition)
    : process_id_(process_id),
      storage_partition_(storage_partition),
      num_pending_connections_(0),
      num_current_succeeded_connections_(0),
      num_previous_succeeded_connections_(0),
      num_current_failed_connections_(0),
      num_previous_failed_connections_(0),
      context_destroyed_(false) {
  if (storage_partition_) {
    url_request_context_getter_ = storage_partition_->GetURLRequestContext();
    // This unretained pointer is safe because we destruct a WebSocketManager
    // only via WebSocketManager::Handle::RenderProcessHostDestroyed which
    // posts a deletion task to the IO thread.
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(
            &WebSocketManager::ObserveURLRequestContextGetter,
            base::Unretained(this)));
  }
}

WebSocketManager::~WebSocketManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!context_destroyed_ && url_request_context_getter_)
    url_request_context_getter_->RemoveObserver(this);

  for (auto impl : impls_) {
    impl->GoAway();
    delete impl;
  }
}

void WebSocketManager::DoCreateWebSocket(
    int frame_id,
    blink::mojom::WebSocketRequest request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (num_pending_connections_ >= kMaxPendingWebSocketConnections) {
    // Too many websockets!
    request.ResetWithReason(
        blink::mojom::WebSocket::kInsufficientResources,
        "Error in connection establishment: net::ERR_INSUFFICIENT_RESOURCES");
    return;
  }
  if (context_destroyed_) {
    request.ResetWithReason(
        blink::mojom::WebSocket::kInsufficientResources,
        "Error in connection establishment: net::ERR_UNEXPECTED");
    return;
  }

  // Keep all WebSocketImpls alive until either the client drops its
  // connection (see OnLostConnectionToClient) or we need to shutdown.

  impls_.insert(CreateWebSocketImpl(this, std::move(request), process_id_,
                                    frame_id, CalculateDelay()));
  ++num_pending_connections_;

  if (!throttling_period_timer_.IsRunning()) {
    throttling_period_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMinutes(2),
        this,
        &WebSocketManager::ThrottlingPeriodTimerCallback);
  }
}

// Calculate delay as described in the per-renderer WebSocket throttling
// design doc: https://goo.gl/tldFNn
base::TimeDelta WebSocketManager::CalculateDelay() const {
  int64_t f = num_previous_failed_connections_ +
              num_current_failed_connections_;
  int64_t s = num_previous_succeeded_connections_ +
              num_current_succeeded_connections_;
  int p = num_pending_connections_;
  return base::TimeDelta::FromMilliseconds(
      base::RandInt(1000, 5000) *
      (1 << std::min(p + f / (s + 1), INT64_C(16))) / 65536);
}

void WebSocketManager::ThrottlingPeriodTimerCallback() {
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

WebSocketImpl* WebSocketManager::CreateWebSocketImpl(
    WebSocketImpl::Delegate* delegate,
    blink::mojom::WebSocketRequest request,
    int child_id,
    int frame_id,
    base::TimeDelta delay) {
  return new WebSocketImpl(delegate, std::move(request), child_id, frame_id,
                           delay);
}

int WebSocketManager::GetClientProcessId() {
  return process_id_;
}

StoragePartition* WebSocketManager::GetStoragePartition() {
  return storage_partition_;
}

void WebSocketManager::OnReceivedResponseFromServer(WebSocketImpl* impl) {
  // The server accepted this WebSocket connection.
  impl->OnHandshakeSucceeded();
  --num_pending_connections_;
  DCHECK_GE(num_pending_connections_, 0);
  ++num_current_succeeded_connections_;
}

void WebSocketManager::OnLostConnectionToClient(WebSocketImpl* impl) {
  // The client is no longer interested in this WebSocket.
  if (!impl->handshake_succeeded()) {
    // Update throttling counters (failure).
    --num_pending_connections_;
    DCHECK_GE(num_pending_connections_, 0);
    ++num_current_failed_connections_;
  }
  impl->GoAway();
  impls_.erase(impl);
  delete impl;
}

void WebSocketManager::OnContextShuttingDown() {
  context_destroyed_ = true;
  url_request_context_getter_ = nullptr;
  for (const auto& impl : impls_) {
    impl->GoAway();
    delete impl;
  }
  impls_.clear();
}

void WebSocketManager::ObserveURLRequestContextGetter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!url_request_context_getter_->GetURLRequestContext()) {
    context_destroyed_ = true;
    url_request_context_getter_ = nullptr;
    return;
  }
  url_request_context_getter_->AddObserver(this);
}

}  // namespace content
