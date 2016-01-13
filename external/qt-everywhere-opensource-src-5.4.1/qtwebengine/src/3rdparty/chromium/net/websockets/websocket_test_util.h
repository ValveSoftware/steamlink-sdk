// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_TEST_UTIL_H_
#define NET_WEBSOCKETS_WEBSOCKET_TEST_UTIL_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_request_test_util.h"
#include "net/websockets/websocket_stream.h"

class GURL;

namespace url {
class Origin;
}  // namespace url

namespace net {

class BoundNetLog;
class DeterministicMockClientSocketFactory;
class DeterministicSocketData;
class URLRequestContext;
class WebSocketHandshakeStreamCreateHelper;
struct SSLSocketDataProvider;

class LinearCongruentialGenerator {
 public:
  explicit LinearCongruentialGenerator(uint32 seed);
  uint32 Generate();

 private:
  uint64 current_;
};

// Alternate version of WebSocketStream::CreateAndConnectStream() for testing
// use only. The difference is the use of a |create_helper| argument in place of
// |requested_subprotocols|. Implemented in websocket_stream.cc.
NET_EXPORT_PRIVATE extern scoped_ptr<WebSocketStreamRequest>
    CreateAndConnectStreamForTesting(
        const GURL& socket_url,
        scoped_ptr<WebSocketHandshakeStreamCreateHelper> create_helper,
        const url::Origin& origin,
        URLRequestContext* url_request_context,
        const BoundNetLog& net_log,
        scoped_ptr<WebSocketStream::ConnectDelegate> connect_delegate);

// Generates a standard WebSocket handshake request. The challenge key used is
// "dGhlIHNhbXBsZSBub25jZQ==". Each header in |extra_headers| must be terminated
// with "\r\n".
extern std::string WebSocketStandardRequest(const std::string& path,
                                            const std::string& origin,
                                            const std::string& extra_headers);

// A response with the appropriate accept header to match the above challenge
// key. Each header in |extra_headers| must be terminated with "\r\n".
extern std::string WebSocketStandardResponse(const std::string& extra_headers);

// This class provides a convenient way to construct a
// DeterministicMockClientSocketFactory for WebSocket tests.
class WebSocketDeterministicMockClientSocketFactoryMaker {
 public:
  WebSocketDeterministicMockClientSocketFactoryMaker();
  ~WebSocketDeterministicMockClientSocketFactoryMaker();

  // Tell the factory to create a socket which expects |expect_written| to be
  // written, and responds with |return_to_read|. The test will fail if the
  // expected text is not written, or all the bytes are not read. This adds data
  // for a new mock-socket using AddRawExpections(), and so can be called
  // multiple times to queue up multiple mock sockets, but usually in those
  // cases the lower-level AddRawExpections() interface is more appropriate.
  void SetExpectations(const std::string& expect_written,
                       const std::string& return_to_read);

  // A low-level interface to permit arbitrary expectations to be added. The
  // mock sockets will be created in the same order that they were added.
  void AddRawExpectations(scoped_ptr<DeterministicSocketData> socket_data);

  // Allow an SSL socket data provider to be added. You must also supply a mock
  // transport socket for it to use. If the mock SSL handshake fails then the
  // mock transport socket will connect but have nothing read or written. If the
  // mock handshake succeeds then the data from the underlying transport socket
  // will be passed through unchanged (without encryption).
  void AddSSLSocketDataProvider(
      scoped_ptr<SSLSocketDataProvider> ssl_socket_data);

  // Call to get a pointer to the factory, which remains owned by this object.
  DeterministicMockClientSocketFactory* factory();

 private:
  struct Detail;
  scoped_ptr<Detail> detail_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketDeterministicMockClientSocketFactoryMaker);
};

// This class encapsulates the details of creating a
// TestURLRequestContext that returns mock ClientSocketHandles that do what is
// required by the tests.
struct WebSocketTestURLRequestContextHost {
 public:
  WebSocketTestURLRequestContextHost();
  ~WebSocketTestURLRequestContextHost();

  void SetExpectations(const std::string& expect_written,
                       const std::string& return_to_read) {
    maker_.SetExpectations(expect_written, return_to_read);
  }

  void AddRawExpectations(scoped_ptr<DeterministicSocketData> socket_data);

  // Allow an SSL socket data provider to be added.
  void AddSSLSocketDataProvider(
      scoped_ptr<SSLSocketDataProvider> ssl_socket_data);

  // Call after calling one of SetExpections() or AddRawExpectations(). The
  // returned pointer remains owned by this object. This should only be called
  // once.
  TestURLRequestContext* GetURLRequestContext();

 private:
  WebSocketDeterministicMockClientSocketFactoryMaker maker_;
  TestURLRequestContext url_request_context_;
  TestNetworkDelegate network_delegate_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketTestURLRequestContextHost);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_TEST_UTIL_H_
