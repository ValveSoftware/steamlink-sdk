// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_url_loader_impl.h"

#include <stdint.h>
#include <string.h>

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "components/scheduler/child/scheduler_tqm_delegate_impl.h"
#include "components/scheduler/child/web_task_runner_impl.h"
#include "components/scheduler/child/worker_scheduler.h"
#include "content/child/request_extra_data.h"
#include "content/child/request_info.h"
#include "content/child/resource_dispatcher.h"
#include "content/public/child/fixed_received_data.h"
#include "content/public/child/request_peer.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/resource_response_info.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "url/gurl.h"

namespace content {
namespace {

const char kTestURL[] = "http://foo";
const char kTestData[] = "blah!";

const char kFtpDirMimeType[] = "text/vnd.chromium.ftp-dir";
// Simple FTP directory listing.  Tests are not concerned with correct parsing,
// but rather correct cleanup when deleted while parsing.  Important details of
// this list are that it contains more than one entry that are not "." or "..".
const char kFtpDirListing[] =
    "drwxr-xr-x    3 ftp      ftp          4096 May 15 18:11 goat\n"
    "drwxr-xr-x    3 ftp      ftp          4096 May 15 18:11 hat";

class TestResourceDispatcher : public ResourceDispatcher {
 public:
  TestResourceDispatcher() :
      ResourceDispatcher(nullptr, nullptr),
      canceled_(false),
      defers_loading_(false) {
  }

  ~TestResourceDispatcher() override {}

  // TestDispatcher implementation:

  int StartAsync(const RequestInfo& request_info,
                 ResourceRequestBodyImpl* request_body,
                 std::unique_ptr<RequestPeer> peer) override {
    EXPECT_FALSE(peer_);
    peer_ = std::move(peer);
    url_ = request_info.url;
    stream_url_ = request_info.resource_body_stream_url;
    return 1;
  }

  void Cancel(int request_id) override {
    EXPECT_FALSE(canceled_);
    canceled_ = true;
  }

  RequestPeer* peer() { return peer_.get(); }

  bool canceled() { return canceled_; }

  const GURL& url() { return url_; }
  const GURL& stream_url() { return stream_url_; }

  void SetDefersLoading(int request_id, bool value) override {
    defers_loading_ = value;
  }
  bool defers_loading() const { return defers_loading_; }

 private:
  std::unique_ptr<RequestPeer> peer_;
  bool canceled_;
  bool defers_loading_;
  GURL url_;
  GURL stream_url_;

  DISALLOW_COPY_AND_ASSIGN(TestResourceDispatcher);
};

class TestWebURLLoaderClient : public blink::WebURLLoaderClient {
 public:
  TestWebURLLoaderClient(ResourceDispatcher* dispatcher,
                         scoped_refptr<scheduler::TaskQueue> task_runner)
      : loader_(new WebURLLoaderImpl(
            dispatcher,
            base::WrapUnique(new scheduler::WebTaskRunnerImpl(task_runner)))),
        delete_on_receive_redirect_(false),
        delete_on_receive_response_(false),
        delete_on_receive_data_(false),
        delete_on_finish_(false),
        delete_on_fail_(false),
        did_receive_redirect_(false),
        did_receive_response_(false),
        check_redirect_request_priority_(false),
        did_finish_(false) {}

  ~TestWebURLLoaderClient() override {}

  // blink::WebURLLoaderClient implementation:
  void willFollowRedirect(
      blink::WebURLLoader* loader,
      blink::WebURLRequest& newRequest,
      const blink::WebURLResponse& redirectResponse) override {
    EXPECT_TRUE(loader_);
    EXPECT_EQ(loader_.get(), loader);

    if (check_redirect_request_priority_)
      EXPECT_EQ(redirect_request_priority, newRequest.getPriority());

    // No test currently simulates mutiple redirects.
    EXPECT_FALSE(did_receive_redirect_);
    did_receive_redirect_ = true;

    if (delete_on_receive_redirect_)
      loader_.reset();
  }

  void didSendData(blink::WebURLLoader* loader,
                   unsigned long long bytesSent,
                   unsigned long long totalBytesToBeSent) override {
    EXPECT_TRUE(loader_);
    EXPECT_EQ(loader_.get(), loader);
  }

  void didReceiveResponse(
      blink::WebURLLoader* loader,
      const blink::WebURLResponse& response) override {
    EXPECT_TRUE(loader_);
    EXPECT_EQ(loader_.get(), loader);
    EXPECT_FALSE(did_receive_response_);

    did_receive_response_ = true;
    response_ = response;
    if (delete_on_receive_response_)
      loader_.reset();
  }

  void didDownloadData(blink::WebURLLoader* loader,
                       int dataLength,
                       int encodedDataLength) override {
    EXPECT_TRUE(loader_);
    EXPECT_EQ(loader_.get(), loader);
  }

  void didReceiveData(blink::WebURLLoader* loader,
                      const char* data,
                      int dataLength,
                      int encodedDataLength) override {
    EXPECT_TRUE(loader_);
    EXPECT_EQ(loader_.get(), loader);
    // The response should have started, but must not have finished, or failed.
    EXPECT_TRUE(did_receive_response_);
    EXPECT_FALSE(did_finish_);
    EXPECT_EQ(net::OK, error_.reason);
    EXPECT_EQ("", error_.domain.utf8());

    received_data_.append(data, dataLength);

    if (delete_on_receive_data_)
      loader_.reset();
  }

  void didReceiveCachedMetadata(blink::WebURLLoader* loader,
                                const char* data,
                                int dataLength) override {
    EXPECT_EQ(loader_.get(), loader);
  }

  void didFinishLoading(blink::WebURLLoader* loader,
                        double finishTime,
                        int64_t totalEncodedDataLength) override {
    EXPECT_TRUE(loader_);
    EXPECT_EQ(loader_.get(), loader);
    EXPECT_TRUE(did_receive_response_);
    EXPECT_FALSE(did_finish_);
    did_finish_ = true;

    if (delete_on_finish_)
      loader_.reset();
  }

  void didFail(blink::WebURLLoader* loader,
               const blink::WebURLError& error) override {
    EXPECT_TRUE(loader_);
    EXPECT_EQ(loader_.get(), loader);
    EXPECT_FALSE(did_finish_);
    error_ = error;

    if (delete_on_fail_)
      loader_.reset();
  }

  WebURLLoaderImpl* loader() { return loader_.get(); }
  void DeleteLoader() {
    loader_.reset();
  }

  void set_delete_on_receive_redirect() { delete_on_receive_redirect_ = true; }
  void set_delete_on_receive_response() { delete_on_receive_response_ = true; }
  void set_delete_on_receive_data() { delete_on_receive_data_ = true; }
  void set_delete_on_finish() { delete_on_finish_ = true; }
  void set_delete_on_fail() { delete_on_fail_ = true; }
  void set_redirect_request_priority(blink::WebURLRequest::Priority priority) {
    check_redirect_request_priority_ = true;
    redirect_request_priority = priority;
  }

  bool did_receive_redirect() const { return did_receive_redirect_; }
  bool did_receive_response() const { return did_receive_response_; }
  const std::string& received_data() const { return received_data_; }
  bool did_finish() const { return did_finish_; }
  const blink::WebURLError& error() const { return error_; }
  const blink::WebURLResponse& response() const { return response_; }

 private:
  std::unique_ptr<WebURLLoaderImpl> loader_;

  bool delete_on_receive_redirect_;
  bool delete_on_receive_response_;
  bool delete_on_receive_data_;
  bool delete_on_finish_;
  bool delete_on_fail_;

  bool did_receive_redirect_;
  bool did_receive_response_;
  bool check_redirect_request_priority_;
  blink::WebURLRequest::Priority redirect_request_priority;
  std::string received_data_;
  bool did_finish_;
  blink::WebURLError error_;
  blink::WebURLResponse response_;

  DISALLOW_COPY_AND_ASSIGN(TestWebURLLoaderClient);
};

class WebURLLoaderImplTest : public testing::Test {
 public:
  explicit WebURLLoaderImplTest()
      : worker_scheduler_(scheduler::WorkerScheduler::Create(
            scheduler::SchedulerTqmDelegateImpl::Create(
                &message_loop_,
                base::WrapUnique(new base::DefaultTickClock())))) {
    worker_scheduler_->Init();
    client_.reset(new TestWebURLLoaderClient(
        &dispatcher_, worker_scheduler_->DefaultTaskRunner()));
  }

  ~WebURLLoaderImplTest() override {}

  void DoStartAsyncRequest() {
    blink::WebURLRequest request;
    request.initialize();
    request.setURL(GURL(kTestURL));
    client()->loader()->loadAsynchronously(request, client());
    ASSERT_TRUE(peer());
  }

  void DoStartAsyncRequestWithPriority(
      blink::WebURLRequest::Priority priority) {
    blink::WebURLRequest request;
    request.initialize();
    request.setPriority(priority);
    request.setURL(GURL(kTestURL));
    client()->loader()->loadAsynchronously(request, client());
    ASSERT_TRUE(peer());
  }

  void DoReceiveRedirect() {
    EXPECT_FALSE(client()->did_receive_redirect());
    net::RedirectInfo redirect_info;
    redirect_info.status_code = 302;
    redirect_info.new_method = "GET";
    redirect_info.new_url = GURL(kTestURL);
    redirect_info.new_first_party_for_cookies = GURL(kTestURL);
    peer()->OnReceivedRedirect(redirect_info,
                               content::ResourceResponseInfo());
    EXPECT_TRUE(client()->did_receive_redirect());
  }

  void DoReceiveResponse() {
    EXPECT_FALSE(client()->did_receive_response());
    peer()->OnReceivedResponse(content::ResourceResponseInfo());
    EXPECT_TRUE(client()->did_receive_response());
  }

  // Assumes it is called only once for a request.
  void DoReceiveData() {
    EXPECT_EQ("", client()->received_data());
    peer()->OnReceivedData(base::WrapUnique(new FixedReceivedData(
        kTestData, strlen(kTestData), strlen(kTestData))));
    EXPECT_EQ(kTestData, client()->received_data());
  }

  void DoCompleteRequest() {
    EXPECT_FALSE(client()->did_finish());
    peer()->OnCompletedRequest(net::OK, false, false, "", base::TimeTicks(),
                               strlen(kTestData));
    EXPECT_TRUE(client()->did_finish());
    // There should be no error.
    EXPECT_EQ(net::OK, client()->error().reason);
    EXPECT_EQ("", client()->error().domain.utf8());
  }

  void DoFailRequest() {
    EXPECT_FALSE(client()->did_finish());
    peer()->OnCompletedRequest(net::ERR_FAILED, false, false, "",
                               base::TimeTicks(), strlen(kTestData));
    EXPECT_FALSE(client()->did_finish());
    EXPECT_EQ(net::ERR_FAILED, client()->error().reason);
    EXPECT_EQ(net::kErrorDomain, client()->error().domain.utf8());
  }

  void DoReceiveResponseFtp() {
    EXPECT_FALSE(client()->did_receive_response());
    content::ResourceResponseInfo response_info;
    response_info.mime_type = kFtpDirMimeType;
    peer()->OnReceivedResponse(response_info);
    EXPECT_TRUE(client()->did_receive_response());
  }

  void DoReceiveDataFtp() {
    peer()->OnReceivedData(base::WrapUnique(new FixedReceivedData(
        kFtpDirListing, strlen(kFtpDirListing), strlen(kFtpDirListing))));
    // The FTP delegate should modify the data the client sees.
    EXPECT_NE(kFtpDirListing, client()->received_data());
  }

  TestWebURLLoaderClient* client() { return client_.get(); }
  TestResourceDispatcher* dispatcher() { return &dispatcher_; }
  RequestPeer* peer() { return dispatcher()->peer(); }
  base::MessageLoop* message_loop() { return &message_loop_; }

 private:
  base::MessageLoop message_loop_;
  // WorkerScheduler is needed because WebURLLoaderImpl needs a
  // scheduler::TaskQueue.
  std::unique_ptr<scheduler::WorkerScheduler> worker_scheduler_;
  TestResourceDispatcher dispatcher_;
  std::unique_ptr<TestWebURLLoaderClient> client_;
};

TEST_F(WebURLLoaderImplTest, Success) {
  DoStartAsyncRequest();
  DoReceiveResponse();
  DoReceiveData();
  DoCompleteRequest();
  EXPECT_FALSE(dispatcher()->canceled());
  EXPECT_EQ(kTestData, client()->received_data());
}

TEST_F(WebURLLoaderImplTest, Redirect) {
  DoStartAsyncRequest();
  DoReceiveRedirect();
  DoReceiveResponse();
  DoReceiveData();
  DoCompleteRequest();
  EXPECT_FALSE(dispatcher()->canceled());
  EXPECT_EQ(kTestData, client()->received_data());
}

TEST_F(WebURLLoaderImplTest, RedirectCorrectPriority) {
  DoStartAsyncRequestWithPriority(
      blink::WebURLRequest::Priority::PriorityVeryHigh);
  client()->set_redirect_request_priority(
      blink::WebURLRequest::Priority::PriorityVeryHigh);
  DoReceiveRedirect();
  DoReceiveResponse();
  DoReceiveData();
  DoCompleteRequest();
  EXPECT_FALSE(dispatcher()->canceled());
  EXPECT_EQ(kTestData, client()->received_data());
}

TEST_F(WebURLLoaderImplTest, Failure) {
  DoStartAsyncRequest();
  DoReceiveResponse();
  DoReceiveData();
  DoFailRequest();
  EXPECT_FALSE(dispatcher()->canceled());
}

// The client may delete the WebURLLoader during any callback from the loader.
// These tests make sure that doesn't result in a crash.
TEST_F(WebURLLoaderImplTest, DeleteOnReceiveRedirect) {
  client()->set_delete_on_receive_redirect();
  DoStartAsyncRequest();
  DoReceiveRedirect();
}

TEST_F(WebURLLoaderImplTest, DeleteOnReceiveResponse) {
  client()->set_delete_on_receive_response();
  DoStartAsyncRequest();
  DoReceiveResponse();
}

TEST_F(WebURLLoaderImplTest, DeleteOnReceiveData) {
  client()->set_delete_on_receive_data();
  DoStartAsyncRequest();
  DoReceiveResponse();
  DoReceiveData();
}

TEST_F(WebURLLoaderImplTest, DeleteOnFinish) {
  client()->set_delete_on_finish();
  DoStartAsyncRequest();
  DoReceiveResponse();
  DoReceiveData();
  DoCompleteRequest();
}

TEST_F(WebURLLoaderImplTest, DeleteOnFail) {
  client()->set_delete_on_fail();
  DoStartAsyncRequest();
  DoReceiveResponse();
  DoReceiveData();
  DoFailRequest();
}

TEST_F(WebURLLoaderImplTest, DeleteBeforeResponseDataURL) {
  blink::WebURLRequest request;
  request.initialize();
  request.setURL(GURL("data:text/html;charset=utf-8,blah!"));
  client()->loader()->loadAsynchronously(request, client());
  client()->DeleteLoader();
  message_loop()->RunUntilIdle();
  EXPECT_FALSE(client()->did_receive_response());
}

// Data URL tests.

TEST_F(WebURLLoaderImplTest, DataURL) {
  blink::WebURLRequest request;
  request.initialize();
  request.setURL(GURL("data:text/html;charset=utf-8,blah!"));
  client()->loader()->loadAsynchronously(request, client());
  message_loop()->RunUntilIdle();
  EXPECT_EQ("blah!", client()->received_data());
  EXPECT_TRUE(client()->did_finish());
  EXPECT_EQ(net::OK, client()->error().reason);
  EXPECT_EQ("", client()->error().domain.utf8());
}

TEST_F(WebURLLoaderImplTest, DataURLDeleteOnReceiveResponse) {
  blink::WebURLRequest request;
  request.initialize();
  request.setURL(GURL("data:text/html;charset=utf-8,blah!"));
  client()->set_delete_on_receive_response();
  client()->loader()->loadAsynchronously(request, client());
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(client()->did_receive_response());
  EXPECT_EQ("", client()->received_data());
  EXPECT_FALSE(client()->did_finish());
}

TEST_F(WebURLLoaderImplTest, DataURLDeleteOnReceiveData) {
  blink::WebURLRequest request;
  request.initialize();
  request.setURL(GURL("data:text/html;charset=utf-8,blah!"));
  client()->set_delete_on_receive_data();
  client()->loader()->loadAsynchronously(request, client());
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(client()->did_receive_response());
  EXPECT_EQ("blah!", client()->received_data());
  EXPECT_FALSE(client()->did_finish());
}

TEST_F(WebURLLoaderImplTest, DataURLDeleteOnFinish) {
  blink::WebURLRequest request;
  request.initialize();
  request.setURL(GURL("data:text/html;charset=utf-8,blah!"));
  client()->set_delete_on_finish();
  client()->loader()->loadAsynchronously(request, client());
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(client()->did_receive_response());
  EXPECT_EQ("blah!", client()->received_data());
  EXPECT_TRUE(client()->did_finish());
}

TEST_F(WebURLLoaderImplTest, DataURLDefersLoading) {
  blink::WebURLRequest request;
  request.initialize();
  request.setURL(GURL("data:text/html;charset=utf-8,blah!"));
  client()->loader()->loadAsynchronously(request, client());

  // setDefersLoading() might be called with either false or true in no
  // specific order. The user of the API will not have sufficient information
  // about the WebURLLoader's internal state, so the latter gracefully needs to
  // handle calling setDefersLoading any number of times with any values from
  // any point in time.

  client()->loader()->setDefersLoading(false);
  client()->loader()->setDefersLoading(true);
  client()->loader()->setDefersLoading(true);
  message_loop()->RunUntilIdle();
  EXPECT_FALSE(client()->did_finish());

  client()->loader()->setDefersLoading(false);
  client()->loader()->setDefersLoading(true);
  message_loop()->RunUntilIdle();
  EXPECT_FALSE(client()->did_finish());

  client()->loader()->setDefersLoading(false);
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(client()->did_finish());

  client()->loader()->setDefersLoading(true);
  client()->loader()->setDefersLoading(false);
  client()->loader()->setDefersLoading(false);
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(client()->did_finish());

  EXPECT_EQ("blah!", client()->received_data());
  EXPECT_EQ(net::OK, client()->error().reason);
  EXPECT_EQ("", client()->error().domain.utf8());
}

TEST_F(WebURLLoaderImplTest, DefersLoadingBeforeStart) {
  client()->loader()->setDefersLoading(true);
  EXPECT_FALSE(dispatcher()->defers_loading());
  DoStartAsyncRequest();
  EXPECT_TRUE(dispatcher()->defers_loading());
}

// FTP integration tests.  These are focused more on safe deletion than correct
// parsing of FTP responses.

TEST_F(WebURLLoaderImplTest, Ftp) {
  DoStartAsyncRequest();
  DoReceiveResponseFtp();
  DoReceiveDataFtp();
  DoCompleteRequest();
  EXPECT_FALSE(dispatcher()->canceled());
}

TEST_F(WebURLLoaderImplTest, FtpDeleteOnReceiveResponse) {
  client()->set_delete_on_receive_response();
  DoStartAsyncRequest();
  DoReceiveResponseFtp();

  // No data should have been received.
  EXPECT_EQ("", client()->received_data());
}

TEST_F(WebURLLoaderImplTest, FtpDeleteOnReceiveFirstData) {
  client()->set_delete_on_receive_data();
  DoStartAsyncRequest();
  DoReceiveResponseFtp();

  EXPECT_NE("", client()->received_data());
}

TEST_F(WebURLLoaderImplTest, FtpDeleteOnReceiveMoreData) {
  DoStartAsyncRequest();
  DoReceiveResponseFtp();
  DoReceiveDataFtp();

  // Directory listings are only parsed once the request completes, so this will
  // cancel in DoReceiveDataFtp, before the request finishes.
  client()->set_delete_on_receive_data();
  peer()->OnCompletedRequest(net::OK, false, false, "", base::TimeTicks(),
                              strlen(kTestData));
  EXPECT_FALSE(client()->did_finish());
}

TEST_F(WebURLLoaderImplTest, FtpDeleteOnFinish) {
  client()->set_delete_on_finish();
  DoStartAsyncRequest();
  DoReceiveResponseFtp();
  DoReceiveDataFtp();
  DoCompleteRequest();
}

TEST_F(WebURLLoaderImplTest, FtpDeleteOnFail) {
  client()->set_delete_on_fail();
  DoStartAsyncRequest();
  DoReceiveResponseFtp();
  DoReceiveDataFtp();
  DoFailRequest();
}

// PlzNavigate: checks that the stream override parameters provided on
// navigation commit are properly applied.
TEST_F(WebURLLoaderImplTest, BrowserSideNavigationCommit) {
  // Initialize the request and the stream override.
  const GURL kNavigationURL = GURL(kTestURL);
  const GURL kStreamURL = GURL("http://bar");
  const std::string kMimeType = "text/html";
  blink::WebURLRequest request;
  request.initialize();
  request.setURL(kNavigationURL);
  request.setFrameType(blink::WebURLRequest::FrameTypeTopLevel);
  request.setRequestContext(blink::WebURLRequest::RequestContextFrame);
  std::unique_ptr<StreamOverrideParameters> stream_override(
      new StreamOverrideParameters());
  stream_override->stream_url = kStreamURL;
  stream_override->response.mime_type = kMimeType;
  RequestExtraData* extra_data = new RequestExtraData();
  extra_data->set_stream_override(std::move(stream_override));
  request.setExtraData(extra_data);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableBrowserSideNavigation);

  client()->loader()->loadAsynchronously(request, client());

  // The stream url should have been added to the RequestInfo.
  ASSERT_TRUE(peer());
  EXPECT_EQ(kNavigationURL, dispatcher()->url());
  EXPECT_EQ(kStreamURL, dispatcher()->stream_url());

  EXPECT_FALSE(client()->did_receive_response());
  peer()->OnReceivedResponse(content::ResourceResponseInfo());
  EXPECT_TRUE(client()->did_receive_response());

  // The response info should have been overriden.
  ASSERT_FALSE(client()->response().isNull());
  EXPECT_EQ(kMimeType, client()->response().mimeType().latin1());

  DoReceiveData();
  DoCompleteRequest();
  EXPECT_FALSE(dispatcher()->canceled());
  EXPECT_EQ(kTestData, client()->received_data());
}

TEST_F(WebURLLoaderImplTest, ResponseIPAddress) {
  GURL url("http://example.test/");

  struct TestCase {
    const char* ip;
    const char* expected;
  } cases[] = {
      {"127.0.0.1", "127.0.0.1"},
      {"123.123.123.123", "123.123.123.123"},
      {"::1", "[::1]"},
      {"2001:0db8:85a3:0000:0000:8a2e:0370:7334",
       "[2001:0db8:85a3:0000:0000:8a2e:0370:7334]"},
      {"2001:db8:85a3:0:0:8a2e:370:7334", "[2001:db8:85a3:0:0:8a2e:370:7334]"},
      {"2001:db8:85a3::8a2e:370:7334", "[2001:db8:85a3::8a2e:370:7334]"},
      {"::ffff:192.0.2.128", "[::ffff:192.0.2.128]"}};

  for (const auto& test : cases) {
    SCOPED_TRACE(test.ip);
    content::ResourceResponseInfo info;
    info.socket_address = net::HostPortPair(test.ip, 443);
    blink::WebURLResponse response;
    response.initialize();
    WebURLLoaderImpl::PopulateURLResponse(url, info, &response, true);
    EXPECT_EQ(test.expected, response.remoteIPAddress().utf8());
  };
}

}  // namespace
}  // namespace content
