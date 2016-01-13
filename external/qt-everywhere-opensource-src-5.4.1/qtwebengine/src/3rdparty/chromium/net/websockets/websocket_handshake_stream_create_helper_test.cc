// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake_stream_create_helper.h"

#include <string>
#include <vector>

#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket_test_util.h"
#include "net/websockets/websocket_basic_handshake_stream.h"
#include "net/websockets/websocket_stream.h"
#include "net/websockets/websocket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {
namespace {

// This class encapsulates the details of creating a mock ClientSocketHandle.
class MockClientSocketHandleFactory {
 public:
  MockClientSocketHandleFactory()
      : histograms_("a"),
        pool_(1, 1, &histograms_, socket_factory_maker_.factory()) {}

  // The created socket expects |expect_written| to be written to the socket,
  // and will respond with |return_to_read|. The test will fail if the expected
  // text is not written, or if all the bytes are not read.
  scoped_ptr<ClientSocketHandle> CreateClientSocketHandle(
      const std::string& expect_written,
      const std::string& return_to_read) {
    socket_factory_maker_.SetExpectations(expect_written, return_to_read);
    scoped_ptr<ClientSocketHandle> socket_handle(new ClientSocketHandle);
    socket_handle->Init(
        "a",
        scoped_refptr<MockTransportSocketParams>(),
        MEDIUM,
        CompletionCallback(),
        &pool_,
        BoundNetLog());
    return socket_handle.Pass();
  }

 private:
  WebSocketDeterministicMockClientSocketFactoryMaker socket_factory_maker_;
  ClientSocketPoolHistograms histograms_;
  MockTransportClientSocketPool pool_;

  DISALLOW_COPY_AND_ASSIGN(MockClientSocketHandleFactory);
};

class TestConnectDelegate : public WebSocketStream::ConnectDelegate {
 public:
  virtual ~TestConnectDelegate() {}

  virtual void OnSuccess(scoped_ptr<WebSocketStream> stream) OVERRIDE {}
  virtual void OnFailure(const std::string& failure_message) OVERRIDE {}
  virtual void OnStartOpeningHandshake(
      scoped_ptr<WebSocketHandshakeRequestInfo> request) OVERRIDE {}
  virtual void OnFinishOpeningHandshake(
      scoped_ptr<WebSocketHandshakeResponseInfo> response) OVERRIDE {}
  virtual void OnSSLCertificateError(
      scoped_ptr<WebSocketEventInterface::SSLErrorCallbacks>
          ssl_error_callbacks,
      const SSLInfo& ssl_info,
      bool fatal) OVERRIDE {}
};

class WebSocketHandshakeStreamCreateHelperTest : public ::testing::Test {
 protected:
  scoped_ptr<WebSocketStream> CreateAndInitializeStream(
      const std::string& socket_url,
      const std::string& socket_path,
      const std::vector<std::string>& sub_protocols,
      const std::string& origin,
      const std::string& extra_request_headers,
      const std::string& extra_response_headers) {
    WebSocketHandshakeStreamCreateHelper create_helper(&connect_delegate_,
                                                       sub_protocols);
    create_helper.set_failure_message(&failure_message_);

    scoped_ptr<ClientSocketHandle> socket_handle =
        socket_handle_factory_.CreateClientSocketHandle(
            WebSocketStandardRequest(
                socket_path, origin, extra_request_headers),
            WebSocketStandardResponse(extra_response_headers));

    scoped_ptr<WebSocketHandshakeStreamBase> handshake(
        create_helper.CreateBasicStream(socket_handle.Pass(), false));

    // If in future the implementation type returned by CreateBasicStream()
    // changes, this static_cast will be wrong. However, in that case the test
    // will fail and AddressSanitizer should identify the issue.
    static_cast<WebSocketBasicHandshakeStream*>(handshake.get())
        ->SetWebSocketKeyForTesting("dGhlIHNhbXBsZSBub25jZQ==");

    HttpRequestInfo request_info;
    request_info.url = GURL(socket_url);
    request_info.method = "GET";
    request_info.load_flags = LOAD_DISABLE_CACHE | LOAD_DO_NOT_PROMPT_FOR_LOGIN;
    int rv = handshake->InitializeStream(
        &request_info, DEFAULT_PRIORITY, BoundNetLog(), CompletionCallback());
    EXPECT_EQ(OK, rv);

    HttpRequestHeaders headers;
    headers.SetHeader("Host", "localhost");
    headers.SetHeader("Connection", "Upgrade");
    headers.SetHeader("Pragma", "no-cache");
    headers.SetHeader("Cache-Control", "no-cache");
    headers.SetHeader("Upgrade", "websocket");
    headers.SetHeader("Origin", origin);
    headers.SetHeader("Sec-WebSocket-Version", "13");
    headers.SetHeader("User-Agent", "");
    headers.SetHeader("Accept-Encoding", "gzip,deflate");
    headers.SetHeader("Accept-Language", "en-us,fr");

    HttpResponseInfo response;
    TestCompletionCallback dummy;

    rv = handshake->SendRequest(headers, &response, dummy.callback());

    EXPECT_EQ(OK, rv);

    rv = handshake->ReadResponseHeaders(dummy.callback());
    EXPECT_EQ(OK, rv);
    EXPECT_EQ(101, response.headers->response_code());
    EXPECT_TRUE(response.headers->HasHeaderValue("Connection", "Upgrade"));
    EXPECT_TRUE(response.headers->HasHeaderValue("Upgrade", "websocket"));
    return handshake->Upgrade();
  }

  MockClientSocketHandleFactory socket_handle_factory_;
  TestConnectDelegate connect_delegate_;
  std::string failure_message_;
};

// Confirm that the basic case works as expected.
TEST_F(WebSocketHandshakeStreamCreateHelperTest, BasicStream) {
  scoped_ptr<WebSocketStream> stream =
      CreateAndInitializeStream("ws://localhost/", "/",
                                std::vector<std::string>(), "http://localhost/",
                                "", "");
  EXPECT_EQ("", stream->GetExtensions());
  EXPECT_EQ("", stream->GetSubProtocol());
}

// Verify that the sub-protocols are passed through.
TEST_F(WebSocketHandshakeStreamCreateHelperTest, SubProtocols) {
  std::vector<std::string> sub_protocols;
  sub_protocols.push_back("chat");
  sub_protocols.push_back("superchat");
  scoped_ptr<WebSocketStream> stream =
      CreateAndInitializeStream("ws://localhost/",
                                "/",
                                sub_protocols,
                                "http://localhost/",
                                "Sec-WebSocket-Protocol: chat, superchat\r\n",
                                "Sec-WebSocket-Protocol: superchat\r\n");
  EXPECT_EQ("superchat", stream->GetSubProtocol());
}

// Verify that extension name is available. Bad extension names are tested in
// websocket_stream_test.cc.
TEST_F(WebSocketHandshakeStreamCreateHelperTest, Extensions) {
  scoped_ptr<WebSocketStream> stream = CreateAndInitializeStream(
      "ws://localhost/",
      "/",
      std::vector<std::string>(),
      "http://localhost/",
      "",
      "Sec-WebSocket-Extensions: permessage-deflate\r\n");
  EXPECT_EQ("permessage-deflate", stream->GetExtensions());
}

// Verify that extension parameters are available. Bad parameters are tested in
// websocket_stream_test.cc.
TEST_F(WebSocketHandshakeStreamCreateHelperTest, ExtensionParameters) {
  scoped_ptr<WebSocketStream> stream = CreateAndInitializeStream(
      "ws://localhost/",
      "/",
      std::vector<std::string>(),
      "http://localhost/",
      "",
      "Sec-WebSocket-Extensions: permessage-deflate;"
      " client_max_window_bits=14; server_max_window_bits=14;"
      " server_no_context_takeover; client_no_context_takeover\r\n");

  EXPECT_EQ(
      "permessage-deflate;"
      " client_max_window_bits=14; server_max_window_bits=14;"
      " server_no_context_takeover; client_no_context_takeover",
      stream->GetExtensions());
}

}  // namespace
}  // namespace net
