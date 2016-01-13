// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"

// Unit tests for testing all job registration tasks.
namespace content {

namespace {

void SaveRegistrationCallback(
    ServiceWorkerStatusCode expected_status,
    bool* called,
    scoped_refptr<ServiceWorkerRegistration>* registration_out,
    ServiceWorkerStatusCode status,
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version) {
  ASSERT_TRUE(!version || version->registration_id() == registration->id())
      << version << " " << registration;
  EXPECT_EQ(expected_status, status);
  *called = true;
  *registration_out = registration;
}

void SaveFoundRegistrationCallback(
    ServiceWorkerStatusCode expected_status,
    bool* called,
    scoped_refptr<ServiceWorkerRegistration>* registration,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& result) {
  EXPECT_EQ(expected_status, status);
  *called = true;
  *registration = result;
}

// Creates a callback which both keeps track of if it's been called,
// as well as the resulting registration. Whent the callback is fired,
// it ensures that the resulting status matches the expectation.
// 'called' is useful for making sure a sychronous callback is or
// isn't called.
ServiceWorkerRegisterJob::RegistrationCallback SaveRegistration(
    ServiceWorkerStatusCode expected_status,
    bool* called,
    scoped_refptr<ServiceWorkerRegistration>* registration) {
  *called = false;
  return base::Bind(
      &SaveRegistrationCallback, expected_status, called, registration);
}

ServiceWorkerStorage::FindRegistrationCallback SaveFoundRegistration(
    ServiceWorkerStatusCode expected_status,
    bool* called,
    scoped_refptr<ServiceWorkerRegistration>* registration) {
  *called = false;
  return base::Bind(&SaveFoundRegistrationCallback,
                    expected_status,
                    called,
                    registration);
}

void SaveUnregistrationCallback(ServiceWorkerStatusCode expected_status,
                                bool* called,
                                ServiceWorkerStatusCode status) {
  EXPECT_EQ(expected_status, status);
  *called = true;
}

ServiceWorkerUnregisterJob::UnregistrationCallback SaveUnregistration(
    ServiceWorkerStatusCode expected_status,
    bool* called) {
  *called = false;
  return base::Bind(&SaveUnregistrationCallback, expected_status, called);
}

}  // namespace

class ServiceWorkerJobTest : public testing::Test {
 public:
  ServiceWorkerJobTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        render_process_id_(88) {}

  virtual void SetUp() OVERRIDE {
    helper_.reset(new EmbeddedWorkerTestHelper(render_process_id_));
  }

  virtual void TearDown() OVERRIDE {
    helper_.reset();
  }

  ServiceWorkerContextCore* context() const { return helper_->context(); }

  ServiceWorkerJobCoordinator* job_coordinator() const {
    return context()->job_coordinator();
  }
  ServiceWorkerStorage* storage() const { return context()->storage(); }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;

  int render_process_id_;
};

TEST_F(ServiceWorkerJobTest, SameDocumentSameRegistration) {
  scoped_refptr<ServiceWorkerRegistration> original_registration;
  bool called;
  job_coordinator()->Register(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called, &original_registration));
  EXPECT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  scoped_refptr<ServiceWorkerRegistration> registration1;
  storage()->FindRegistrationForDocument(
      GURL("http://www.example.com/"),
      SaveFoundRegistration(SERVICE_WORKER_OK, &called, &registration1));
  scoped_refptr<ServiceWorkerRegistration> registration2;
  storage()->FindRegistrationForDocument(
      GURL("http://www.example.com/"),
      SaveFoundRegistration(SERVICE_WORKER_OK, &called, &registration2));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  ASSERT_TRUE(registration1);
  ASSERT_EQ(registration1, original_registration);
  ASSERT_EQ(registration1, registration2);
}

TEST_F(ServiceWorkerJobTest, SameMatchSameRegistration) {
  bool called;
  scoped_refptr<ServiceWorkerRegistration> original_registration;
  job_coordinator()->Register(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called, &original_registration));
  EXPECT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  ASSERT_NE(static_cast<ServiceWorkerRegistration*>(NULL),
            original_registration.get());

  scoped_refptr<ServiceWorkerRegistration> registration1;
  storage()->FindRegistrationForDocument(
      GURL("http://www.example.com/one"),
      SaveFoundRegistration(SERVICE_WORKER_OK, &called, &registration1));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  scoped_refptr<ServiceWorkerRegistration> registration2;
  storage()->FindRegistrationForDocument(
      GURL("http://www.example.com/two"),
      SaveFoundRegistration(SERVICE_WORKER_OK, &called, &registration2));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  ASSERT_EQ(registration1, original_registration);
  ASSERT_EQ(registration1, registration2);
}

TEST_F(ServiceWorkerJobTest, DifferentMatchDifferentRegistration) {
  bool called1;
  scoped_refptr<ServiceWorkerRegistration> original_registration1;
  job_coordinator()->Register(
      GURL("http://www.example.com/one/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called1, &original_registration1));

  bool called2;
  scoped_refptr<ServiceWorkerRegistration> original_registration2;
  job_coordinator()->Register(
      GURL("http://www.example.com/two/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called2, &original_registration2));

  EXPECT_FALSE(called1);
  EXPECT_FALSE(called2);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called2);
  EXPECT_TRUE(called1);

  scoped_refptr<ServiceWorkerRegistration> registration1;
  storage()->FindRegistrationForDocument(
      GURL("http://www.example.com/one/"),
      SaveFoundRegistration(SERVICE_WORKER_OK, &called1, &registration1));
  scoped_refptr<ServiceWorkerRegistration> registration2;
  storage()->FindRegistrationForDocument(
      GURL("http://www.example.com/two/"),
      SaveFoundRegistration(SERVICE_WORKER_OK, &called2, &registration2));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called2);
  EXPECT_TRUE(called1);
  ASSERT_NE(registration1, registration2);
}

// Make sure basic registration is working.
TEST_F(ServiceWorkerJobTest, Register) {
  bool called = false;
  scoped_refptr<ServiceWorkerRegistration> registration;
  job_coordinator()->Register(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called, &registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_NE(scoped_refptr<ServiceWorkerRegistration>(NULL), registration);
}

// Make sure registrations are cleaned up when they are unregistered.
TEST_F(ServiceWorkerJobTest, Unregister) {
  GURL pattern("http://www.example.com/*");

  bool called;
  scoped_refptr<ServiceWorkerRegistration> registration;
  job_coordinator()->Register(
      pattern,
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called, &registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  job_coordinator()->Unregister(pattern,
                                SaveUnregistration(SERVICE_WORKER_OK, &called));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_TRUE(registration->HasOneRef());

  storage()->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(SERVICE_WORKER_ERROR_NOT_FOUND,
                            &called, &registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_EQ(scoped_refptr<ServiceWorkerRegistration>(NULL), registration);
}

TEST_F(ServiceWorkerJobTest, Unregister_NothingRegistered) {
  GURL pattern("http://www.example.com/*");

  bool called;
  job_coordinator()->Unregister(pattern,
                                SaveUnregistration(SERVICE_WORKER_OK, &called));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);
}

// Make sure that when a new registration replaces an existing
// registration, that the old one is cleaned up.
TEST_F(ServiceWorkerJobTest, RegisterNewScript) {
  GURL pattern("http://www.example.com/*");

  bool called;
  scoped_refptr<ServiceWorkerRegistration> old_registration;
  job_coordinator()->Register(
      pattern,
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called, &old_registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  scoped_refptr<ServiceWorkerRegistration> old_registration_by_pattern;
  storage()->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          SERVICE_WORKER_OK, &called, &old_registration_by_pattern));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_EQ(old_registration, old_registration_by_pattern);
  old_registration_by_pattern = NULL;

  scoped_refptr<ServiceWorkerRegistration> new_registration;
  job_coordinator()->Register(
      pattern,
      GURL("http://www.example.com/service_worker_new.js"),
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called, &new_registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_TRUE(old_registration->HasOneRef());

  ASSERT_NE(old_registration, new_registration);

  scoped_refptr<ServiceWorkerRegistration> new_registration_by_pattern;
  storage()->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          SERVICE_WORKER_OK, &called, &new_registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_NE(new_registration_by_pattern, old_registration);
}

// Make sure that when registering a duplicate pattern+script_url
// combination, that the same registration is used.
TEST_F(ServiceWorkerJobTest, RegisterDuplicateScript) {
  GURL pattern("http://www.example.com/*");
  GURL script_url("http://www.example.com/service_worker.js");

  bool called;
  scoped_refptr<ServiceWorkerRegistration> old_registration;
  job_coordinator()->Register(
      pattern,
      script_url,
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called, &old_registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  scoped_refptr<ServiceWorkerRegistration> old_registration_by_pattern;
  storage()->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          SERVICE_WORKER_OK, &called, &old_registration_by_pattern));
  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_TRUE(old_registration_by_pattern);

  scoped_refptr<ServiceWorkerRegistration> new_registration;
  job_coordinator()->Register(
      pattern,
      script_url,
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called, &new_registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_EQ(old_registration, new_registration);

  ASSERT_FALSE(old_registration->HasOneRef());

  scoped_refptr<ServiceWorkerRegistration> new_registration_by_pattern;
  storage()->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          SERVICE_WORKER_OK, &called, &new_registration_by_pattern));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_EQ(new_registration, old_registration);
}

class FailToStartWorkerTestHelper : public EmbeddedWorkerTestHelper {
 public:
  FailToStartWorkerTestHelper(int mock_render_process_id)
      : EmbeddedWorkerTestHelper(mock_render_process_id) {}

  virtual void OnStartWorker(int embedded_worker_id,
                             int64 service_worker_version_id,
                             const GURL& scope,
                             const GURL& script_url) OVERRIDE {
    // Simulate failure by sending worker stopped instead of started.
    EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
    registry()->OnWorkerStopped(worker->process_id(), embedded_worker_id);
  }
};

TEST_F(ServiceWorkerJobTest, Register_FailToStartWorker) {
  helper_.reset(new FailToStartWorkerTestHelper(render_process_id_));

  bool called = false;
  scoped_refptr<ServiceWorkerRegistration> registration;
  job_coordinator()->Register(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      SaveRegistration(
          SERVICE_WORKER_ERROR_START_WORKER_FAILED, &called, &registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(called);
  ASSERT_EQ(scoped_refptr<ServiceWorkerRegistration>(NULL), registration);
}

// Register and then unregister the pattern, in parallel. Job coordinator should
// process jobs until the last job.
TEST_F(ServiceWorkerJobTest, ParallelRegUnreg) {
  GURL pattern("http://www.example.com/*");
  GURL script_url("http://www.example.com/service_worker.js");

  bool registration_called = false;
  scoped_refptr<ServiceWorkerRegistration> registration;
  job_coordinator()->Register(
      pattern,
      script_url,
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &registration_called, &registration));

  bool unregistration_called = false;
  job_coordinator()->Unregister(
      pattern,
      SaveUnregistration(SERVICE_WORKER_OK, &unregistration_called));

  ASSERT_FALSE(registration_called);
  ASSERT_FALSE(unregistration_called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(registration_called);
  ASSERT_TRUE(unregistration_called);

  bool find_called = false;
  storage()->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          SERVICE_WORKER_ERROR_NOT_FOUND, &find_called, &registration));

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(scoped_refptr<ServiceWorkerRegistration>(), registration);
}

// Register conflicting scripts for the same pattern. The most recent
// registration should win, and the old registration should have been
// shutdown.
TEST_F(ServiceWorkerJobTest, ParallelRegNewScript) {
  GURL pattern("http://www.example.com/*");

  GURL script_url1("http://www.example.com/service_worker1.js");
  bool registration1_called = false;
  scoped_refptr<ServiceWorkerRegistration> registration1;
  job_coordinator()->Register(
      pattern,
      script_url1,
      render_process_id_,
      SaveRegistration(
          SERVICE_WORKER_OK, &registration1_called, &registration1));

  GURL script_url2("http://www.example.com/service_worker2.js");
  bool registration2_called = false;
  scoped_refptr<ServiceWorkerRegistration> registration2;
  job_coordinator()->Register(
      pattern,
      script_url2,
      render_process_id_,
      SaveRegistration(
          SERVICE_WORKER_OK, &registration2_called, &registration2));

  ASSERT_FALSE(registration1_called);
  ASSERT_FALSE(registration2_called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(registration1_called);
  ASSERT_TRUE(registration2_called);

  scoped_refptr<ServiceWorkerRegistration> registration;
  bool find_called = false;
  storage()->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          SERVICE_WORKER_OK, &find_called, &registration));

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(registration2, registration);
}

// Register the exact same pattern + script. Requests should be
// coalesced such that both callers get the exact same registration
// object.
TEST_F(ServiceWorkerJobTest, ParallelRegSameScript) {
  GURL pattern("http://www.example.com/*");

  GURL script_url("http://www.example.com/service_worker1.js");
  bool registration1_called = false;
  scoped_refptr<ServiceWorkerRegistration> registration1;
  job_coordinator()->Register(
      pattern,
      script_url,
      render_process_id_,
      SaveRegistration(
          SERVICE_WORKER_OK, &registration1_called, &registration1));

  bool registration2_called = false;
  scoped_refptr<ServiceWorkerRegistration> registration2;
  job_coordinator()->Register(
      pattern,
      script_url,
      render_process_id_,
      SaveRegistration(
          SERVICE_WORKER_OK, &registration2_called, &registration2));

  ASSERT_FALSE(registration1_called);
  ASSERT_FALSE(registration2_called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(registration1_called);
  ASSERT_TRUE(registration2_called);

  ASSERT_EQ(registration1, registration2);

  scoped_refptr<ServiceWorkerRegistration> registration;
  bool find_called = false;
  storage()->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          SERVICE_WORKER_OK, &find_called, &registration));

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(registration, registration1);
}

// Call simulataneous unregister calls.
TEST_F(ServiceWorkerJobTest, ParallelUnreg) {
  GURL pattern("http://www.example.com/*");

  GURL script_url("http://www.example.com/service_worker.js");
  bool unregistration1_called = false;
  job_coordinator()->Unregister(
      pattern,
      SaveUnregistration(SERVICE_WORKER_OK, &unregistration1_called));

  bool unregistration2_called = false;
  job_coordinator()->Unregister(
      pattern, SaveUnregistration(SERVICE_WORKER_OK, &unregistration2_called));

  ASSERT_FALSE(unregistration1_called);
  ASSERT_FALSE(unregistration2_called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(unregistration1_called);
  ASSERT_TRUE(unregistration2_called);

  // There isn't really a way to test that they are being coalesced,
  // but we can make sure they can exist simultaneously without
  // crashing.
  scoped_refptr<ServiceWorkerRegistration> registration;
  bool find_called = false;
  storage()->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          SERVICE_WORKER_ERROR_NOT_FOUND, &find_called, &registration));

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(scoped_refptr<ServiceWorkerRegistration>(), registration);
}

TEST_F(ServiceWorkerJobTest, AbortAll_Register) {
  GURL pattern1("http://www1.example.com/*");
  GURL pattern2("http://www2.example.com/*");
  GURL script_url1("http://www1.example.com/service_worker.js");
  GURL script_url2("http://www2.example.com/service_worker.js");

  bool registration_called1 = false;
  scoped_refptr<ServiceWorkerRegistration> registration1;
  job_coordinator()->Register(
      pattern1,
      script_url1,
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_ERROR_ABORT,
                       &registration_called1, &registration1));

  bool registration_called2 = false;
  scoped_refptr<ServiceWorkerRegistration> registration2;
  job_coordinator()->Register(
      pattern2,
      script_url2,
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_ERROR_ABORT,
                       &registration_called2, &registration2));

  ASSERT_FALSE(registration_called1);
  ASSERT_FALSE(registration_called2);
  job_coordinator()->AbortAll();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(registration_called1);
  ASSERT_TRUE(registration_called2);

  bool find_called1 = false;
  storage()->FindRegistrationForPattern(
      pattern1,
      SaveFoundRegistration(
          SERVICE_WORKER_ERROR_NOT_FOUND, &find_called1, &registration1));

  bool find_called2 = false;
  storage()->FindRegistrationForPattern(
      pattern2,
      SaveFoundRegistration(
          SERVICE_WORKER_ERROR_NOT_FOUND, &find_called2, &registration2));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(find_called1);
  ASSERT_TRUE(find_called2);
  EXPECT_EQ(scoped_refptr<ServiceWorkerRegistration>(), registration1);
  EXPECT_EQ(scoped_refptr<ServiceWorkerRegistration>(), registration2);
}

TEST_F(ServiceWorkerJobTest, AbortAll_Unregister) {
  GURL pattern1("http://www1.example.com/*");
  GURL pattern2("http://www2.example.com/*");

  bool unregistration_called1 = false;
  scoped_refptr<ServiceWorkerRegistration> registration1;
  job_coordinator()->Unregister(
      pattern1,
      SaveUnregistration(SERVICE_WORKER_ERROR_ABORT,
                         &unregistration_called1));

  bool unregistration_called2 = false;
  job_coordinator()->Unregister(
      pattern2,
      SaveUnregistration(SERVICE_WORKER_ERROR_ABORT,
                         &unregistration_called2));

  ASSERT_FALSE(unregistration_called1);
  ASSERT_FALSE(unregistration_called2);
  job_coordinator()->AbortAll();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(unregistration_called1);
  ASSERT_TRUE(unregistration_called2);
}

TEST_F(ServiceWorkerJobTest, AbortAll_RegUnreg) {
  GURL pattern("http://www.example.com/*");
  GURL script_url("http://www.example.com/service_worker.js");

  bool registration_called = false;
  scoped_refptr<ServiceWorkerRegistration> registration;
  job_coordinator()->Register(
      pattern,
      script_url,
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_ERROR_ABORT,
                       &registration_called, &registration));

  bool unregistration_called = false;
  job_coordinator()->Unregister(
      pattern,
      SaveUnregistration(SERVICE_WORKER_ERROR_ABORT,
                         &unregistration_called));

  ASSERT_FALSE(registration_called);
  ASSERT_FALSE(unregistration_called);
  job_coordinator()->AbortAll();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(registration_called);
  ASSERT_TRUE(unregistration_called);

  bool find_called = false;
  storage()->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          SERVICE_WORKER_ERROR_NOT_FOUND, &find_called, &registration));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(find_called);
  EXPECT_EQ(scoped_refptr<ServiceWorkerRegistration>(), registration);
}

}  // namespace content
