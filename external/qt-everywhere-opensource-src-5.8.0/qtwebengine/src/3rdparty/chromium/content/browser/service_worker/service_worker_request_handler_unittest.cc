// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_request_handler.h"

#include <utility>

#include "base/run_loop.h"
#include "content/browser/fileapi/mock_url_request_delegate.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/resource_request_body_impl.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request_context.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

int kMockProviderId = 1;

void EmptyCallback() {
}

}

class ServiceWorkerRequestHandlerTest : public testing::Test {
 public:
  ServiceWorkerRequestHandlerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));

    // A new unstored registration/version.
    registration_ = new ServiceWorkerRegistration(GURL("https://host/scope/"),
                                                  1L, context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(registration_.get(),
                                        GURL("https://host/script.js"), 1L,
                                        context()->AsWeakPtr());

    // An empty host.
    std::unique_ptr<ServiceWorkerProviderHost> host(
        new ServiceWorkerProviderHost(
            helper_->mock_render_process_id(), MSG_ROUTING_NONE,
            kMockProviderId, SERVICE_WORKER_PROVIDER_FOR_WINDOW,
            ServiceWorkerProviderHost::FrameSecurityLevel::SECURE,
            context()->AsWeakPtr(), nullptr));
    host->SetDocumentUrl(GURL("https://host/scope/"));
    provider_host_ = host->AsWeakPtr();
    context()->AddProviderHost(std::move(host));

    context()->storage()->LazyInitialize(base::Bind(&EmptyCallback));
    base::RunLoop().RunUntilIdle();

    version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
    registration_->SetActiveVersion(version_);
    context()->storage()->StoreRegistration(
        registration_.get(),
        version_.get(),
        base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
    provider_host_->AssociateRegistration(registration_.get(),
                                          false /* notify_controllerchange */);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    version_ = nullptr;
    registration_ = nullptr;
    helper_.reset();
  }

  ServiceWorkerContextCore* context() const { return helper_->context(); }
  ServiceWorkerContextWrapper* context_wrapper() const {
    return helper_->context_wrapper();
  }

  bool InitializeHandlerCheck(const std::string& url,
                              const std::string& method,
                              bool skip_service_worker,
                              ResourceType resource_type) {
    const GURL kDocUrl(url);
    std::unique_ptr<net::URLRequest> request =
        url_request_context_.CreateRequest(kDocUrl, net::DEFAULT_PRIORITY,
                                           &url_request_delegate_);
    request->set_method(method);
    ServiceWorkerRequestHandler::InitializeHandler(
        request.get(), context_wrapper(), &blob_storage_context_,
        helper_->mock_render_process_id(), kMockProviderId, skip_service_worker,
        FETCH_REQUEST_MODE_NO_CORS, FETCH_CREDENTIALS_MODE_OMIT,
        FetchRedirectMode::FOLLOW_MODE, resource_type,
        REQUEST_CONTEXT_TYPE_HYPERLINK, REQUEST_CONTEXT_FRAME_TYPE_TOP_LEVEL,
        nullptr);
    return ServiceWorkerRequestHandler::GetHandler(request.get()) != nullptr;
  }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  net::URLRequestContext url_request_context_;
  MockURLRequestDelegate url_request_delegate_;
  storage::BlobStorageContext blob_storage_context_;
};

TEST_F(ServiceWorkerRequestHandlerTest, InitializeHandler) {
  // Cannot initialize a handler for non-secure origins.
  EXPECT_FALSE(InitializeHandlerCheck(
      "ftp://host/scope/doc", "GET", false, RESOURCE_TYPE_MAIN_FRAME));
  // HTTP is ok because it might redirect to HTTPS.
  EXPECT_TRUE(InitializeHandlerCheck("http://host/scope/doc", "GET", false,
                                     RESOURCE_TYPE_MAIN_FRAME));
  EXPECT_TRUE(InitializeHandlerCheck("https://host/scope/doc", "GET", false,
                                     RESOURCE_TYPE_MAIN_FRAME));

  // OPTIONS is also supported. See crbug.com/434660.
  EXPECT_TRUE(InitializeHandlerCheck(
      "https://host/scope/doc", "OPTIONS", false, RESOURCE_TYPE_MAIN_FRAME));

  // Check provider host's URL after initializing a handler for main
  // frame.
  provider_host_->SetDocumentUrl(GURL(""));
  EXPECT_FALSE(InitializeHandlerCheck(
      "http://host/scope/doc", "GET", true, RESOURCE_TYPE_MAIN_FRAME));
  EXPECT_STREQ("http://host/scope/doc",
               provider_host_->document_url().spec().c_str());
  EXPECT_FALSE(InitializeHandlerCheck(
      "https://host/scope/doc", "GET", true, RESOURCE_TYPE_MAIN_FRAME));
  EXPECT_STREQ("https://host/scope/doc",
               provider_host_->document_url().spec().c_str());

  // Check provider host's URL after initializing a handler for a subframe.
  provider_host_->SetDocumentUrl(GURL(""));
  EXPECT_FALSE(InitializeHandlerCheck(
      "http://host/scope/doc", "GET", true, RESOURCE_TYPE_SUB_FRAME));
  EXPECT_STREQ("http://host/scope/doc",
               provider_host_->document_url().spec().c_str());
  EXPECT_FALSE(InitializeHandlerCheck(
      "https://host/scope/doc", "GET", true, RESOURCE_TYPE_SUB_FRAME));
  EXPECT_STREQ("https://host/scope/doc",
               provider_host_->document_url().spec().c_str());

  // Check provider host's URL after initializing a handler for an image.
  provider_host_->SetDocumentUrl(GURL(""));
  EXPECT_FALSE(InitializeHandlerCheck(
      "http://host/scope/doc", "GET", true, RESOURCE_TYPE_IMAGE));
  EXPECT_STREQ("", provider_host_->document_url().spec().c_str());
  EXPECT_FALSE(InitializeHandlerCheck(
      "https://host/scope/doc", "GET", true, RESOURCE_TYPE_IMAGE));
  EXPECT_STREQ("", provider_host_->document_url().spec().c_str());
}

}  // namespace content
