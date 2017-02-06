// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/async_revalidation_driver.h"

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_content_browser_client.h"
#include "ipc/ipc_message.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate_impl.h"
#include "net/base/request_priority.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

// Dummy implementation of ResourceThrottle, an instance of which is needed to
// initialize AsyncRevalidationDriver.
class ResourceThrottleStub : public ResourceThrottle {
 public:
  ResourceThrottleStub() {}

  // If true, defers the request in WillStartRequest.
  void set_defer_request_on_will_start_request(
      bool defer_request_on_will_start_request) {
    defer_request_on_will_start_request_ = defer_request_on_will_start_request;
  }

  // ResourceThrottler implementation:
  void WillStartRequest(bool* defer) override {
    *defer = defer_request_on_will_start_request_;
  }

  const char* GetNameForLogging() const override {
    return "ResourceThrottleStub";
  }

 private:
  bool defer_request_on_will_start_request_ = false;

  DISALLOW_COPY_AND_ASSIGN(ResourceThrottleStub);
};

// There are multiple layers of boilerplate needed to use a URLRequestTestJob
// subclass.  Subclasses of AsyncRevalidationDriverTest can use
// BindCreateProtocolHandlerCallback() to bypass most of that boilerplate.
using CreateProtocolHandlerCallback = base::Callback<
    std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>()>;

template <typename T>
CreateProtocolHandlerCallback BindCreateProtocolHandlerCallback() {
  static_assert(std::is_base_of<net::URLRequestJob, T>::value,
                "Template argument to BindCreateProtocolHandlerCallback() must "
                "be a subclass of URLRequestJob.");

  class TemplatedProtocolHandler
      : public net::URLRequestJobFactory::ProtocolHandler {
   public:
    static std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>
    Create() {
      return base::WrapUnique(new TemplatedProtocolHandler());
    }

    // URLRequestJobFactory::ProtocolHandler implementation:
    net::URLRequestJob* MaybeCreateJob(
        net::URLRequest* request,
        net::NetworkDelegate* network_delegate) const override {
      return new T(request, network_delegate);
    }
  };

  return base::Bind(&TemplatedProtocolHandler::Create);
}

// An implementation of NetworkDelegate that captures the status of the last
// URLRequest to be destroyed.
class StatusCapturingNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  const net::URLRequestStatus& last_status() { return last_status_; }

 private:
  // net::NetworkDelegate implementation.
  void OnURLRequestDestroyed(net::URLRequest* request) override {
    last_status_ = request->status();
  }

  net::URLRequestStatus last_status_;
};

class AsyncRevalidationDriverTest : public testing::Test {
 protected:
  // Constructor for test fixtures that subclass this one.
  AsyncRevalidationDriverTest(
      const CreateProtocolHandlerCallback& create_protocol_handler_callback)
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        create_protocol_handler_callback_(create_protocol_handler_callback),
        raw_ptr_resource_throttle_(nullptr),
        raw_ptr_request_(nullptr) {
    test_url_request_context_.set_job_factory(&job_factory_);
    test_url_request_context_.set_network_delegate(&network_delegate_);
  }

  // Constructor for tests that use this fixture directly.
  AsyncRevalidationDriverTest()
      : AsyncRevalidationDriverTest(
            base::Bind(net::URLRequestTestJob::CreateProtocolHandler)) {}

  bool async_revalidation_complete_called() const {
    return async_revalidation_complete_called_;
  }

  const net::URLRequestStatus& last_status() {
    return network_delegate_.last_status();
  }

  void SetUpAsyncRevalidationDriverWithRequestToUrl(const GURL& url) {
    std::unique_ptr<net::URLRequest> request(
        test_url_request_context_.CreateRequest(url, net::DEFAULT_PRIORITY,
                                                nullptr /* delegate */));
    raw_ptr_request_ = request.get();
    raw_ptr_resource_throttle_ = new ResourceThrottleStub();
    // This use of base::Unretained() is safe because |driver_|, and the closure
    // passed to it, will be destroyed before this object is.
    driver_.reset(new AsyncRevalidationDriver(
        std::move(request), base::WrapUnique(raw_ptr_resource_throttle_),
        base::Bind(&AsyncRevalidationDriverTest::OnAsyncRevalidationComplete,
                   base::Unretained(this))));
  }

  void SetUpAsyncRevalidationDriverWithDefaultRequest() {
    SetUpAsyncRevalidationDriverWithRequestToUrl(
        net::URLRequestTestJob::test_url_1());
  }

  void SetUp() override {
    job_factory_.SetProtocolHandler("test",
                                    create_protocol_handler_callback_.Run());
    SetUpAsyncRevalidationDriverWithDefaultRequest();
  }

  void OnAsyncRevalidationComplete() {
    EXPECT_FALSE(async_revalidation_complete_called_);
    async_revalidation_complete_called_ = true;
    driver_.reset();
  }

  TestBrowserThreadBundle thread_bundle_;
  net::URLRequestJobFactoryImpl job_factory_;
  net::TestURLRequestContext test_url_request_context_;
  StatusCapturingNetworkDelegate network_delegate_;
  CreateProtocolHandlerCallback create_protocol_handler_callback_;

  // The AsyncRevalidationDriver owns the URLRequest and the ResourceThrottle.
  ResourceThrottleStub* raw_ptr_resource_throttle_;
  net::URLRequest* raw_ptr_request_;
  std::unique_ptr<AsyncRevalidationDriver> driver_;
  bool async_revalidation_complete_called_ = false;
};

TEST_F(AsyncRevalidationDriverTest, NormalRequestCompletes) {
  driver_->StartRequest();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(async_revalidation_complete_called());
}

// Verifies that request that should be deferred at start is deferred.
TEST_F(AsyncRevalidationDriverTest, DeferOnStart) {
  raw_ptr_resource_throttle_->set_defer_request_on_will_start_request(true);

  driver_->StartRequest();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(raw_ptr_request_->is_pending());
  EXPECT_FALSE(async_revalidation_complete_called());
}

// Verifies that resuming a deferred request works. Assumes that DeferOnStart
// passes.
TEST_F(AsyncRevalidationDriverTest, ResumeDeferredRequestWorks) {
  raw_ptr_resource_throttle_->set_defer_request_on_will_start_request(true);

  driver_->StartRequest();
  base::RunLoop().RunUntilIdle();

  ResourceController* driver_as_resource_controller = driver_.get();
  driver_as_resource_controller->Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(async_revalidation_complete_called());
}

// Verifies that redirects are not followed.
TEST_F(AsyncRevalidationDriverTest, RedirectsAreNotFollowed) {
  SetUpAsyncRevalidationDriverWithRequestToUrl(
      net::URLRequestTestJob::test_url_redirect_to_url_2());

  driver_->StartRequest();
  while (net::URLRequestTestJob::ProcessOnePendingMessage())
    base::RunLoop().RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(last_status().is_success());
  EXPECT_EQ(net::ERR_ABORTED, last_status().error());
  EXPECT_TRUE(async_revalidation_complete_called());
}

// A mock URLRequestJob which simulates an SSL client auth request.
class MockClientCertURLRequestJob : public net::URLRequestTestJob {
 public:
  MockClientCertURLRequestJob(net::URLRequest* request,
                              net::NetworkDelegate* network_delegate)
      : net::URLRequestTestJob(request, network_delegate, true),
        weak_factory_(this) {}

  // net::URLRequestTestJob implementation:
  void Start() override {
    scoped_refptr<net::SSLCertRequestInfo> cert_request_info(
        new net::SSLCertRequestInfo);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&MockClientCertURLRequestJob::NotifyCertificateRequested,
                   weak_factory_.GetWeakPtr(),
                   base::RetainedRef(cert_request_info)));
  }

  void ContinueWithCertificate(
      net::X509Certificate* cert,
      net::SSLPrivateKey* client_private_key) override {
    ADD_FAILURE() << "Certificate supplied.";
  }

  void Kill() override {
    weak_factory_.InvalidateWeakPtrs();
    URLRequestJob::Kill();
  }

 private:
  base::WeakPtrFactory<MockClientCertURLRequestJob> weak_factory_;
};

class AsyncRevalidationDriverClientCertTest
    : public AsyncRevalidationDriverTest {
 protected:
  AsyncRevalidationDriverClientCertTest()
      : AsyncRevalidationDriverTest(
            BindCreateProtocolHandlerCallback<MockClientCertURLRequestJob>()) {}
};

// Test browser client that causes the test to fail if SelectClientCertificate()
// is called. Automatically sets itself as the browser client when constructed
// and restores the old browser client in the destructor.
class ScopedDontSelectCertificateBrowserClient
    : public TestContentBrowserClient {
 public:
  ScopedDontSelectCertificateBrowserClient() {
    old_client_ = SetBrowserClientForTesting(this);
  }

  ~ScopedDontSelectCertificateBrowserClient() override {
    SetBrowserClientForTesting(old_client_);
  }

  void SelectClientCertificate(
      WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      std::unique_ptr<ClientCertificateDelegate> delegate) override {
    ADD_FAILURE() << "SelectClientCertificate was called.";
  }

 private:
  ContentBrowserClient* old_client_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDontSelectCertificateBrowserClient);
};

// Verifies that async revalidation requests do not attempt to provide client
// certificates.
TEST_F(AsyncRevalidationDriverClientCertTest, RequestRejected) {
  // Ensure that SelectClientCertificate is not called during this test.
  ScopedDontSelectCertificateBrowserClient test_client;

  // Start the request and wait for it to pause.
  driver_->StartRequest();

  // Because TestBrowserThreadBundle only uses one real thread, this is
  // sufficient to ensure that tasks posted to the "UI thread" have run.
  base::RunLoop().RunUntilIdle();

  // Check that the request aborted.
  EXPECT_FALSE(last_status().is_success());
  EXPECT_EQ(net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED, last_status().error());
  EXPECT_TRUE(async_revalidation_complete_called());
}

// A mock URLRequestJob which simulates an SSL certificate error.
class MockSSLErrorURLRequestJob : public net::URLRequestTestJob {
 public:
  MockSSLErrorURLRequestJob(net::URLRequest* request,
                            net::NetworkDelegate* network_delegate)
      : net::URLRequestTestJob(request, network_delegate, true),
        weak_factory_(this) {}

  // net::URLRequestTestJob implementation:
  void Start() override {
    // This SSLInfo isn't really valid, but it is good enough for testing.
    net::SSLInfo ssl_info;
    ssl_info.SetCertError(net::ERR_CERT_DATE_INVALID);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&MockSSLErrorURLRequestJob::NotifySSLCertificateError,
                   weak_factory_.GetWeakPtr(), ssl_info, false));
  }

  void ContinueDespiteLastError() override {
    ADD_FAILURE() << "ContinueDespiteLastError called.";
  }

 private:
  base::WeakPtrFactory<MockSSLErrorURLRequestJob> weak_factory_;
};

class AsyncRevalidationDriverSSLErrorTest : public AsyncRevalidationDriverTest {
 protected:
  AsyncRevalidationDriverSSLErrorTest()
      : AsyncRevalidationDriverTest(
            BindCreateProtocolHandlerCallback<MockSSLErrorURLRequestJob>()) {}
};

// Verifies that async revalidation requests do not attempt to recover from SSL
// certificate errors.
TEST_F(AsyncRevalidationDriverSSLErrorTest, RequestWithSSLErrorRejected) {
  // Start the request and wait for it to pause.
  driver_->StartRequest();
  base::RunLoop().RunUntilIdle();

  // Check that the request has been aborted.
  EXPECT_FALSE(last_status().is_success());
  EXPECT_EQ(net::ERR_ABORTED, last_status().error());
  EXPECT_TRUE(async_revalidation_complete_called());
}

// A URLRequestTestJob that sets |request_time| and |was_cached| on their
// response_info, and causes the test to fail if Read() is called.
class FromCacheURLRequestJob : public net::URLRequestTestJob {
 public:
  FromCacheURLRequestJob(net::URLRequest* request,
                         net::NetworkDelegate* network_delegate)
      : net::URLRequestTestJob(request, network_delegate, true) {}

  void GetResponseInfo(net::HttpResponseInfo* info) override {
    URLRequestTestJob::GetResponseInfo(info);
    info->request_time = base::Time::Now();
    info->was_cached = true;
  }

  int ReadRawData(net::IOBuffer* buf, int buf_size) override {
    ADD_FAILURE() << "ReadRawData() was called.";
    return URLRequestTestJob::ReadRawData(buf, buf_size);
  }

 private:
  ~FromCacheURLRequestJob() override {}

  DISALLOW_COPY_AND_ASSIGN(FromCacheURLRequestJob);
};

class AsyncRevalidationDriverFromCacheTest
    : public AsyncRevalidationDriverTest {
 protected:
  AsyncRevalidationDriverFromCacheTest()
      : AsyncRevalidationDriverTest(
            BindCreateProtocolHandlerCallback<FromCacheURLRequestJob>()) {}
};

TEST_F(AsyncRevalidationDriverFromCacheTest,
       CacheNotReadOnSuccessfulRevalidation) {
  driver_->StartRequest();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(async_revalidation_complete_called());
}

}  // namespace
}  // namespace content
