// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_controllee_request_handler.h"

#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/fileapi/mock_url_request_delegate.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_url_request_job.h"
#include "content/common/resource_request_body_impl.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_content_browser_client.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

int kMockProviderId = 1;

}

class FailureHelper : public EmbeddedWorkerTestHelper {
 public:
  FailureHelper() : EmbeddedWorkerTestHelper(base::FilePath()) {}
  ~FailureHelper() override {}

 protected:
  void OnStartWorker(int embedded_worker_id,
                     int64_t service_worker_version_id,
                     const GURL& scope,
                     const GURL& script_url,
                     bool pause_after_download) override {
    SimulateWorkerStopped(embedded_worker_id);
  }
};

class ServiceWorkerControlleeRequestHandlerTest : public testing::Test {
 public:
  class ServiceWorkerRequestTestResources {
   public:
    ServiceWorkerRequestTestResources(
        ServiceWorkerControlleeRequestHandlerTest* test,
        const GURL& url,
        ResourceType type)
        : test_(test),
          request_(test->url_request_context_.CreateRequest(
              url,
              net::DEFAULT_PRIORITY,
              &test->url_request_delegate_)),
          handler_(new ServiceWorkerControlleeRequestHandler(
              test->context()->AsWeakPtr(),
              test->provider_host_,
              base::WeakPtr<storage::BlobStorageContext>(),
              FETCH_REQUEST_MODE_NO_CORS,
              FETCH_CREDENTIALS_MODE_OMIT,
              FetchRedirectMode::FOLLOW_MODE,
              type,
              REQUEST_CONTEXT_TYPE_HYPERLINK,
              REQUEST_CONTEXT_FRAME_TYPE_TOP_LEVEL,
              scoped_refptr<ResourceRequestBodyImpl>())),
          job_(nullptr) {}

    ServiceWorkerURLRequestJob* MaybeCreateJob() {
      job_.reset(handler_->MaybeCreateJob(request_.get(), nullptr,
                                          &test_->mock_resource_context_));
      return static_cast<ServiceWorkerURLRequestJob*>(job_.get());
    }

    void ResetHandler() { handler_.reset(nullptr); }

   private:
    ServiceWorkerControlleeRequestHandlerTest* test_;
    std::unique_ptr<net::URLRequest> request_;
    std::unique_ptr<ServiceWorkerControlleeRequestHandler> handler_;
    std::unique_ptr<net::URLRequestJob> job_;
  };

  ServiceWorkerControlleeRequestHandlerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    SetUpWithHelper(new EmbeddedWorkerTestHelper(base::FilePath()));
  }

  void SetUpWithHelper(EmbeddedWorkerTestHelper* helper) {
    helper_.reset(helper);

    // A new unstored registration/version.
    scope_ = GURL("https://host/scope/");
    script_url_ = GURL("https://host/script.js");
    registration_ = new ServiceWorkerRegistration(
        scope_, 1L, context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_.get(), script_url_, 1L, context()->AsWeakPtr());

    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    records.push_back(
        ServiceWorkerDatabase::ResourceRecord(10, version_->script_url(), 100));
    version_->script_cache_map()->SetResources(records);

    // An empty host.
    std::unique_ptr<ServiceWorkerProviderHost> host(
        new ServiceWorkerProviderHost(
            helper_->mock_render_process_id(), MSG_ROUTING_NONE,
            kMockProviderId, SERVICE_WORKER_PROVIDER_FOR_WINDOW,
            ServiceWorkerProviderHost::FrameSecurityLevel::SECURE,
            context()->AsWeakPtr(), NULL));
    provider_host_ = host->AsWeakPtr();
    context()->AddProviderHost(std::move(host));

    context()->storage()->LazyInitialize(base::Bind(&base::DoNothing));
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    version_ = NULL;
    registration_ = NULL;
    helper_.reset();
  }

  ServiceWorkerContextCore* context() const { return helper_->context(); }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  net::URLRequestContext url_request_context_;
  MockURLRequestDelegate url_request_delegate_;
  MockResourceContext mock_resource_context_;
  GURL scope_;
  GURL script_url_;
};

class ServiceWorkerTestContentBrowserClient : public TestContentBrowserClient {
 public:
  ServiceWorkerTestContentBrowserClient() {}
  bool AllowServiceWorker(const GURL& scope,
                          const GURL& first_party,
                          content::ResourceContext* context,
                          int render_process_id,
                          int render_frame_id) override {
    return false;
  }
};

TEST_F(ServiceWorkerControlleeRequestHandlerTest, DisallowServiceWorker) {
  ServiceWorkerTestContentBrowserClient test_browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&test_browser_client);

  // Store an activated worker.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  context()->storage()->StoreRegistration(
      registration_.get(),
      version_.get(),
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), RESOURCE_TYPE_MAIN_FRAME);
  ServiceWorkerURLRequestJob* sw_job = test_resources.MaybeCreateJob();

  EXPECT_FALSE(sw_job->ShouldFallbackToNetwork());
  EXPECT_FALSE(sw_job->ShouldForwardToServiceWorker());
  EXPECT_FALSE(version_->HasControllee());
  base::RunLoop().RunUntilIdle();

  // Verify we did not use the worker.
  EXPECT_TRUE(sw_job->ShouldFallbackToNetwork());
  EXPECT_FALSE(sw_job->ShouldForwardToServiceWorker());
  EXPECT_FALSE(version_->HasControllee());

  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(ServiceWorkerControlleeRequestHandlerTest, ActivateWaitingVersion) {
  // Store a registration that is installed but not activated yet.
  version_->SetStatus(ServiceWorkerVersion::INSTALLED);
  registration_->SetWaitingVersion(version_);
  context()->storage()->StoreRegistration(
      registration_.get(),
      version_.get(),
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), RESOURCE_TYPE_MAIN_FRAME);
  ServiceWorkerURLRequestJob* sw_job = test_resources.MaybeCreateJob();

  EXPECT_FALSE(sw_job->ShouldFallbackToNetwork());
  EXPECT_FALSE(sw_job->ShouldForwardToServiceWorker());
  EXPECT_FALSE(version_->HasControllee());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED,
            version_->status());
  EXPECT_FALSE(sw_job->ShouldFallbackToNetwork());
  EXPECT_TRUE(sw_job->ShouldForwardToServiceWorker());
  EXPECT_TRUE(version_->HasControllee());

  // Navigations should trigger an update too.
  test_resources.ResetHandler();
  EXPECT_TRUE(version_->update_timer_.IsRunning());
}

// Test that an installing registration is associated with a provider host.
TEST_F(ServiceWorkerControlleeRequestHandlerTest, InstallingRegistration) {
  // Create an installing registration.
  version_->SetStatus(ServiceWorkerVersion::INSTALLING);
  registration_->SetInstallingVersion(version_);
  context()->storage()->NotifyInstallingRegistration(registration_.get());

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), RESOURCE_TYPE_MAIN_FRAME);
  ServiceWorkerURLRequestJob* job = test_resources.MaybeCreateJob();

  base::RunLoop().RunUntilIdle();

  // The handler should have fallen back to network and destroyed the job. The
  // registration should be associated with the provider host, although it is
  // not controlled since there is no active version.
  EXPECT_FALSE(job);
  EXPECT_EQ(registration_.get(), provider_host_->associated_registration());
  EXPECT_EQ(version_.get(), provider_host_->installing_version());
  EXPECT_FALSE(version_->HasControllee());
  EXPECT_FALSE(provider_host_->controlling_version());
}

// Test to not regress crbug/414118.
TEST_F(ServiceWorkerControlleeRequestHandlerTest, DeletedProviderHost) {
  // Store a registration so the call to FindRegistrationForDocument will read
  // from the database.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  context()->storage()->StoreRegistration(
      registration_.get(),
      version_.get(),
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();
  version_ = NULL;
  registration_ = NULL;

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), RESOURCE_TYPE_MAIN_FRAME);
  ServiceWorkerURLRequestJob* sw_job = test_resources.MaybeCreateJob();

  EXPECT_FALSE(sw_job->ShouldFallbackToNetwork());
  EXPECT_FALSE(sw_job->ShouldForwardToServiceWorker());

  // Shouldn't crash if the ProviderHost is deleted prior to completion of
  // the database lookup.
  context()->RemoveProviderHost(helper_->mock_render_process_id(),
                                kMockProviderId);
  EXPECT_FALSE(provider_host_.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(sw_job->ShouldFallbackToNetwork());
  EXPECT_FALSE(sw_job->ShouldForwardToServiceWorker());
}

}  // namespace content
