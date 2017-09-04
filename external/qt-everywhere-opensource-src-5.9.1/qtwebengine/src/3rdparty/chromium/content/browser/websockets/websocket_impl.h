// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBSOCKETS_WEBSOCKET_IMPL_H_
#define CONTENT_BROWSER_WEBSOCKETS_WEBSOCKET_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/platform/modules/websockets/websocket.mojom.h"

class GURL;

namespace url {
class Origin;
}  // namespace url

namespace net {
class WebSocketChannel;
}  // namespace net

namespace content {
class StoragePartition;

// Host of net::WebSocketChannel.
class CONTENT_EXPORT WebSocketImpl
    : NON_EXPORTED_BASE(public blink::mojom::WebSocket) {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual int GetClientProcessId() = 0;
    virtual StoragePartition* GetStoragePartition() = 0;
    virtual void OnReceivedResponseFromServer(WebSocketImpl* impl) = 0;
    virtual void OnLostConnectionToClient(WebSocketImpl* impl) = 0;
  };

  WebSocketImpl(Delegate* delegate,
                blink::mojom::WebSocketRequest request,
                int child_id,
                int frame_id,
                base::TimeDelta delay);
  ~WebSocketImpl() override;

  // The renderer process is going away.
  // This function is virtual for testing.
  virtual void GoAway();

  // blink::mojom::WebSocket methods:
  void AddChannelRequest(const GURL& url,
                         const std::vector<std::string>& requested_protocols,
                         const url::Origin& origin,
                         const GURL& first_party_for_cookies,
                         const std::string& user_agent_override,
                         blink::mojom::WebSocketClientPtr client) override;
  void SendFrame(bool fin,
                 blink::mojom::WebSocketMessageType type,
                 const std::vector<uint8_t>& data) override;
  void SendFlowControl(int64_t quota) override;
  void StartClosingHandshake(uint16_t code, const std::string& reason) override;

  bool handshake_succeeded() const { return handshake_succeeded_; }
  void OnHandshakeSucceeded() { handshake_succeeded_ = true; }

 protected:
  class WebSocketEventHandler;

  void OnConnectionError();
  void AddChannel(const GURL& socket_url,
                  const std::vector<std::string>& requested_protocols,
                  const url::Origin& origin,
                  const GURL& first_party_for_cookies,
                  const std::string& user_agent_override);

  Delegate* delegate_;
  mojo::Binding<blink::mojom::WebSocket> binding_;

  blink::mojom::WebSocketClientPtr client_;

  // The channel we use to send events to the network.
  std::unique_ptr<net::WebSocketChannel> channel_;

  // Delay used for per-renderer WebSocket throttling.
  base::TimeDelta delay_;

  // SendFlowControl() is delayed when OnFlowControl() is called before
  // AddChannel() is called.
  // Zero indicates there is no pending SendFlowControl().
  int64_t pending_flow_control_quota_;

  int child_id_;
  int frame_id_;

  // handshake_succeeded_ is set and used by WebSocketManager to manage
  // counters for per-renderer WebSocket throttling.
  bool handshake_succeeded_;

  base::WeakPtrFactory<WebSocketImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBSOCKETS_WEBSOCKET_IMPL_H_
