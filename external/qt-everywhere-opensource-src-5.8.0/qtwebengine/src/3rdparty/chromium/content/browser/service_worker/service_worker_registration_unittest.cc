// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration.h"

#include <stdint.h>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration_handle.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

int CreateInflightRequest(ServiceWorkerVersion* version) {
  version->StartWorker(ServiceWorkerMetrics::EventType::PUSH,
                       base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();
  return version->StartRequest(
      ServiceWorkerMetrics::EventType::PUSH,
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
}

}  // namespace

class ServiceWorkerRegistrationTest : public testing::Test {
 public:
  ServiceWorkerRegistrationTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));

    helper_->context()->storage()->LazyInitialize(base::Bind(&base::DoNothing));
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    helper_.reset();
    base::RunLoop().RunUntilIdle();
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }
  ServiceWorkerStorage* storage() { return helper_->context()->storage(); }

  class RegistrationListener : public ServiceWorkerRegistration::Listener {
   public:
    RegistrationListener() {}
    ~RegistrationListener() {
      if (observed_registration_.get())
        observed_registration_->RemoveListener(this);
    }

    void OnVersionAttributesChanged(
        ServiceWorkerRegistration* registration,
        ChangedVersionAttributesMask changed_mask,
        const ServiceWorkerRegistrationInfo& info) override {
      observed_registration_ = registration;
      observed_changed_mask_ = changed_mask;
      observed_info_ = info;
    }

    void OnRegistrationFailed(
        ServiceWorkerRegistration* registration) override {
      NOTREACHED();
    }

    void OnUpdateFound(ServiceWorkerRegistration* registration) override {
      NOTREACHED();
    }

    void Reset() {
      observed_registration_ = NULL;
      observed_changed_mask_ = ChangedVersionAttributesMask();
      observed_info_ = ServiceWorkerRegistrationInfo();
    }

    scoped_refptr<ServiceWorkerRegistration> observed_registration_;
    ChangedVersionAttributesMask observed_changed_mask_;
    ServiceWorkerRegistrationInfo observed_info_;
  };

 private:
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  TestBrowserThreadBundle thread_bundle_;
};

TEST_F(ServiceWorkerRegistrationTest, SetAndUnsetVersions) {
  const GURL kScope("http://www.example.not/");
  const GURL kScript("http://www.example.not/service_worker.js");
  int64_t kRegistrationId = 1L;
  scoped_refptr<ServiceWorkerRegistration> registration =
      new ServiceWorkerRegistration(kScope, kRegistrationId,
                                    context()->AsWeakPtr());

  const int64_t version_1_id = 1L;
  const int64_t version_2_id = 2L;
  scoped_refptr<ServiceWorkerVersion> version_1 = new ServiceWorkerVersion(
      registration.get(), kScript, version_1_id, context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> version_2 = new ServiceWorkerVersion(
      registration.get(), kScript, version_2_id, context()->AsWeakPtr());

  RegistrationListener listener;
  registration->AddListener(&listener);
  registration->SetActiveVersion(version_1);

  EXPECT_EQ(version_1.get(), registration->active_version());
  EXPECT_EQ(registration, listener.observed_registration_);
  EXPECT_EQ(ChangedVersionAttributesMask::ACTIVE_VERSION,
            listener.observed_changed_mask_.changed());
  EXPECT_EQ(kScope, listener.observed_info_.pattern);
  EXPECT_EQ(version_1_id, listener.observed_info_.active_version.version_id);
  EXPECT_EQ(kScript, listener.observed_info_.active_version.script_url);
  EXPECT_EQ(listener.observed_info_.installing_version.version_id,
            kInvalidServiceWorkerVersionId);
  EXPECT_EQ(listener.observed_info_.waiting_version.version_id,
            kInvalidServiceWorkerVersionId);
  listener.Reset();

  registration->SetInstallingVersion(version_2);

  EXPECT_EQ(version_2.get(), registration->installing_version());
  EXPECT_EQ(ChangedVersionAttributesMask::INSTALLING_VERSION,
            listener.observed_changed_mask_.changed());
  EXPECT_EQ(version_1_id, listener.observed_info_.active_version.version_id);
  EXPECT_EQ(version_2_id,
            listener.observed_info_.installing_version.version_id);
  EXPECT_EQ(listener.observed_info_.waiting_version.version_id,
            kInvalidServiceWorkerVersionId);
  listener.Reset();

  registration->SetWaitingVersion(version_2);

  EXPECT_EQ(version_2.get(), registration->waiting_version());
  EXPECT_FALSE(registration->installing_version());
  EXPECT_TRUE(listener.observed_changed_mask_.waiting_changed());
  EXPECT_TRUE(listener.observed_changed_mask_.installing_changed());
  EXPECT_EQ(version_1_id, listener.observed_info_.active_version.version_id);
  EXPECT_EQ(version_2_id, listener.observed_info_.waiting_version.version_id);
  EXPECT_EQ(listener.observed_info_.installing_version.version_id,
            kInvalidServiceWorkerVersionId);
  listener.Reset();

  registration->UnsetVersion(version_2.get());

  EXPECT_FALSE(registration->waiting_version());
  EXPECT_EQ(ChangedVersionAttributesMask::WAITING_VERSION,
            listener.observed_changed_mask_.changed());
  EXPECT_EQ(version_1_id, listener.observed_info_.active_version.version_id);
  EXPECT_EQ(listener.observed_info_.waiting_version.version_id,
            kInvalidServiceWorkerVersionId);
  EXPECT_EQ(listener.observed_info_.installing_version.version_id,
            kInvalidServiceWorkerVersionId);
}

TEST_F(ServiceWorkerRegistrationTest, FailedRegistrationNoCrash) {
  const GURL kScope("http://www.example.not/");
  int64_t kRegistrationId = 1L;
  scoped_refptr<ServiceWorkerRegistration> registration =
      new ServiceWorkerRegistration(kScope, kRegistrationId,
                                    context()->AsWeakPtr());
  std::unique_ptr<ServiceWorkerRegistrationHandle> handle(
      new ServiceWorkerRegistrationHandle(
          context()->AsWeakPtr(), base::WeakPtr<ServiceWorkerProviderHost>(),
          registration.get()));
  registration->NotifyRegistrationFailed();
  // Don't crash when handle gets destructed.
}

// Sets up a registration with a waiting worker, and an active worker
// with a controllee and an inflight request.
class ServiceWorkerActivationTest : public ServiceWorkerRegistrationTest {
 public:
  ServiceWorkerActivationTest() : ServiceWorkerRegistrationTest() {}

  void SetUp() override {
    ServiceWorkerRegistrationTest::SetUp();

    const GURL kScope("https://www.example.not/");
    const GURL kScript("https://www.example.not/service_worker.js");

    registration_ = new ServiceWorkerRegistration(
        kScope, storage()->NewRegistrationId(), context()->AsWeakPtr());

    // Create an active version.
    scoped_refptr<ServiceWorkerVersion> version_1 = new ServiceWorkerVersion(
        registration_.get(), kScript, storage()->NewVersionId(),
        context()->AsWeakPtr());
    registration_->SetActiveVersion(version_1);
    version_1->SetStatus(ServiceWorkerVersion::ACTIVATED);

    // Store the registration.
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    records.push_back(ServiceWorkerDatabase::ResourceRecord(
        10, version_1->script_url(), 100));
    version_1->script_cache_map()->SetResources(records);
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
    context()->storage()->StoreRegistration(
        registration_.get(), version_1.get(),
        CreateReceiverOnCurrentThread(&status));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(SERVICE_WORKER_OK, status);

    // Give the active version a controllee.
    host_.reset(new ServiceWorkerProviderHost(
        33 /* dummy render process id */,
        MSG_ROUTING_NONE /* render_frame_id */, 1 /* dummy provider_id */,
        SERVICE_WORKER_PROVIDER_FOR_WINDOW,
        ServiceWorkerProviderHost::FrameSecurityLevel::SECURE,
        context()->AsWeakPtr(), nullptr));
    version_1->AddControllee(host_.get());

    // Give the active version an in-flight request.
    inflight_request_id_ = CreateInflightRequest(version_1.get());

    // Create a waiting version.
    scoped_refptr<ServiceWorkerVersion> version_2 = new ServiceWorkerVersion(
        registration_.get(), kScript, storage()->NewVersionId(),
        context()->AsWeakPtr());
    registration_->SetWaitingVersion(version_2);
    version_2->SetStatus(ServiceWorkerVersion::INSTALLED);

    // Set it to activate when ready. The original version should still be
    // active.
    registration_->ActivateWaitingVersionWhenReady();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(version_1.get(), registration_->active_version());
  }

  void TearDown() override {
    registration_->active_version()->RemoveListener(registration_.get());
    ServiceWorkerRegistrationTest::TearDown();
  }

  ServiceWorkerRegistration* registration() { return registration_.get(); }
  ServiceWorkerProviderHost* controllee() { return host_.get(); }
  int inflight_request_id() const { return inflight_request_id_; }

 private:
  scoped_refptr<ServiceWorkerRegistration> registration_;
  std::unique_ptr<ServiceWorkerProviderHost> host_;
  int inflight_request_id_ = -1;
};

// Test activation triggered by finishing all requests.
TEST_F(ServiceWorkerActivationTest, NoInflightRequest) {
  scoped_refptr<ServiceWorkerRegistration> reg = registration();
  scoped_refptr<ServiceWorkerVersion> version_1 = reg->active_version();
  scoped_refptr<ServiceWorkerVersion> version_2 = reg->waiting_version();

  // Remove the controllee. Since there is an in-flight request,
  // activation should not yet happen.
  version_1->RemoveControllee(controllee());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_1.get(), reg->active_version());

  // Finish the request. Activation should happen.
  version_1->FinishRequest(inflight_request_id(), true /* was_handled */);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_2.get(), reg->active_version());
}

// Test activation triggered by loss of controllee.
TEST_F(ServiceWorkerActivationTest, NoControllee) {
  scoped_refptr<ServiceWorkerRegistration> reg = registration();
  scoped_refptr<ServiceWorkerVersion> version_1 = reg->active_version();
  scoped_refptr<ServiceWorkerVersion> version_2 = reg->waiting_version();

  // Finish the request. Since there is a controllee, activation should not yet
  // happen.
  version_1->FinishRequest(inflight_request_id(), true /* was_handled */);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_1.get(), reg->active_version());

  // Remove the controllee. Activation should happen.
  version_1->RemoveControllee(controllee());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_2.get(), reg->active_version());
}

// Test activation triggered by skipWaiting.
TEST_F(ServiceWorkerActivationTest, SkipWaiting) {
  scoped_refptr<ServiceWorkerRegistration> reg = registration();
  scoped_refptr<ServiceWorkerVersion> version_1 = reg->active_version();
  scoped_refptr<ServiceWorkerVersion> version_2 = reg->waiting_version();

  // Finish the in-flight request. Since there is a controllee,
  // activation should not happen.
  version_1->FinishRequest(inflight_request_id(), true /* was_handled */);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_1.get(), reg->active_version());

  // Call skipWaiting. Activation should happen.
  version_2->OnSkipWaiting(77 /* dummy request_id */);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_2.get(), reg->active_version());
}

// Test activation triggered by skipWaiting and finishing requests.
TEST_F(ServiceWorkerActivationTest, SkipWaitingWithInflightRequest) {
  scoped_refptr<ServiceWorkerRegistration> reg = registration();
  scoped_refptr<ServiceWorkerVersion> version_1 = reg->active_version();
  scoped_refptr<ServiceWorkerVersion> version_2 = reg->waiting_version();

  // Set skip waiting flag. Since there is still an in-flight request,
  // activation should not happen.
  version_2->OnSkipWaiting(77 /* dummy request_id */);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_1.get(), reg->active_version());

  // Finish the request. Activation should happen.
  version_1->FinishRequest(inflight_request_id(), true /* was_handled */);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_2.get(), reg->active_version());
}

}  // namespace content
