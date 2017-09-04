// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <vector>

#include "content/browser/websockets/websocket_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

// This number is unlikely to occur by chance.
static const int kMagicRenderProcessId = 506116062;

class TestWebSocketImpl : public WebSocketImpl {
 public:
  TestWebSocketImpl(Delegate* delegate,
                    blink::mojom::WebSocketRequest request,
                    int process_id,
                    int frame_id,
                    base::TimeDelta delay)
      : WebSocketImpl(delegate,
                      std::move(request),
                      process_id,
                      frame_id,
                      delay) {}

  base::TimeDelta delay() const { return delay_; }

  void SimulateConnectionError() {
    OnConnectionError();
  }
};

class TestWebSocketManager : public WebSocketManager {
 public:
  TestWebSocketManager()
      : WebSocketManager(kMagicRenderProcessId, nullptr) {}

  const std::vector<TestWebSocketImpl*>& sockets() const {
    return sockets_;
  }

  int num_pending_connections() const {
    return num_pending_connections_;
  }
  int64_t num_failed_connections() const {
    return num_current_failed_connections_ + num_previous_failed_connections_;
  }
  int64_t num_succeeded_connections() const {
    return num_current_succeeded_connections_ +
           num_previous_succeeded_connections_;
  }

  void DoCreateWebSocket(blink::mojom::WebSocketRequest request) {
    WebSocketManager::DoCreateWebSocket(MSG_ROUTING_NONE, std::move(request));
  }

 private:
  WebSocketImpl* CreateWebSocketImpl(WebSocketImpl::Delegate* delegate,
                                     blink::mojom::WebSocketRequest request,
                                     int process_id,
                                     int frame_id,
                                     base::TimeDelta delay) override {
    TestWebSocketImpl* impl = new TestWebSocketImpl(
        delegate, std::move(request), process_id, frame_id, delay);
    // We keep a vector of sockets here to track their creation order.
    sockets_.push_back(impl);
    return impl;
  }

  void OnLostConnectionToClient(WebSocketImpl* impl) override {
    auto it = std::find(sockets_.begin(), sockets_.end(),
                        static_cast<TestWebSocketImpl*>(impl));
    ASSERT_TRUE(it != sockets_.end());
    sockets_.erase(it);

    WebSocketManager::OnLostConnectionToClient(impl);
  }

  std::vector<TestWebSocketImpl*> sockets_;
};

class WebSocketManagerTest : public ::testing::Test {
 public:
  WebSocketManagerTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {
    websocket_manager_.reset(new TestWebSocketManager());
  }

  void AddMultipleChannels(int number_of_channels) {
    for (int i = 0; i < number_of_channels; ++i) {
      blink::mojom::WebSocketPtr websocket;
      websocket_manager_->DoCreateWebSocket(mojo::GetProxy(&websocket));
    }
  }

  void AddAndCancelMultipleChannels(int number_of_channels) {
    for (int i = 0; i < number_of_channels; ++i) {
      blink::mojom::WebSocketPtr websocket;
      websocket_manager_->DoCreateWebSocket(mojo::GetProxy(&websocket));
      websocket_manager_->sockets().back()->SimulateConnectionError();
    }
  }

  TestWebSocketManager* websocket_manager() { return websocket_manager_.get(); }

 private:
  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestWebSocketManager> websocket_manager_;
};

TEST_F(WebSocketManagerTest, Construct) {
  // Do nothing.
}

TEST_F(WebSocketManagerTest, CreateWebSocket) {
  blink::mojom::WebSocketPtr websocket;

  websocket_manager()->DoCreateWebSocket(mojo::GetProxy(&websocket));

  EXPECT_EQ(1U, websocket_manager()->sockets().size());
}

TEST_F(WebSocketManagerTest, SendFrameButNotConnectedYet) {
  blink::mojom::WebSocketPtr websocket;

  websocket_manager()->DoCreateWebSocket(mojo::GetProxy(&websocket));

  // This should not crash.
  mojo::Array<uint8_t> data;
  websocket->SendFrame(
      true, blink::mojom::WebSocketMessageType::TEXT, std::move(data));
}

TEST_F(WebSocketManagerTest, DelayFor4thPendingConnectionIsZero) {
  AddMultipleChannels(4);

  EXPECT_EQ(4, websocket_manager()->num_pending_connections());
  EXPECT_EQ(0, websocket_manager()->num_failed_connections());
  EXPECT_EQ(0, websocket_manager()->num_succeeded_connections());

  ASSERT_EQ(4U, websocket_manager()->sockets().size());
  EXPECT_EQ(base::TimeDelta(), websocket_manager()->sockets()[3]->delay());
}

TEST_F(WebSocketManagerTest, DelayFor8thPendingConnectionIsNonZero) {
  AddMultipleChannels(8);

  EXPECT_EQ(8, websocket_manager()->num_pending_connections());
  EXPECT_EQ(0, websocket_manager()->num_failed_connections());
  EXPECT_EQ(0, websocket_manager()->num_succeeded_connections());

  ASSERT_EQ(8U, websocket_manager()->sockets().size());
  EXPECT_LT(base::TimeDelta(), websocket_manager()->sockets()[7]->delay());
}

TEST_F(WebSocketManagerTest, DelayFor17thPendingConnection) {
  AddMultipleChannels(17);

  EXPECT_EQ(17, websocket_manager()->num_pending_connections());
  EXPECT_EQ(0, websocket_manager()->num_failed_connections());
  EXPECT_EQ(0, websocket_manager()->num_succeeded_connections());

  ASSERT_EQ(17U, websocket_manager()->sockets().size());
  EXPECT_LE(base::TimeDelta::FromMilliseconds(1000),
            websocket_manager()->sockets()[16]->delay());
  EXPECT_GE(base::TimeDelta::FromMilliseconds(5000),
            websocket_manager()->sockets()[16]->delay());
}

// The 256th connection is rejected by per-renderer WebSocket throttling.
// This is not counted as a failure.
TEST_F(WebSocketManagerTest, Rejects256thPendingConnection) {
  AddMultipleChannels(256);

  EXPECT_EQ(255, websocket_manager()->num_pending_connections());
  EXPECT_EQ(0, websocket_manager()->num_failed_connections());
  EXPECT_EQ(0, websocket_manager()->num_succeeded_connections());

  ASSERT_EQ(255U, websocket_manager()->sockets().size());
}

TEST_F(WebSocketManagerTest, DelayIsZeroAfter3FailedConnections) {
  AddAndCancelMultipleChannels(3);

  EXPECT_EQ(0, websocket_manager()->num_pending_connections());
  EXPECT_EQ(3, websocket_manager()->num_failed_connections());
  EXPECT_EQ(0, websocket_manager()->num_succeeded_connections());

  AddMultipleChannels(1);

  ASSERT_EQ(1U, websocket_manager()->sockets().size());
  EXPECT_EQ(base::TimeDelta(), websocket_manager()->sockets()[0]->delay());
}

TEST_F(WebSocketManagerTest, DelayIsNonZeroAfter7FailedConnections) {
  AddAndCancelMultipleChannels(7);

  EXPECT_EQ(0, websocket_manager()->num_pending_connections());
  EXPECT_EQ(7, websocket_manager()->num_failed_connections());
  EXPECT_EQ(0, websocket_manager()->num_succeeded_connections());

  AddMultipleChannels(1);

  ASSERT_EQ(1U, websocket_manager()->sockets().size());
  EXPECT_LT(base::TimeDelta(), websocket_manager()->sockets()[0]->delay());
}

TEST_F(WebSocketManagerTest, DelayAfter16FailedConnections) {
  AddAndCancelMultipleChannels(16);

  EXPECT_EQ(0, websocket_manager()->num_pending_connections());
  EXPECT_EQ(16, websocket_manager()->num_failed_connections());
  EXPECT_EQ(0, websocket_manager()->num_succeeded_connections());

  AddMultipleChannels(1);

  ASSERT_EQ(1U, websocket_manager()->sockets().size());
  EXPECT_LE(base::TimeDelta::FromMilliseconds(1000),
            websocket_manager()->sockets()[0]->delay());
  EXPECT_GE(base::TimeDelta::FromMilliseconds(5000),
            websocket_manager()->sockets()[0]->delay());
}

TEST_F(WebSocketManagerTest, NotRejectedAfter255FailedConnections) {
  AddAndCancelMultipleChannels(255);

  EXPECT_EQ(0, websocket_manager()->num_pending_connections());
  EXPECT_EQ(255, websocket_manager()->num_failed_connections());
  EXPECT_EQ(0, websocket_manager()->num_succeeded_connections());

  AddMultipleChannels(1);

  EXPECT_EQ(1, websocket_manager()->num_pending_connections());
  EXPECT_EQ(255, websocket_manager()->num_failed_connections());
  EXPECT_EQ(0, websocket_manager()->num_succeeded_connections());
}

}  // namespace
}  // namespace content
