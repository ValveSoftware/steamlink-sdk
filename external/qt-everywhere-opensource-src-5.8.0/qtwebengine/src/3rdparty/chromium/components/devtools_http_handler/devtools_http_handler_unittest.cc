// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_http_handler/devtools_http_handler.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/devtools_http_handler/devtools_http_handler_delegate.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/server_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace devtools_http_handler {
namespace {

const uint16_t kDummyPort = 4321;
const base::FilePath::CharType kDevToolsActivePortFileName[] =
    FILE_PATH_LITERAL("DevToolsActivePort");

class DummyServerSocket : public net::ServerSocket {
 public:
  DummyServerSocket() {}

  // net::ServerSocket "implementation"
  int Listen(const net::IPEndPoint& address, int backlog) override {
    return net::OK;
  }

  int GetLocalAddress(net::IPEndPoint* address) const override {
    *address = net::IPEndPoint(net::IPAddress::IPv4Localhost(), kDummyPort);
    return net::OK;
  }

  int Accept(std::unique_ptr<net::StreamSocket>* socket,
             const net::CompletionCallback& callback) override {
    return net::ERR_IO_PENDING;
  }
};

void QuitFromHandlerThread(const base::Closure& quit_closure) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, quit_closure);
}

class DummyServerSocketFactory
    : public DevToolsHttpHandler::ServerSocketFactory {
 public:
  DummyServerSocketFactory(base::Closure quit_closure_1,
                           base::Closure quit_closure_2)
      : quit_closure_1_(quit_closure_1),
        quit_closure_2_(quit_closure_2) {}

  ~DummyServerSocketFactory() override {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, quit_closure_2_);
  }

 protected:
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&QuitFromHandlerThread, quit_closure_1_));
    return base::WrapUnique(new DummyServerSocket());
  }

  base::Closure quit_closure_1_;
  base::Closure quit_closure_2_;
};

class FailingServerSocketFactory : public DummyServerSocketFactory {
 public:
  FailingServerSocketFactory(const base::Closure& quit_closure_1,
                             const base::Closure& quit_closure_2)
      : DummyServerSocketFactory(quit_closure_1, quit_closure_2) {
  }

 private:
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&QuitFromHandlerThread, quit_closure_1_));
    return nullptr;
  }
};

class DummyDelegate : public DevToolsHttpHandlerDelegate {
 public:
  std::string GetDiscoveryPageHTML() override { return std::string(); }

  std::string GetFrontendResource(const std::string& path) override {
    return std::string();
  }

  std::string GetPageThumbnailData(const GURL& url) override {
    return std::string();
  }

  content::DevToolsExternalAgentProxyDelegate*
      HandleWebSocketConnection(const std::string& path) override {
    return nullptr;
  }
};

}

class DevToolsHttpHandlerTest : public testing::Test {
 public:
  DevToolsHttpHandlerTest() : testing::Test() { }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(DevToolsHttpHandlerTest, TestStartStop) {
  base::RunLoop run_loop, run_loop_2;
  std::unique_ptr<DevToolsHttpHandler::ServerSocketFactory> factory(
      new DummyServerSocketFactory(run_loop.QuitClosure(),
                                   run_loop_2.QuitClosure()));
  std::unique_ptr<DevToolsHttpHandler> devtools_http_handler(
      new DevToolsHttpHandler(std::move(factory), std::string(),
                              new DummyDelegate(), base::FilePath(),
                              base::FilePath(), std::string(), std::string()));
  // Our dummy socket factory will post a quit message once the server will
  // become ready.
  run_loop.Run();
  devtools_http_handler.reset();
  // Make sure the handler actually stops.
  run_loop_2.Run();
}

TEST_F(DevToolsHttpHandlerTest, TestServerSocketFailed) {
  base::RunLoop run_loop, run_loop_2;
  std::unique_ptr<DevToolsHttpHandler::ServerSocketFactory> factory(
      new FailingServerSocketFactory(run_loop.QuitClosure(),
                                     run_loop_2.QuitClosure()));
  std::unique_ptr<DevToolsHttpHandler> devtools_http_handler(
      new DevToolsHttpHandler(std::move(factory), std::string(),
                              new DummyDelegate(), base::FilePath(),
                              base::FilePath(), std::string(), std::string()));
  // Our dummy socket factory will post a quit message once the server will
  // become ready.
  run_loop.Run();
  for (int i = 0; i < 5; i++) {
    RunAllPendingInMessageLoop(BrowserThread::UI);
    RunAllPendingInMessageLoop(BrowserThread::FILE);
  }
  devtools_http_handler.reset();
  // Make sure the handler actually stops.
  run_loop_2.Run();
}


TEST_F(DevToolsHttpHandlerTest, TestDevToolsActivePort) {
  base::RunLoop run_loop, run_loop_2;
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  std::unique_ptr<DevToolsHttpHandler::ServerSocketFactory> factory(
      new DummyServerSocketFactory(run_loop.QuitClosure(),
                                   run_loop_2.QuitClosure()));
  std::unique_ptr<DevToolsHttpHandler> devtools_http_handler(
      new DevToolsHttpHandler(std::move(factory), std::string(),
                              new DummyDelegate(), temp_dir.path(),
                              base::FilePath(), std::string(), std::string()));
  // Our dummy socket factory will post a quit message once the server will
  // become ready.
  run_loop.Run();
  devtools_http_handler.reset();
  // Make sure the handler actually stops.
  run_loop_2.Run();

  // Now make sure the DevToolsActivePort was written into the
  // temporary directory and its contents are as expected.
  base::FilePath active_port_file = temp_dir.path().Append(
      kDevToolsActivePortFileName);
  EXPECT_TRUE(base::PathExists(active_port_file));
  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(active_port_file, &file_contents));
  int port = 0;
  EXPECT_TRUE(base::StringToInt(file_contents, &port));
  EXPECT_EQ(static_cast<int>(kDummyPort), port);
}

}  // namespace devtools_http_handler
