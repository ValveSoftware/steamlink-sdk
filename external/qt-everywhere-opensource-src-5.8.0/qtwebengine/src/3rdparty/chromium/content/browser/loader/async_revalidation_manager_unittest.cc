// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/async_revalidation_manager.h"

#include <deque>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory_handle.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader_delegate_impl.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/resource_messages.h"
#include "content/common/resource_request.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/appcache_info.h"
#include "content/public/common/process_type.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_param_traits.h"
#include "net/base/load_flags.h"
#include "net/base/network_delegate.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

namespace {

// This class is a variation on URLRequestTestJob that
// returns ERR_IO_PENDING before every read, not just the first one.
class URLRequestTestDelayedCompletionJob : public net::URLRequestTestJob {
 public:
  URLRequestTestDelayedCompletionJob(net::URLRequest* request,
                                     net::NetworkDelegate* network_delegate,
                                     const std::string& response_headers,
                                     const std::string& response_data)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               response_headers,
                               response_data,
                               false) {}

 private:
  bool NextReadAsync() override { return true; }
};

// A URLRequestJob implementation which sets the |async_revalidation_required|
// flag on the HttpResponseInfo object to true if the request has the
// LOAD_SUPPORT_ASYNC_REVALIDATION flag.
class AsyncRevalidationRequiredURLRequestTestJob
    : public net::URLRequestTestJob {
 public:
  AsyncRevalidationRequiredURLRequestTestJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate)
      : URLRequestTestJob(request,
                          network_delegate,
                          net::URLRequestTestJob::test_headers(),
                          std::string(),
                          false) {}

  void GetResponseInfo(net::HttpResponseInfo* info) override {
    URLRequestTestJob::GetResponseInfo(info);
    if (request()->load_flags() & net::LOAD_SUPPORT_ASYNC_REVALIDATION)
      info->async_revalidation_required = true;
  }
};

// A URLRequestJob implementation which serves a redirect and sets the
// |async_revalidation_required| flag on the HttpResponseInfo object to true if
// the request has the LOAD_SUPPORT_ASYNC_REVALIDATION flag.
class RedirectAndRevalidateURLRequestTestJob : public net::URLRequestTestJob {
 public:
  RedirectAndRevalidateURLRequestTestJob(net::URLRequest* request,
                                         net::NetworkDelegate* network_delegate)
      : URLRequestTestJob(request,
                          network_delegate,
                          CreateRedirectHeaders(),
                          std::string(),
                          false) {}

  void GetResponseInfo(net::HttpResponseInfo* info) override {
    URLRequestTestJob::GetResponseInfo(info);
    if (request()->load_flags() & net::LOAD_SUPPORT_ASYNC_REVALIDATION)
      info->async_revalidation_required = true;
  }

 private:
  static std::string CreateRedirectHeaders() {
    static const char kRedirectHeaders[] =
        "HTTP/1.1 302 MOVED\n"
        "Location: http://example.com/async-revalidate/from-redirect\n"
        "\n";
    return std::string(kRedirectHeaders, arraysize(kRedirectHeaders) - 1);
  }
};

class TestURLRequestJobFactory : public net::URLRequestJobFactory {
 public:
  TestURLRequestJobFactory() = default;

  // Sets the contents of the response. |headers| should have "\n" as line
  // breaks and end in "\n\n".
  void SetResponse(const std::string& headers, const std::string& data) {
    response_headers_ = headers;
    response_data_ = data;
  }

  net::URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    std::string path = request->url().path();
    if (base::StartsWith(path, "/async-revalidate",
                         base::CompareCase::SENSITIVE)) {
      return new AsyncRevalidationRequiredURLRequestTestJob(request,
                                                            network_delegate);
    }
    if (base::StartsWith(path, "/redirect", base::CompareCase::SENSITIVE)) {
      return new RedirectAndRevalidateURLRequestTestJob(request,
                                                        network_delegate);
    }
    return new URLRequestTestDelayedCompletionJob(
        request, network_delegate, response_headers_, response_data_);
  }

  net::URLRequestJob* MaybeInterceptRedirect(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const GURL& location) const override {
    return nullptr;
  }

  net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return nullptr;
  }

  bool IsHandledProtocol(const std::string& scheme) const override {
    // If non-standard schemes need to be tested in future it will be
    // necessary to call ChildProcessSecurityPolicyImpl::
    // RegisterWebSafeScheme() for them.
    return scheme == url::kHttpScheme || scheme == url::kHttpsScheme;
  }

  bool IsHandledURL(const GURL& url) const override {
    return IsHandledProtocol(url.scheme());
  }

  bool IsSafeRedirectTarget(const GURL& location) const override {
    return false;
  }

 private:
  std::string response_headers_;
  std::string response_data_;

  DISALLOW_COPY_AND_ASSIGN(TestURLRequestJobFactory);
};

// On Windows, ResourceMsg_SetDataBuffer supplies a HANDLE which is not
// automatically released.
//
// See ResourceDispatcher::ReleaseResourcesInDataMessage.
//
// TODO(ricea): Maybe share this implementation with
// resource_dispatcher_host_unittest.cc.
void ReleaseHandlesInMessage(const IPC::Message& message) {
  if (message.type() == ResourceMsg_SetDataBuffer::ID) {
    base::PickleIterator iter(message);
    int request_id;
    CHECK(iter.ReadInt(&request_id));
    base::SharedMemoryHandle shm_handle;
    if (IPC::ParamTraits<base::SharedMemoryHandle>::Read(&message, &iter,
                                                         &shm_handle)) {
      if (base::SharedMemory::IsHandleValid(shm_handle))
        base::SharedMemory::CloseHandle(shm_handle);
    }
  }
}

// This filter just deletes any messages that are sent through it.
class BlackholeFilter : public ResourceMessageFilter {
 public:
  explicit BlackholeFilter(ResourceContext* resource_context)
      : ResourceMessageFilter(
            ChildProcessHostImpl::GenerateChildProcessUniqueId(),
            PROCESS_TYPE_RENDERER,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            base::Bind(&BlackholeFilter::GetContexts, base::Unretained(this))),
        resource_context_(resource_context) {
    ChildProcessSecurityPolicyImpl::GetInstance()->Add(child_id());
  }

  bool Send(IPC::Message* msg) override {
    std::unique_ptr<IPC::Message> take_ownership(msg);
    ReleaseHandlesInMessage(*msg);
    return true;
  }

 private:
  ~BlackholeFilter() override {
    ChildProcessSecurityPolicyImpl::GetInstance()->Remove(child_id());
  }

  void GetContexts(ResourceType resource_type,
                   int origin_pid,
                   ResourceContext** resource_context,
                   net::URLRequestContext** request_context) {
    *resource_context = resource_context_;
    *request_context = resource_context_->GetRequestContext();
  }

  ResourceContext* resource_context_;

  DISALLOW_COPY_AND_ASSIGN(BlackholeFilter);
};

ResourceRequest CreateResourceRequest(const char* method,
                                      ResourceType type,
                                      const GURL& url) {
  ResourceRequest request;
  request.method = std::string(method);
  request.url = url;
  request.first_party_for_cookies = url;  // Bypass third-party cookie blocking.
  request.referrer_policy = blink::WebReferrerPolicyDefault;
  request.load_flags = 0;
  request.origin_pid = 0;
  request.resource_type = type;
  request.request_context = 0;
  request.appcache_host_id = kAppCacheNoHostId;
  request.download_to_file = false;
  request.should_reset_appcache = false;
  request.is_main_frame = true;
  request.parent_is_main_frame = false;
  request.parent_render_frame_id = -1;
  request.transition_type = ui::PAGE_TRANSITION_LINK;
  request.allow_download = true;
  return request;
}

class AsyncRevalidationManagerTest : public ::testing::Test {
 protected:
  AsyncRevalidationManagerTest(
      std::unique_ptr<net::TestNetworkDelegate> network_delegate)
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        network_delegate_(std::move(network_delegate)) {
    host_.SetLoaderDelegate(&loader_delegate_);
    browser_context_.reset(new TestBrowserContext());
    BrowserContext::EnsureResourceContextInitialized(browser_context_.get());
    base::RunLoop().RunUntilIdle();
    ResourceContext* resource_context = browser_context_->GetResourceContext();
    filter_ = new BlackholeFilter(resource_context);
    net::URLRequestContext* request_context =
        resource_context->GetRequestContext();
    job_factory_.reset(new TestURLRequestJobFactory);
    request_context->set_job_factory(job_factory_.get());
    request_context->set_network_delegate(network_delegate_.get());
    host_.EnableStaleWhileRevalidateForTesting();
  }

  AsyncRevalidationManagerTest()
      : AsyncRevalidationManagerTest(
            base::WrapUnique(new net::TestNetworkDelegate)) {}

  void TearDown() override {
    host_.CancelRequestsForProcess(filter_->child_id());
    host_.Shutdown();
    host_.CancelRequestsForContext(browser_context_->GetResourceContext());
    browser_context_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void SetResponse(const std::string& headers, const std::string& data) {
    job_factory_->SetResponse(headers, data);
  }

  // Creates a request using the current test object as the filter and
  // SubResource as the resource type.
  void MakeTestRequest(int render_view_id, int request_id, const GURL& url) {
    ResourceRequest request =
        CreateResourceRequest("GET", RESOURCE_TYPE_SUB_RESOURCE, url);
    ResourceHostMsg_RequestResource msg(render_view_id, request_id, request);
    host_.OnMessageReceived(msg, filter_.get());
    base::RunLoop().RunUntilIdle();
  }

  void EnsureSchemeIsAllowed(const std::string& scheme) {
    ChildProcessSecurityPolicyImpl* policy =
        ChildProcessSecurityPolicyImpl::GetInstance();
    if (!policy->IsWebSafeScheme(scheme))
      policy->RegisterWebSafeScheme(scheme);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestURLRequestJobFactory> job_factory_;
  scoped_refptr<BlackholeFilter> filter_;
  std::unique_ptr<net::TestNetworkDelegate> network_delegate_;
  LoaderDelegateImpl loader_delegate_;
  ResourceDispatcherHostImpl host_;
};

TEST_F(AsyncRevalidationManagerTest, SupportsAsyncRevalidation) {
  SetResponse(net::URLRequestTestJob::test_headers(), "delay complete");
  MakeTestRequest(0, 1, GURL("http://example.com/baz"));

  net::URLRequest* url_request(
      host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 1)));
  ASSERT_TRUE(url_request);

  EXPECT_TRUE(url_request->load_flags() & net::LOAD_SUPPORT_ASYNC_REVALIDATION);
}

TEST_F(AsyncRevalidationManagerTest, AsyncRevalidationNotSupportedForPOST) {
  SetResponse(net::URLRequestTestJob::test_headers(), "delay complete");
  // Create POST request.
  ResourceRequest request = CreateResourceRequest(
      "POST", RESOURCE_TYPE_SUB_RESOURCE, GURL("http://example.com/baz.php"));
  ResourceHostMsg_RequestResource msg(0, 1, request);
  host_.OnMessageReceived(msg, filter_.get());
  base::RunLoop().RunUntilIdle();

  net::URLRequest* url_request(
      host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 1)));
  ASSERT_TRUE(url_request);

  EXPECT_FALSE(url_request->load_flags() &
               net::LOAD_SUPPORT_ASYNC_REVALIDATION);
}

TEST_F(AsyncRevalidationManagerTest,
       AsyncRevalidationNotSupportedAfterRedirect) {
  static const char kRedirectHeaders[] =
      "HTTP/1.1 302 MOVED\n"
      "Location: http://example.com/var\n"
      "\n";
  SetResponse(kRedirectHeaders, "");

  MakeTestRequest(0, 1, GURL("http://example.com/baz"));

  net::URLRequest* url_request(
      host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 1)));
  ASSERT_TRUE(url_request);

  EXPECT_FALSE(url_request->load_flags() &
               net::LOAD_SUPPORT_ASYNC_REVALIDATION);
}

// A NetworkDelegate that records the URLRequests as they are created.
class URLRequestRecordingNetworkDelegate : public net::TestNetworkDelegate {
 public:
  URLRequestRecordingNetworkDelegate() : requests_() {}

  net::URLRequest* NextRequest() {
    EXPECT_FALSE(requests_.empty());
    if (requests_.empty())
      return nullptr;
    net::URLRequest* request = requests_.front();
    EXPECT_TRUE(request);
    requests_.pop_front();
    return request;
  }

  bool NextRequestWasDestroyed() {
    net::URLRequest* request = requests_.front();
    requests_.pop_front();
    return request == nullptr;
  }

  bool IsEmpty() const { return requests_.empty(); }

  int OnBeforeURLRequest(net::URLRequest* request,
                         const net::CompletionCallback& callback,
                         GURL* new_url) override {
    requests_.push_back(request);
    return TestNetworkDelegate::OnBeforeURLRequest(request, callback, new_url);
  }

  void OnURLRequestDestroyed(net::URLRequest* request) override {
    for (auto& recorded_request : requests_) {
      if (recorded_request == request)
        recorded_request = nullptr;
    }
    net::TestNetworkDelegate::OnURLRequestDestroyed(request);
  }

 private:
  std::deque<net::URLRequest*> requests_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestRecordingNetworkDelegate);
};

class AsyncRevalidationManagerRecordingTest
    : public AsyncRevalidationManagerTest {
 public:
  AsyncRevalidationManagerRecordingTest()
      : AsyncRevalidationManagerTest(
            base::WrapUnique(new URLRequestRecordingNetworkDelegate)) {}

  void TearDown() override {
    EXPECT_TRUE(IsEmpty());
    AsyncRevalidationManagerTest::TearDown();
  }

  URLRequestRecordingNetworkDelegate* recording_network_delegate() const {
    return static_cast<URLRequestRecordingNetworkDelegate*>(
        network_delegate_.get());
  }

  bool NextRequestWasDestroyed() {
    return recording_network_delegate()->NextRequestWasDestroyed();
  }

  net::URLRequest* NextRequest() {
    return recording_network_delegate()->NextRequest();
  }

  bool IsEmpty() const { return recording_network_delegate()->IsEmpty(); }
};

// Verify that an async revalidation is actually created when needed.
TEST_F(AsyncRevalidationManagerRecordingTest, Issued) {
  // Create the original request.
  MakeTestRequest(0, 1, GURL("http://example.com/async-revalidate"));

  net::URLRequest* initial_request = NextRequest();
  ASSERT_TRUE(initial_request);
  EXPECT_TRUE(initial_request->load_flags() &
              net::LOAD_SUPPORT_ASYNC_REVALIDATION);

  net::URLRequest* async_request = NextRequest();
  ASSERT_TRUE(async_request);
}

// Verify the the URL of the async revalidation matches the original request.
TEST_F(AsyncRevalidationManagerRecordingTest, URLMatches) {
  // Create the original request.
  MakeTestRequest(0, 1, GURL("http://example.com/async-revalidate/u"));

  // Discard the original request.
  NextRequest();

  net::URLRequest* async_request = NextRequest();
  ASSERT_TRUE(async_request);
  EXPECT_EQ(GURL("http://example.com/async-revalidate/u"),
            async_request->url());
}

TEST_F(AsyncRevalidationManagerRecordingTest,
       AsyncRevalidationsDoNotSupportAsyncRevalidation) {
  // Create the original request.
  MakeTestRequest(0, 1, GURL("http://example.com/async-revalidate"));

  // Discard the original request.
  NextRequest();

  // Get the async revalidation request.
  net::URLRequest* async_request = NextRequest();
  ASSERT_TRUE(async_request);
  EXPECT_FALSE(async_request->load_flags() &
               net::LOAD_SUPPORT_ASYNC_REVALIDATION);
}

TEST_F(AsyncRevalidationManagerRecordingTest, AsyncRevalidationsNotDuplicated) {
  // Create the original request.
  MakeTestRequest(0, 1, GURL("http://example.com/async-revalidate"));

  // Discard the original request.
  NextRequest();

  // Get the async revalidation request.
  net::URLRequest* async_request = NextRequest();
  EXPECT_TRUE(async_request);

  // Start a second request to the same URL.
  MakeTestRequest(0, 2, GURL("http://example.com/async-revalidate"));

  // Discard the second request.
  NextRequest();

  // There should not be another async revalidation request.
  EXPECT_TRUE(IsEmpty());
}

// Async revalidation to different URLs should not be treated as duplicates.
TEST_F(AsyncRevalidationManagerRecordingTest,
       AsyncRevalidationsToSeparateURLsAreSeparate) {
  // Create two requests to two URLs.
  MakeTestRequest(0, 1, GURL("http://example.com/async-revalidate"));
  MakeTestRequest(0, 2, GURL("http://example.com/async-revalidate2"));

  net::URLRequest* initial_request = NextRequest();
  ASSERT_TRUE(initial_request);
  net::URLRequest* initial_async_revalidation = NextRequest();
  ASSERT_TRUE(initial_async_revalidation);
  net::URLRequest* second_request = NextRequest();
  ASSERT_TRUE(second_request);
  net::URLRequest* second_async_revalidation = NextRequest();
  ASSERT_TRUE(second_async_revalidation);

  EXPECT_EQ("http://example.com/async-revalidate",
            initial_request->url().spec());
  EXPECT_EQ("http://example.com/async-revalidate",
            initial_async_revalidation->url().spec());
  EXPECT_EQ("http://example.com/async-revalidate2",
            second_request->url().spec());
  EXPECT_EQ("http://example.com/async-revalidate2",
            second_async_revalidation->url().spec());
}

// Nothing after the first redirect leg has stale-while-revalidate applied.
// TODO(ricea): s-w-r should work with redirects. Change this test when it does.
TEST_F(AsyncRevalidationManagerRecordingTest, NoSWRAfterFirstRedirectLeg) {
  MakeTestRequest(0, 1, GURL("http://example.com/redirect"));

  net::URLRequest* initial_request = NextRequest();
  EXPECT_TRUE(initial_request);

  EXPECT_FALSE(initial_request->load_flags() &
               net::LOAD_SUPPORT_ASYNC_REVALIDATION);

  // An async revalidation happens for the redirect. It has no body, so it has
  // already completed.
  EXPECT_TRUE(NextRequestWasDestroyed());

  // But no others.
  EXPECT_TRUE(IsEmpty());
}

}  // namespace

}  // namespace content
