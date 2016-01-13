// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_websocket_stream.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "net/base/completion_callback.h"
#include "net/proxy/proxy_server.h"
#include "net/socket/next_proto.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_websocket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

struct SpdyWebSocketStreamEvent {
  enum EventType {
    EVENT_CREATED,
    EVENT_SENT_HEADERS,
    EVENT_RECEIVED_HEADER,
    EVENT_SENT_DATA,
    EVENT_RECEIVED_DATA,
    EVENT_CLOSE,
  };
  SpdyWebSocketStreamEvent(EventType type,
                           const SpdyHeaderBlock& headers,
                           int result,
                           const std::string& data)
      : event_type(type),
        headers(headers),
        result(result),
        data(data) {}

  EventType event_type;
  SpdyHeaderBlock headers;
  int result;
  std::string data;
};

class SpdyWebSocketStreamEventRecorder : public SpdyWebSocketStream::Delegate {
 public:
  explicit SpdyWebSocketStreamEventRecorder(const CompletionCallback& callback)
      : callback_(callback) {}
  virtual ~SpdyWebSocketStreamEventRecorder() {}

  typedef base::Callback<void(SpdyWebSocketStreamEvent*)> StreamEventCallback;

  void SetOnCreated(const StreamEventCallback& callback) {
    on_created_ = callback;
  }
  void SetOnSentHeaders(const StreamEventCallback& callback) {
    on_sent_headers_ = callback;
  }
  void SetOnReceivedHeader(const StreamEventCallback& callback) {
    on_received_header_ = callback;
  }
  void SetOnSentData(const StreamEventCallback& callback) {
    on_sent_data_ = callback;
  }
  void SetOnReceivedData(const StreamEventCallback& callback) {
    on_received_data_ = callback;
  }
  void SetOnClose(const StreamEventCallback& callback) {
    on_close_ = callback;
  }

  virtual void OnCreatedSpdyStream(int result) OVERRIDE {
    events_.push_back(
        SpdyWebSocketStreamEvent(SpdyWebSocketStreamEvent::EVENT_CREATED,
                                 SpdyHeaderBlock(),
                                 result,
                                 std::string()));
    if (!on_created_.is_null())
      on_created_.Run(&events_.back());
  }
  virtual void OnSentSpdyHeaders() OVERRIDE {
    events_.push_back(
        SpdyWebSocketStreamEvent(SpdyWebSocketStreamEvent::EVENT_SENT_HEADERS,
                                 SpdyHeaderBlock(),
                                 OK,
                                 std::string()));
    if (!on_sent_data_.is_null())
      on_sent_data_.Run(&events_.back());
  }
  virtual void OnSpdyResponseHeadersUpdated(
      const SpdyHeaderBlock& response_headers) OVERRIDE {
    events_.push_back(
        SpdyWebSocketStreamEvent(
            SpdyWebSocketStreamEvent::EVENT_RECEIVED_HEADER,
            response_headers,
            OK,
            std::string()));
    if (!on_received_header_.is_null())
      on_received_header_.Run(&events_.back());
  }
  virtual void OnSentSpdyData(size_t bytes_sent) OVERRIDE {
    events_.push_back(
        SpdyWebSocketStreamEvent(
            SpdyWebSocketStreamEvent::EVENT_SENT_DATA,
            SpdyHeaderBlock(),
            static_cast<int>(bytes_sent),
            std::string()));
    if (!on_sent_data_.is_null())
      on_sent_data_.Run(&events_.back());
  }
  virtual void OnReceivedSpdyData(scoped_ptr<SpdyBuffer> buffer) OVERRIDE {
    std::string buffer_data;
    size_t buffer_len = 0;
    if (buffer) {
      buffer_len = buffer->GetRemainingSize();
      buffer_data.append(buffer->GetRemainingData(), buffer_len);
    }
    events_.push_back(
        SpdyWebSocketStreamEvent(
            SpdyWebSocketStreamEvent::EVENT_RECEIVED_DATA,
            SpdyHeaderBlock(),
            buffer_len,
            buffer_data));
    if (!on_received_data_.is_null())
      on_received_data_.Run(&events_.back());
  }
  virtual void OnCloseSpdyStream() OVERRIDE {
    events_.push_back(
        SpdyWebSocketStreamEvent(
            SpdyWebSocketStreamEvent::EVENT_CLOSE,
            SpdyHeaderBlock(),
            OK,
            std::string()));
    if (!on_close_.is_null())
      on_close_.Run(&events_.back());
    if (!callback_.is_null())
      callback_.Run(OK);
  }

  const std::vector<SpdyWebSocketStreamEvent>& GetSeenEvents() const {
    return events_;
  }

 private:
  std::vector<SpdyWebSocketStreamEvent> events_;
  StreamEventCallback on_created_;
  StreamEventCallback on_sent_headers_;
  StreamEventCallback on_received_header_;
  StreamEventCallback on_sent_data_;
  StreamEventCallback on_received_data_;
  StreamEventCallback on_close_;
  CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SpdyWebSocketStreamEventRecorder);
};

}  // namespace

class SpdyWebSocketStreamTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<NextProto> {
 public:
  OrderedSocketData* data() { return data_.get(); }

  void DoSendHelloFrame(SpdyWebSocketStreamEvent* event) {
    // Record the actual stream_id.
    created_stream_id_ = websocket_stream_->stream_->stream_id();
    websocket_stream_->SendData(kMessageFrame, kMessageFrameLength);
  }

  void DoSendClosingFrame(SpdyWebSocketStreamEvent* event) {
    websocket_stream_->SendData(kClosingFrame, kClosingFrameLength);
  }

  void DoClose(SpdyWebSocketStreamEvent* event) {
    websocket_stream_->Close();
  }

  void DoSync(SpdyWebSocketStreamEvent* event) {
    sync_callback_.callback().Run(OK);
  }

 protected:
  SpdyWebSocketStreamTest()
      : spdy_util_(GetParam()),
        spdy_settings_id_to_set_(SETTINGS_MAX_CONCURRENT_STREAMS),
        spdy_settings_flags_to_set_(SETTINGS_FLAG_PLEASE_PERSIST),
        spdy_settings_value_to_set_(1),
        session_deps_(GetParam()),
        stream_id_(0),
        created_stream_id_(0) {}
  virtual ~SpdyWebSocketStreamTest() {}

  virtual void SetUp() {
    host_port_pair_.set_host("example.com");
    host_port_pair_.set_port(80);
    spdy_session_key_ = SpdySessionKey(host_port_pair_,
                                       ProxyServer::Direct(),
                                       PRIVACY_MODE_DISABLED);

    spdy_settings_to_send_[spdy_settings_id_to_set_] =
        SettingsFlagsAndValue(
            SETTINGS_FLAG_PERSISTED, spdy_settings_value_to_set_);
  }

  virtual void TearDown() {
    base::MessageLoop::current()->RunUntilIdle();
  }

  void Prepare(SpdyStreamId stream_id) {
    stream_id_ = stream_id;

    request_frame_.reset(spdy_util_.ConstructSpdyWebSocketSynStream(
        stream_id_,
        "/echo",
        "example.com",
        "http://example.com/wsdemo"));

    response_frame_.reset(
        spdy_util_.ConstructSpdyWebSocketSynReply(stream_id_));

    message_frame_.reset(spdy_util_.ConstructSpdyWebSocketDataFrame(
        kMessageFrame,
        kMessageFrameLength,
        stream_id_,
        false));

    closing_frame_.reset(spdy_util_.ConstructSpdyWebSocketDataFrame(
        kClosingFrame,
        kClosingFrameLength,
        stream_id_,
        false));

    closing_frame_fin_.reset(spdy_util_.ConstructSpdyWebSocketDataFrame(
        kClosingFrame,
        kClosingFrameLength,
        stream_id_,
        true));
  }

  void InitSession(MockRead* reads, size_t reads_count,
                   MockWrite* writes, size_t writes_count) {
    data_.reset(new OrderedSocketData(reads, reads_count,
                                      writes, writes_count));
    session_deps_.socket_factory->AddSocketDataProvider(data_.get());
    http_session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    session_ = CreateInsecureSpdySession(
        http_session_, spdy_session_key_, BoundNetLog());
  }

  void SendRequest() {
    scoped_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock);
    spdy_util_.SetHeader("path", "/echo", headers.get());
    spdy_util_.SetHeader("host", "example.com", headers.get());
    spdy_util_.SetHeader("version", "WebSocket/13", headers.get());
    spdy_util_.SetHeader("scheme", "ws", headers.get());
    spdy_util_.SetHeader("origin", "http://example.com/wsdemo", headers.get());
    websocket_stream_->SendRequest(headers.Pass());
  }

  SpdyWebSocketTestUtil spdy_util_;
  SpdySettingsIds spdy_settings_id_to_set_;
  SpdySettingsFlags spdy_settings_flags_to_set_;
  uint32 spdy_settings_value_to_set_;
  SettingsMap spdy_settings_to_send_;
  SpdySessionDependencies session_deps_;
  scoped_ptr<OrderedSocketData> data_;
  scoped_refptr<HttpNetworkSession> http_session_;
  base::WeakPtr<SpdySession> session_;
  scoped_ptr<SpdyWebSocketStream> websocket_stream_;
  SpdyStreamId stream_id_;
  SpdyStreamId created_stream_id_;
  scoped_ptr<SpdyFrame> request_frame_;
  scoped_ptr<SpdyFrame> response_frame_;
  scoped_ptr<SpdyFrame> message_frame_;
  scoped_ptr<SpdyFrame> closing_frame_;
  scoped_ptr<SpdyFrame> closing_frame_fin_;
  HostPortPair host_port_pair_;
  SpdySessionKey spdy_session_key_;
  TestCompletionCallback completion_callback_;
  TestCompletionCallback sync_callback_;

  static const char kMessageFrame[];
  static const char kClosingFrame[];
  static const size_t kMessageFrameLength;
  static const size_t kClosingFrameLength;
};

INSTANTIATE_TEST_CASE_P(
    NextProto,
    SpdyWebSocketStreamTest,
    testing::Values(kProtoDeprecatedSPDY2,
                    kProtoSPDY3, kProtoSPDY31, kProtoSPDY4));

// TODO(toyoshim): Replace old framing data to new one, then use HEADERS and
// data frames.
const char SpdyWebSocketStreamTest::kMessageFrame[] = "\x81\x05hello";
const char SpdyWebSocketStreamTest::kClosingFrame[] = "\x88\0";
const size_t SpdyWebSocketStreamTest::kMessageFrameLength =
    arraysize(SpdyWebSocketStreamTest::kMessageFrame) - 1;
const size_t SpdyWebSocketStreamTest::kClosingFrameLength =
    arraysize(SpdyWebSocketStreamTest::kClosingFrame) - 1;

TEST_P(SpdyWebSocketStreamTest, Basic) {
  Prepare(1);
  MockWrite writes[] = {
    CreateMockWrite(*request_frame_.get(), 1),
    CreateMockWrite(*message_frame_.get(), 3),
    CreateMockWrite(*closing_frame_.get(), 5)
  };

  MockRead reads[] = {
    CreateMockRead(*response_frame_.get(), 2),
    CreateMockRead(*message_frame_.get(), 4),
    // Skip sequence 6 to notify closing has been sent.
    CreateMockRead(*closing_frame_.get(), 7),
    MockRead(SYNCHRONOUS, 0, 8)  // EOF cause OnCloseSpdyStream event.
  };

  InitSession(reads, arraysize(reads), writes, arraysize(writes));

  SpdyWebSocketStreamEventRecorder delegate(completion_callback_.callback());
  delegate.SetOnReceivedHeader(
      base::Bind(&SpdyWebSocketStreamTest::DoSendHelloFrame,
                 base::Unretained(this)));
  delegate.SetOnReceivedData(
      base::Bind(&SpdyWebSocketStreamTest::DoSendClosingFrame,
                 base::Unretained(this)));

  websocket_stream_.reset(new SpdyWebSocketStream(session_, &delegate));

  BoundNetLog net_log;
  GURL url("ws://example.com/echo");
  ASSERT_EQ(OK, websocket_stream_->InitializeStream(url, HIGHEST, net_log));

  ASSERT_TRUE(websocket_stream_->stream_.get());

  SendRequest();

  completion_callback_.WaitForResult();

  EXPECT_EQ(stream_id_, created_stream_id_);

  websocket_stream_.reset();

  const std::vector<SpdyWebSocketStreamEvent>& events =
      delegate.GetSeenEvents();
  ASSERT_EQ(7U, events.size());

  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_SENT_HEADERS,
            events[0].event_type);
  EXPECT_EQ(OK, events[0].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_RECEIVED_HEADER,
            events[1].event_type);
  EXPECT_EQ(OK, events[1].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_SENT_DATA,
            events[2].event_type);
  EXPECT_EQ(static_cast<int>(kMessageFrameLength), events[2].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_RECEIVED_DATA,
            events[3].event_type);
  EXPECT_EQ(static_cast<int>(kMessageFrameLength), events[3].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_SENT_DATA,
            events[4].event_type);
  EXPECT_EQ(static_cast<int>(kClosingFrameLength), events[4].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_RECEIVED_DATA,
            events[5].event_type);
  EXPECT_EQ(static_cast<int>(kClosingFrameLength), events[5].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_CLOSE,
            events[6].event_type);
  EXPECT_EQ(OK, events[6].result);

  // EOF close SPDY session.
  EXPECT_FALSE(
      HasSpdySession(http_session_->spdy_session_pool(), spdy_session_key_));
  EXPECT_TRUE(data()->at_read_eof());
  EXPECT_TRUE(data()->at_write_eof());
}

// A SPDY websocket may still send it's close frame after
// recieving a close with SPDY stream FIN.
TEST_P(SpdyWebSocketStreamTest, RemoteCloseWithFin) {
  Prepare(1);
  MockWrite writes[] = {
    CreateMockWrite(*request_frame_.get(), 1),
    CreateMockWrite(*closing_frame_.get(), 4),
  };
  MockRead reads[] = {
    CreateMockRead(*response_frame_.get(), 2),
    CreateMockRead(*closing_frame_fin_.get(), 3),
    MockRead(SYNCHRONOUS, 0, 5)  // EOF cause OnCloseSpdyStream event.
  };
  InitSession(reads, arraysize(reads), writes, arraysize(writes));

  SpdyWebSocketStreamEventRecorder delegate(completion_callback_.callback());
  delegate.SetOnReceivedData(
      base::Bind(&SpdyWebSocketStreamTest::DoSendClosingFrame,
                 base::Unretained(this)));

  websocket_stream_.reset(new SpdyWebSocketStream(session_, &delegate));
  BoundNetLog net_log;
  GURL url("ws://example.com/echo");
  ASSERT_EQ(OK, websocket_stream_->InitializeStream(url, HIGHEST, net_log));

  SendRequest();
  completion_callback_.WaitForResult();
  websocket_stream_.reset();

  const std::vector<SpdyWebSocketStreamEvent>& events =
      delegate.GetSeenEvents();
  EXPECT_EQ(5U, events.size());

  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_SENT_HEADERS,
            events[0].event_type);
  EXPECT_EQ(OK, events[0].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_RECEIVED_HEADER,
            events[1].event_type);
  EXPECT_EQ(OK, events[1].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_RECEIVED_DATA,
            events[2].event_type);
  EXPECT_EQ(static_cast<int>(kClosingFrameLength), events[2].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_SENT_DATA,
            events[3].event_type);
  EXPECT_EQ(static_cast<int>(kClosingFrameLength), events[3].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_CLOSE,
            events[4].event_type);
  EXPECT_EQ(OK, events[4].result);

  // EOF closes SPDY session.
  EXPECT_FALSE(
      HasSpdySession(http_session_->spdy_session_pool(), spdy_session_key_));
  EXPECT_TRUE(data()->at_read_eof());
  EXPECT_TRUE(data()->at_write_eof());
}

TEST_P(SpdyWebSocketStreamTest, DestructionBeforeClose) {
  Prepare(1);
  MockWrite writes[] = {
    CreateMockWrite(*request_frame_.get(), 1),
    CreateMockWrite(*message_frame_.get(), 3)
  };

  MockRead reads[] = {
    CreateMockRead(*response_frame_.get(), 2),
    CreateMockRead(*message_frame_.get(), 4),
    MockRead(ASYNC, ERR_IO_PENDING, 5)
  };

  InitSession(reads, arraysize(reads), writes, arraysize(writes));

  SpdyWebSocketStreamEventRecorder delegate(completion_callback_.callback());
  delegate.SetOnReceivedHeader(
      base::Bind(&SpdyWebSocketStreamTest::DoSendHelloFrame,
                 base::Unretained(this)));
  delegate.SetOnReceivedData(
      base::Bind(&SpdyWebSocketStreamTest::DoSync,
                 base::Unretained(this)));

  websocket_stream_.reset(new SpdyWebSocketStream(session_, &delegate));

  BoundNetLog net_log;
  GURL url("ws://example.com/echo");
  ASSERT_EQ(OK, websocket_stream_->InitializeStream(url, HIGHEST, net_log));

  SendRequest();

  sync_callback_.WaitForResult();

  // WebSocketStream destruction remove its SPDY stream from the session.
  EXPECT_TRUE(session_->IsStreamActive(stream_id_));
  websocket_stream_.reset();
  EXPECT_FALSE(session_->IsStreamActive(stream_id_));

  const std::vector<SpdyWebSocketStreamEvent>& events =
      delegate.GetSeenEvents();
  ASSERT_GE(4U, events.size());

  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_SENT_HEADERS,
            events[0].event_type);
  EXPECT_EQ(OK, events[0].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_RECEIVED_HEADER,
            events[1].event_type);
  EXPECT_EQ(OK, events[1].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_SENT_DATA,
            events[2].event_type);
  EXPECT_EQ(static_cast<int>(kMessageFrameLength), events[2].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_RECEIVED_DATA,
            events[3].event_type);
  EXPECT_EQ(static_cast<int>(kMessageFrameLength), events[3].result);

  EXPECT_TRUE(
      HasSpdySession(http_session_->spdy_session_pool(), spdy_session_key_));
  EXPECT_TRUE(data()->at_read_eof());
  EXPECT_TRUE(data()->at_write_eof());
}

TEST_P(SpdyWebSocketStreamTest, DestructionAfterExplicitClose) {
  Prepare(1);
  MockWrite writes[] = {
    CreateMockWrite(*request_frame_.get(), 1),
    CreateMockWrite(*message_frame_.get(), 3),
    CreateMockWrite(*closing_frame_.get(), 5)
  };

  MockRead reads[] = {
    CreateMockRead(*response_frame_.get(), 2),
    CreateMockRead(*message_frame_.get(), 4),
    MockRead(ASYNC, ERR_IO_PENDING, 6)
  };

  InitSession(reads, arraysize(reads), writes, arraysize(writes));

  SpdyWebSocketStreamEventRecorder delegate(completion_callback_.callback());
  delegate.SetOnReceivedHeader(
      base::Bind(&SpdyWebSocketStreamTest::DoSendHelloFrame,
                 base::Unretained(this)));
  delegate.SetOnReceivedData(
      base::Bind(&SpdyWebSocketStreamTest::DoClose,
                 base::Unretained(this)));

  websocket_stream_.reset(new SpdyWebSocketStream(session_, &delegate));

  BoundNetLog net_log;
  GURL url("ws://example.com/echo");
  ASSERT_EQ(OK, websocket_stream_->InitializeStream(url, HIGHEST, net_log));

  SendRequest();

  completion_callback_.WaitForResult();

  // SPDY stream has already been removed from the session by Close().
  EXPECT_FALSE(session_->IsStreamActive(stream_id_));
  websocket_stream_.reset();

  const std::vector<SpdyWebSocketStreamEvent>& events =
      delegate.GetSeenEvents();
  ASSERT_EQ(5U, events.size());

  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_SENT_HEADERS,
            events[0].event_type);
  EXPECT_EQ(OK, events[0].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_RECEIVED_HEADER,
            events[1].event_type);
  EXPECT_EQ(OK, events[1].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_SENT_DATA,
            events[2].event_type);
  EXPECT_EQ(static_cast<int>(kMessageFrameLength), events[2].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_RECEIVED_DATA,
            events[3].event_type);
  EXPECT_EQ(static_cast<int>(kMessageFrameLength), events[3].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_CLOSE, events[4].event_type);

  EXPECT_TRUE(
      HasSpdySession(http_session_->spdy_session_pool(), spdy_session_key_));
}

TEST_P(SpdyWebSocketStreamTest, IOPending) {
  Prepare(1);
  scoped_ptr<SpdyFrame> settings_frame(
      spdy_util_.ConstructSpdySettings(spdy_settings_to_send_));
  scoped_ptr<SpdyFrame> settings_ack(spdy_util_.ConstructSpdySettingsAck());
  MockWrite writes[] = {
    CreateMockWrite(*settings_ack, 1),
    CreateMockWrite(*request_frame_.get(), 2),
    CreateMockWrite(*message_frame_.get(), 4),
    CreateMockWrite(*closing_frame_.get(), 6)
  };

  MockRead reads[] = {
    CreateMockRead(*settings_frame.get(), 0),
    CreateMockRead(*response_frame_.get(), 3),
    CreateMockRead(*message_frame_.get(), 5),
    CreateMockRead(*closing_frame_.get(), 7),
    MockRead(SYNCHRONOUS, 0, 8)  // EOF cause OnCloseSpdyStream event.
  };

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);
  http_session_ =
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_);

  session_ = CreateInsecureSpdySession(
      http_session_, spdy_session_key_, BoundNetLog());

  // Create a dummy WebSocketStream which cause ERR_IO_PENDING to another
  // WebSocketStream under test.
  SpdyWebSocketStreamEventRecorder block_delegate((CompletionCallback()));

  scoped_ptr<SpdyWebSocketStream> block_stream(
      new SpdyWebSocketStream(session_, &block_delegate));
  BoundNetLog block_net_log;
  GURL block_url("ws://example.com/block");
  ASSERT_EQ(OK,
            block_stream->InitializeStream(block_url, HIGHEST, block_net_log));

  data.RunFor(1);

  // Create a WebSocketStream under test.
  SpdyWebSocketStreamEventRecorder delegate(completion_callback_.callback());
  delegate.SetOnCreated(
      base::Bind(&SpdyWebSocketStreamTest::DoSync,
                 base::Unretained(this)));
  delegate.SetOnReceivedHeader(
      base::Bind(&SpdyWebSocketStreamTest::DoSendHelloFrame,
                 base::Unretained(this)));
  delegate.SetOnReceivedData(
      base::Bind(&SpdyWebSocketStreamTest::DoSendClosingFrame,
                 base::Unretained(this)));

  websocket_stream_.reset(new SpdyWebSocketStream(session_, &delegate));
  BoundNetLog net_log;
  GURL url("ws://example.com/echo");
  ASSERT_EQ(ERR_IO_PENDING, websocket_stream_->InitializeStream(
      url, HIGHEST, net_log));

  // Delete the fist stream to allow create the second stream.
  block_stream.reset();
  ASSERT_EQ(OK, sync_callback_.WaitForResult());

  SendRequest();

  data.RunFor(8);
  completion_callback_.WaitForResult();

  websocket_stream_.reset();

  const std::vector<SpdyWebSocketStreamEvent>& block_events =
      block_delegate.GetSeenEvents();
  ASSERT_EQ(0U, block_events.size());

  const std::vector<SpdyWebSocketStreamEvent>& events =
      delegate.GetSeenEvents();
  ASSERT_EQ(8U, events.size());
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_CREATED,
            events[0].event_type);
  EXPECT_EQ(0, events[0].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_SENT_HEADERS,
            events[1].event_type);
  EXPECT_EQ(OK, events[1].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_RECEIVED_HEADER,
            events[2].event_type);
  EXPECT_EQ(OK, events[2].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_SENT_DATA,
            events[3].event_type);
  EXPECT_EQ(static_cast<int>(kMessageFrameLength), events[3].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_RECEIVED_DATA,
            events[4].event_type);
  EXPECT_EQ(static_cast<int>(kMessageFrameLength), events[4].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_SENT_DATA,
            events[5].event_type);
  EXPECT_EQ(static_cast<int>(kClosingFrameLength), events[5].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_RECEIVED_DATA,
            events[6].event_type);
  EXPECT_EQ(static_cast<int>(kClosingFrameLength), events[6].result);
  EXPECT_EQ(SpdyWebSocketStreamEvent::EVENT_CLOSE,
            events[7].event_type);
  EXPECT_EQ(OK, events[7].result);

  // EOF close SPDY session.
  EXPECT_FALSE(
      HasSpdySession(http_session_->spdy_session_pool(), spdy_session_key_));
  EXPECT_TRUE(data.at_read_eof());
  EXPECT_TRUE(data.at_write_eof());
}

}  // namespace net
