// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_EMBEDDED_TEST_SERVER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_EMBEDDED_TEST_SERVER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "net/socket/tcp_listen_socket.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace net {
namespace test_server {

class HttpConnection;
class HttpResponse;
struct HttpRequest;

// This class is required to be able to have composition instead of inheritance,
class HttpListenSocket : public TCPListenSocket {
 public:
  HttpListenSocket(const SocketDescriptor socket_descriptor,
                   StreamListenSocket::Delegate* delegate);
  virtual ~HttpListenSocket();
  virtual void Listen();

 private:
  friend class EmbeddedTestServer;

  // Detaches the current from |thread_checker_|.
  void DetachFromThread();

  base::ThreadChecker thread_checker_;
};

// Class providing an HTTP server for testing purpose. This is a basic server
// providing only an essential subset of HTTP/1.1 protocol. Especially,
// it assumes that the request syntax is correct. It *does not* support
// a Chunked Transfer Encoding.
//
// The common use case for unit tests is below:
//
// void SetUp() {
//   test_server_.reset(new EmbeddedTestServer());
//   ASSERT_TRUE(test_server_.InitializeAndWaitUntilReady());
//   test_server_->RegisterRequestHandler(
//       base::Bind(&FooTest::HandleRequest, base::Unretained(this)));
// }
//
// scoped_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
//   GURL absolute_url = test_server_->GetURL(request.relative_url);
//   if (absolute_url.path() != "/test")
//     return scoped_ptr<HttpResponse>();
//
//   scoped_ptr<HttpResponse> http_response(new HttpResponse());
//   http_response->set_code(test_server::SUCCESS);
//   http_response->set_content("hello");
//   http_response->set_content_type("text/plain");
//   return http_response.Pass();
// }
//
// For a test that spawns another process such as browser_tests, it is
// suggested to call InitializeAndWaitUntilReady in SetUpOnMainThread after
// the process is spawned. If you have to do it before the process spawns,
// you need to stop the server's thread so that there is no no other
// threads running while spawning the process. To do so, please follow
// the following example:
//
// void SetUp() {
//   ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
//   // EmbeddedTestServer spawns a thread to initialize socket.
//   // Stop the thread in preparation for fork and exec.
//   embedded_test_server()->StopThread();
//   ...
//   InProcessBrowserTest::SetUp();
// }
//
// void SetUpOnMainThread() {
//   embedded_test_server()->RestartThreadAndListen();
// }
//
class EmbeddedTestServer : public StreamListenSocket::Delegate {
 public:
  typedef base::Callback<scoped_ptr<HttpResponse>(
      const HttpRequest& request)> HandleRequestCallback;

  // Creates a http test server. InitializeAndWaitUntilReady() must be called
  // to start the server.
  EmbeddedTestServer();
  virtual ~EmbeddedTestServer();

  // Initializes and waits until the server is ready to accept requests.
  bool InitializeAndWaitUntilReady() WARN_UNUSED_RESULT;

  // Shuts down the http server and waits until the shutdown is complete.
  bool ShutdownAndWaitUntilComplete() WARN_UNUSED_RESULT;

  // Checks if the server is started.
  bool Started() const {
    return listen_socket_.get() != NULL;
  }

  // Returns the base URL to the server, which looks like
  // http://127.0.0.1:<port>/, where <port> is the actual port number used by
  // the server.
  const GURL& base_url() const { return base_url_; }

  // Returns a URL to the server based on the given relative URL, which
  // should start with '/'. For example: GetURL("/path?query=foo") =>
  // http://127.0.0.1:<port>/path?query=foo.
  GURL GetURL(const std::string& relative_url) const;

  // Returns the port number used by the server.
  int port() const { return port_; }

  // Registers request handler which serves files from |directory|.
  // For instance, a request to "/foo.html" is served by "foo.html" under
  // |directory|. Files under sub directories are also handled in the same way
  // (i.e. "/foo/bar.html" is served by "foo/bar.html" under |directory|).
  void ServeFilesFromDirectory(const base::FilePath& directory);

  // The most general purpose method. Any request processing can be added using
  // this method. Takes ownership of the object. The |callback| is called
  // on UI thread.
  void RegisterRequestHandler(const HandleRequestCallback& callback);

  // Stops IO thread that handles http requests.
  void StopThread();

  // Restarts IO thread and listen on the socket.
  void RestartThreadAndListen();

 private:
  void StartThread();

  // Initializes and starts the server. If initialization succeeds, Starts()
  // will return true.
  void InitializeOnIOThread();
  void ListenOnIOThread();

  // Shuts down the server.
  void ShutdownOnIOThread();

  // Handles a request when it is parsed. It passes the request to registed
  // request handlers and sends a http response.
  void HandleRequest(HttpConnection* connection,
                     scoped_ptr<HttpRequest> request);

  // StreamListenSocket::Delegate overrides:
  virtual void DidAccept(StreamListenSocket* server,
                         scoped_ptr<StreamListenSocket> connection) OVERRIDE;
  virtual void DidRead(StreamListenSocket* connection,
                       const char* data,
                       int length) OVERRIDE;
  virtual void DidClose(StreamListenSocket* connection) OVERRIDE;

  HttpConnection* FindConnection(StreamListenSocket* socket);

  // Posts a task to the |io_thread_| and waits for a reply.
  bool PostTaskToIOThreadAndWait(
      const base::Closure& closure) WARN_UNUSED_RESULT;

  scoped_ptr<base::Thread> io_thread_;

  scoped_ptr<HttpListenSocket> listen_socket_;
  int port_;
  GURL base_url_;

  // Owns the HttpConnection objects.
  std::map<StreamListenSocket*, HttpConnection*> connections_;

  // Vector of registered request handlers.
  std::vector<HandleRequestCallback> request_handlers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<EmbeddedTestServer> weak_factory_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedTestServer);
};

}  // namespace test_servers
}  // namespace net

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_EMBEDDED_TEST_SERVER_H_
