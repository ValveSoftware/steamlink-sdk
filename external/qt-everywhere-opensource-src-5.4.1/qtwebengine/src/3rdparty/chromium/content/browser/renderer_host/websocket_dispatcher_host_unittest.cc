// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/websocket_dispatcher_host.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "content/browser/renderer_host/websocket_host.h"
#include "content/common/websocket.h"
#include "content/common/websocket_messages.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

// This number is unlikely to occur by chance.
static const int kMagicRenderProcessId = 506116062;

// A mock of WebsocketHost which records received messages.
class MockWebSocketHost : public WebSocketHost {
 public:
  MockWebSocketHost(int routing_id,
                    WebSocketDispatcherHost* dispatcher,
                    net::URLRequestContext* url_request_context)
      : WebSocketHost(routing_id, dispatcher, url_request_context) {
  }

  virtual ~MockWebSocketHost() {}

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE{
    received_messages_.push_back(message);
    return true;
  }

  std::vector<IPC::Message> received_messages_;
};

class WebSocketDispatcherHostTest : public ::testing::Test {
 public:
  WebSocketDispatcherHostTest() {
    dispatcher_host_ = new WebSocketDispatcherHost(
        kMagicRenderProcessId,
        base::Bind(&WebSocketDispatcherHostTest::OnGetRequestContext,
                   base::Unretained(this)),
        base::Bind(&WebSocketDispatcherHostTest::CreateWebSocketHost,
                   base::Unretained(this)));
  }

  virtual ~WebSocketDispatcherHostTest() {}

 protected:
  scoped_refptr<WebSocketDispatcherHost> dispatcher_host_;

  // Stores allocated MockWebSocketHost instances. Doesn't take ownership of
  // them.
  std::vector<MockWebSocketHost*> mock_hosts_;

 private:
  net::URLRequestContext* OnGetRequestContext() {
    return NULL;
  }

  WebSocketHost* CreateWebSocketHost(int routing_id) {
    MockWebSocketHost* host =
        new MockWebSocketHost(routing_id, dispatcher_host_.get(), NULL);
    mock_hosts_.push_back(host);
    return host;
  }
};

TEST_F(WebSocketDispatcherHostTest, Construct) {
  // Do nothing.
}

TEST_F(WebSocketDispatcherHostTest, UnrelatedMessage) {
  IPC::Message message;
  EXPECT_FALSE(dispatcher_host_->OnMessageReceived(message));
}

TEST_F(WebSocketDispatcherHostTest, RenderProcessIdGetter) {
  EXPECT_EQ(kMagicRenderProcessId, dispatcher_host_->render_process_id());
}

TEST_F(WebSocketDispatcherHostTest, AddChannelRequest) {
  int routing_id = 123;
  GURL socket_url("ws://example.com/test");
  std::vector<std::string> requested_protocols;
  requested_protocols.push_back("hello");
  url::Origin origin("http://example.com/test");
  int render_frame_id = -2;
  WebSocketHostMsg_AddChannelRequest message(
      routing_id, socket_url, requested_protocols, origin, render_frame_id);

  ASSERT_TRUE(dispatcher_host_->OnMessageReceived(message));

  ASSERT_EQ(1U, mock_hosts_.size());
  MockWebSocketHost* host = mock_hosts_[0];

  ASSERT_EQ(1U, host->received_messages_.size());
  const IPC::Message& forwarded_message = host->received_messages_[0];
  EXPECT_EQ(WebSocketHostMsg_AddChannelRequest::ID, forwarded_message.type());
  EXPECT_EQ(routing_id, forwarded_message.routing_id());
}

TEST_F(WebSocketDispatcherHostTest, SendFrameButNoHostYet) {
  int routing_id = 123;
  std::vector<char> data;
  WebSocketMsg_SendFrame message(
      routing_id, true, WEB_SOCKET_MESSAGE_TYPE_TEXT, data);

  // Expected to be ignored.
  EXPECT_TRUE(dispatcher_host_->OnMessageReceived(message));

  EXPECT_EQ(0U, mock_hosts_.size());
}

TEST_F(WebSocketDispatcherHostTest, SendFrame) {
  int routing_id = 123;

  GURL socket_url("ws://example.com/test");
  std::vector<std::string> requested_protocols;
  requested_protocols.push_back("hello");
  url::Origin origin("http://example.com/test");
  int render_frame_id = -2;
  WebSocketHostMsg_AddChannelRequest add_channel_message(
      routing_id, socket_url, requested_protocols, origin, render_frame_id);

  ASSERT_TRUE(dispatcher_host_->OnMessageReceived(add_channel_message));

  std::vector<char> data;
  WebSocketMsg_SendFrame send_frame_message(
      routing_id, true, WEB_SOCKET_MESSAGE_TYPE_TEXT, data);

  EXPECT_TRUE(dispatcher_host_->OnMessageReceived(send_frame_message));

  ASSERT_EQ(1U, mock_hosts_.size());
  MockWebSocketHost* host = mock_hosts_[0];

  ASSERT_EQ(2U, host->received_messages_.size());
  {
    const IPC::Message& forwarded_message = host->received_messages_[0];
    EXPECT_EQ(WebSocketHostMsg_AddChannelRequest::ID, forwarded_message.type());
    EXPECT_EQ(routing_id, forwarded_message.routing_id());
  }
  {
    const IPC::Message& forwarded_message = host->received_messages_[1];
    EXPECT_EQ(WebSocketMsg_SendFrame::ID, forwarded_message.type());
    EXPECT_EQ(routing_id, forwarded_message.routing_id());
  }
}

}  // namespace
}  // namespace content
