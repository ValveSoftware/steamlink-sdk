// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/websocket_dispatcher_host.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/renderer_host/websocket_host.h"
#include "content/common/websocket.h"
#include "content/common/websocket_messages.h"
#include "ipc/ipc_message.h"
#include "net/websockets/websocket_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

// This number is unlikely to occur by chance.
static const int kMagicRenderProcessId = 506116062;

class WebSocketDispatcherHostTest;

// A mock of WebsocketHost which records received messages.
class MockWebSocketHost : public WebSocketHost {
 public:
  MockWebSocketHost(int routing_id,
                    WebSocketDispatcherHost* dispatcher,
                    net::URLRequestContext* url_request_context,
                    base::TimeDelta delay,
                    WebSocketDispatcherHostTest* owner);

  ~MockWebSocketHost() override {}

  bool OnMessageReceived(const IPC::Message& message) override {
    received_messages_.push_back(message);
    switch (message.type()) {
      case WebSocketMsg_DropChannel::ID:
        // Needed for PerRendererThrottlingFailedHandshakes, because without
        // calling WebSocketHost::OnMessageReceived() (and thus
        // WebSocketHost::OnDropChannel()), the connection stays pending and
        // we cannot test per-renderer throttling with failed connections.
        return WebSocketHost::OnMessageReceived(message);

      default:
        return true;
    }
  }

  void GoAway() override;

  std::vector<IPC::Message> received_messages_;
  base::WeakPtr<WebSocketDispatcherHostTest> owner_;
  base::TimeDelta delay_;
};

class TestingWebSocketDispatcherHost : public WebSocketDispatcherHost {
 public:
  TestingWebSocketDispatcherHost(
      int process_id,
      const GetRequestContextCallback& get_context_callback,
      const WebSocketHostFactory& websocket_host_factory)
      : WebSocketDispatcherHost(process_id,
                                get_context_callback,
                                websocket_host_factory) {}

  // This is needed because BrowserMessageFilter::Send() tries post the task to
  // the IO thread, which doesn't exist in the context of these tests.
  bool Send(IPC::Message* message) override {
    delete message;
    return true;
  }

  using WebSocketDispatcherHost::num_pending_connections;
  using WebSocketDispatcherHost::num_failed_connections;
  using WebSocketDispatcherHost::num_succeeded_connections;

 private:
  ~TestingWebSocketDispatcherHost() override {}
};

class WebSocketDispatcherHostTest : public ::testing::Test {
 public:
  WebSocketDispatcherHostTest()
      : next_routing_id_(123),
        weak_ptr_factory_(this) {
    dispatcher_host_ = new TestingWebSocketDispatcherHost(
        kMagicRenderProcessId,
        base::Bind(&WebSocketDispatcherHostTest::OnGetRequestContext,
                   base::Unretained(this)),
        base::Bind(&WebSocketDispatcherHostTest::CreateWebSocketHost,
                   base::Unretained(this)));
  }

  ~WebSocketDispatcherHostTest() override {
    // We need to invalidate the issued WeakPtrs at the beginning of the
    // destructor in order not to access destructed member variables.
    weak_ptr_factory_.InvalidateWeakPtrs();
  }

  void GoAway(int routing_id) {
    gone_hosts_.push_back(routing_id);
  }

  base::WeakPtr<WebSocketDispatcherHostTest> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  // Adds |n| connections. Returns true if succeeded.
  bool AddMultipleChannels(int number_of_channels) {
    for (int i = 0; i < number_of_channels; ++i) {
      int routing_id = next_routing_id_++;

      WebSocketHostMsg_AddChannelRequest_Params params;
      params.socket_url = GURL("ws://example.com/test");
      params.origin = url::Origin(GURL("http://example.com"));
      params.first_party_for_cookies = GURL("http://example.com");
      params.user_agent_override = "";
      params.render_frame_id = -3;

      WebSocketHostMsg_AddChannelRequest message(routing_id, params);
      if (!dispatcher_host_->OnMessageReceived(message))
        return false;
    }

    return true;
  }

  // Adds and cancels |n| connections. Returns true if succeeded.
  bool AddAndCancelMultipleChannels(int number_of_channels) {
    for (int i = 0; i < number_of_channels; ++i) {
      int routing_id = next_routing_id_++;

      WebSocketHostMsg_AddChannelRequest_Params params;
      params.socket_url = GURL("ws://example.com/test");
      params.origin = url::Origin(GURL("http://example.com"));
      params.first_party_for_cookies = GURL("http://example.com");
      params.user_agent_override = "";
      params.render_frame_id = -3;

      WebSocketHostMsg_AddChannelRequest messageAddChannelRequest(
          routing_id, params);
      if (!dispatcher_host_->OnMessageReceived(messageAddChannelRequest))
        return false;

      WebSocketMsg_DropChannel messageDropChannel(
          routing_id, false, net::kWebSocketErrorAbnormalClosure, "");
      if (!dispatcher_host_->OnMessageReceived(messageDropChannel))
        return false;
    }

    return true;
  }

  scoped_refptr<TestingWebSocketDispatcherHost> dispatcher_host_;

  // Stores allocated MockWebSocketHost instances. Doesn't take ownership of
  // them.
  std::vector<MockWebSocketHost*> mock_hosts_;
  std::vector<int> gone_hosts_;

 private:
  net::URLRequestContext* OnGetRequestContext() {
    return NULL;
  }

  WebSocketHost* CreateWebSocketHost(int routing_id, base::TimeDelta delay) {
    MockWebSocketHost* host = new MockWebSocketHost(
        routing_id, dispatcher_host_.get(), NULL, delay, this);
    mock_hosts_.push_back(host);
    return host;
  }

  base::MessageLoop message_loop_;

  int next_routing_id_;

  base::WeakPtrFactory<WebSocketDispatcherHostTest> weak_ptr_factory_;
};

MockWebSocketHost::MockWebSocketHost(
    int routing_id,
    WebSocketDispatcherHost* dispatcher,
    net::URLRequestContext* url_request_context,
    base::TimeDelta delay,
    WebSocketDispatcherHostTest* owner)
    : WebSocketHost(routing_id, dispatcher, url_request_context, delay),
      owner_(owner->GetWeakPtr()),
      delay_(delay) {}

void MockWebSocketHost::GoAway() {
  if (owner_)
    owner_->GoAway(routing_id());
}

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
  std::vector<std::string> requested_protocols;
  requested_protocols.push_back("hello");

  WebSocketHostMsg_AddChannelRequest_Params params;
  params.socket_url = GURL("ws://example.com/test");
  params.requested_protocols = requested_protocols;
  params.origin = url::Origin(GURL("http://example.com"));
  params.first_party_for_cookies = GURL("http://example.com");
  params.user_agent_override = "";
  params.render_frame_id = -2;

  WebSocketHostMsg_AddChannelRequest message(routing_id, params);

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

  std::vector<std::string> requested_protocols;
  requested_protocols.push_back("hello");

  WebSocketHostMsg_AddChannelRequest_Params params;
  params.socket_url = GURL("ws://example.com/test");
  params.requested_protocols = requested_protocols;
  params.origin = url::Origin(GURL("http://example.com"));
  params.first_party_for_cookies = GURL("http://example.com");
  params.user_agent_override = "";
  params.render_frame_id = -2;

  WebSocketHostMsg_AddChannelRequest add_channel_message(routing_id, params);

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

TEST_F(WebSocketDispatcherHostTest, Destruct) {
  WebSocketHostMsg_AddChannelRequest_Params params1;
  params1.socket_url = GURL("ws://example.com/test");
  params1.requested_protocols = std::vector<std::string>();
  params1.origin = url::Origin(GURL("http://example.com"));
  params1.first_party_for_cookies = GURL("http://example.com");
  params1.user_agent_override = "";
  params1.render_frame_id = -1;

  WebSocketHostMsg_AddChannelRequest message1(123, params1);

  WebSocketHostMsg_AddChannelRequest_Params params2;
  params2.socket_url = GURL("ws://example.com/test2");
  params2.requested_protocols = std::vector<std::string>();
  params2.origin = url::Origin(GURL("http://example.com"));
  params2.first_party_for_cookies = GURL("http://example.com");
  params2.user_agent_override = "";
  params2.render_frame_id = -1;

  WebSocketHostMsg_AddChannelRequest message2(456, params2);

  ASSERT_TRUE(dispatcher_host_->OnMessageReceived(message1));
  ASSERT_TRUE(dispatcher_host_->OnMessageReceived(message2));

  ASSERT_EQ(2u, mock_hosts_.size());

  mock_hosts_.clear();
  dispatcher_host_ = NULL;

  ASSERT_EQ(2u, gone_hosts_.size());
  // The gone_hosts_ ordering is not predictable because it depends on the
  // hash_map ordering.
  std::sort(gone_hosts_.begin(), gone_hosts_.end());
  EXPECT_EQ(123, gone_hosts_[0]);
  EXPECT_EQ(456, gone_hosts_[1]);
}

TEST_F(WebSocketDispatcherHostTest, DelayFor4thPendingConnectionIsZero) {
  ASSERT_TRUE(AddMultipleChannels(4));

  EXPECT_EQ(4, dispatcher_host_->num_pending_connections());
  EXPECT_EQ(0, dispatcher_host_->num_failed_connections());
  EXPECT_EQ(0, dispatcher_host_->num_succeeded_connections());

  ASSERT_EQ(4U, mock_hosts_.size());
  EXPECT_EQ(base::TimeDelta(), mock_hosts_[3]->delay_);
}

TEST_F(WebSocketDispatcherHostTest, DelayFor8thPendingConnectionIsNonZero) {
  ASSERT_TRUE(AddMultipleChannels(8));

  EXPECT_EQ(8, dispatcher_host_->num_pending_connections());
  EXPECT_EQ(0, dispatcher_host_->num_failed_connections());
  EXPECT_EQ(0, dispatcher_host_->num_succeeded_connections());

  ASSERT_EQ(8U, mock_hosts_.size());
  EXPECT_LT(base::TimeDelta(), mock_hosts_[7]->delay_);
}

TEST_F(WebSocketDispatcherHostTest, DelayFor17thPendingConnection) {
  ASSERT_TRUE(AddMultipleChannels(17));

  EXPECT_EQ(17, dispatcher_host_->num_pending_connections());
  EXPECT_EQ(0, dispatcher_host_->num_failed_connections());
  EXPECT_EQ(0, dispatcher_host_->num_succeeded_connections());

  ASSERT_EQ(17U, mock_hosts_.size());
  EXPECT_LE(base::TimeDelta::FromMilliseconds(1000), mock_hosts_[16]->delay_);
  EXPECT_GE(base::TimeDelta::FromMilliseconds(5000), mock_hosts_[16]->delay_);
}

// The 256th connection is rejected by per-renderer WebSocket throttling.
// This is not counted as a failure.
TEST_F(WebSocketDispatcherHostTest, Rejects256thPendingConnection) {
  ASSERT_TRUE(AddMultipleChannels(256));

  EXPECT_EQ(255, dispatcher_host_->num_pending_connections());
  EXPECT_EQ(0, dispatcher_host_->num_failed_connections());
  EXPECT_EQ(0, dispatcher_host_->num_succeeded_connections());

  ASSERT_EQ(255U, mock_hosts_.size());
}

TEST_F(WebSocketDispatcherHostTest, DelayIsZeroAfter3FailedConnections) {
  ASSERT_TRUE(AddAndCancelMultipleChannels(3));

  EXPECT_EQ(0, dispatcher_host_->num_pending_connections());
  EXPECT_EQ(3, dispatcher_host_->num_failed_connections());
  EXPECT_EQ(0, dispatcher_host_->num_succeeded_connections());

  ASSERT_TRUE(AddMultipleChannels(1));

  ASSERT_EQ(4U, mock_hosts_.size());
  EXPECT_EQ(base::TimeDelta(), mock_hosts_[3]->delay_);
}

TEST_F(WebSocketDispatcherHostTest, DelayIsNonZeroAfter7FailedConnections) {
  ASSERT_TRUE(AddAndCancelMultipleChannels(7));

  EXPECT_EQ(0, dispatcher_host_->num_pending_connections());
  EXPECT_EQ(7, dispatcher_host_->num_failed_connections());
  EXPECT_EQ(0, dispatcher_host_->num_succeeded_connections());

  ASSERT_TRUE(AddMultipleChannels(1));

  ASSERT_EQ(8U, mock_hosts_.size());
  EXPECT_LT(base::TimeDelta(), mock_hosts_[7]->delay_);
}

TEST_F(WebSocketDispatcherHostTest, DelayAfter16FailedConnections) {
  ASSERT_TRUE(AddAndCancelMultipleChannels(16));

  EXPECT_EQ(0, dispatcher_host_->num_pending_connections());
  EXPECT_EQ(16, dispatcher_host_->num_failed_connections());
  EXPECT_EQ(0, dispatcher_host_->num_succeeded_connections());

  ASSERT_TRUE(AddMultipleChannels(1));

  ASSERT_EQ(17U, mock_hosts_.size());
  EXPECT_LE(base::TimeDelta::FromMilliseconds(1000), mock_hosts_[16]->delay_);
  EXPECT_GE(base::TimeDelta::FromMilliseconds(5000), mock_hosts_[16]->delay_);
}

TEST_F(WebSocketDispatcherHostTest, NotRejectedAfter255FailedConnections) {
  ASSERT_TRUE(AddAndCancelMultipleChannels(255));

  EXPECT_EQ(0, dispatcher_host_->num_pending_connections());
  EXPECT_EQ(255, dispatcher_host_->num_failed_connections());
  EXPECT_EQ(0, dispatcher_host_->num_succeeded_connections());

  ASSERT_TRUE(AddMultipleChannels(1));

  EXPECT_EQ(1, dispatcher_host_->num_pending_connections());
  EXPECT_EQ(255, dispatcher_host_->num_failed_connections());
  EXPECT_EQ(0, dispatcher_host_->num_succeeded_connections());
}

// This is a regression test for https://crrev.com/998173003/.
TEST_F(WebSocketDispatcherHostTest, InvalidScheme) {
  int routing_id = 123;

  std::vector<std::string> requested_protocols;
  requested_protocols.push_back("hello");

  WebSocketHostMsg_AddChannelRequest_Params params;
  params.socket_url = GURL("http://example.com/test");
  params.requested_protocols = requested_protocols;
  params.origin = url::Origin(GURL("http://example.com"));
  params.first_party_for_cookies = GURL("http://example.com");
  params.user_agent_override = "";
  params.render_frame_id = -2;

  WebSocketHostMsg_AddChannelRequest message(routing_id, params);

  ASSERT_TRUE(dispatcher_host_->OnMessageReceived(message));

  ASSERT_EQ(1U, mock_hosts_.size());
  MockWebSocketHost* host = mock_hosts_[0];

  // Tests that WebSocketHost::OnMessageReceived() doesn't cause a crash and
  // the connection with an invalid scheme fails here.
  // We call WebSocketHost::OnMessageReceived() here explicitly because
  // MockWebSocketHost does not call WebSocketHost::OnMessageReceived() for
  // WebSocketHostMsg_AddChannelRequest.
  host->WebSocketHost::OnMessageReceived(message);

  EXPECT_EQ(0, dispatcher_host_->num_pending_connections());
  EXPECT_EQ(1, dispatcher_host_->num_failed_connections());
  EXPECT_EQ(0, dispatcher_host_->num_succeeded_connections());
}

}  // namespace
}  // namespace content
