// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stack>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "content/browser/appcache/mock_appcache_policy.h"
#include "content/browser/appcache/mock_appcache_service.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/appcache/appcache.h"
#include "webkit/browser/appcache/appcache_backend_impl.h"
#include "webkit/browser/appcache/appcache_request_handler.h"
#include "webkit/browser/appcache/appcache_url_request_job.h"

using appcache::AppCache;
using appcache::AppCacheBackendImpl;
using appcache::AppCacheEntry;
using appcache::AppCacheFrontend;
using appcache::AppCacheGroup;
using appcache::AppCacheHost;
using appcache::AppCacheInfo;
using appcache::AppCacheRequestHandler;
using appcache::AppCacheURLRequestJob;
using appcache::kAppCacheNoCacheId;

namespace content {

static const int kMockProcessId = 1;

class AppCacheRequestHandlerTest : public testing::Test {
 public:
  class MockFrontend : public AppCacheFrontend {
   public:
    virtual void OnCacheSelected(
        int host_id, const appcache::AppCacheInfo& info) OVERRIDE {}

    virtual void OnStatusChanged(const std::vector<int>& host_ids,
                                 appcache::AppCacheStatus status) OVERRIDE {}

    virtual void OnEventRaised(const std::vector<int>& host_ids,
                               appcache::AppCacheEventID event_id) OVERRIDE {}

    virtual void OnErrorEventRaised(
        const std::vector<int>& host_ids,
        const appcache::AppCacheErrorDetails& details) OVERRIDE {}

    virtual void OnProgressEventRaised(const std::vector<int>& host_ids,
                                       const GURL& url,
                                       int num_total,
                                       int num_complete) OVERRIDE {
    }

    virtual void OnLogMessage(int host_id,
                              appcache::AppCacheLogLevel log_level,
                              const std::string& message) OVERRIDE {
    }

    virtual void OnContentBlocked(int host_id,
                                  const GURL& manifest_url) OVERRIDE {}
  };

  // Helper callback to run a test on our io_thread. The io_thread is spun up
  // once and reused for all tests.
  template <class Method>
  void MethodWrapper(Method method) {
    SetUpTest();
    (this->*method)();
  }

  // Subclasses to simulate particular responses so test cases can
  // exercise fallback code paths.

  class MockURLRequestDelegate : public net::URLRequest::Delegate {
    virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE {}
    virtual void OnReadCompleted(net::URLRequest* request,
                                 int bytes_read) OVERRIDE {
    }
  };

  class MockURLRequestJob : public net::URLRequestJob {
   public:
    MockURLRequestJob(net::URLRequest* request,
                      net::NetworkDelegate* network_delegate,
                      int response_code)
        : net::URLRequestJob(request, network_delegate),
          response_code_(response_code),
          has_response_info_(false) {}
    MockURLRequestJob(net::URLRequest* request,
                      net::NetworkDelegate* network_delegate,
                      const net::HttpResponseInfo& info)
        : net::URLRequestJob(request, network_delegate),
          response_code_(info.headers->response_code()),
          has_response_info_(true),
          response_info_(info) {}

  protected:
    virtual ~MockURLRequestJob() {}
    virtual void Start() OVERRIDE {
      NotifyHeadersComplete();
    }
    virtual int GetResponseCode() const OVERRIDE {
      return response_code_;
    }
    virtual void GetResponseInfo(
        net::HttpResponseInfo* info) OVERRIDE {
      if (!has_response_info_)
        return;
      *info = response_info_;
    }

  private:
    int response_code_;
    bool has_response_info_;
    net::HttpResponseInfo response_info_;
  };

  class MockURLRequestJobFactory : public net::URLRequestJobFactory {
   public:
    MockURLRequestJobFactory() : job_(NULL) {
    }

    virtual ~MockURLRequestJobFactory() {
      DCHECK(!job_);
    }

    void SetJob(net::URLRequestJob* job) {
      job_ = job;
    }

    virtual net::URLRequestJob* MaybeCreateJobWithProtocolHandler(
        const std::string& scheme,
        net::URLRequest* request,
        net::NetworkDelegate* network_delegate) const OVERRIDE {
      if (job_) {
        net::URLRequestJob* temp = job_;
        job_ = NULL;
        return temp;
      } else {
        // Some of these tests trigger UpdateJobs which start URLRequests.
        // We short circuit those be returning error jobs.
        return new net::URLRequestErrorJob(request,
                                           network_delegate,
                                           net::ERR_INTERNET_DISCONNECTED);
      }
    }

    virtual bool IsHandledProtocol(const std::string& scheme) const OVERRIDE {
      return scheme == "http";
    };

    virtual bool IsHandledURL(const GURL& url) const OVERRIDE {
      return url.SchemeIs("http");
    }

    virtual bool IsSafeRedirectTarget(const GURL& location) const OVERRIDE {
      return false;
    }

   private:
    mutable net::URLRequestJob* job_;
  };

  class MockURLRequest : public net::URLRequest {
   public:
    MockURLRequest(const GURL& url, net::URLRequestContext* context)
        : net::URLRequest(url, net::DEFAULT_PRIORITY, NULL, context) {}


    MockURLRequestDelegate delegate_;
  };

  static void SetUpTestCase() {
    io_thread_.reset(new base::Thread("AppCacheRequestHandlerTest Thread"));
    base::Thread::Options options(base::MessageLoop::TYPE_IO, 0);
    io_thread_->StartWithOptions(options);
  }

  static void TearDownTestCase() {
    io_thread_.reset(NULL);
  }

  // Test harness --------------------------------------------------

  AppCacheRequestHandlerTest() : host_(NULL) {}

  template <class Method>
  void RunTestOnIOThread(Method method) {
    test_finished_event_ .reset(new base::WaitableEvent(false, false));
    io_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&AppCacheRequestHandlerTest::MethodWrapper<Method>,
                   base::Unretained(this), method));
    test_finished_event_->Wait();
  }

  void SetUpTest() {
    DCHECK(base::MessageLoop::current() == io_thread_->message_loop());
    mock_service_.reset(new MockAppCacheService);
    mock_service_->set_request_context(&empty_context_);
    mock_policy_.reset(new MockAppCachePolicy);
    mock_service_->set_appcache_policy(mock_policy_.get());
    mock_frontend_.reset(new MockFrontend);
    backend_impl_.reset(new AppCacheBackendImpl);
    backend_impl_->Initialize(mock_service_.get(), mock_frontend_.get(),
                              kMockProcessId);
    const int kHostId = 1;
    backend_impl_->RegisterHost(kHostId);
    host_ = backend_impl_->GetHost(kHostId);
    job_factory_.reset(new MockURLRequestJobFactory());
    empty_context_.set_job_factory(job_factory_.get());
  }

  void TearDownTest() {
    DCHECK(base::MessageLoop::current() == io_thread_->message_loop());
    job_ = NULL;
    handler_.reset();
    request_.reset();
    backend_impl_.reset();
    mock_frontend_.reset();
    mock_service_.reset();
    mock_policy_.reset();
    job_factory_.reset();
    host_ = NULL;
  }

  void TestFinished() {
    // We unwind the stack prior to finishing up to let stack
    // based objects get deleted.
    DCHECK(base::MessageLoop::current() == io_thread_->message_loop());
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&AppCacheRequestHandlerTest::TestFinishedUnwound,
                   base::Unretained(this)));
  }

  void TestFinishedUnwound() {
    TearDownTest();
    test_finished_event_->Signal();
  }

  void PushNextTask(const base::Closure& task) {
    task_stack_.push(task);
  }

  void ScheduleNextTask() {
    DCHECK(base::MessageLoop::current() == io_thread_->message_loop());
    if (task_stack_.empty()) {
      TestFinished();
      return;
    }
    base::MessageLoop::current()->PostTask(FROM_HERE, task_stack_.top());
    task_stack_.pop();
  }

  // MainResource_Miss --------------------------------------------------

  void MainResource_Miss() {
    PushNextTask(
        base::Bind(&AppCacheRequestHandlerTest::Verify_MainResource_Miss,
                   base::Unretained(this)));

    request_.reset(new MockURLRequest(GURL("http://blah/"), &empty_context_));
    handler_.reset(host_->CreateRequestHandler(request_.get(),
                                               ResourceType::MAIN_FRAME));
    EXPECT_TRUE(handler_.get());

    job_ = handler_->MaybeLoadResource(request_.get(),
                                       request_->context()->network_delegate());
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_waiting());

    // We have to wait for completion of storage->FindResponseForMainRequest.
    ScheduleNextTask();
  }

  void Verify_MainResource_Miss() {
    EXPECT_FALSE(job_->is_waiting());
    EXPECT_TRUE(job_->is_delivering_network_response());

    int64 cache_id = kAppCacheNoCacheId;
    GURL manifest_url;
    handler_->GetExtraResponseInfo(&cache_id, &manifest_url);
    EXPECT_EQ(kAppCacheNoCacheId, cache_id);
    EXPECT_EQ(GURL(), manifest_url);
    EXPECT_EQ(0, handler_->found_group_id_);

    AppCacheURLRequestJob* fallback_job;
    fallback_job = handler_->MaybeLoadFallbackForRedirect(
        request_.get(),
        request_->context()->network_delegate(),
        GURL("http://blah/redirect"));
    EXPECT_FALSE(fallback_job);
    fallback_job = handler_->MaybeLoadFallbackForResponse(
        request_.get(), request_->context()->network_delegate());
    EXPECT_FALSE(fallback_job);

    EXPECT_TRUE(host_->preferred_manifest_url().is_empty());

    TestFinished();
  }

  // MainResource_Hit --------------------------------------------------

  void MainResource_Hit() {
    PushNextTask(
        base::Bind(&AppCacheRequestHandlerTest::Verify_MainResource_Hit,
                   base::Unretained(this)));

    request_.reset(new MockURLRequest(GURL("http://blah/"), &empty_context_));
    handler_.reset(host_->CreateRequestHandler(request_.get(),
                                               ResourceType::MAIN_FRAME));
    EXPECT_TRUE(handler_.get());

    mock_storage()->SimulateFindMainResource(
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1),
        GURL(), AppCacheEntry(),
        1, 2, GURL("http://blah/manifest/"));

    job_ = handler_->MaybeLoadResource(request_.get(),
                                       request_->context()->network_delegate());
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_waiting());

    // We have to wait for completion of storage->FindResponseForMainRequest.
    ScheduleNextTask();
  }

  void Verify_MainResource_Hit() {
    EXPECT_FALSE(job_->is_waiting());
    EXPECT_TRUE(job_->is_delivering_appcache_response());

    int64 cache_id = kAppCacheNoCacheId;
    GURL manifest_url;
    handler_->GetExtraResponseInfo(&cache_id, &manifest_url);
    EXPECT_EQ(1, cache_id);
    EXPECT_EQ(GURL("http://blah/manifest/"), manifest_url);
    EXPECT_EQ(2, handler_->found_group_id_);

    AppCacheURLRequestJob* fallback_job;
    fallback_job = handler_->MaybeLoadFallbackForResponse(
        request_.get(), request_->context()->network_delegate());
    EXPECT_FALSE(fallback_job);

    EXPECT_EQ(GURL("http://blah/manifest/"),
              host_->preferred_manifest_url());

    TestFinished();
  }

  // MainResource_Fallback --------------------------------------------------

  void MainResource_Fallback() {
    PushNextTask(
        base::Bind(&AppCacheRequestHandlerTest::Verify_MainResource_Fallback,
                   base::Unretained(this)));

    request_.reset(new MockURLRequest(GURL("http://blah/"), &empty_context_));
    handler_.reset(host_->CreateRequestHandler(request_.get(),
                                               ResourceType::MAIN_FRAME));
    EXPECT_TRUE(handler_.get());

    mock_storage()->SimulateFindMainResource(
        AppCacheEntry(),
        GURL("http://blah/fallbackurl"),
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1),
        1, 2, GURL("http://blah/manifest/"));

    job_ = handler_->MaybeLoadResource(request_.get(),
                                       request_->context()->network_delegate());
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_waiting());

    // We have to wait for completion of storage->FindResponseForMainRequest.
    ScheduleNextTask();
  }

  void SimulateResponseCode(int response_code) {
    job_factory_->SetJob(
        new MockURLRequestJob(
            request_.get(),
            request_->context()->network_delegate(),
            response_code));
    request_->Start();
    // All our simulation needs  to satisfy are the following two DCHECKs
    DCHECK(request_->status().is_success());
    DCHECK_EQ(response_code, request_->GetResponseCode());
  }

  void SimulateResponseInfo(const net::HttpResponseInfo& info) {
    job_factory_->SetJob(
        new MockURLRequestJob(
            request_.get(),
            request_->context()->network_delegate(), info));
    request_->set_delegate(&request_->delegate_);
    request_->Start();
  }

  void Verify_MainResource_Fallback() {
    EXPECT_FALSE(job_->is_waiting());
    EXPECT_TRUE(job_->is_delivering_network_response());

    // When the request is restarted, the existing job is dropped so a
    // real network job gets created. We expect NULL here which will cause
    // the net library to create a real job.
    job_ = handler_->MaybeLoadResource(request_.get(),
                                       request_->context()->network_delegate());
    EXPECT_FALSE(job_.get());

    // Simulate an http error of the real network job.
    SimulateResponseCode(500);

    job_ = handler_->MaybeLoadFallbackForResponse(
        request_.get(), request_->context()->network_delegate());
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_delivering_appcache_response());

    int64 cache_id = kAppCacheNoCacheId;
    GURL manifest_url;
    handler_->GetExtraResponseInfo(&cache_id, &manifest_url);
    EXPECT_EQ(1, cache_id);
    EXPECT_EQ(GURL("http://blah/manifest/"), manifest_url);
    EXPECT_TRUE(host_->main_resource_was_namespace_entry_);
    EXPECT_EQ(GURL("http://blah/fallbackurl"), host_->namespace_entry_url_);

    EXPECT_EQ(GURL("http://blah/manifest/"),
              host_->preferred_manifest_url());

    TestFinished();
  }

  // MainResource_FallbackOverride --------------------------------------------

  void MainResource_FallbackOverride() {
    PushNextTask(base::Bind(
        &AppCacheRequestHandlerTest::Verify_MainResource_FallbackOverride,
        base::Unretained(this)));

    request_.reset(new MockURLRequest(GURL("http://blah/fallback-override"),
                                      &empty_context_));
    handler_.reset(host_->CreateRequestHandler(request_.get(),
                                               ResourceType::MAIN_FRAME));
    EXPECT_TRUE(handler_.get());

    mock_storage()->SimulateFindMainResource(
        AppCacheEntry(),
        GURL("http://blah/fallbackurl"),
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1),
        1, 2, GURL("http://blah/manifest/"));

    job_ = handler_->MaybeLoadResource(request_.get(),
                                       request_->context()->network_delegate());
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_waiting());

    // We have to wait for completion of storage->FindResponseForMainRequest.
    ScheduleNextTask();
  }

  void Verify_MainResource_FallbackOverride() {
    EXPECT_FALSE(job_->is_waiting());
    EXPECT_TRUE(job_->is_delivering_network_response());

    // When the request is restarted, the existing job is dropped so a
    // real network job gets created. We expect NULL here which will cause
    // the net library to create a real job.
    job_ = handler_->MaybeLoadResource(request_.get(),
                                       request_->context()->network_delegate());
    EXPECT_FALSE(job_.get());

    // Simulate an http error of the real network job, but with custom
    // headers that override the fallback behavior.
    const char kOverrideHeaders[] =
        "HTTP/1.1 404 BOO HOO\0"
        "x-chromium-appcache-fallback-override: disallow-fallback\0"
        "\0";
    net::HttpResponseInfo info;
    info.headers = new net::HttpResponseHeaders(
        std::string(kOverrideHeaders, arraysize(kOverrideHeaders)));
    SimulateResponseInfo(info);

    job_ = handler_->MaybeLoadFallbackForResponse(
        request_.get(), request_->context()->network_delegate());
    EXPECT_FALSE(job_.get());

    TestFinished();
  }

  // SubResource_Miss_WithNoCacheSelected ----------------------------------

  void SubResource_Miss_WithNoCacheSelected() {
    request_.reset(new MockURLRequest(GURL("http://blah/"), &empty_context_));
    handler_.reset(host_->CreateRequestHandler(request_.get(),
                                               ResourceType::SUB_RESOURCE));

    // We avoid creating handler when possible, sub-resource requests are not
    // subject to retrieval from an appcache when there's no associated cache.
    EXPECT_FALSE(handler_.get());

    TestFinished();
  }

  // SubResource_Miss_WithCacheSelected ----------------------------------

  void SubResource_Miss_WithCacheSelected() {
    // A sub-resource load where the resource is not in an appcache, or
    // in a network or fallback namespace, should result in a failed request.
    host_->AssociateCompleteCache(MakeNewCache());

    request_.reset(new MockURLRequest(GURL("http://blah/"), &empty_context_));
    handler_.reset(host_->CreateRequestHandler(request_.get(),
                                               ResourceType::SUB_RESOURCE));
    EXPECT_TRUE(handler_.get());

    job_ = handler_->MaybeLoadResource(request_.get(),
                                       request_->context()->network_delegate());
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_delivering_error_response());

    AppCacheURLRequestJob* fallback_job;
    fallback_job = handler_->MaybeLoadFallbackForRedirect(
        request_.get(),
        request_->context()->network_delegate(),
        GURL("http://blah/redirect"));
    EXPECT_FALSE(fallback_job);
    fallback_job = handler_->MaybeLoadFallbackForResponse(
        request_.get(), request_->context()->network_delegate());
    EXPECT_FALSE(fallback_job);

    TestFinished();
  }

  // SubResource_Miss_WithWaitForCacheSelection -----------------------------

  void SubResource_Miss_WithWaitForCacheSelection() {
    // Precondition, the host is waiting on cache selection.
    scoped_refptr<AppCache> cache(MakeNewCache());
    host_->pending_selected_cache_id_ = cache->cache_id();
    host_->set_preferred_manifest_url(cache->owning_group()->manifest_url());

    request_.reset(new MockURLRequest(GURL("http://blah/"), &empty_context_));
    handler_.reset(host_->CreateRequestHandler(request_.get(),
                                               ResourceType::SUB_RESOURCE));
    EXPECT_TRUE(handler_.get());
    job_ = handler_->MaybeLoadResource(request_.get(),
                                       request_->context()->network_delegate());
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_waiting());

    host_->FinishCacheSelection(cache.get(), NULL);
    EXPECT_FALSE(job_->is_waiting());
    EXPECT_TRUE(job_->is_delivering_error_response());

    AppCacheURLRequestJob* fallback_job;
    fallback_job = handler_->MaybeLoadFallbackForRedirect(
        request_.get(),
        request_->context()->network_delegate(),
        GURL("http://blah/redirect"));
    EXPECT_FALSE(fallback_job);
    fallback_job = handler_->MaybeLoadFallbackForResponse(
        request_.get(), request_->context()->network_delegate());
    EXPECT_FALSE(fallback_job);

    TestFinished();
  }

  // SubResource_Hit -----------------------------

  void SubResource_Hit() {
    host_->AssociateCompleteCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1), AppCacheEntry(), false);

    request_.reset(new MockURLRequest(GURL("http://blah/"), &empty_context_));
    handler_.reset(host_->CreateRequestHandler(request_.get(),
                                               ResourceType::SUB_RESOURCE));
    EXPECT_TRUE(handler_.get());
    job_ = handler_->MaybeLoadResource(request_.get(),
                                       request_->context()->network_delegate());
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_delivering_appcache_response());

    AppCacheURLRequestJob* fallback_job;
    fallback_job = handler_->MaybeLoadFallbackForRedirect(
        request_.get(),
        request_->context()->network_delegate(),
        GURL("http://blah/redirect"));
    EXPECT_FALSE(fallback_job);
    fallback_job = handler_->MaybeLoadFallbackForResponse(
        request_.get(), request_->context()->network_delegate());
    EXPECT_FALSE(fallback_job);

    TestFinished();
  }

  // SubResource_RedirectFallback -----------------------------

  void SubResource_RedirectFallback() {
    // Redirects to resources in the a different origin are subject to
    // fallback namespaces.
    host_->AssociateCompleteCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(
        AppCacheEntry(), AppCacheEntry(AppCacheEntry::EXPLICIT, 1), false);

    request_.reset(new MockURLRequest(GURL("http://blah/"), &empty_context_));
    handler_.reset(host_->CreateRequestHandler(request_.get(),
                                               ResourceType::SUB_RESOURCE));
    EXPECT_TRUE(handler_.get());
    job_ = handler_->MaybeLoadResource(request_.get(),
                                       request_->context()->network_delegate());
    EXPECT_FALSE(job_.get());

    job_ = handler_->MaybeLoadFallbackForRedirect(
        request_.get(),
        request_->context()->network_delegate(),
        GURL("http://not_blah/redirect"));
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_delivering_appcache_response());

    AppCacheURLRequestJob* fallback_job;
    fallback_job = handler_->MaybeLoadFallbackForResponse(
        request_.get(), request_->context()->network_delegate());
    EXPECT_FALSE(fallback_job);

    TestFinished();
  }

  // SubResource_NoRedirectFallback -----------------------------

  void SubResource_NoRedirectFallback() {
    // Redirects to resources in the same-origin are not subject to
    // fallback namespaces.
    host_->AssociateCompleteCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(
        AppCacheEntry(), AppCacheEntry(AppCacheEntry::EXPLICIT, 1), false);

    request_.reset(new MockURLRequest(GURL("http://blah/"), &empty_context_));
    handler_.reset(host_->CreateRequestHandler(request_.get(),
                                               ResourceType::SUB_RESOURCE));
    EXPECT_TRUE(handler_.get());
    job_ = handler_->MaybeLoadResource(request_.get(),
                                       request_->context()->network_delegate());
    EXPECT_FALSE(job_.get());

    AppCacheURLRequestJob* fallback_job;
    fallback_job = handler_->MaybeLoadFallbackForRedirect(
        request_.get(),
        request_->context()->network_delegate(),
        GURL("http://blah/redirect"));
    EXPECT_FALSE(fallback_job);

    SimulateResponseCode(200);
    fallback_job = handler_->MaybeLoadFallbackForResponse(
        request_.get(), request_->context()->network_delegate());
    EXPECT_FALSE(fallback_job);

    TestFinished();
  }

  // SubResource_Network -----------------------------

  void SubResource_Network() {
    // A sub-resource load where the resource is in a network namespace,
    // should result in the system using a 'real' job to do the network
    // retrieval.
    host_->AssociateCompleteCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(
        AppCacheEntry(), AppCacheEntry(), true);

    request_.reset(new MockURLRequest(GURL("http://blah/"), &empty_context_));
    handler_.reset(host_->CreateRequestHandler(request_.get(),
                                               ResourceType::SUB_RESOURCE));
    EXPECT_TRUE(handler_.get());
    job_ = handler_->MaybeLoadResource(request_.get(),
                                       request_->context()->network_delegate());
    EXPECT_FALSE(job_.get());

    AppCacheURLRequestJob* fallback_job;
    fallback_job = handler_->MaybeLoadFallbackForRedirect(
        request_.get(),
        request_->context()->network_delegate(),
        GURL("http://blah/redirect"));
    EXPECT_FALSE(fallback_job);
    fallback_job = handler_->MaybeLoadFallbackForResponse(
        request_.get(), request_->context()->network_delegate());
    EXPECT_FALSE(fallback_job);

    TestFinished();
  }

  // DestroyedHost -----------------------------

  void DestroyedHost() {
    host_->AssociateCompleteCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1), AppCacheEntry(), false);

    request_.reset(new MockURLRequest(GURL("http://blah/"), &empty_context_));
    handler_.reset(host_->CreateRequestHandler(request_.get(),
                                               ResourceType::SUB_RESOURCE));
    EXPECT_TRUE(handler_.get());

    backend_impl_->UnregisterHost(1);
    host_ = NULL;

    EXPECT_FALSE(handler_->MaybeLoadResource(
        request_.get(), request_->context()->network_delegate()));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForRedirect(
        request_.get(),
        request_->context()->network_delegate(),
        GURL("http://blah/redirect")));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForResponse(
        request_.get(), request_->context()->network_delegate()));

    TestFinished();
  }

  // DestroyedHostWithWaitingJob -----------------------------

  void DestroyedHostWithWaitingJob() {
    // Precondition, the host is waiting on cache selection.
    host_->pending_selected_cache_id_ = 1;

    request_.reset(new MockURLRequest(GURL("http://blah/"), &empty_context_));
    handler_.reset(host_->CreateRequestHandler(request_.get(),
                                               ResourceType::SUB_RESOURCE));
    EXPECT_TRUE(handler_.get());

    job_ = handler_->MaybeLoadResource(request_.get(),
                                       request_->context()->network_delegate());
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_waiting());

    backend_impl_->UnregisterHost(1);
    host_ = NULL;
    EXPECT_TRUE(job_->has_been_killed());

    EXPECT_FALSE(handler_->MaybeLoadResource(
        request_.get(), request_->context()->network_delegate()));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForRedirect(
        request_.get(),
        request_->context()->network_delegate(),
        GURL("http://blah/redirect")));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForResponse(
        request_.get(), request_->context()->network_delegate()));

    TestFinished();
  }

  // UnsupportedScheme -----------------------------

  void UnsupportedScheme() {
    // Precondition, the host is waiting on cache selection.
    host_->pending_selected_cache_id_ = 1;

    request_.reset(new MockURLRequest(GURL("ftp://blah/"), &empty_context_));
    handler_.reset(host_->CreateRequestHandler(request_.get(),
                                               ResourceType::SUB_RESOURCE));
    EXPECT_TRUE(handler_.get());  // we could redirect to http (conceivably)

    EXPECT_FALSE(handler_->MaybeLoadResource(
        request_.get(), request_->context()->network_delegate()));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForRedirect(
        request_.get(),
        request_->context()->network_delegate(),
        GURL("ftp://blah/redirect")));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForResponse(
        request_.get(), request_->context()->network_delegate()));

    TestFinished();
  }

  // CanceledRequest -----------------------------

  void CanceledRequest() {
    request_.reset(new MockURLRequest(GURL("http://blah/"), &empty_context_));
    handler_.reset(host_->CreateRequestHandler(request_.get(),
                                               ResourceType::MAIN_FRAME));
    EXPECT_TRUE(handler_.get());

    job_ = handler_->MaybeLoadResource(request_.get(),
                                       request_->context()->network_delegate());
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_waiting());
    EXPECT_FALSE(job_->has_been_started());

    job_factory_->SetJob(job_);
    request_->Start();
    EXPECT_TRUE(job_->has_been_started());

    request_->Cancel();
    EXPECT_TRUE(job_->has_been_killed());

    EXPECT_FALSE(handler_->MaybeLoadFallbackForResponse(
        request_.get(), request_->context()->network_delegate()));

    TestFinished();
  }

  // WorkerRequest -----------------------------

  void WorkerRequest() {
    EXPECT_TRUE(AppCacheRequestHandler::IsMainResourceType(
        ResourceType::MAIN_FRAME));
    EXPECT_TRUE(AppCacheRequestHandler::IsMainResourceType(
        ResourceType::SUB_FRAME));
    EXPECT_TRUE(AppCacheRequestHandler::IsMainResourceType(
        ResourceType::SHARED_WORKER));
    EXPECT_FALSE(AppCacheRequestHandler::IsMainResourceType(
        ResourceType::WORKER));

    request_.reset(new MockURLRequest(GURL("http://blah/"), &empty_context_));

    const int kParentHostId = host_->host_id();
    const int kWorkerHostId = 2;
    const int kAbandonedWorkerHostId = 3;
    const int kNonExsitingHostId = 700;

    backend_impl_->RegisterHost(kWorkerHostId);
    AppCacheHost* worker_host = backend_impl_->GetHost(kWorkerHostId);
    worker_host->SelectCacheForWorker(kParentHostId, kMockProcessId);
    handler_.reset(worker_host->CreateRequestHandler(
        request_.get(), ResourceType::SHARED_WORKER));
    EXPECT_TRUE(handler_.get());
    // Verify that the handler is associated with the parent host.
    EXPECT_EQ(host_, handler_->host_);

    // Create a new worker host, but associate it with a parent host that
    // does not exists to simulate the host having been torn down.
    backend_impl_->UnregisterHost(kWorkerHostId);
    backend_impl_->RegisterHost(kAbandonedWorkerHostId);
    worker_host = backend_impl_->GetHost(kAbandonedWorkerHostId);
    EXPECT_EQ(NULL, backend_impl_->GetHost(kNonExsitingHostId));
    worker_host->SelectCacheForWorker(kNonExsitingHostId, kMockProcessId);
    handler_.reset(worker_host->CreateRequestHandler(
        request_.get(), ResourceType::SHARED_WORKER));
    EXPECT_FALSE(handler_.get());

    TestFinished();
  }

  // MainResource_Blocked --------------------------------------------------

  void MainResource_Blocked() {
    PushNextTask(
        base::Bind(&AppCacheRequestHandlerTest::Verify_MainResource_Blocked,
                   base::Unretained(this)));

    request_.reset(new MockURLRequest(GURL("http://blah/"), &empty_context_));
    handler_.reset(host_->CreateRequestHandler(request_.get(),
                                               ResourceType::MAIN_FRAME));
    EXPECT_TRUE(handler_.get());

    mock_policy_->can_load_return_value_ = false;
    mock_storage()->SimulateFindMainResource(
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1),
        GURL(), AppCacheEntry(),
        1, 2, GURL("http://blah/manifest/"));

    job_ = handler_->MaybeLoadResource(request_.get(),
                                       request_->context()->network_delegate());
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_waiting());

    // We have to wait for completion of storage->FindResponseForMainRequest.
    ScheduleNextTask();
  }

  void Verify_MainResource_Blocked() {
    EXPECT_FALSE(job_->is_waiting());
    EXPECT_FALSE(job_->is_delivering_appcache_response());

    EXPECT_EQ(0, handler_->found_cache_id_);
    EXPECT_EQ(0, handler_->found_group_id_);
    EXPECT_TRUE(handler_->found_manifest_url_.is_empty());
    EXPECT_TRUE(host_->preferred_manifest_url().is_empty());
    EXPECT_TRUE(host_->main_resource_blocked_);
    EXPECT_TRUE(host_->blocked_manifest_url_ == GURL("http://blah/manifest/"));

    TestFinished();
  }

  // Test case helpers --------------------------------------------------

  AppCache* MakeNewCache() {
    AppCache* cache = new AppCache(
        mock_storage(), mock_storage()->NewCacheId());
    cache->set_complete(true);
    AppCacheGroup* group = new AppCacheGroup(
        mock_storage(), GURL("http://blah/manifest"),
        mock_storage()->NewGroupId());
    group->AddCache(cache);
    return cache;
  }

  MockAppCacheStorage* mock_storage() {
    return reinterpret_cast<MockAppCacheStorage*>(mock_service_->storage());
  }

  // Data members --------------------------------------------------

  scoped_ptr<base::WaitableEvent> test_finished_event_;
  std::stack<base::Closure> task_stack_;
  scoped_ptr<MockAppCacheService> mock_service_;
  scoped_ptr<AppCacheBackendImpl> backend_impl_;
  scoped_ptr<MockFrontend> mock_frontend_;
  scoped_ptr<MockAppCachePolicy> mock_policy_;
  AppCacheHost* host_;
  net::URLRequestContext empty_context_;
  scoped_ptr<MockURLRequestJobFactory> job_factory_;
  scoped_ptr<MockURLRequest> request_;
  scoped_ptr<AppCacheRequestHandler> handler_;
  scoped_refptr<AppCacheURLRequestJob> job_;

  static scoped_ptr<base::Thread> io_thread_;
};

// static
scoped_ptr<base::Thread> AppCacheRequestHandlerTest::io_thread_;

TEST_F(AppCacheRequestHandlerTest, MainResource_Miss) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::MainResource_Miss);
}

TEST_F(AppCacheRequestHandlerTest, MainResource_Hit) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::MainResource_Hit);
}

TEST_F(AppCacheRequestHandlerTest, MainResource_Fallback) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::MainResource_Fallback);
}

TEST_F(AppCacheRequestHandlerTest, MainResource_FallbackOverride) {
  RunTestOnIOThread(
      &AppCacheRequestHandlerTest::MainResource_FallbackOverride);
}

TEST_F(AppCacheRequestHandlerTest, SubResource_Miss_WithNoCacheSelected) {
  RunTestOnIOThread(
      &AppCacheRequestHandlerTest::SubResource_Miss_WithNoCacheSelected);
}

TEST_F(AppCacheRequestHandlerTest, SubResource_Miss_WithCacheSelected) {
  RunTestOnIOThread(
      &AppCacheRequestHandlerTest::SubResource_Miss_WithCacheSelected);
}

TEST_F(AppCacheRequestHandlerTest,
       SubResource_Miss_WithWaitForCacheSelection) {
  RunTestOnIOThread(
      &AppCacheRequestHandlerTest::SubResource_Miss_WithWaitForCacheSelection);
}

TEST_F(AppCacheRequestHandlerTest, SubResource_Hit) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::SubResource_Hit);
}

TEST_F(AppCacheRequestHandlerTest, SubResource_RedirectFallback) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::SubResource_RedirectFallback);
}

TEST_F(AppCacheRequestHandlerTest, SubResource_NoRedirectFallback) {
  RunTestOnIOThread(
    &AppCacheRequestHandlerTest::SubResource_NoRedirectFallback);
}

TEST_F(AppCacheRequestHandlerTest, SubResource_Network) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::SubResource_Network);
}

TEST_F(AppCacheRequestHandlerTest, DestroyedHost) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::DestroyedHost);
}

TEST_F(AppCacheRequestHandlerTest, DestroyedHostWithWaitingJob) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::DestroyedHostWithWaitingJob);
}

TEST_F(AppCacheRequestHandlerTest, UnsupportedScheme) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::UnsupportedScheme);
}

TEST_F(AppCacheRequestHandlerTest, CanceledRequest) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::CanceledRequest);
}

TEST_F(AppCacheRequestHandlerTest, WorkerRequest) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::WorkerRequest);
}

TEST_F(AppCacheRequestHandlerTest, MainResource_Blocked) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::MainResource_Blocked);
}

}  // namespace content
