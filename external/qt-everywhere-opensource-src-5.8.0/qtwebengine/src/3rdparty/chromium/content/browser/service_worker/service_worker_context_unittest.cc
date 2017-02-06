// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/service_worker_context.h"

#include <stdint.h>

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
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
                          int64_t* store_registration_id,
                          ServiceWorkerStatusCode status,
                          const std::string& status_message,
                          int64_t registration_id) {
  EXPECT_EQ(SERVICE_WORKER_OK, status) << ServiceWorkerStatusToString(status);
  *called = true;
  *store_registration_id = registration_id;
}

ServiceWorkerContextCore::RegistrationCallback MakeRegisteredCallback(
    bool* called,
    int64_t* store_registration_id) {
  return base::Bind(&SaveResponseCallback, called, store_registration_id);
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
    bool expect_waiting,
    bool expect_active,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  ASSERT_EQ(expect_status, status);
  if (status != SERVICE_WORKER_OK) {
    EXPECT_FALSE(registration.get());
    return;
  }

  if (expect_waiting) {
    EXPECT_TRUE(registration->waiting_version());
  } else {
    EXPECT_FALSE(registration->waiting_version());
  }

  if (expect_active) {
    EXPECT_TRUE(registration->active_version());
  } else {
    EXPECT_FALSE(registration->active_version());
  }
}

class RejectInstallTestHelper : public EmbeddedWorkerTestHelper {
 public:
  RejectInstallTestHelper() : EmbeddedWorkerTestHelper(base::FilePath()) {}

  void OnInstallEvent(int embedded_worker_id,
                      int request_id) override {
    SimulateSend(new ServiceWorkerHostMsg_InstallEventFinished(
        embedded_worker_id, request_id,
        blink::WebServiceWorkerEventResultRejected, true));
  }
};

class RejectActivateTestHelper : public EmbeddedWorkerTestHelper {
 public:
  RejectActivateTestHelper() : EmbeddedWorkerTestHelper(base::FilePath()) {}

  void OnActivateEvent(int embedded_worker_id, int request_id) override {
    SimulateSend(
        new ServiceWorkerHostMsg_ActivateEventFinished(
            embedded_worker_id, request_id,
            blink::WebServiceWorkerEventResultRejected));
  }
};

enum NotificationType {
  REGISTRATION_STORED,
  REGISTRATION_DELETED,
  STORAGE_RECOVERED,
};

struct NotificationLog {
  NotificationType type;
  GURL pattern;
  int64_t registration_id;
};

}  // namespace

class ServiceWorkerContextTest : public ServiceWorkerContextObserver,
                                 public testing::Test {
 public:
  ServiceWorkerContextTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));
    helper_->context_wrapper()->AddObserver(this);
  }

  void TearDown() override { helper_.reset(); }

  // ServiceWorkerContextObserver overrides.
  void OnRegistrationStored(int64_t registration_id,
                            const GURL& pattern) override {
    NotificationLog log;
    log.type = REGISTRATION_STORED;
    log.pattern = pattern;
    log.registration_id = registration_id;
    notifications_.push_back(log);
  }
  void OnRegistrationDeleted(int64_t registration_id,
                             const GURL& pattern) override {
    NotificationLog log;
    log.type = REGISTRATION_DELETED;
    log.pattern = pattern;
    log.registration_id = registration_id;
    notifications_.push_back(log);
  }
  void OnStorageWiped() override {
    NotificationLog log;
    log.type = STORAGE_RECOVERED;
    notifications_.push_back(log);
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  std::vector<NotificationLog> notifications_;
};

// Make sure basic registration is working.
TEST_F(ServiceWorkerContextTest, Register) {
  GURL pattern("http://www.example.com/");
  GURL script_url("http://www.example.com/service_worker.js");

  int64_t registration_id = kInvalidServiceWorkerRegistrationId;
  bool called = false;
  context()->RegisterServiceWorker(
      pattern,
      script_url,
      NULL,
      MakeRegisteredCallback(&called, &registration_id));

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

  ASSERT_EQ(1u, notifications_.size());
  EXPECT_EQ(REGISTRATION_STORED, notifications_[0].type);
  EXPECT_EQ(pattern, notifications_[0].pattern);
  EXPECT_EQ(registration_id, notifications_[0].registration_id);

  context()->storage()->FindRegistrationForId(
      registration_id,
      pattern.GetOrigin(),
      base::Bind(&ExpectRegisteredWorkers,
                 SERVICE_WORKER_OK,
                 false /* expect_waiting */,
                 true /* expect_active */));
  base::RunLoop().RunUntilIdle();
}

// Test registration when the service worker rejects the install event. The
// registration callback should indicate success, but there should be no waiting
// or active worker in the registration.
TEST_F(ServiceWorkerContextTest, Register_RejectInstall) {
  GURL pattern("http://www.example.com/");
  GURL script_url("http://www.example.com/service_worker.js");

  helper_.reset();  // Make sure the process lookups stay overridden.
  helper_.reset(new RejectInstallTestHelper);
  helper_->context_wrapper()->AddObserver(this);
  int64_t registration_id = kInvalidServiceWorkerRegistrationId;
  bool called = false;
  context()->RegisterServiceWorker(
      pattern,
      script_url,
      NULL,
      MakeRegisteredCallback(&called, &registration_id));

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

  ASSERT_EQ(1u, notifications_.size());
  EXPECT_EQ(REGISTRATION_STORED, notifications_[0].type);
  EXPECT_EQ(pattern, notifications_[0].pattern);
  EXPECT_EQ(registration_id, notifications_[0].registration_id);

  context()->storage()->FindRegistrationForId(
      registration_id,
      pattern.GetOrigin(),
      base::Bind(&ExpectRegisteredWorkers,
                 SERVICE_WORKER_ERROR_NOT_FOUND,
                 false /* expect_waiting */,
                 false /* expect_active */));
  base::RunLoop().RunUntilIdle();
}

// Test registration when the service worker rejects the activate event. The
// worker should be activated anyway.
TEST_F(ServiceWorkerContextTest, Register_RejectActivate) {
  GURL pattern("http://www.example.com/");
  GURL script_url("http://www.example.com/service_worker.js");

  helper_.reset();
  helper_.reset(new RejectActivateTestHelper);
  helper_->context_wrapper()->AddObserver(this);
  int64_t registration_id = kInvalidServiceWorkerRegistrationId;
  bool called = false;
  context()->RegisterServiceWorker(
      pattern, script_url, NULL,
      MakeRegisteredCallback(&called, &registration_id));

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

  ASSERT_EQ(1u, notifications_.size());
  EXPECT_EQ(REGISTRATION_STORED, notifications_[0].type);
  EXPECT_EQ(pattern, notifications_[0].pattern);
  EXPECT_EQ(registration_id, notifications_[0].registration_id);

  context()->storage()->FindRegistrationForId(
      registration_id, pattern.GetOrigin(),
      base::Bind(&ExpectRegisteredWorkers, SERVICE_WORKER_OK,
                 false /* expect_waiting */, true /* expect_active */));
  base::RunLoop().RunUntilIdle();
}

// Make sure registrations are cleaned up when they are unregistered.
TEST_F(ServiceWorkerContextTest, Unregister) {
  GURL pattern("http://www.example.com/");

  bool called = false;
  int64_t registration_id = kInvalidServiceWorkerRegistrationId;
  context()->RegisterServiceWorker(
      pattern,
      GURL("http://www.example.com/service_worker.js"),
      NULL,
      MakeRegisteredCallback(&called, &registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);
  EXPECT_NE(kInvalidServiceWorkerRegistrationId, registration_id);

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
                 false /* expect_waiting */,
                 false /* expect_active */));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2u, notifications_.size());
  EXPECT_EQ(REGISTRATION_STORED, notifications_[0].type);
  EXPECT_EQ(pattern, notifications_[0].pattern);
  EXPECT_EQ(registration_id, notifications_[0].registration_id);
  EXPECT_EQ(REGISTRATION_DELETED, notifications_[1].type);
  EXPECT_EQ(pattern, notifications_[1].pattern);
  EXPECT_EQ(registration_id, notifications_[1].registration_id);
}

// Make sure registrations are cleaned up when they are unregistered in bulk.
TEST_F(ServiceWorkerContextTest, UnregisterMultiple) {
  GURL origin1_p1("http://www.example.com/test");
  GURL origin1_p2("http://www.example.com/hello");
  GURL origin2_p1("http://www.example.com:8080/again");
  GURL origin3_p1("http://www.other.com/");

  bool called = false;
  int64_t registration_id1 = kInvalidServiceWorkerRegistrationId;
  int64_t registration_id2 = kInvalidServiceWorkerRegistrationId;
  int64_t registration_id3 = kInvalidServiceWorkerRegistrationId;
  int64_t registration_id4 = kInvalidServiceWorkerRegistrationId;
  context()->RegisterServiceWorker(
      origin1_p1,
      GURL("http://www.example.com/service_worker.js"),
      NULL,
      MakeRegisteredCallback(&called, &registration_id1));
  context()->RegisterServiceWorker(
      origin1_p2,
      GURL("http://www.example.com/service_worker2.js"),
      NULL,
      MakeRegisteredCallback(&called, &registration_id2));
  context()->RegisterServiceWorker(
      origin2_p1,
      GURL("http://www.example.com:8080/service_worker3.js"),
      NULL,
      MakeRegisteredCallback(&called, &registration_id3));
  context()->RegisterServiceWorker(
      origin3_p1,
      GURL("http://www.other.com/service_worker4.js"),
      NULL,
      MakeRegisteredCallback(&called, &registration_id4));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  EXPECT_NE(kInvalidServiceWorkerRegistrationId, registration_id1);
  EXPECT_NE(kInvalidServiceWorkerRegistrationId, registration_id2);
  EXPECT_NE(kInvalidServiceWorkerRegistrationId, registration_id3);
  EXPECT_NE(kInvalidServiceWorkerRegistrationId, registration_id4);

  called = false;
  context()->UnregisterServiceWorkers(origin1_p1.GetOrigin(),
                                      MakeUnregisteredCallback(&called));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  context()->storage()->FindRegistrationForId(
      registration_id1,
      origin1_p1.GetOrigin(),
      base::Bind(&ExpectRegisteredWorkers,
                 SERVICE_WORKER_ERROR_NOT_FOUND,
                 false /* expect_waiting */,
                 false /* expect_active */));
  context()->storage()->FindRegistrationForId(
      registration_id2,
      origin1_p2.GetOrigin(),
      base::Bind(&ExpectRegisteredWorkers,
                 SERVICE_WORKER_ERROR_NOT_FOUND,
                 false /* expect_waiting */,
                 false /* expect_active */));
  context()->storage()->FindRegistrationForId(
      registration_id3,
      origin2_p1.GetOrigin(),
      base::Bind(&ExpectRegisteredWorkers,
                 SERVICE_WORKER_OK,
                 false /* expect_waiting */,
                 true /* expect_active */));

  context()->storage()->FindRegistrationForId(
      registration_id4,
      origin3_p1.GetOrigin(),
      base::Bind(&ExpectRegisteredWorkers,
                 SERVICE_WORKER_OK,
                 false /* expect_waiting */,
                 true /* expect_active */));

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(6u, notifications_.size());
  EXPECT_EQ(REGISTRATION_STORED, notifications_[0].type);
  EXPECT_EQ(registration_id1, notifications_[0].registration_id);
  EXPECT_EQ(origin1_p1, notifications_[0].pattern);
  EXPECT_EQ(REGISTRATION_STORED, notifications_[1].type);
  EXPECT_EQ(origin1_p2, notifications_[1].pattern);
  EXPECT_EQ(registration_id2, notifications_[1].registration_id);
  EXPECT_EQ(REGISTRATION_STORED, notifications_[2].type);
  EXPECT_EQ(origin2_p1, notifications_[2].pattern);
  EXPECT_EQ(registration_id3, notifications_[2].registration_id);
  EXPECT_EQ(REGISTRATION_STORED, notifications_[3].type);
  EXPECT_EQ(origin3_p1, notifications_[3].pattern);
  EXPECT_EQ(registration_id4, notifications_[3].registration_id);
  EXPECT_EQ(REGISTRATION_DELETED, notifications_[4].type);
  EXPECT_EQ(origin1_p2, notifications_[4].pattern);
  EXPECT_EQ(registration_id2, notifications_[4].registration_id);
  EXPECT_EQ(REGISTRATION_DELETED, notifications_[5].type);
  EXPECT_EQ(origin1_p1, notifications_[5].pattern);
  EXPECT_EQ(registration_id1, notifications_[5].registration_id);
}

// Make sure registering a new script shares an existing registration.
TEST_F(ServiceWorkerContextTest, RegisterNewScript) {
  GURL pattern("http://www.example.com/");

  bool called = false;
  int64_t old_registration_id = kInvalidServiceWorkerRegistrationId;
  context()->RegisterServiceWorker(
      pattern,
      GURL("http://www.example.com/service_worker.js"),
      NULL,
      MakeRegisteredCallback(&called, &old_registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);
  EXPECT_NE(kInvalidServiceWorkerRegistrationId, old_registration_id);

  called = false;
  int64_t new_registration_id = kInvalidServiceWorkerRegistrationId;
  context()->RegisterServiceWorker(
      pattern,
      GURL("http://www.example.com/service_worker_new.js"),
      NULL,
      MakeRegisteredCallback(&called, &new_registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  EXPECT_NE(kInvalidServiceWorkerRegistrationId, new_registration_id);
  EXPECT_EQ(old_registration_id, new_registration_id);

  ASSERT_EQ(2u, notifications_.size());
  EXPECT_EQ(REGISTRATION_STORED, notifications_[0].type);
  EXPECT_EQ(pattern, notifications_[0].pattern);
  EXPECT_EQ(old_registration_id, notifications_[0].registration_id);
  EXPECT_EQ(REGISTRATION_STORED, notifications_[1].type);
  EXPECT_EQ(pattern, notifications_[1].pattern);
  EXPECT_EQ(new_registration_id, notifications_[1].registration_id);
}

// Make sure that when registering a duplicate pattern+script_url
// combination, that the same registration is used.
TEST_F(ServiceWorkerContextTest, RegisterDuplicateScript) {
  GURL pattern("http://www.example.com/");
  GURL script_url("http://www.example.com/service_worker.js");

  bool called = false;
  int64_t old_registration_id = kInvalidServiceWorkerRegistrationId;
  context()->RegisterServiceWorker(
      pattern,
      script_url,
      NULL,
      MakeRegisteredCallback(&called, &old_registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);
  EXPECT_NE(kInvalidServiceWorkerRegistrationId, old_registration_id);

  called = false;
  int64_t new_registration_id = kInvalidServiceWorkerRegistrationId;
  context()->RegisterServiceWorker(
      pattern,
      script_url,
      NULL,
      MakeRegisteredCallback(&called, &new_registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);
  EXPECT_EQ(old_registration_id, new_registration_id);

  ASSERT_EQ(2u, notifications_.size());
  EXPECT_EQ(REGISTRATION_STORED, notifications_[0].type);
  EXPECT_EQ(pattern, notifications_[0].pattern);
  EXPECT_EQ(old_registration_id, notifications_[0].registration_id);
  EXPECT_EQ(REGISTRATION_STORED, notifications_[1].type);
  EXPECT_EQ(pattern, notifications_[1].pattern);
  EXPECT_EQ(old_registration_id, notifications_[1].registration_id);
}

TEST_F(ServiceWorkerContextTest, ProviderHostIterator) {
  const int kRenderProcessId1 = 1;
  const int kRenderProcessId2 = 2;
  const GURL kOrigin1 = GURL("http://www.example.com/");
  const GURL kOrigin2 = GURL("https://www.example.com/");
  int provider_id = 1;

  // Host1 (provider_id=1): process_id=1, origin1.
  ServiceWorkerProviderHost* host1(new ServiceWorkerProviderHost(
      kRenderProcessId1, MSG_ROUTING_NONE, provider_id++,
      SERVICE_WORKER_PROVIDER_FOR_WINDOW,
      ServiceWorkerProviderHost::FrameSecurityLevel::SECURE,
      context()->AsWeakPtr(), nullptr));
  host1->SetDocumentUrl(kOrigin1);

  // Host2 (provider_id=2): process_id=2, origin2.
  ServiceWorkerProviderHost* host2(new ServiceWorkerProviderHost(
      kRenderProcessId2, MSG_ROUTING_NONE, provider_id++,
      SERVICE_WORKER_PROVIDER_FOR_WINDOW,
      ServiceWorkerProviderHost::FrameSecurityLevel::SECURE,
      context()->AsWeakPtr(), nullptr));
  host2->SetDocumentUrl(kOrigin2);

  // Host3 (provider_id=3): process_id=2, origin1.
  ServiceWorkerProviderHost* host3(new ServiceWorkerProviderHost(
      kRenderProcessId2, MSG_ROUTING_NONE, provider_id++,
      SERVICE_WORKER_PROVIDER_FOR_WINDOW,
      ServiceWorkerProviderHost::FrameSecurityLevel::SECURE,
      context()->AsWeakPtr(), nullptr));
  host3->SetDocumentUrl(kOrigin1);

  // Host4 (provider_id=4): process_id=2, origin2, for ServiceWorker.
  ServiceWorkerProviderHost* host4(new ServiceWorkerProviderHost(
      kRenderProcessId2, MSG_ROUTING_NONE, provider_id++,
      SERVICE_WORKER_PROVIDER_FOR_CONTROLLER,
      ServiceWorkerProviderHost::FrameSecurityLevel::SECURE,
      context()->AsWeakPtr(), nullptr));
  host4->SetDocumentUrl(kOrigin2);

  context()->AddProviderHost(base::WrapUnique(host1));
  context()->AddProviderHost(base::WrapUnique(host2));
  context()->AddProviderHost(base::WrapUnique(host3));
  context()->AddProviderHost(base::WrapUnique(host4));

  // Iterate over all provider hosts.
  std::set<ServiceWorkerProviderHost*> results;
  for (auto it = context()->GetProviderHostIterator(); !it->IsAtEnd();
       it->Advance()) {
    results.insert(it->GetProviderHost());
  }
  EXPECT_EQ(4u, results.size());
  EXPECT_TRUE(ContainsKey(results, host1));
  EXPECT_TRUE(ContainsKey(results, host2));
  EXPECT_TRUE(ContainsKey(results, host3));
  EXPECT_TRUE(ContainsKey(results, host4));

  // Iterate over the client provider hosts that belong to kOrigin1.
  results.clear();
  for (auto it = context()->GetClientProviderHostIterator(kOrigin1);
       !it->IsAtEnd(); it->Advance()) {
    results.insert(it->GetProviderHost());
  }
  EXPECT_EQ(2u, results.size());
  EXPECT_TRUE(ContainsKey(results, host1));
  EXPECT_TRUE(ContainsKey(results, host3));

  // Iterate over the provider hosts that belong to kOrigin2.
  // (This should not include host4 as it's not for controllee.)
  results.clear();
  for (auto it = context()->GetClientProviderHostIterator(kOrigin2);
       !it->IsAtEnd(); it->Advance()) {
    results.insert(it->GetProviderHost());
  }
  EXPECT_EQ(1u, results.size());
  EXPECT_TRUE(ContainsKey(results, host2));

  context()->RemoveProviderHost(kRenderProcessId1, 1);
  context()->RemoveProviderHost(kRenderProcessId2, 2);
  context()->RemoveProviderHost(kRenderProcessId2, 3);
  context()->RemoveProviderHost(kRenderProcessId2, 4);
}

class ServiceWorkerContextRecoveryTest
    : public ServiceWorkerContextTest,
      public testing::WithParamInterface<bool> {
 public:
  ServiceWorkerContextRecoveryTest() {}
  virtual ~ServiceWorkerContextRecoveryTest() {}
};

INSTANTIATE_TEST_CASE_P(ServiceWorkerContextRecoveryTest,
                        ServiceWorkerContextRecoveryTest,
                        testing::Values(true, false));

TEST_P(ServiceWorkerContextRecoveryTest, DeleteAndStartOver) {
  GURL pattern("http://www.example.com/");
  GURL script_url("http://www.example.com/service_worker.js");

  bool is_storage_on_disk = GetParam();
  if (is_storage_on_disk) {
    // Reinitialize the helper to test on-disk storage.
    base::ScopedTempDir user_data_directory;
    ASSERT_TRUE(user_data_directory.CreateUniqueTempDir());
    helper_.reset(new EmbeddedWorkerTestHelper(user_data_directory.path()));
    helper_->context_wrapper()->AddObserver(this);
  }

  int64_t registration_id = kInvalidServiceWorkerRegistrationId;
  bool called = false;
  context()->RegisterServiceWorker(
      pattern,
      script_url,
      NULL,
      MakeRegisteredCallback(&called, &registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  context()->storage()->FindRegistrationForId(
      registration_id,
      pattern.GetOrigin(),
      base::Bind(&ExpectRegisteredWorkers,
                 SERVICE_WORKER_OK,
                 false /* expect_waiting */,
                 true /* expect_active */));
  base::RunLoop().RunUntilIdle();

  // Next handle ids should be 0 (the next call should return 1).
  EXPECT_EQ(0, context()->GetNewServiceWorkerHandleId());
  EXPECT_EQ(0, context()->GetNewRegistrationHandleId());

  context()->ScheduleDeleteAndStartOver();

  // The storage is disabled while the recovery process is running, so the
  // operation should be aborted.
  context()->storage()->FindRegistrationForId(
      registration_id, pattern.GetOrigin(),
      base::Bind(&ExpectRegisteredWorkers, SERVICE_WORKER_ERROR_ABORT,
                 false /* expect_waiting */, true /* expect_active */));
  base::RunLoop().RunUntilIdle();

  // The context started over and the storage was re-initialized, so the
  // registration should not be found.
  context()->storage()->FindRegistrationForId(
      registration_id,
      pattern.GetOrigin(),
      base::Bind(&ExpectRegisteredWorkers,
                 SERVICE_WORKER_ERROR_NOT_FOUND,
                 false /* expect_waiting */,
                 true /* expect_active */));
  base::RunLoop().RunUntilIdle();

  called = false;
  context()->RegisterServiceWorker(
      pattern,
      script_url,
      NULL,
      MakeRegisteredCallback(&called, &registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  context()->storage()->FindRegistrationForId(
      registration_id,
      pattern.GetOrigin(),
      base::Bind(&ExpectRegisteredWorkers,
                 SERVICE_WORKER_OK,
                 false /* expect_waiting */,
                 true /* expect_active */));
  base::RunLoop().RunUntilIdle();

  // The new context should take over next handle ids.
  EXPECT_EQ(1, context()->GetNewServiceWorkerHandleId());
  EXPECT_EQ(1, context()->GetNewRegistrationHandleId());

  ASSERT_EQ(3u, notifications_.size());
  EXPECT_EQ(REGISTRATION_STORED, notifications_[0].type);
  EXPECT_EQ(pattern, notifications_[0].pattern);
  EXPECT_EQ(registration_id, notifications_[0].registration_id);
  EXPECT_EQ(STORAGE_RECOVERED, notifications_[1].type);
  EXPECT_EQ(REGISTRATION_STORED, notifications_[2].type);
  EXPECT_EQ(pattern, notifications_[2].pattern);
  EXPECT_EQ(registration_id, notifications_[2].registration_id);
}


}  // namespace content
