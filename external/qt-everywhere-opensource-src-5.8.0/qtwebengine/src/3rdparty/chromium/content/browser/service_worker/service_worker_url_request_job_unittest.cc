// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_url_request_job.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/fileapi/mock_url_request_delegate.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_response_info.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_context.h"
#include "content/browser/streams/stream_registry.h"
#include "content/common/resource_request_body_impl.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/io_buffer.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_job.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ServiceWorkerURLRequestJobTest;

namespace {

const int kProviderID = 100;
const char kTestData[] = "Here is sample text for the blob.";

class MockHttpProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  MockHttpProtocolHandler(
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      const ResourceContext* resource_context,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      ServiceWorkerURLRequestJob::Delegate* delegate)
      : provider_host_(provider_host),
        resource_context_(resource_context),
        blob_storage_context_(blob_storage_context),
        job_(nullptr),
        delegate_(delegate),
        resource_type_(RESOURCE_TYPE_MAIN_FRAME) {}

  ~MockHttpProtocolHandler() override {}

  void set_resource_type(ResourceType type) { resource_type_ = type; }

  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    if (job_ && job_->ShouldFallbackToNetwork()) {
      // Simulate fallback to network by constructing a valid response.
      return new net::URLRequestTestJob(request, network_delegate,
                                        net::URLRequestTestJob::test_headers(),
                                        "PASS", true);
    }

    job_ = new ServiceWorkerURLRequestJob(
        request, network_delegate, provider_host_->client_uuid(),
        blob_storage_context_, resource_context_, FETCH_REQUEST_MODE_NO_CORS,
        FETCH_CREDENTIALS_MODE_OMIT, FetchRedirectMode::FOLLOW_MODE,
        resource_type_, REQUEST_CONTEXT_TYPE_HYPERLINK,
        REQUEST_CONTEXT_FRAME_TYPE_TOP_LEVEL,
        scoped_refptr<ResourceRequestBodyImpl>(), ServiceWorkerFetchType::FETCH,
        delegate_);
    job_->ForwardToServiceWorker();
    return job_;
  }
  ServiceWorkerURLRequestJob* job() { return job_; }

 private:
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  const ResourceContext* resource_context_;
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
  mutable ServiceWorkerURLRequestJob* job_;
  ServiceWorkerURLRequestJob::Delegate* delegate_;
  ResourceType resource_type_;
};

// Returns a BlobProtocolHandler that uses |blob_storage_context|. Caller owns
// the memory.
std::unique_ptr<storage::BlobProtocolHandler> CreateMockBlobProtocolHandler(
    storage::BlobStorageContext* blob_storage_context) {
  // The FileSystemContext and task runner are not actually used but a
  // task runner is needed to avoid a DCHECK in BlobURLRequestJob ctor.
  return base::WrapUnique(new storage::BlobProtocolHandler(
      blob_storage_context, nullptr,
      base::ThreadTaskRunnerHandle::Get().get()));
}

}  // namespace

class ServiceWorkerURLRequestJobTest
    : public testing::Test,
      public ServiceWorkerURLRequestJob::Delegate {
 protected:
  ServiceWorkerURLRequestJobTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        blob_data_(new storage::BlobDataBuilder("blob-id:myblob")) {}
  ~ServiceWorkerURLRequestJobTest() override {}

  void SetUp() override {
    browser_context_.reset(new TestBrowserContext);
    InitializeResourceContext(browser_context_.get());
    SetUpWithHelper(new EmbeddedWorkerTestHelper(base::FilePath()));
  }

  void SetUpWithHelper(EmbeddedWorkerTestHelper* helper,
                       bool set_main_script_http_response_info = true) {
    helper_.reset(helper);

    registration_ = new ServiceWorkerRegistration(
        GURL("https://example.com/"), 1L, helper_->context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_.get(), GURL("https://example.com/service_worker.js"), 1L,
        helper_->context()->AsWeakPtr());
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    records.push_back(
        ServiceWorkerDatabase::ResourceRecord(10, version_->script_url(), 100));
    version_->script_cache_map()->SetResources(records);

    // Make the registration findable via storage functions.
    helper_->context()->storage()->LazyInitialize(base::Bind(&base::DoNothing));
    base::RunLoop().RunUntilIdle();
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
    helper_->context()->storage()->StoreRegistration(
        registration_.get(),
        version_.get(),
        CreateReceiverOnCurrentThread(&status));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(SERVICE_WORKER_OK, status);

    if (set_main_script_http_response_info) {
      net::HttpResponseInfo http_info;
      http_info.ssl_info.cert =
          net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
      EXPECT_TRUE(http_info.ssl_info.is_valid());
      http_info.ssl_info.security_bits = 0x100;
      // SSL3 TLS_DHE_RSA_WITH_AES_256_CBC_SHA
      http_info.ssl_info.connection_status = 0x300039;
      version_->SetMainScriptHttpResponseInfo(http_info);
    }

    std::unique_ptr<ServiceWorkerProviderHost> provider_host(
        new ServiceWorkerProviderHost(
            helper_->mock_render_process_id(), MSG_ROUTING_NONE, kProviderID,
            SERVICE_WORKER_PROVIDER_FOR_WINDOW,
            ServiceWorkerProviderHost::FrameSecurityLevel::SECURE,
            helper_->context()->AsWeakPtr(), nullptr));
    provider_host_ = provider_host->AsWeakPtr();
    provider_host->SetDocumentUrl(GURL("https://example.com/"));
    registration_->SetActiveVersion(version_);
    provider_host->AssociateRegistration(registration_.get(),
                                         false /* notify_controllerchange */);

    ChromeBlobStorageContext* chrome_blob_storage_context =
        ChromeBlobStorageContext::GetFor(browser_context_.get());
    // Wait for chrome_blob_storage_context to finish initializing.
    base::RunLoop().RunUntilIdle();
    storage::BlobStorageContext* blob_storage_context =
        chrome_blob_storage_context->context();

    url_request_job_factory_.reset(new net::URLRequestJobFactoryImpl);
    std::unique_ptr<MockHttpProtocolHandler> handler(
        new MockHttpProtocolHandler(provider_host->AsWeakPtr(),
                                    browser_context_->GetResourceContext(),
                                    blob_storage_context->AsWeakPtr(), this));
    http_protocol_handler_ = handler.get();
    url_request_job_factory_->SetProtocolHandler("https", std::move(handler));
    url_request_job_factory_->SetProtocolHandler(
        "blob", CreateMockBlobProtocolHandler(blob_storage_context));
    url_request_context_.set_job_factory(url_request_job_factory_.get());

    helper_->context()->AddProviderHost(std::move(provider_host));
  }

  void TearDown() override {
    version_ = nullptr;
    registration_ = nullptr;
    helper_.reset();
  }

  void TestRequestResult(int expected_status_code,
                         const std::string& expected_status_text,
                         const std::string& expected_response,
                         bool expect_valid_ssl) {
    EXPECT_TRUE(request_->status().is_success());
    EXPECT_EQ(expected_status_code,
              request_->response_headers()->response_code());
    EXPECT_EQ(expected_status_text,
              request_->response_headers()->GetStatusText());
    EXPECT_EQ(expected_response, url_request_delegate_.response_data());
    const net::SSLInfo& ssl_info = request_->response_info().ssl_info;
    if (expect_valid_ssl) {
      EXPECT_TRUE(ssl_info.is_valid());
      EXPECT_EQ(ssl_info.security_bits, 0x100);
      EXPECT_EQ(ssl_info.connection_status, 0x300039);
    } else {
      EXPECT_FALSE(ssl_info.is_valid());
    }
  }

  void TestRequest(int expected_status_code,
                   const std::string& expected_status_text,
                   const std::string& expected_response,
                   bool expect_valid_ssl) {
    request_ = url_request_context_.CreateRequest(
        GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
        &url_request_delegate_);

    request_->set_method("GET");
    request_->Start();
    base::RunLoop().RunUntilIdle();
    TestRequestResult(expected_status_code, expected_status_text,
                      expected_response, expect_valid_ssl);
  }

  bool HasWork() { return version_->HasWork(); }

  // ServiceWorkerURLRequestJob::Delegate implementation:
  void OnPrepareToRestart() override { times_prepare_to_restart_invoked_++; }

  ServiceWorkerVersion* GetServiceWorkerVersion(
      ServiceWorkerMetrics::URLRequestJobResult* result) override {
    if (!provider_host_) {
      *result = ServiceWorkerMetrics::REQUEST_JOB_ERROR_NO_PROVIDER_HOST;
      return nullptr;
    }
    if (!provider_host_->active_version()) {
      *result = ServiceWorkerMetrics::REQUEST_JOB_ERROR_NO_ACTIVE_VERSION;
      return nullptr;
    }
    return provider_host_->active_version();
  }

  bool RequestStillValid(
      ServiceWorkerMetrics::URLRequestJobResult* result) override {
    if (!provider_host_) {
      *result = ServiceWorkerMetrics::REQUEST_JOB_ERROR_NO_PROVIDER_HOST;
      return false;
    }
    return true;
  }

  void MainResourceLoadFailed() override {
    CHECK(provider_host_);
    // Detach the controller so subresource requests also skip the worker.
    provider_host_->NotifyControllerLost();
  }

  // Runs a request where the active worker starts a request in ACTIVATING state
  // and fails to reach ACTIVATED.
  void RunFailToActivateTest(ResourceType resource_type) {
    http_protocol_handler_->set_resource_type(resource_type);

    // Start a request with an activating worker.
    version_->SetStatus(ServiceWorkerVersion::ACTIVATING);
    request_ = url_request_context_.CreateRequest(
        GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
        &url_request_delegate_);
    request_->set_method("GET");
    request_->Start();

    // Proceed until the job starts waiting for the worker to activate.
    base::RunLoop().RunUntilIdle();

    // Simulate another worker kicking out the incumbent worker.  PostTask since
    // it might respond synchronously, and the MockURLRequestDelegate would
    // complain that the message loop isn't being run.
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&ServiceWorkerVersion::SetStatus, version_,
                              ServiceWorkerVersion::REDUNDANT));
    base::RunLoop().RunUntilIdle();
  }

  TestBrowserThreadBundle thread_bundle_;

  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;

  std::unique_ptr<net::URLRequestJobFactoryImpl> url_request_job_factory_;
  net::URLRequestContext url_request_context_;
  MockURLRequestDelegate url_request_delegate_;
  std::unique_ptr<net::URLRequest> request_;

  std::unique_ptr<storage::BlobDataBuilder> blob_data_;

  int times_prepare_to_restart_invoked_ = 0;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  // Not owned.
  MockHttpProtocolHandler* http_protocol_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerURLRequestJobTest);
};

TEST_F(ServiceWorkerURLRequestJobTest, Simple) {
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  TestRequest(200, "OK", std::string(), true /* expect_valid_ssl */);

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(GURL(), info->original_url_via_service_worker());
  EXPECT_EQ(blink::WebServiceWorkerResponseTypeDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());
  EXPECT_FALSE(info->response_is_in_cache_storage());
  EXPECT_EQ(std::string(), info->response_cache_storage_cache_name());
}

class ProviderDeleteHelper : public EmbeddedWorkerTestHelper {
 public:
  ProviderDeleteHelper() : EmbeddedWorkerTestHelper(base::FilePath()) {}
  ~ProviderDeleteHelper() override {}

 protected:
  void OnFetchEvent(int embedded_worker_id,
                    int response_id,
                    int event_finish_id,
                    const ServiceWorkerFetchRequest& request) override {
    context()->RemoveProviderHost(mock_render_process_id(), kProviderID);
    SimulateSend(new ServiceWorkerHostMsg_FetchEventResponse(
        embedded_worker_id, response_id,
        SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
        ServiceWorkerResponse(
            GURL(), 200, "OK", blink::WebServiceWorkerResponseTypeDefault,
            ServiceWorkerHeaderMap(), std::string(), 0, GURL(),
            blink::WebServiceWorkerResponseErrorUnknown, base::Time(),
            false /* response_is_in_cache_storage */,
            std::string() /* response_cache_storage_cache_name */,
            ServiceWorkerHeaderList() /* cors_exposed_header_names */)));
    SimulateSend(new ServiceWorkerHostMsg_FetchEventFinished(
        embedded_worker_id, event_finish_id,
        blink::WebServiceWorkerEventResultCompleted));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProviderDeleteHelper);
};

TEST_F(ServiceWorkerURLRequestJobTest, DeletedProviderHostOnFetchEvent) {
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  // Shouldn't crash if the ProviderHost is deleted prior to completion of
  // the fetch event.
  SetUpWithHelper(new ProviderDeleteHelper);

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  TestRequest(500, "Service Worker Response Error", std::string(),
              false /* expect_valid_ssl */);

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(GURL(), info->original_url_via_service_worker());
  EXPECT_EQ(blink::WebServiceWorkerResponseTypeDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());
}

TEST_F(ServiceWorkerURLRequestJobTest, DeletedProviderHostBeforeFetchEvent) {
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
      &url_request_delegate_);

  request_->set_method("GET");
  request_->Start();
  helper_->context()->RemoveProviderHost(helper_->mock_render_process_id(),
                                         kProviderID);
  base::RunLoop().RunUntilIdle();
  TestRequestResult(500, "Service Worker Response Error", std::string(),
                    false /* expect_valid_ssl */);

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(GURL(), info->original_url_via_service_worker());
  EXPECT_EQ(blink::WebServiceWorkerResponseTypeDefault,
            info->response_type_via_service_worker());
  EXPECT_TRUE(info->service_worker_start_time().is_null());
  EXPECT_TRUE(info->service_worker_ready_time().is_null());
}

// Responds to fetch events with a blob.
class BlobResponder : public EmbeddedWorkerTestHelper {
 public:
  BlobResponder(const std::string& blob_uuid, uint64_t blob_size)
      : EmbeddedWorkerTestHelper(base::FilePath()),
        blob_uuid_(blob_uuid),
        blob_size_(blob_size) {}
  ~BlobResponder() override {}

 protected:
  void OnFetchEvent(int embedded_worker_id,
                    int response_id,
                    int event_finish_id,
                    const ServiceWorkerFetchRequest& request) override {
    SimulateSend(new ServiceWorkerHostMsg_FetchEventResponse(
        embedded_worker_id, response_id,
        SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
        ServiceWorkerResponse(
            GURL(), 200, "OK", blink::WebServiceWorkerResponseTypeDefault,
            ServiceWorkerHeaderMap(), blob_uuid_, blob_size_, GURL(),
            blink::WebServiceWorkerResponseErrorUnknown, base::Time(),
            false /* response_is_in_cache_storage */,
            std::string() /* response_cache_storage_cache_name */,
            ServiceWorkerHeaderList() /* cors_exposed_header_names */)));
    SimulateSend(new ServiceWorkerHostMsg_FetchEventFinished(
        embedded_worker_id, event_finish_id,
        blink::WebServiceWorkerEventResultCompleted));
  }

  std::string blob_uuid_;
  uint64_t blob_size_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlobResponder);
};

TEST_F(ServiceWorkerURLRequestJobTest, BlobResponse) {
  ChromeBlobStorageContext* blob_storage_context =
      ChromeBlobStorageContext::GetFor(browser_context_.get());
  std::string expected_response;
  expected_response.reserve((sizeof(kTestData) - 1) * 1024);
  for (int i = 0; i < 1024; ++i) {
    blob_data_->AppendData(kTestData);
    expected_response += kTestData;
  }
  std::unique_ptr<storage::BlobDataHandle> blob_handle =
      blob_storage_context->context()->AddFinishedBlob(blob_data_.get());
  SetUpWithHelper(
      new BlobResponder(blob_handle->uuid(), expected_response.size()));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  TestRequest(200, "OK", expected_response, true /* expect_valid_ssl */);

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(GURL(), info->original_url_via_service_worker());
  EXPECT_EQ(blink::WebServiceWorkerResponseTypeDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());
}

TEST_F(ServiceWorkerURLRequestJobTest, NonExistentBlobUUIDResponse) {
  SetUpWithHelper(new BlobResponder("blob-id:nothing-is-here", 0));
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  TestRequest(500, "Service Worker Response Error", std::string(),
              true /* expect_valid_ssl */);

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(GURL(), info->original_url_via_service_worker());
  EXPECT_EQ(blink::WebServiceWorkerResponseTypeDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());
}

// Responds to fetch events with a stream.
class StreamResponder : public EmbeddedWorkerTestHelper {
 public:
  explicit StreamResponder(const GURL& stream_url)
      : EmbeddedWorkerTestHelper(base::FilePath()), stream_url_(stream_url) {}
  ~StreamResponder() override {}

 protected:
  void OnFetchEvent(int embedded_worker_id,
                    int response_id,
                    int event_finish_id,
                    const ServiceWorkerFetchRequest& request) override {
    SimulateSend(new ServiceWorkerHostMsg_FetchEventResponse(
        embedded_worker_id, response_id,
        SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
        ServiceWorkerResponse(
            GURL(), 200, "OK", blink::WebServiceWorkerResponseTypeDefault,
            ServiceWorkerHeaderMap(), "", 0, stream_url_,
            blink::WebServiceWorkerResponseErrorUnknown, base::Time(),
            false /* response_is_in_cache_storage */,
            std::string() /* response_cache_storage_cache_name */,
            ServiceWorkerHeaderList() /* cors_exposed_header_names */)));
    SimulateSend(new ServiceWorkerHostMsg_FetchEventFinished(
        embedded_worker_id, event_finish_id,
        blink::WebServiceWorkerEventResultCompleted));
  }

  const GURL stream_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StreamResponder);
};

TEST_F(ServiceWorkerURLRequestJobTest, StreamResponse) {
  const GURL stream_url("blob://stream");
  StreamContext* stream_context =
      GetStreamContextForResourceContext(
          browser_context_->GetResourceContext());
  scoped_refptr<Stream> stream =
      new Stream(stream_context->registry(), nullptr, stream_url);
  SetUpWithHelper(new StreamResponder(stream_url));
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
      &url_request_delegate_);
  request_->set_method("GET");
  request_->Start();

  std::string expected_response;
  expected_response.reserve((sizeof(kTestData) - 1) * 1024);
  for (int i = 0; i < 1024; ++i) {
    expected_response += kTestData;
    stream->AddData(kTestData, sizeof(kTestData) - 1);
  }
  stream->Finalize();

  EXPECT_FALSE(HasWork());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasWork());
  EXPECT_TRUE(request_->status().is_success());
  EXPECT_EQ(200,
            request_->response_headers()->response_code());
  EXPECT_EQ("OK",
            request_->response_headers()->GetStatusText());
  EXPECT_EQ(expected_response, url_request_delegate_.response_data());

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(GURL(), info->original_url_via_service_worker());
  EXPECT_EQ(blink::WebServiceWorkerResponseTypeDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());

  request_.reset();
  EXPECT_FALSE(HasWork());
}

TEST_F(ServiceWorkerURLRequestJobTest, StreamResponse_DelayedRegistration) {
  const GURL stream_url("blob://stream");
  StreamContext* stream_context =
      GetStreamContextForResourceContext(
          browser_context_->GetResourceContext());
  SetUpWithHelper(new StreamResponder(stream_url));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
      &url_request_delegate_);
  request_->set_method("GET");
  request_->Start();

  scoped_refptr<Stream> stream =
      new Stream(stream_context->registry(), nullptr, stream_url);
  std::string expected_response;
  expected_response.reserve((sizeof(kTestData) - 1) * 1024);
  for (int i = 0; i < 1024; ++i) {
    expected_response += kTestData;
    stream->AddData(kTestData, sizeof(kTestData) - 1);
  }
  stream->Finalize();

  EXPECT_FALSE(HasWork());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasWork());
  EXPECT_TRUE(request_->status().is_success());
  EXPECT_EQ(200,
            request_->response_headers()->response_code());
  EXPECT_EQ("OK",
            request_->response_headers()->GetStatusText());
  EXPECT_EQ(expected_response, url_request_delegate_.response_data());

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(GURL(), info->original_url_via_service_worker());
  EXPECT_EQ(blink::WebServiceWorkerResponseTypeDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());

  request_.reset();
  EXPECT_FALSE(HasWork());
}


TEST_F(ServiceWorkerURLRequestJobTest, StreamResponse_QuickFinalize) {
  const GURL stream_url("blob://stream");
  StreamContext* stream_context =
      GetStreamContextForResourceContext(
          browser_context_->GetResourceContext());
  scoped_refptr<Stream> stream =
      new Stream(stream_context->registry(), nullptr, stream_url);
  std::string expected_response;
  expected_response.reserve((sizeof(kTestData) - 1) * 1024);
  for (int i = 0; i < 1024; ++i) {
    expected_response += kTestData;
    stream->AddData(kTestData, sizeof(kTestData) - 1);
  }
  stream->Finalize();
  SetUpWithHelper(new StreamResponder(stream_url));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
      &url_request_delegate_);
  request_->set_method("GET");
  request_->Start();
  EXPECT_FALSE(HasWork());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasWork());
  EXPECT_TRUE(request_->status().is_success());
  EXPECT_EQ(200,
            request_->response_headers()->response_code());
  EXPECT_EQ("OK",
            request_->response_headers()->GetStatusText());
  EXPECT_EQ(expected_response, url_request_delegate_.response_data());

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(GURL(), info->original_url_via_service_worker());
  EXPECT_EQ(blink::WebServiceWorkerResponseTypeDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());

  request_.reset();
  EXPECT_FALSE(HasWork());
}

TEST_F(ServiceWorkerURLRequestJobTest, StreamResponse_Flush) {
  const GURL stream_url("blob://stream");
  StreamContext* stream_context =
      GetStreamContextForResourceContext(
          browser_context_->GetResourceContext());
  scoped_refptr<Stream> stream =
      new Stream(stream_context->registry(), nullptr, stream_url);
  SetUpWithHelper(new StreamResponder(stream_url));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
      &url_request_delegate_);
  request_->set_method("GET");
  request_->Start();
  std::string expected_response;
  expected_response.reserve((sizeof(kTestData) - 1) * 1024);
  for (int i = 0; i < 1024; ++i) {
    expected_response += kTestData;
    stream->AddData(kTestData, sizeof(kTestData) - 1);
    stream->Flush();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(expected_response, url_request_delegate_.response_data());
  }
  stream->Finalize();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_->status().is_success());
  EXPECT_EQ(200,
            request_->response_headers()->response_code());
  EXPECT_EQ("OK",
            request_->response_headers()->GetStatusText());
  EXPECT_EQ(expected_response, url_request_delegate_.response_data());

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(GURL(), info->original_url_via_service_worker());
  EXPECT_EQ(blink::WebServiceWorkerResponseTypeDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());
}

TEST_F(ServiceWorkerURLRequestJobTest, StreamResponseAndCancel) {
  const GURL stream_url("blob://stream");
  StreamContext* stream_context =
      GetStreamContextForResourceContext(
          browser_context_->GetResourceContext());
  scoped_refptr<Stream> stream =
      new Stream(stream_context->registry(), nullptr, stream_url);
  ASSERT_EQ(stream.get(),
            stream_context->registry()->GetStream(stream_url).get());
  SetUpWithHelper(new StreamResponder(stream_url));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
      &url_request_delegate_);
  request_->set_method("GET");
  request_->Start();
  EXPECT_FALSE(HasWork());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasWork());

  std::string expected_response;
  expected_response.reserve((sizeof(kTestData) - 1) * 1024);
  for (int i = 0; i < 512; ++i) {
    expected_response += kTestData;
    stream->AddData(kTestData, sizeof(kTestData) - 1);
  }
  ASSERT_TRUE(stream_context->registry()->GetStream(stream_url).get());
  request_->Cancel();
  EXPECT_FALSE(HasWork());
  ASSERT_FALSE(stream_context->registry()->GetStream(stream_url).get());
  for (int i = 0; i < 512; ++i) {
    expected_response += kTestData;
    stream->AddData(kTestData, sizeof(kTestData) - 1);
  }
  stream->Finalize();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_->status().is_success());

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(GURL(), info->original_url_via_service_worker());
  EXPECT_EQ(blink::WebServiceWorkerResponseTypeDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());
}

TEST_F(ServiceWorkerURLRequestJobTest,
       StreamResponse_DelayedRegistrationAndCancel) {
  const GURL stream_url("blob://stream");
  StreamContext* stream_context =
      GetStreamContextForResourceContext(
          browser_context_->GetResourceContext());
  SetUpWithHelper(new StreamResponder(stream_url));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
      &url_request_delegate_);
  request_->set_method("GET");
  request_->Start();
  EXPECT_FALSE(HasWork());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasWork());
  request_->Cancel();
  EXPECT_FALSE(HasWork());

  scoped_refptr<Stream> stream =
      new Stream(stream_context->registry(), nullptr, stream_url);
  // The stream should not be registered to the stream registry.
  ASSERT_FALSE(stream_context->registry()->GetStream(stream_url).get());
  for (int i = 0; i < 1024; ++i)
    stream->AddData(kTestData, sizeof(kTestData) - 1);
  stream->Finalize();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_->status().is_success());

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  EXPECT_FALSE(ServiceWorkerResponseInfo::ForRequest(request_.get()));
}

// Helper to simulate failing to dispatch a fetch event to a worker.
class FailFetchHelper : public EmbeddedWorkerTestHelper {
 public:
  FailFetchHelper() : EmbeddedWorkerTestHelper(base::FilePath()) {}
  ~FailFetchHelper() override {}

 protected:
  void OnFetchEvent(int embedded_worker_id,
                    int response_id,
                    int event_finish_id,
                    const ServiceWorkerFetchRequest& request) override {
    SimulateWorkerStopped(embedded_worker_id);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FailFetchHelper);
};

TEST_F(ServiceWorkerURLRequestJobTest, FailFetchDispatch) {
  SetUpWithHelper(new FailFetchHelper);

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
      &url_request_delegate_);
  request_->set_method("GET");
  request_->Start();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_->status().is_success());
  // We should have fallen back to network.
  EXPECT_EQ(200, request_->GetResponseCode());
  EXPECT_EQ("PASS", url_request_delegate_.response_data());
  EXPECT_FALSE(HasWork());
  ServiceWorkerProviderHost* host = helper_->context()->GetProviderHost(
      helper_->mock_render_process_id(), kProviderID);
  ASSERT_TRUE(host);
  EXPECT_EQ(host->controlling_version(), nullptr);

  EXPECT_EQ(1, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());
}

// TODO(horo): Remove this test when crbug.com/485900 is fixed.
TEST_F(ServiceWorkerURLRequestJobTest, MainScriptHTTPResponseInfoNotSet) {
  // Shouldn't crash if MainScriptHttpResponseInfo is not set.
  SetUpWithHelper(new EmbeddedWorkerTestHelper(base::FilePath()), false);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
      &url_request_delegate_);
  request_->set_method("GET");
  request_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_->status().is_success());
  EXPECT_EQ(200, request_->GetResponseCode());
  EXPECT_EQ("", url_request_delegate_.response_data());

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(GURL(), info->original_url_via_service_worker());
  EXPECT_EQ(blink::WebServiceWorkerResponseTypeDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());
}

TEST_F(ServiceWorkerURLRequestJobTest, FailToActivate_MainResource) {
  RunFailToActivateTest(RESOURCE_TYPE_MAIN_FRAME);

  // The load should fail and we should have fallen back to network because
  // this is a main resource request.
  EXPECT_TRUE(request_->status().is_success());
  EXPECT_EQ(200, request_->GetResponseCode());
  EXPECT_EQ("PASS", url_request_delegate_.response_data());

  // The controller should be reset since the main resource request failed.
  ServiceWorkerProviderHost* host = helper_->context()->GetProviderHost(
      helper_->mock_render_process_id(), kProviderID);
  ASSERT_TRUE(host);
  EXPECT_EQ(host->controlling_version(), nullptr);
}

TEST_F(ServiceWorkerURLRequestJobTest, FailToActivate_Subresource) {
  RunFailToActivateTest(RESOURCE_TYPE_IMAGE);

  // The load should fail and we should not fall back to network because
  // this is a subresource request.
  EXPECT_TRUE(request_->status().is_success());
  EXPECT_EQ(500, request_->GetResponseCode());
  EXPECT_EQ("Service Worker Response Error",
            request_->response_headers()->GetStatusText());

  // The controller should still be set after a subresource request fails.
  ServiceWorkerProviderHost* host = helper_->context()->GetProviderHost(
      helper_->mock_render_process_id(), kProviderID);
  ASSERT_TRUE(host);
  EXPECT_EQ(host->controlling_version(), version_);
}

class EarlyResponseHelper : public EmbeddedWorkerTestHelper {
 public:
  EarlyResponseHelper() : EmbeddedWorkerTestHelper(base::FilePath()) {}
  ~EarlyResponseHelper() override {}

  void FinishWaitUntil() {
    SimulateSend(new ServiceWorkerHostMsg_FetchEventFinished(
        embedded_worker_id_, event_finish_id_,
        blink::WebServiceWorkerEventResultCompleted));
  }

 protected:
  void OnFetchEvent(int embedded_worker_id,
                    int response_id,
                    int event_finish_id,
                    const ServiceWorkerFetchRequest& request) override {
    embedded_worker_id_ = embedded_worker_id;
    event_finish_id_ = event_finish_id;
    SimulateSend(new ServiceWorkerHostMsg_FetchEventResponse(
        embedded_worker_id, response_id,
        SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
        ServiceWorkerResponse(
            GURL(), 200, "OK", blink::WebServiceWorkerResponseTypeDefault,
            ServiceWorkerHeaderMap(), std::string(), 0, GURL(),
            blink::WebServiceWorkerResponseErrorUnknown, base::Time(),
            false /* response_is_in_cache_storage */,
            std::string() /* response_cache_storage_cache_name */,
            ServiceWorkerHeaderList() /* cors_exposed_header_names */)));
  }

 private:
  int embedded_worker_id_ = 0;
  int event_finish_id_ = 0;
  DISALLOW_COPY_AND_ASSIGN(EarlyResponseHelper);
};

// This simulates the case when a response is returned and the fetch event is
// still in flight.
TEST_F(ServiceWorkerURLRequestJobTest, EarlyResponse) {
  EarlyResponseHelper* helper = new EarlyResponseHelper;
  SetUpWithHelper(helper);

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  TestRequest(200, "OK", std::string(), true /* expect_valid_ssl */);

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(GURL(), info->original_url_via_service_worker());
  EXPECT_EQ(blink::WebServiceWorkerResponseTypeDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());
  EXPECT_FALSE(info->response_is_in_cache_storage());
  EXPECT_EQ(std::string(), info->response_cache_storage_cache_name());

  EXPECT_TRUE(version_->HasWork());
  helper->FinishWaitUntil();
  EXPECT_FALSE(version_->HasWork());
}

// Helper for controlling when to respond to a fetch event.
class DelayedResponseHelper : public EmbeddedWorkerTestHelper {
 public:
  DelayedResponseHelper() : EmbeddedWorkerTestHelper(base::FilePath()) {}
  ~DelayedResponseHelper() override {}

  void Respond() {
    SimulateSend(new ServiceWorkerHostMsg_FetchEventResponse(
        embedded_worker_id_, response_id_,
        SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
        ServiceWorkerResponse(
            GURL(), 200, "OK", blink::WebServiceWorkerResponseTypeDefault,
            ServiceWorkerHeaderMap(), std::string(), 0, GURL(),
            blink::WebServiceWorkerResponseErrorUnknown, base::Time(),
            false /* response_is_in_cache_storage */,
            std::string() /* response_cache_storage_cache_name */,
            ServiceWorkerHeaderList() /* cors_exposed_header_names */)));
    SimulateSend(new ServiceWorkerHostMsg_FetchEventFinished(
        embedded_worker_id_, event_finish_id_,
        blink::WebServiceWorkerEventResultCompleted));
  }

 protected:
  void OnFetchEvent(int embedded_worker_id,
                    int response_id,
                    int event_finish_id,
                    const ServiceWorkerFetchRequest& request) override {
    embedded_worker_id_ = embedded_worker_id;
    response_id_ = response_id;
    event_finish_id_ = event_finish_id;
  }

 private:
  int embedded_worker_id_ = 0;
  int response_id_ = 0;
  int event_finish_id_ = 0;
  DISALLOW_COPY_AND_ASSIGN(DelayedResponseHelper);
};

// Test cancelling the URLRequest while the fetch event is in flight.
TEST_F(ServiceWorkerURLRequestJobTest, CancelRequest) {
  DelayedResponseHelper* helper = new DelayedResponseHelper;
  SetUpWithHelper(helper);

  // Start the URL request. The job will be waiting for the
  // worker to respond to the fetch event.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
      &url_request_delegate_);
  request_->set_method("GET");
  request_->Start();
  base::RunLoop().RunUntilIdle();

  // Cancel the URL request.
  request_->Cancel();
  base::RunLoop().RunUntilIdle();

  // Respond to the fetch event.
  EXPECT_TRUE(version_->HasWork());
  helper->Respond();
  base::RunLoop().RunUntilIdle();

  // The fetch event request should no longer be in-flight.
  EXPECT_FALSE(version_->HasWork());
}

// TODO(kinuko): Add more tests with different response data and also for
// FallbackToNetwork case.

}  // namespace content
