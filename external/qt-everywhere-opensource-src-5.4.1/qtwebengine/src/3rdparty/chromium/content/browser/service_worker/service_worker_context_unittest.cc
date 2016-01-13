// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/service_worker_context.h"

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

void SaveResponseCallback(bool* called,
                          int64* store_registration_id,
                          int64* store_version_id,
                          ServiceWorkerStatusCode status,
                          int64 registration_id,
                          int64 version_id) {
  EXPECT_EQ(SERVICE_WORKER_OK, status) << ServiceWorkerStatusToString(status);
  *called = true;
  *store_registration_id = registration_id;
  *store_version_id = version_id;
}

ServiceWorkerContextCore::RegistrationCallback MakeRegisteredCallback(
    bool* called,
    int64* store_registration_id,
    int64* store_version_id) {
  return base::Bind(&SaveResponseCallback, called,
                    store_registration_id,
                    store_version_id);
}

void CallCompletedCallback(bool* called, ServiceWorkerStatusCode) {
  *called = true;
}

ServiceWorkerContextCore::UnregistrationCallback MakeUnregisteredCallback(
    bool* called) {
  return base::Bind(&CallCompletedCallback, called);
}

void ExpectRegisteredWorkers(
    ServiceWorkerStatusCode expect_status,
    int64 expect_version_id,
    bool expect_waiting,
    bool expect_active,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  ASSERT_EQ(expect_status, status);
  if (status != SERVICE_WORKER_OK) {
    EXPECT_FALSE(registration);
    return;
  }

  if (expect_waiting) {
    EXPECT_TRUE(registration->waiting_version());
    EXPECT_EQ(expect_version_id,
              registration->waiting_version()->version_id());
  } else {
    EXPECT_FALSE(registration->waiting_version());
  }

  if (expect_active) {
    EXPECT_TRUE(registration->active_version());
    EXPECT_EQ(expect_version_id,
              registration->active_version()->version_id());
  } else {
    EXPECT_FALSE(registration->active_version());
  }
}

class RejectInstallTestHelper : public EmbeddedWorkerTestHelper {
 public:
  RejectInstallTestHelper(int mock_render_process_id)
      : EmbeddedWorkerTestHelper(mock_render_process_id) {}

  virtual void OnInstallEvent(int embedded_worker_id,
                              int request_id,
                              int active_version_id) OVERRIDE {
    SimulateSend(
        new ServiceWorkerHostMsg_InstallEventFinished(
            embedded_worker_id, request_id,
            blink::WebServiceWorkerEventResultRejected));
  }
};

class RejectActivateTestHelper : public EmbeddedWorkerTestHelper {
 public:
  RejectActivateTestHelper(int mock_render_process_id)
      : EmbeddedWorkerTestHelper(mock_render_process_id) {}

  virtual void OnActivateEvent(int embedded_worker_id,
                               int request_id) OVERRIDE {
    SimulateSend(
        new ServiceWorkerHostMsg_ActivateEventFinished(
            embedded_worker_id, request_id,
            blink::WebServiceWorkerEventResultRejected));
  }
};

}  // namespace

class ServiceWorkerContextTest : public testing::Test {
 public:
  ServiceWorkerContextTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        render_process_id_(99) {}

  virtual void SetUp() OVERRIDE {
    helper_.reset(new EmbeddedWorkerTestHelper(render_process_id_));
  }

  virtual void TearDown() OVERRIDE {
    helper_.reset();
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  const int render_process_id_;
};

// Make sure basic registration is working.
TEST_F(ServiceWorkerContextTest, Register) {
  int64 registration_id = kInvalidServiceWorkerRegistrationId;
  int64 version_id = kInvalidServiceWorkerVersionId;
  bool called = false;
  context()->RegisterServiceWorker(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      NULL,
      MakeRegisteredCallback(&called, &registration_id, &version_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  EXPECT_EQ(4UL, helper_->ipc_sink()->message_count());
  EXPECT_TRUE(helper_->ipc_sink()->GetUniqueMessageMatching(
      EmbeddedWorkerMsg_StartWorker::ID));
  EXPECT_TRUE(helper_->inner_ipc_sink()->GetUniqueMessageMatching(
      ServiceWorkerMsg_InstallEvent::ID));
  EXPECT_TRUE(helper_->inner_ipc_sink()->GetUniqueMessageMatching(
      ServiceWorkerMsg_ActivateEvent::ID));
  EXPECT_TRUE(helper_->ipc_sink()->GetUniqueMessageMatching(
      EmbeddedWorkerMsg_StopWorker::ID));
  EXPECT_NE(kInvalidServiceWorkerRegistrationId, registration_id);
  EXPECT_NE(kInvalidServiceWorkerVersionId, version_id);

  context()->storage()->FindRegistrationForId(
      registration_id,
      GURL("http://www.example.com"),
      base::Bind(&ExpectRegisteredWorkers,
                 SERVICE_WORKER_OK,
                 version_id,
                 false /* expect_waiting */,
                 true /* expect_active */));
  base::RunLoop().RunUntilIdle();
}

// Test registration when the service worker rejects the install event. The
// registration callback should indicate success, but there should be no waiting
// or active worker in the registration.
TEST_F(ServiceWorkerContextTest, Register_RejectInstall) {
  helper_.reset();  // Make sure the process lookups stay overridden.
  helper_.reset(new RejectInstallTestHelper(render_process_id_));
  int64 registration_id = kInvalidServiceWorkerRegistrationId;
  int64 version_id = kInvalidServiceWorkerVersionId;
  bool called = false;
  context()->RegisterServiceWorker(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      NULL,
      MakeRegisteredCallback(&called, &registration_id, &version_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  EXPECT_EQ(3UL, helper_->ipc_sink()->message_count());
  EXPECT_TRUE(helper_->ipc_sink()->GetUniqueMessageMatching(
      EmbeddedWorkerMsg_StartWorker::ID));
  EXPECT_TRUE(helper_->inner_ipc_sink()->GetUniqueMessageMatching(
      ServiceWorkerMsg_InstallEvent::ID));
  EXPECT_FALSE(helper_->inner_ipc_sink()->GetUniqueMessageMatching(
      ServiceWorkerMsg_ActivateEvent::ID));
  EXPECT_TRUE(helper_->ipc_sink()->GetUniqueMessageMatching(
      EmbeddedWorkerMsg_StopWorker::ID));
  EXPECT_NE(kInvalidServiceWorkerRegistrationId, registration_id);
  EXPECT_NE(kInvalidServiceWorkerVersionId, version_id);

  context()->storage()->FindRegistrationForId(
      registration_id,
      GURL("http://www.example.com"),
      base::Bind(&ExpectRegisteredWorkers,
                 SERVICE_WORKER_ERROR_NOT_FOUND,
                 kInvalidServiceWorkerVersionId,
                 false /* expect_waiting */,
                 false /* expect_active */));
  base::RunLoop().RunUntilIdle();
}

// Test registration when the service worker rejects the activate event. The
// registration callback should indicate success, but there should be no waiting
// or active worker in the registration.
TEST_F(ServiceWorkerContextTest, Register_RejectActivate) {
  helper_.reset();  // Make sure the process lookups stay overridden.
  helper_.reset(new RejectActivateTestHelper(render_process_id_));
  int64 registration_id = kInvalidServiceWorkerRegistrationId;
  int64 version_id = kInvalidServiceWorkerVersionId;
  bool called = false;
  context()->RegisterServiceWorker(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      NULL,
      MakeRegisteredCallback(&called, &registration_id, &version_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  EXPECT_EQ(4UL, helper_->ipc_sink()->message_count());
  EXPECT_TRUE(helper_->ipc_sink()->GetUniqueMessageMatching(
      EmbeddedWorkerMsg_StartWorker::ID));
  EXPECT_TRUE(helper_->inner_ipc_sink()->GetUniqueMessageMatching(
      ServiceWorkerMsg_InstallEvent::ID));
  EXPECT_TRUE(helper_->inner_ipc_sink()->GetUniqueMessageMatching(
      ServiceWorkerMsg_ActivateEvent::ID));
  EXPECT_TRUE(helper_->ipc_sink()->GetUniqueMessageMatching(
      EmbeddedWorkerMsg_StopWorker::ID));
  EXPECT_NE(kInvalidServiceWorkerRegistrationId, registration_id);
  EXPECT_NE(kInvalidServiceWorkerVersionId, version_id);

  context()->storage()->FindRegistrationForId(
      registration_id,
      GURL("http://www.example.com"),
      base::Bind(&ExpectRegisteredWorkers,
                 SERVICE_WORKER_ERROR_NOT_FOUND,
                 kInvalidServiceWorkerVersionId,
                 false /* expect_waiting */,
                 false /* expect_active */));
  base::RunLoop().RunUntilIdle();
}

// Make sure registrations are cleaned up when they are unregistered.
TEST_F(ServiceWorkerContextTest, Unregister) {
  GURL pattern("http://www.example.com/*");

  bool called = false;
  int64 registration_id = kInvalidServiceWorkerRegistrationId;
  int64 version_id = kInvalidServiceWorkerVersionId;
  context()->RegisterServiceWorker(
      pattern,
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      NULL,
      MakeRegisteredCallback(&called, &registration_id, &version_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);
  EXPECT_NE(kInvalidServiceWorkerRegistrationId, registration_id);
  EXPECT_NE(kInvalidServiceWorkerVersionId, version_id);

  called = false;
  context()->UnregisterServiceWorker(pattern,
                                     MakeUnregisteredCallback(&called));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  context()->storage()->FindRegistrationForId(
      registration_id,
      pattern.GetOrigin(),
      base::Bind(&ExpectRegisteredWorkers,
                 SERVICE_WORKER_ERROR_NOT_FOUND,
                 kInvalidServiceWorkerVersionId,
                 false /* expect_waiting */,
                 false /* expect_active */));
  base::RunLoop().RunUntilIdle();
}

// Make sure that when a new registration replaces an existing
// registration, that the old one is cleaned up.
TEST_F(ServiceWorkerContextTest, RegisterNewScript) {
  GURL pattern("http://www.example.com/*");

  bool called = false;
  int64 old_registration_id = kInvalidServiceWorkerRegistrationId;
  int64 old_version_id = kInvalidServiceWorkerVersionId;
  context()->RegisterServiceWorker(
      pattern,
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      NULL,
      MakeRegisteredCallback(&called, &old_registration_id, &old_version_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);
  EXPECT_NE(kInvalidServiceWorkerRegistrationId, old_registration_id);
  EXPECT_NE(kInvalidServiceWorkerVersionId, old_version_id);

  called = false;
  int64 new_registration_id = kInvalidServiceWorkerRegistrationId;
  int64 new_version_id = kInvalidServiceWorkerVersionId;
  context()->RegisterServiceWorker(
      pattern,
      GURL("http://www.example.com/service_worker_new.js"),
      render_process_id_,
      NULL,
      MakeRegisteredCallback(&called, &new_registration_id, &new_version_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  // Returned IDs should be valid, and should differ from the values
  // returned for the previous registration.
  EXPECT_NE(kInvalidServiceWorkerRegistrationId, new_registration_id);
  EXPECT_NE(kInvalidServiceWorkerVersionId, new_version_id);
  EXPECT_NE(old_registration_id, new_registration_id);
  EXPECT_NE(old_version_id, new_version_id);
}

// Make sure that when registering a duplicate pattern+script_url
// combination, that the same registration is used.
TEST_F(ServiceWorkerContextTest, RegisterDuplicateScript) {
  GURL pattern("http://www.example.com/*");
  GURL script_url("http://www.example.com/service_worker.js");

  bool called = false;
  int64 old_registration_id = kInvalidServiceWorkerRegistrationId;
  int64 old_version_id = kInvalidServiceWorkerVersionId;
  context()->RegisterServiceWorker(
      pattern,
      script_url,
      render_process_id_,
      NULL,
      MakeRegisteredCallback(&called, &old_registration_id, &old_version_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);
  EXPECT_NE(kInvalidServiceWorkerRegistrationId, old_registration_id);
  EXPECT_NE(kInvalidServiceWorkerVersionId, old_version_id);

  called = false;
  int64 new_registration_id = kInvalidServiceWorkerRegistrationId;
  int64 new_version_id = kInvalidServiceWorkerVersionId;
  context()->RegisterServiceWorker(
      pattern,
      script_url,
      render_process_id_,
      NULL,
      MakeRegisteredCallback(&called, &new_registration_id, &new_version_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);
  EXPECT_EQ(old_registration_id, new_registration_id);
  EXPECT_EQ(old_version_id, new_version_id);
}

}  // namespace content
