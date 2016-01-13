// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/fileapi/mock_url_request_delegate.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_url_request_job.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/io_buffer.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/blob/blob_storage_context.h"
#include "webkit/browser/blob/blob_url_request_job.h"
#include "webkit/browser/blob/blob_url_request_job_factory.h"
#include "webkit/common/blob/blob_data.h"

namespace content {

class ServiceWorkerURLRequestJobTest;

namespace {

const int kProcessID = 1;
const int kProviderID = 100;
const char kTestData[] = "Here is sample text for the blob.";

class MockHttpProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  MockHttpProtocolHandler(
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      base::WeakPtr<webkit_blob::BlobStorageContext> blob_storage_context)
      : provider_host_(provider_host),
        blob_storage_context_(blob_storage_context) {}
  virtual ~MockHttpProtocolHandler() {}

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    ServiceWorkerURLRequestJob* job = new ServiceWorkerURLRequestJob(
        request, network_delegate, provider_host_, blob_storage_context_);
    job->ForwardToServiceWorker();
    return job;
  }

 private:
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  base::WeakPtr<webkit_blob::BlobStorageContext> blob_storage_context_;
};

// Returns a BlobProtocolHandler that uses |blob_storage_context|. Caller owns
// the memory.
webkit_blob::BlobProtocolHandler* CreateMockBlobProtocolHandler(
    webkit_blob::BlobStorageContext* blob_storage_context) {
  // The FileSystemContext and MessageLoopProxy are not actually used but a
  // MessageLoopProxy is needed to avoid a DCHECK in BlobURLRequestJob ctor.
  return new webkit_blob::BlobProtocolHandler(
      blob_storage_context, NULL, base::MessageLoopProxy::current().get());
}

}  // namespace

class ServiceWorkerURLRequestJobTest : public testing::Test {
 protected:
  ServiceWorkerURLRequestJobTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        blob_data_(new webkit_blob::BlobData("blob-id:myblob")) {}
  virtual ~ServiceWorkerURLRequestJobTest() {}

  virtual void SetUp() OVERRIDE {
    browser_context_.reset(new TestBrowserContext);
    SetUpWithHelper(new EmbeddedWorkerTestHelper(kProcessID));
  }

  void SetUpWithHelper(EmbeddedWorkerTestHelper* helper) {
    helper_.reset(helper);

    registration_ = new ServiceWorkerRegistration(
        GURL("http://example.com/*"),
        GURL("http://example.com/service_worker.js"),
        1L,
        helper_->context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_, 1L, helper_->context()->AsWeakPtr());

    scoped_ptr<ServiceWorkerProviderHost> provider_host(
        new ServiceWorkerProviderHost(
            kProcessID, kProviderID, helper_->context()->AsWeakPtr(), NULL));
    provider_host->SetActiveVersion(version_.get());

    ChromeBlobStorageContext* chrome_blob_storage_context =
        ChromeBlobStorageContext::GetFor(browser_context_.get());
    // Wait for chrome_blob_storage_context to finish initializing.
    base::RunLoop().RunUntilIdle();
    webkit_blob::BlobStorageContext* blob_storage_context =
        chrome_blob_storage_context->context();

    url_request_job_factory_.reset(new net::URLRequestJobFactoryImpl);
    url_request_job_factory_->SetProtocolHandler(
        "http",
        new MockHttpProtocolHandler(provider_host->AsWeakPtr(),
                                    blob_storage_context->AsWeakPtr()));
    url_request_job_factory_->SetProtocolHandler(
        "blob", CreateMockBlobProtocolHandler(blob_storage_context));
    url_request_context_.set_job_factory(url_request_job_factory_.get());

    helper_->context()->AddProviderHost(provider_host.Pass());
  }

  virtual void TearDown() OVERRIDE {
    version_ = NULL;
    registration_ = NULL;
    helper_.reset();
  }

  void TestRequest(int expected_status_code,
                   const std::string& expected_status_text,
                   const std::string& expected_response) {
    request_ = url_request_context_.CreateRequest(
        GURL("http://example.com/foo.html"),
        net::DEFAULT_PRIORITY,
        &url_request_delegate_,
        NULL);

    request_->set_method("GET");
    request_->Start();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(request_->status().is_success());
    EXPECT_EQ(expected_status_code,
              request_->response_headers()->response_code());
    EXPECT_EQ(expected_status_text,
              request_->response_headers()->GetStatusText());
    EXPECT_EQ(expected_response, url_request_delegate_.response_data());
  }

  TestBrowserThreadBundle thread_bundle_;

  scoped_ptr<TestBrowserContext> browser_context_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;

  scoped_ptr<net::URLRequestJobFactoryImpl> url_request_job_factory_;
  net::URLRequestContext url_request_context_;
  MockURLRequestDelegate url_request_delegate_;
  scoped_ptr<net::URLRequest> request_;

  scoped_refptr<webkit_blob::BlobData> blob_data_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerURLRequestJobTest);
};

TEST_F(ServiceWorkerURLRequestJobTest, Simple) {
  version_->SetStatus(ServiceWorkerVersion::ACTIVE);
  TestRequest(200, "OK", std::string());
}

TEST_F(ServiceWorkerURLRequestJobTest, WaitForActivation) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->SetStatus(ServiceWorkerVersion::INSTALLED);
  version_->DispatchActivateEvent(CreateReceiverOnCurrentThread(&status));

  TestRequest(200, "OK", std::string());

  EXPECT_EQ(SERVICE_WORKER_OK, status);
}

// Responds to fetch events with a blob.
class BlobResponder : public EmbeddedWorkerTestHelper {
 public:
  BlobResponder(int mock_render_process_id, const std::string& blob_uuid)
      : EmbeddedWorkerTestHelper(mock_render_process_id),
        blob_uuid_(blob_uuid) {}
  virtual ~BlobResponder() {}

 protected:
  virtual void OnFetchEvent(int embedded_worker_id,
                            int request_id,
                            const ServiceWorkerFetchRequest& request) OVERRIDE {
    SimulateSend(new ServiceWorkerHostMsg_FetchEventFinished(
        embedded_worker_id,
        request_id,
        SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
        ServiceWorkerResponse(200,
                              "OK",
                              std::map<std::string, std::string>(),
                              blob_uuid_)));
  }

  std::string blob_uuid_;
  DISALLOW_COPY_AND_ASSIGN(BlobResponder);
};

TEST_F(ServiceWorkerURLRequestJobTest, BlobResponse) {
  ChromeBlobStorageContext* blob_storage_context =
      ChromeBlobStorageContext::GetFor(browser_context_.get());
  std::string expected_response;
  for (int i = 0; i < 1024; ++i) {
    blob_data_->AppendData(kTestData);
    expected_response += kTestData;
  }
  scoped_ptr<webkit_blob::BlobDataHandle> blob_handle =
      blob_storage_context->context()->AddFinishedBlob(blob_data_);
  SetUpWithHelper(new BlobResponder(kProcessID, blob_handle->uuid()));

  version_->SetStatus(ServiceWorkerVersion::ACTIVE);
  TestRequest(200, "OK", expected_response);
}

TEST_F(ServiceWorkerURLRequestJobTest, NonExistentBlobUUIDResponse) {
  SetUpWithHelper(new BlobResponder(kProcessID, "blob-id:nothing-is-here"));
  version_->SetStatus(ServiceWorkerVersion::ACTIVE);
  TestRequest(500, "Service Worker Response Error", std::string());
}

// TODO(kinuko): Add more tests with different response data and also for
// FallbackToNetwork case.

}  // namespace content
