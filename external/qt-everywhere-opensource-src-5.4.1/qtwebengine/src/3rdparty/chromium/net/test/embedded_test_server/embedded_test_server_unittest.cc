// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/embedded_test_server.h"

#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test_server {

namespace {

// Gets the content from the given URLFetcher.
std::string GetContentFromFetcher(const URLFetcher& fetcher) {
  std::string result;
  const bool success = fetcher.GetResponseAsString(&result);
  EXPECT_TRUE(success);
  return result;
}

// Gets the content type from the given URLFetcher.
std::string GetContentTypeFromFetcher(const URLFetcher& fetcher) {
  const HttpResponseHeaders* headers = fetcher.GetResponseHeaders();
  if (headers) {
    std::string content_type;
    if (headers->GetMimeType(&content_type))
      return content_type;
  }
  return std::string();
}

}  // namespace

class EmbeddedTestServerTest: public testing::Test,
                              public URLFetcherDelegate {
 public:
  EmbeddedTestServerTest()
      : num_responses_received_(0),
        num_responses_expected_(0),
        io_thread_("io_thread") {
  }

  virtual void SetUp() OVERRIDE {
    base::Thread::Options thread_options;
    thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
    ASSERT_TRUE(io_thread_.StartWithOptions(thread_options));

    request_context_getter_ = new TestURLRequestContextGetter(
        io_thread_.message_loop_proxy());

    server_.reset(new EmbeddedTestServer);
    ASSERT_TRUE(server_->InitializeAndWaitUntilReady());
  }

  virtual void TearDown() OVERRIDE {
    ASSERT_TRUE(server_->ShutdownAndWaitUntilComplete());
  }

  // URLFetcherDelegate override.
  virtual void OnURLFetchComplete(const URLFetcher* source) OVERRIDE {
    ++num_responses_received_;
    if (num_responses_received_ == num_responses_expected_)
      base::MessageLoop::current()->Quit();
  }

  // Waits until the specified number of responses are received.
  void WaitForResponses(int num_responses) {
    num_responses_received_ = 0;
    num_responses_expected_ = num_responses;
    // Will be terminated in OnURLFetchComplete().
    base::MessageLoop::current()->Run();
  }

  // Handles |request| sent to |path| and returns the response per |content|,
  // |content type|, and |code|. Saves the request URL for verification.
  scoped_ptr<HttpResponse> HandleRequest(const std::string& path,
                                         const std::string& content,
                                         const std::string& content_type,
                                         HttpStatusCode code,
                                         const HttpRequest& request) {
    request_relative_url_ = request.relative_url;

    GURL absolute_url = server_->GetURL(request.relative_url);
    if (absolute_url.path() == path) {
      scoped_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
      http_response->set_code(code);
      http_response->set_content(content);
      http_response->set_content_type(content_type);
      return http_response.PassAs<HttpResponse>();
    }

    return scoped_ptr<HttpResponse>();
  }

 protected:
  int num_responses_received_;
  int num_responses_expected_;
  std::string request_relative_url_;
  base::Thread io_thread_;
  scoped_refptr<TestURLRequestContextGetter> request_context_getter_;
  scoped_ptr<EmbeddedTestServer> server_;
};

TEST_F(EmbeddedTestServerTest, GetBaseURL) {
  EXPECT_EQ(base::StringPrintf("http://127.0.0.1:%d/", server_->port()),
                               server_->base_url().spec());
}

TEST_F(EmbeddedTestServerTest, GetURL) {
  EXPECT_EQ(base::StringPrintf("http://127.0.0.1:%d/path?query=foo",
                               server_->port()),
            server_->GetURL("/path?query=foo").spec());
}

TEST_F(EmbeddedTestServerTest, RegisterRequestHandler) {
  server_->RegisterRequestHandler(
      base::Bind(&EmbeddedTestServerTest::HandleRequest,
                 base::Unretained(this),
                 "/test",
                 "<b>Worked!</b>",
                 "text/html",
                 HTTP_OK));

  scoped_ptr<URLFetcher> fetcher(
      URLFetcher::Create(server_->GetURL("/test?q=foo"),
                              URLFetcher::GET,
                              this));
  fetcher->SetRequestContext(request_context_getter_.get());
  fetcher->Start();
  WaitForResponses(1);

  EXPECT_EQ(URLRequestStatus::SUCCESS, fetcher->GetStatus().status());
  EXPECT_EQ(HTTP_OK, fetcher->GetResponseCode());
  EXPECT_EQ("<b>Worked!</b>", GetContentFromFetcher(*fetcher));
  EXPECT_EQ("text/html", GetContentTypeFromFetcher(*fetcher));

  EXPECT_EQ("/test?q=foo", request_relative_url_);
}

TEST_F(EmbeddedTestServerTest, ServeFilesFromDirectory) {
  base::FilePath src_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));
  server_->ServeFilesFromDirectory(
      src_dir.AppendASCII("net").AppendASCII("data"));

  scoped_ptr<URLFetcher> fetcher(
      URLFetcher::Create(server_->GetURL("/test.html"),
                              URLFetcher::GET,
                              this));
  fetcher->SetRequestContext(request_context_getter_.get());
  fetcher->Start();
  WaitForResponses(1);

  EXPECT_EQ(URLRequestStatus::SUCCESS, fetcher->GetStatus().status());
  EXPECT_EQ(HTTP_OK, fetcher->GetResponseCode());
  EXPECT_EQ("<p>Hello World!</p>", GetContentFromFetcher(*fetcher));
  EXPECT_EQ("", GetContentTypeFromFetcher(*fetcher));
}

TEST_F(EmbeddedTestServerTest, DefaultNotFoundResponse) {
  scoped_ptr<URLFetcher> fetcher(
      URLFetcher::Create(server_->GetURL("/non-existent"),
                              URLFetcher::GET,
                              this));
  fetcher->SetRequestContext(request_context_getter_.get());

  fetcher->Start();
  WaitForResponses(1);
  EXPECT_EQ(URLRequestStatus::SUCCESS, fetcher->GetStatus().status());
  EXPECT_EQ(HTTP_NOT_FOUND, fetcher->GetResponseCode());
}

TEST_F(EmbeddedTestServerTest, ConcurrentFetches) {
  server_->RegisterRequestHandler(
      base::Bind(&EmbeddedTestServerTest::HandleRequest,
                 base::Unretained(this),
                 "/test1",
                 "Raspberry chocolate",
                 "text/html",
                 HTTP_OK));
  server_->RegisterRequestHandler(
      base::Bind(&EmbeddedTestServerTest::HandleRequest,
                 base::Unretained(this),
                 "/test2",
                 "Vanilla chocolate",
                 "text/html",
                 HTTP_OK));
  server_->RegisterRequestHandler(
      base::Bind(&EmbeddedTestServerTest::HandleRequest,
                 base::Unretained(this),
                 "/test3",
                 "No chocolates",
                 "text/plain",
                 HTTP_NOT_FOUND));

  scoped_ptr<URLFetcher> fetcher1 = scoped_ptr<URLFetcher>(
      URLFetcher::Create(server_->GetURL("/test1"),
                              URLFetcher::GET,
                              this));
  fetcher1->SetRequestContext(request_context_getter_.get());
  scoped_ptr<URLFetcher> fetcher2 = scoped_ptr<URLFetcher>(
      URLFetcher::Create(server_->GetURL("/test2"),
                              URLFetcher::GET,
                              this));
  fetcher2->SetRequestContext(request_context_getter_.get());
  scoped_ptr<URLFetcher> fetcher3 = scoped_ptr<URLFetcher>(
      URLFetcher::Create(server_->GetURL("/test3"),
                              URLFetcher::GET,
                              this));
  fetcher3->SetRequestContext(request_context_getter_.get());

  // Fetch the three URLs concurrently.
  fetcher1->Start();
  fetcher2->Start();
  fetcher3->Start();
  WaitForResponses(3);

  EXPECT_EQ(URLRequestStatus::SUCCESS, fetcher1->GetStatus().status());
  EXPECT_EQ(HTTP_OK, fetcher1->GetResponseCode());
  EXPECT_EQ("Raspberry chocolate", GetContentFromFetcher(*fetcher1));
  EXPECT_EQ("text/html", GetContentTypeFromFetcher(*fetcher1));

  EXPECT_EQ(URLRequestStatus::SUCCESS, fetcher2->GetStatus().status());
  EXPECT_EQ(HTTP_OK, fetcher2->GetResponseCode());
  EXPECT_EQ("Vanilla chocolate", GetContentFromFetcher(*fetcher2));
  EXPECT_EQ("text/html", GetContentTypeFromFetcher(*fetcher2));

  EXPECT_EQ(URLRequestStatus::SUCCESS, fetcher3->GetStatus().status());
  EXPECT_EQ(HTTP_NOT_FOUND, fetcher3->GetResponseCode());
  EXPECT_EQ("No chocolates", GetContentFromFetcher(*fetcher3));
  EXPECT_EQ("text/plain", GetContentTypeFromFetcher(*fetcher3));
}

// Below test exercises EmbeddedTestServer's ability to cope with the situation
// where there is no MessageLoop available on the thread at EmbeddedTestServer
// initialization and/or destruction.

typedef std::tr1::tuple<bool, bool> ThreadingTestParams;

class EmbeddedTestServerThreadingTest
    : public testing::TestWithParam<ThreadingTestParams> {};

class EmbeddedTestServerThreadingTestDelegate
    : public base::PlatformThread::Delegate,
      public URLFetcherDelegate {
 public:
  EmbeddedTestServerThreadingTestDelegate(
      bool message_loop_present_on_initialize,
      bool message_loop_present_on_shutdown)
      : message_loop_present_on_initialize_(message_loop_present_on_initialize),
        message_loop_present_on_shutdown_(message_loop_present_on_shutdown) {}

  // base::PlatformThread::Delegate:
  virtual void ThreadMain() OVERRIDE {
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_runner;
    base::Thread io_thread("io_thread");
    base::Thread::Options thread_options;
    thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
    ASSERT_TRUE(io_thread.StartWithOptions(thread_options));
    io_thread_runner = io_thread.message_loop_proxy();

    scoped_ptr<base::MessageLoop> loop;
    if (message_loop_present_on_initialize_)
      loop.reset(new base::MessageLoopForIO);

    // Create the test server instance.
    EmbeddedTestServer server;
    base::FilePath src_dir;
    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));
    ASSERT_TRUE(server.InitializeAndWaitUntilReady());

    // Make a request and wait for the reply.
    if (!loop)
      loop.reset(new base::MessageLoopForIO);

    scoped_ptr<URLFetcher> fetcher(URLFetcher::Create(
        server.GetURL("/test?q=foo"), URLFetcher::GET, this));
    fetcher->SetRequestContext(
        new TestURLRequestContextGetter(loop->message_loop_proxy()));
    fetcher->Start();
    loop->Run();
    fetcher.reset();

    // Shut down.
    if (message_loop_present_on_shutdown_)
      loop.reset();

    ASSERT_TRUE(server.ShutdownAndWaitUntilComplete());
  }

  // URLFetcherDelegate override.
  virtual void OnURLFetchComplete(const URLFetcher* source) OVERRIDE {
    base::MessageLoop::current()->Quit();
  }

 private:
  bool message_loop_present_on_initialize_;
  bool message_loop_present_on_shutdown_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedTestServerThreadingTestDelegate);
};

TEST_P(EmbeddedTestServerThreadingTest, RunTest) {
  // The actual test runs on a separate thread so it can screw with the presence
  // of a MessageLoop - the test suite already sets up a MessageLoop for the
  // main test thread.
  base::PlatformThreadHandle thread_handle;
  EmbeddedTestServerThreadingTestDelegate delegate(
      std::tr1::get<0>(GetParam()),
      std::tr1::get<1>(GetParam()));
  ASSERT_TRUE(base::PlatformThread::Create(0, &delegate, &thread_handle));
  base::PlatformThread::Join(thread_handle);
}

INSTANTIATE_TEST_CASE_P(EmbeddedTestServerThreadingTestInstantiation,
                        EmbeddedTestServerThreadingTest,
                        testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace test_server
}  // namespace net
