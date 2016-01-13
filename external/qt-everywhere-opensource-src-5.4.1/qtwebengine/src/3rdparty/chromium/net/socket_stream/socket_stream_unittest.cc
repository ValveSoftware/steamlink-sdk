// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket_stream/socket_stream.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/auth.h"
#include "net/base/net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::ASCIIToUTF16;

namespace net {

namespace {

struct SocketStreamEvent {
  enum EventType {
    EVENT_START_OPEN_CONNECTION, EVENT_CONNECTED, EVENT_SENT_DATA,
    EVENT_RECEIVED_DATA, EVENT_CLOSE, EVENT_AUTH_REQUIRED, EVENT_ERROR,
  };

  SocketStreamEvent(EventType type,
                    SocketStream* socket_stream,
                    int num,
                    const std::string& str,
                    AuthChallengeInfo* auth_challenge_info,
                    int error)
      : event_type(type), socket(socket_stream), number(num), data(str),
        auth_info(auth_challenge_info), error_code(error) {}

  EventType event_type;
  SocketStream* socket;
  int number;
  std::string data;
  scoped_refptr<AuthChallengeInfo> auth_info;
  int error_code;
};

class SocketStreamEventRecorder : public SocketStream::Delegate {
 public:
  // |callback| will be run when the OnClose() or OnError() method is called.
  // For OnClose(), |callback| is called with OK. For OnError(), it's called
  // with the error code.
  explicit SocketStreamEventRecorder(const CompletionCallback& callback)
      : callback_(callback) {}
  virtual ~SocketStreamEventRecorder() {}

  void SetOnStartOpenConnection(
      const base::Callback<int(SocketStreamEvent*)>& callback) {
    on_start_open_connection_ = callback;
  }
  void SetOnConnected(
      const base::Callback<void(SocketStreamEvent*)>& callback) {
    on_connected_ = callback;
  }
  void SetOnSentData(
      const base::Callback<void(SocketStreamEvent*)>& callback) {
    on_sent_data_ = callback;
  }
  void SetOnReceivedData(
      const base::Callback<void(SocketStreamEvent*)>& callback) {
    on_received_data_ = callback;
  }
  void SetOnClose(const base::Callback<void(SocketStreamEvent*)>& callback) {
    on_close_ = callback;
  }
  void SetOnAuthRequired(
      const base::Callback<void(SocketStreamEvent*)>& callback) {
    on_auth_required_ = callback;
  }
  void SetOnError(const base::Callback<void(SocketStreamEvent*)>& callback) {
    on_error_ = callback;
  }

  virtual int OnStartOpenConnection(
      SocketStream* socket,
      const CompletionCallback& callback) OVERRIDE {
    connection_callback_ = callback;
    events_.push_back(
        SocketStreamEvent(SocketStreamEvent::EVENT_START_OPEN_CONNECTION,
                          socket, 0, std::string(), NULL, OK));
    if (!on_start_open_connection_.is_null())
      return on_start_open_connection_.Run(&events_.back());
    return OK;
  }
  virtual void OnConnected(SocketStream* socket,
                           int num_pending_send_allowed) OVERRIDE {
    events_.push_back(
        SocketStreamEvent(SocketStreamEvent::EVENT_CONNECTED,
                          socket, num_pending_send_allowed, std::string(),
                          NULL, OK));
    if (!on_connected_.is_null())
      on_connected_.Run(&events_.back());
  }
  virtual void OnSentData(SocketStream* socket,
                          int amount_sent) OVERRIDE {
    events_.push_back(
        SocketStreamEvent(SocketStreamEvent::EVENT_SENT_DATA, socket,
                          amount_sent, std::string(), NULL, OK));
    if (!on_sent_data_.is_null())
      on_sent_data_.Run(&events_.back());
  }
  virtual void OnReceivedData(SocketStream* socket,
                              const char* data, int len) OVERRIDE {
    events_.push_back(
        SocketStreamEvent(SocketStreamEvent::EVENT_RECEIVED_DATA, socket, len,
                          std::string(data, len), NULL, OK));
    if (!on_received_data_.is_null())
      on_received_data_.Run(&events_.back());
  }
  virtual void OnClose(SocketStream* socket) OVERRIDE {
    events_.push_back(
        SocketStreamEvent(SocketStreamEvent::EVENT_CLOSE, socket, 0,
                          std::string(), NULL, OK));
    if (!on_close_.is_null())
      on_close_.Run(&events_.back());
    if (!callback_.is_null())
      callback_.Run(OK);
  }
  virtual void OnAuthRequired(SocketStream* socket,
                              AuthChallengeInfo* auth_info) OVERRIDE {
    events_.push_back(
        SocketStreamEvent(SocketStreamEvent::EVENT_AUTH_REQUIRED, socket, 0,
                          std::string(), auth_info, OK));
    if (!on_auth_required_.is_null())
      on_auth_required_.Run(&events_.back());
  }
  virtual void OnError(const SocketStream* socket, int error) OVERRIDE {
    events_.push_back(
        SocketStreamEvent(SocketStreamEvent::EVENT_ERROR, NULL, 0,
                          std::string(), NULL, error));
    if (!on_error_.is_null())
      on_error_.Run(&events_.back());
    if (!callback_.is_null())
      callback_.Run(error);
  }

  void DoClose(SocketStreamEvent* event) {
    event->socket->Close();
  }
  void DoRestartWithAuth(SocketStreamEvent* event) {
    VLOG(1) << "RestartWithAuth username=" << credentials_.username()
            << " password=" << credentials_.password();
    event->socket->RestartWithAuth(credentials_);
  }
  void SetAuthInfo(const AuthCredentials& credentials) {
    credentials_ = credentials;
  }
  // Wakes up the SocketStream waiting for completion of OnStartOpenConnection()
  // of its delegate.
  void CompleteConnection(int result) {
    connection_callback_.Run(result);
  }

  const std::vector<SocketStreamEvent>& GetSeenEvents() const {
    return events_;
  }

 private:
  std::vector<SocketStreamEvent> events_;
  base::Callback<int(SocketStreamEvent*)> on_start_open_connection_;
  base::Callback<void(SocketStreamEvent*)> on_connected_;
  base::Callback<void(SocketStreamEvent*)> on_sent_data_;
  base::Callback<void(SocketStreamEvent*)> on_received_data_;
  base::Callback<void(SocketStreamEvent*)> on_close_;
  base::Callback<void(SocketStreamEvent*)> on_auth_required_;
  base::Callback<void(SocketStreamEvent*)> on_error_;
  const CompletionCallback callback_;
  CompletionCallback connection_callback_;
  AuthCredentials credentials_;

  DISALLOW_COPY_AND_ASSIGN(SocketStreamEventRecorder);
};

// This is used for the test OnErrorDetachDelegate.
class SelfDeletingDelegate : public SocketStream::Delegate {
 public:
  // |callback| must cause the test message loop to exit when called.
  explicit SelfDeletingDelegate(const CompletionCallback& callback)
      : socket_stream_(), callback_(callback) {}

  virtual ~SelfDeletingDelegate() {}

  // Call DetachDelegate(), delete |this|, then run the callback.
  virtual void OnError(const SocketStream* socket, int error) OVERRIDE {
    // callback_ will be deleted when we delete |this|, so copy it to call it
    // afterwards.
    CompletionCallback callback = callback_;
    socket_stream_->DetachDelegate();
    delete this;
    callback.Run(OK);
  }

  // This can't be passed in the constructor because this object needs to be
  // created before SocketStream.
  void set_socket_stream(const scoped_refptr<SocketStream>& socket_stream) {
    socket_stream_ = socket_stream;
    EXPECT_EQ(socket_stream_->delegate(), this);
  }

  virtual void OnConnected(SocketStream* socket, int max_pending_send_allowed)
      OVERRIDE {
    ADD_FAILURE() << "OnConnected() should not be called";
  }
  virtual void OnSentData(SocketStream* socket, int amount_sent) OVERRIDE {
    ADD_FAILURE() << "OnSentData() should not be called";
  }
  virtual void OnReceivedData(SocketStream* socket, const char* data, int len)
      OVERRIDE {
    ADD_FAILURE() << "OnReceivedData() should not be called";
  }
  virtual void OnClose(SocketStream* socket) OVERRIDE {
    ADD_FAILURE() << "OnClose() should not be called";
  }

 private:
  scoped_refptr<SocketStream> socket_stream_;
  const CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SelfDeletingDelegate);
};

class TestURLRequestContextWithProxy : public TestURLRequestContext {
 public:
  explicit TestURLRequestContextWithProxy(const std::string& proxy)
      : TestURLRequestContext(true) {
    context_storage_.set_proxy_service(ProxyService::CreateFixed(proxy));
    Init();
  }
  virtual ~TestURLRequestContextWithProxy() {}
};

class TestSocketStreamNetworkDelegate : public TestNetworkDelegate {
 public:
  TestSocketStreamNetworkDelegate()
      : before_connect_result_(OK) {}
  virtual ~TestSocketStreamNetworkDelegate() {}

  virtual int OnBeforeSocketStreamConnect(
      SocketStream* stream,
      const CompletionCallback& callback) OVERRIDE {
    return before_connect_result_;
  }

  void SetBeforeConnectResult(int result) {
    before_connect_result_ = result;
  }

 private:
  int before_connect_result_;
};

}  // namespace

class SocketStreamTest : public PlatformTest {
 public:
  virtual ~SocketStreamTest() {}
  virtual void SetUp() {
    mock_socket_factory_.reset();
    handshake_request_ = kWebSocketHandshakeRequest;
    handshake_response_ = kWebSocketHandshakeResponse;
  }
  virtual void TearDown() {
    mock_socket_factory_.reset();
  }

  virtual void SetWebSocketHandshakeMessage(
      const char* request, const char* response) {
    handshake_request_ = request;
    handshake_response_ = response;
  }
  virtual void AddWebSocketMessage(const std::string& message) {
    messages_.push_back(message);
  }

  virtual MockClientSocketFactory* GetMockClientSocketFactory() {
    mock_socket_factory_.reset(new MockClientSocketFactory);
    return mock_socket_factory_.get();
  }

  // Functions for SocketStreamEventRecorder to handle calls to the
  // SocketStream::Delegate methods from the SocketStream.

  virtual void DoSendWebSocketHandshake(SocketStreamEvent* event) {
    event->socket->SendData(
        handshake_request_.data(), handshake_request_.size());
  }

  virtual void DoCloseFlushPendingWriteTest(SocketStreamEvent* event) {
    // handshake response received.
    for (size_t i = 0; i < messages_.size(); i++) {
      std::vector<char> frame;
      frame.push_back('\0');
      frame.insert(frame.end(), messages_[i].begin(), messages_[i].end());
      frame.push_back('\xff');
      EXPECT_TRUE(event->socket->SendData(&frame[0], frame.size()));
    }
    // Actual StreamSocket close must happen after all frames queued by
    // SendData above are sent out.
    event->socket->Close();
  }

  virtual void DoCloseFlushPendingWriteTestWithSetContextNull(
      SocketStreamEvent* event) {
    event->socket->DetachContext();
    // handshake response received.
    for (size_t i = 0; i < messages_.size(); i++) {
      std::vector<char> frame;
      frame.push_back('\0');
      frame.insert(frame.end(), messages_[i].begin(), messages_[i].end());
      frame.push_back('\xff');
      EXPECT_TRUE(event->socket->SendData(&frame[0], frame.size()));
    }
    // Actual StreamSocket close must happen after all frames queued by
    // SendData above are sent out.
    event->socket->Close();
  }

  virtual void DoFailByTooBigDataAndClose(SocketStreamEvent* event) {
    std::string frame(event->number + 1, 0x00);
    VLOG(1) << event->number;
    EXPECT_FALSE(event->socket->SendData(&frame[0], frame.size()));
    event->socket->Close();
  }

  virtual int DoSwitchToSpdyTest(SocketStreamEvent* event) {
    return ERR_PROTOCOL_SWITCHED;
  }

  // Notifies |io_test_callback_| of that this method is called, and keeps the
  // SocketStream waiting.
  virtual int DoIOPending(SocketStreamEvent* event) {
    io_test_callback_.callback().Run(OK);
    return ERR_IO_PENDING;
  }

  static const char kWebSocketHandshakeRequest[];
  static const char kWebSocketHandshakeResponse[];

 protected:
  TestCompletionCallback io_test_callback_;

 private:
  std::string handshake_request_;
  std::string handshake_response_;
  std::vector<std::string> messages_;

  scoped_ptr<MockClientSocketFactory> mock_socket_factory_;
};

const char SocketStreamTest::kWebSocketHandshakeRequest[] =
    "GET /demo HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
    "Sec-WebSocket-Protocol: sample\r\n"
    "Upgrade: WebSocket\r\n"
    "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
    "Origin: http://example.com\r\n"
    "\r\n"
    "^n:ds[4U";

const char SocketStreamTest::kWebSocketHandshakeResponse[] =
    "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
    "Upgrade: WebSocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Origin: http://example.com\r\n"
    "Sec-WebSocket-Location: ws://example.com/demo\r\n"
    "Sec-WebSocket-Protocol: sample\r\n"
    "\r\n"
    "8jKS'y:G*Co,Wxa-";

TEST_F(SocketStreamTest, CloseFlushPendingWrite) {
  TestCompletionCallback test_callback;

  scoped_ptr<SocketStreamEventRecorder> delegate(
      new SocketStreamEventRecorder(test_callback.callback()));
  delegate->SetOnConnected(base::Bind(
      &SocketStreamTest::DoSendWebSocketHandshake, base::Unretained(this)));
  delegate->SetOnReceivedData(base::Bind(
      &SocketStreamTest::DoCloseFlushPendingWriteTest,
      base::Unretained(this)));

  TestURLRequestContext context;

  scoped_refptr<SocketStream> socket_stream(
      new SocketStream(GURL("ws://example.com/demo"), delegate.get(),
                       &context, NULL));

  MockWrite data_writes[] = {
    MockWrite(SocketStreamTest::kWebSocketHandshakeRequest),
    MockWrite(ASYNC, "\0message1\xff", 10),
    MockWrite(ASYNC, "\0message2\xff", 10)
  };
  MockRead data_reads[] = {
    MockRead(SocketStreamTest::kWebSocketHandshakeResponse),
    // Server doesn't close the connection after handshake.
    MockRead(ASYNC, ERR_IO_PENDING)
  };
  AddWebSocketMessage("message1");
  AddWebSocketMessage("message2");

  DelayedSocketData data_provider(
      1, data_reads, arraysize(data_reads),
      data_writes, arraysize(data_writes));

  MockClientSocketFactory* mock_socket_factory =
      GetMockClientSocketFactory();
  mock_socket_factory->AddSocketDataProvider(&data_provider);

  socket_stream->SetClientSocketFactory(mock_socket_factory);

  socket_stream->Connect();

  test_callback.WaitForResult();

  EXPECT_TRUE(data_provider.at_read_eof());
  EXPECT_TRUE(data_provider.at_write_eof());

  const std::vector<SocketStreamEvent>& events = delegate->GetSeenEvents();
  ASSERT_EQ(7U, events.size());

  EXPECT_EQ(SocketStreamEvent::EVENT_START_OPEN_CONNECTION,
            events[0].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_CONNECTED, events[1].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_SENT_DATA, events[2].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_RECEIVED_DATA, events[3].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_SENT_DATA, events[4].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_SENT_DATA, events[5].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_CLOSE, events[6].event_type);
}

TEST_F(SocketStreamTest, ResolveFailure) {
  TestCompletionCallback test_callback;

  scoped_ptr<SocketStreamEventRecorder> delegate(
      new SocketStreamEventRecorder(test_callback.callback()));

  // Make resolver fail.
  TestURLRequestContext context;
  scoped_ptr<MockHostResolver> mock_host_resolver(
      new MockHostResolver());
  mock_host_resolver->rules()->AddSimulatedFailure("example.com");
  context.set_host_resolver(mock_host_resolver.get());

  scoped_refptr<SocketStream> socket_stream(
      new SocketStream(GURL("ws://example.com/demo"), delegate.get(),
                       &context, NULL));

  // No read/write on socket is expected.
  StaticSocketDataProvider data_provider(NULL, 0, NULL, 0);
  MockClientSocketFactory* mock_socket_factory =
      GetMockClientSocketFactory();
  mock_socket_factory->AddSocketDataProvider(&data_provider);
  socket_stream->SetClientSocketFactory(mock_socket_factory);

  socket_stream->Connect();

  test_callback.WaitForResult();

  const std::vector<SocketStreamEvent>& events = delegate->GetSeenEvents();
  ASSERT_EQ(2U, events.size());

  EXPECT_EQ(SocketStreamEvent::EVENT_ERROR, events[0].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_CLOSE, events[1].event_type);
}

TEST_F(SocketStreamTest, ExceedMaxPendingSendAllowed) {
  TestCompletionCallback test_callback;

  scoped_ptr<SocketStreamEventRecorder> delegate(
      new SocketStreamEventRecorder(test_callback.callback()));
  delegate->SetOnConnected(base::Bind(
      &SocketStreamTest::DoFailByTooBigDataAndClose, base::Unretained(this)));

  TestURLRequestContext context;

  scoped_refptr<SocketStream> socket_stream(
      new SocketStream(GURL("ws://example.com/demo"), delegate.get(),
                       &context, NULL));

  DelayedSocketData data_provider(1, NULL, 0, NULL, 0);

  MockClientSocketFactory* mock_socket_factory =
      GetMockClientSocketFactory();
  mock_socket_factory->AddSocketDataProvider(&data_provider);

  socket_stream->SetClientSocketFactory(mock_socket_factory);

  socket_stream->Connect();

  test_callback.WaitForResult();

  const std::vector<SocketStreamEvent>& events = delegate->GetSeenEvents();
  ASSERT_EQ(4U, events.size());

  EXPECT_EQ(SocketStreamEvent::EVENT_START_OPEN_CONNECTION,
            events[0].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_CONNECTED, events[1].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_ERROR, events[2].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_CLOSE, events[3].event_type);
}

TEST_F(SocketStreamTest, BasicAuthProxy) {
  MockClientSocketFactory mock_socket_factory;
  MockWrite data_writes1[] = {
    MockWrite("CONNECT example.com:80 HTTP/1.1\r\n"
              "Host: example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
    MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("\r\n"),
  };
  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  mock_socket_factory.AddSocketDataProvider(&data1);

  MockWrite data_writes2[] = {
    MockWrite("CONNECT example.com:80 HTTP/1.1\r\n"
              "Host: example.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead data_reads2[] = {
    MockRead("HTTP/1.1 200 Connection Established\r\n"),
    MockRead("Proxy-agent: Apache/2.2.8\r\n"),
    MockRead("\r\n"),
    // SocketStream::DoClose is run asynchronously.  Socket can be read after
    // "\r\n".  We have to give ERR_IO_PENDING to SocketStream then to indicate
    // server doesn't close the connection.
    MockRead(ASYNC, ERR_IO_PENDING)
  };
  StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                 data_writes2, arraysize(data_writes2));
  mock_socket_factory.AddSocketDataProvider(&data2);

  TestCompletionCallback test_callback;

  scoped_ptr<SocketStreamEventRecorder> delegate(
      new SocketStreamEventRecorder(test_callback.callback()));
  delegate->SetOnConnected(base::Bind(&SocketStreamEventRecorder::DoClose,
                                      base::Unretained(delegate.get())));
  delegate->SetAuthInfo(AuthCredentials(ASCIIToUTF16("foo"),
                                        ASCIIToUTF16("bar")));
  delegate->SetOnAuthRequired(base::Bind(
      &SocketStreamEventRecorder::DoRestartWithAuth,
      base::Unretained(delegate.get())));

  TestURLRequestContextWithProxy context("myproxy:70");

  scoped_refptr<SocketStream> socket_stream(
      new SocketStream(GURL("ws://example.com/demo"), delegate.get(),
                       &context, NULL));

  socket_stream->SetClientSocketFactory(&mock_socket_factory);

  socket_stream->Connect();

  test_callback.WaitForResult();

  const std::vector<SocketStreamEvent>& events = delegate->GetSeenEvents();
  ASSERT_EQ(5U, events.size());

  EXPECT_EQ(SocketStreamEvent::EVENT_START_OPEN_CONNECTION,
            events[0].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_AUTH_REQUIRED, events[1].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_CONNECTED, events[2].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_ERROR, events[3].event_type);
  EXPECT_EQ(ERR_ABORTED, events[3].error_code);
  EXPECT_EQ(SocketStreamEvent::EVENT_CLOSE, events[4].event_type);

  // TODO(eroman): Add back NetLogTest here...
}

TEST_F(SocketStreamTest, BasicAuthProxyWithAuthCache) {
  MockClientSocketFactory mock_socket_factory;
  MockWrite data_writes[] = {
    // WebSocket(SocketStream) always uses CONNECT when it is configured to use
    // proxy so the port may not be 443.
    MockWrite("CONNECT example.com:80 HTTP/1.1\r\n"
              "Host: example.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 Connection Established\r\n"),
    MockRead("Proxy-agent: Apache/2.2.8\r\n"),
    MockRead("\r\n"),
    MockRead(ASYNC, ERR_IO_PENDING)
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                 data_writes, arraysize(data_writes));
  mock_socket_factory.AddSocketDataProvider(&data);

  TestCompletionCallback test_callback;
  scoped_ptr<SocketStreamEventRecorder> delegate(
      new SocketStreamEventRecorder(test_callback.callback()));
  delegate->SetOnConnected(base::Bind(&SocketStreamEventRecorder::DoClose,
                                      base::Unretained(delegate.get())));

  TestURLRequestContextWithProxy context("myproxy:70");
  HttpAuthCache* auth_cache =
      context.http_transaction_factory()->GetSession()->http_auth_cache();
  auth_cache->Add(GURL("http://myproxy:70"),
                  "MyRealm1",
                  HttpAuth::AUTH_SCHEME_BASIC,
                  "Basic realm=MyRealm1",
                  AuthCredentials(ASCIIToUTF16("foo"),
                                  ASCIIToUTF16("bar")),
                  "/");

  scoped_refptr<SocketStream> socket_stream(
      new SocketStream(GURL("ws://example.com/demo"), delegate.get(),
                       &context, NULL));

  socket_stream->SetClientSocketFactory(&mock_socket_factory);

  socket_stream->Connect();

  test_callback.WaitForResult();

  const std::vector<SocketStreamEvent>& events = delegate->GetSeenEvents();
  ASSERT_EQ(4U, events.size());
  EXPECT_EQ(SocketStreamEvent::EVENT_START_OPEN_CONNECTION,
            events[0].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_CONNECTED, events[1].event_type);
  EXPECT_EQ(ERR_ABORTED, events[2].error_code);
  EXPECT_EQ(SocketStreamEvent::EVENT_CLOSE, events[3].event_type);
}

TEST_F(SocketStreamTest, WSSBasicAuthProxyWithAuthCache) {
  MockClientSocketFactory mock_socket_factory;
  MockWrite data_writes1[] = {
    MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
              "Host: example.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 200 Connection Established\r\n"),
    MockRead("Proxy-agent: Apache/2.2.8\r\n"),
    MockRead("\r\n"),
    MockRead(ASYNC, ERR_IO_PENDING)
  };
  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  mock_socket_factory.AddSocketDataProvider(&data1);

  SSLSocketDataProvider data2(ASYNC, OK);
  mock_socket_factory.AddSSLSocketDataProvider(&data2);

  TestCompletionCallback test_callback;
  scoped_ptr<SocketStreamEventRecorder> delegate(
      new SocketStreamEventRecorder(test_callback.callback()));
  delegate->SetOnConnected(base::Bind(&SocketStreamEventRecorder::DoClose,
                                      base::Unretained(delegate.get())));

  TestURLRequestContextWithProxy context("myproxy:70");
  HttpAuthCache* auth_cache =
      context.http_transaction_factory()->GetSession()->http_auth_cache();
  auth_cache->Add(GURL("http://myproxy:70"),
                  "MyRealm1",
                  HttpAuth::AUTH_SCHEME_BASIC,
                  "Basic realm=MyRealm1",
                  AuthCredentials(ASCIIToUTF16("foo"),
                                  ASCIIToUTF16("bar")),
                  "/");

  scoped_refptr<SocketStream> socket_stream(
      new SocketStream(GURL("wss://example.com/demo"), delegate.get(),
                       &context, NULL));

  socket_stream->SetClientSocketFactory(&mock_socket_factory);

  socket_stream->Connect();

  test_callback.WaitForResult();

  const std::vector<SocketStreamEvent>& events = delegate->GetSeenEvents();
  ASSERT_EQ(4U, events.size());
  EXPECT_EQ(SocketStreamEvent::EVENT_START_OPEN_CONNECTION,
            events[0].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_CONNECTED, events[1].event_type);
  EXPECT_EQ(ERR_ABORTED, events[2].error_code);
  EXPECT_EQ(SocketStreamEvent::EVENT_CLOSE, events[3].event_type);
}

TEST_F(SocketStreamTest, IOPending) {
  TestCompletionCallback test_callback;

  scoped_ptr<SocketStreamEventRecorder> delegate(
      new SocketStreamEventRecorder(test_callback.callback()));
  delegate->SetOnStartOpenConnection(base::Bind(
      &SocketStreamTest::DoIOPending, base::Unretained(this)));
  delegate->SetOnConnected(base::Bind(
      &SocketStreamTest::DoSendWebSocketHandshake, base::Unretained(this)));
  delegate->SetOnReceivedData(base::Bind(
      &SocketStreamTest::DoCloseFlushPendingWriteTest,
      base::Unretained(this)));

  TestURLRequestContext context;

  scoped_refptr<SocketStream> socket_stream(
      new SocketStream(GURL("ws://example.com/demo"), delegate.get(),
                       &context, NULL));

  MockWrite data_writes[] = {
    MockWrite(SocketStreamTest::kWebSocketHandshakeRequest),
    MockWrite(ASYNC, "\0message1\xff", 10),
    MockWrite(ASYNC, "\0message2\xff", 10)
  };
  MockRead data_reads[] = {
    MockRead(SocketStreamTest::kWebSocketHandshakeResponse),
    // Server doesn't close the connection after handshake.
    MockRead(ASYNC, ERR_IO_PENDING)
  };
  AddWebSocketMessage("message1");
  AddWebSocketMessage("message2");

  DelayedSocketData data_provider(
      1, data_reads, arraysize(data_reads),
      data_writes, arraysize(data_writes));

  MockClientSocketFactory* mock_socket_factory =
      GetMockClientSocketFactory();
  mock_socket_factory->AddSocketDataProvider(&data_provider);

  socket_stream->SetClientSocketFactory(mock_socket_factory);

  socket_stream->Connect();
  io_test_callback_.WaitForResult();
  EXPECT_EQ(SocketStream::STATE_RESOLVE_PROTOCOL_COMPLETE,
            socket_stream->next_state_);
  delegate->CompleteConnection(OK);

  EXPECT_EQ(OK, test_callback.WaitForResult());

  EXPECT_TRUE(data_provider.at_read_eof());
  EXPECT_TRUE(data_provider.at_write_eof());

  const std::vector<SocketStreamEvent>& events = delegate->GetSeenEvents();
  ASSERT_EQ(7U, events.size());

  EXPECT_EQ(SocketStreamEvent::EVENT_START_OPEN_CONNECTION,
            events[0].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_CONNECTED, events[1].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_SENT_DATA, events[2].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_RECEIVED_DATA, events[3].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_SENT_DATA, events[4].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_SENT_DATA, events[5].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_CLOSE, events[6].event_type);
}

TEST_F(SocketStreamTest, SwitchToSpdy) {
  TestCompletionCallback test_callback;

  scoped_ptr<SocketStreamEventRecorder> delegate(
      new SocketStreamEventRecorder(test_callback.callback()));
  delegate->SetOnStartOpenConnection(base::Bind(
      &SocketStreamTest::DoSwitchToSpdyTest, base::Unretained(this)));

  TestURLRequestContext context;

  scoped_refptr<SocketStream> socket_stream(
      new SocketStream(GURL("ws://example.com/demo"), delegate.get(),
                       &context, NULL));

  socket_stream->Connect();

  EXPECT_EQ(ERR_PROTOCOL_SWITCHED, test_callback.WaitForResult());

  const std::vector<SocketStreamEvent>& events = delegate->GetSeenEvents();
  ASSERT_EQ(2U, events.size());

  EXPECT_EQ(SocketStreamEvent::EVENT_START_OPEN_CONNECTION,
            events[0].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_ERROR, events[1].event_type);
  EXPECT_EQ(ERR_PROTOCOL_SWITCHED, events[1].error_code);
}

TEST_F(SocketStreamTest, SwitchAfterPending) {
  TestCompletionCallback test_callback;

  scoped_ptr<SocketStreamEventRecorder> delegate(
      new SocketStreamEventRecorder(test_callback.callback()));
  delegate->SetOnStartOpenConnection(base::Bind(
      &SocketStreamTest::DoIOPending, base::Unretained(this)));

  TestURLRequestContext context;

  scoped_refptr<SocketStream> socket_stream(
      new SocketStream(GURL("ws://example.com/demo"), delegate.get(),
                       &context, NULL));

  socket_stream->Connect();
  io_test_callback_.WaitForResult();

  EXPECT_EQ(SocketStream::STATE_RESOLVE_PROTOCOL_COMPLETE,
            socket_stream->next_state_);
  delegate->CompleteConnection(ERR_PROTOCOL_SWITCHED);

  EXPECT_EQ(ERR_PROTOCOL_SWITCHED, test_callback.WaitForResult());

  const std::vector<SocketStreamEvent>& events = delegate->GetSeenEvents();
  ASSERT_EQ(2U, events.size());

  EXPECT_EQ(SocketStreamEvent::EVENT_START_OPEN_CONNECTION,
            events[0].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_ERROR, events[1].event_type);
  EXPECT_EQ(ERR_PROTOCOL_SWITCHED, events[1].error_code);
}

// Test a connection though a secure proxy.
TEST_F(SocketStreamTest, SecureProxyConnectError) {
  MockClientSocketFactory mock_socket_factory;
  MockWrite data_writes[] = {
    MockWrite("CONNECT example.com:80 HTTP/1.1\r\n"
              "Host: example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n")
  };
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 Connection Established\r\n"),
    MockRead("Proxy-agent: Apache/2.2.8\r\n"),
    MockRead("\r\n"),
    // SocketStream::DoClose is run asynchronously.  Socket can be read after
    // "\r\n".  We have to give ERR_IO_PENDING to SocketStream then to indicate
    // server doesn't close the connection.
    MockRead(ASYNC, ERR_IO_PENDING)
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  mock_socket_factory.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(SYNCHRONOUS, ERR_SSL_PROTOCOL_ERROR);
  mock_socket_factory.AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback test_callback;
  TestURLRequestContextWithProxy context("https://myproxy:70");

  scoped_ptr<SocketStreamEventRecorder> delegate(
      new SocketStreamEventRecorder(test_callback.callback()));
  delegate->SetOnConnected(base::Bind(&SocketStreamEventRecorder::DoClose,
                                      base::Unretained(delegate.get())));

  scoped_refptr<SocketStream> socket_stream(
      new SocketStream(GURL("ws://example.com/demo"), delegate.get(),
                       &context, NULL));

  socket_stream->SetClientSocketFactory(&mock_socket_factory);

  socket_stream->Connect();

  test_callback.WaitForResult();

  const std::vector<SocketStreamEvent>& events = delegate->GetSeenEvents();
  ASSERT_EQ(3U, events.size());

  EXPECT_EQ(SocketStreamEvent::EVENT_START_OPEN_CONNECTION,
            events[0].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_ERROR, events[1].event_type);
  EXPECT_EQ(ERR_SSL_PROTOCOL_ERROR, events[1].error_code);
  EXPECT_EQ(SocketStreamEvent::EVENT_CLOSE, events[2].event_type);
}

// Test a connection though a secure proxy.
TEST_F(SocketStreamTest, SecureProxyConnect) {
  MockClientSocketFactory mock_socket_factory;
  MockWrite data_writes[] = {
    MockWrite("CONNECT example.com:80 HTTP/1.1\r\n"
              "Host: example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n")
  };
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 Connection Established\r\n"),
    MockRead("Proxy-agent: Apache/2.2.8\r\n"),
    MockRead("\r\n"),
    // SocketStream::DoClose is run asynchronously.  Socket can be read after
    // "\r\n".  We have to give ERR_IO_PENDING to SocketStream then to indicate
    // server doesn't close the connection.
    MockRead(ASYNC, ERR_IO_PENDING)
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  mock_socket_factory.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  mock_socket_factory.AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback test_callback;
  TestURLRequestContextWithProxy context("https://myproxy:70");

  scoped_ptr<SocketStreamEventRecorder> delegate(
      new SocketStreamEventRecorder(test_callback.callback()));
  delegate->SetOnConnected(base::Bind(&SocketStreamEventRecorder::DoClose,
                                      base::Unretained(delegate.get())));

  scoped_refptr<SocketStream> socket_stream(
      new SocketStream(GURL("ws://example.com/demo"), delegate.get(),
                       &context, NULL));

  socket_stream->SetClientSocketFactory(&mock_socket_factory);

  socket_stream->Connect();

  test_callback.WaitForResult();

  const std::vector<SocketStreamEvent>& events = delegate->GetSeenEvents();
  ASSERT_EQ(4U, events.size());

  EXPECT_EQ(SocketStreamEvent::EVENT_START_OPEN_CONNECTION,
            events[0].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_CONNECTED, events[1].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_ERROR, events[2].event_type);
  EXPECT_EQ(ERR_ABORTED, events[2].error_code);
  EXPECT_EQ(SocketStreamEvent::EVENT_CLOSE, events[3].event_type);
}

TEST_F(SocketStreamTest, BeforeConnectFailed) {
  TestCompletionCallback test_callback;

  scoped_ptr<SocketStreamEventRecorder> delegate(
      new SocketStreamEventRecorder(test_callback.callback()));

  TestURLRequestContext context;
  TestSocketStreamNetworkDelegate network_delegate;
  network_delegate.SetBeforeConnectResult(ERR_ACCESS_DENIED);
  context.set_network_delegate(&network_delegate);

  scoped_refptr<SocketStream> socket_stream(
      new SocketStream(GURL("ws://example.com/demo"), delegate.get(),
                       &context, NULL));

  socket_stream->Connect();

  test_callback.WaitForResult();

  const std::vector<SocketStreamEvent>& events = delegate->GetSeenEvents();
  ASSERT_EQ(2U, events.size());

  EXPECT_EQ(SocketStreamEvent::EVENT_ERROR, events[0].event_type);
  EXPECT_EQ(ERR_ACCESS_DENIED, events[0].error_code);
  EXPECT_EQ(SocketStreamEvent::EVENT_CLOSE, events[1].event_type);
}

// Check that a connect failure, followed by the delegate calling DetachDelegate
// and deleting itself in the OnError callback, is handled correctly.
TEST_F(SocketStreamTest, OnErrorDetachDelegate) {
  MockClientSocketFactory mock_socket_factory;
  TestCompletionCallback test_callback;

  // SelfDeletingDelegate is self-owning; we just need a pointer to it to
  // connect it and the SocketStream.
  SelfDeletingDelegate* delegate =
      new SelfDeletingDelegate(test_callback.callback());
  MockConnect mock_connect(ASYNC, ERR_CONNECTION_REFUSED);
  StaticSocketDataProvider data;
  data.set_connect_data(mock_connect);
  mock_socket_factory.AddSocketDataProvider(&data);

  TestURLRequestContext context;
  scoped_refptr<SocketStream> socket_stream(
      new SocketStream(GURL("ws://localhost:9998/echo"), delegate,
                       &context, NULL));
  socket_stream->SetClientSocketFactory(&mock_socket_factory);
  delegate->set_socket_stream(socket_stream);
  // The delegate pointer will become invalid during the test. Set it to NULL to
  // avoid holding a dangling pointer.
  delegate = NULL;

  socket_stream->Connect();

  EXPECT_EQ(OK, test_callback.WaitForResult());
}

TEST_F(SocketStreamTest, NullContextSocketStreamShouldNotCrash) {
  TestCompletionCallback test_callback;

  scoped_ptr<SocketStreamEventRecorder> delegate(
      new SocketStreamEventRecorder(test_callback.callback()));
  TestURLRequestContext context;
  scoped_refptr<SocketStream> socket_stream(
      new SocketStream(GURL("ws://example.com/demo"), delegate.get(),
                       &context, NULL));
  delegate->SetOnStartOpenConnection(base::Bind(
      &SocketStreamTest::DoIOPending, base::Unretained(this)));
  delegate->SetOnConnected(base::Bind(
      &SocketStreamTest::DoSendWebSocketHandshake, base::Unretained(this)));
  delegate->SetOnReceivedData(base::Bind(
      &SocketStreamTest::DoCloseFlushPendingWriteTestWithSetContextNull,
      base::Unretained(this)));

  MockWrite data_writes[] = {
    MockWrite(SocketStreamTest::kWebSocketHandshakeRequest),
  };
  MockRead data_reads[] = {
    MockRead(SocketStreamTest::kWebSocketHandshakeResponse),
  };
  AddWebSocketMessage("message1");
  AddWebSocketMessage("message2");

  DelayedSocketData data_provider(
      1, data_reads, arraysize(data_reads),
      data_writes, arraysize(data_writes));

  MockClientSocketFactory* mock_socket_factory = GetMockClientSocketFactory();
  mock_socket_factory->AddSocketDataProvider(&data_provider);
  socket_stream->SetClientSocketFactory(mock_socket_factory);

  socket_stream->Connect();
  io_test_callback_.WaitForResult();
  delegate->CompleteConnection(OK);
  EXPECT_EQ(OK, test_callback.WaitForResult());

  EXPECT_TRUE(data_provider.at_read_eof());
  EXPECT_TRUE(data_provider.at_write_eof());

  const std::vector<SocketStreamEvent>& events = delegate->GetSeenEvents();
  ASSERT_EQ(5U, events.size());

  EXPECT_EQ(SocketStreamEvent::EVENT_START_OPEN_CONNECTION,
            events[0].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_CONNECTED, events[1].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_SENT_DATA, events[2].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_RECEIVED_DATA, events[3].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_CLOSE, events[4].event_type);
}

}  // namespace net
