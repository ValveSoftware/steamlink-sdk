// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/blimp_metrics_service_client.h"

#include <list>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/stability_metrics_helper.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace engine {

namespace {
// Shows notifications which correspond to PersistentPrefStore's reading errors.
void HandleReadError(PersistentPrefStore::PrefReadError error) {
}
}  // namespace

class BlimpMetricsServiceClientTest : public testing::Test {
 public:
  BlimpMetricsServiceClientTest() {}

 protected:
  void SetUp() override {
    // Set up NotificationService for StabilityMetricsHelper to use.
    notification_service_.reset(content::NotificationService::Create());

    // Set up PrefRegistry.
    pref_registry_ = new user_prefs::PrefRegistrySyncable();
    metrics::MetricsService::RegisterPrefs(pref_registry_.get());
    metrics::StabilityMetricsHelper::RegisterPrefs(pref_registry_.get());

    // Set up PrefService.
    PrefServiceFactory pref_service_factory;
    pref_service_factory.set_user_prefs(new InMemoryPrefStore());
    pref_service_factory.set_read_error_callback(base::Bind(&HandleReadError));
    pref_service_ = pref_service_factory.Create(pref_registry_.get());
    ASSERT_NE(nullptr, pref_service_.get());

    // Set up RequestContextGetter.
    request_context_getter_ = new net::TestURLRequestContextGetter(
        base::ThreadTaskRunnerHandle::Get());

    // Set task runner global for MetricsService.
    base::SetRecordActionTaskRunner(base::ThreadTaskRunnerHandle::Get());

    // Set up BlimpMetricsServiceClient
    client_.reset(new BlimpMetricsServiceClient(pref_service_.get(),
                                              request_context_getter_));
    base::RunLoop().RunUntilIdle();

    // Checks of InitializeBlimpMetrics.
    // TODO(jessicag): Check request_context_getter_ set.
    // PrefService unique pointer set up by BlimpMetricsServiceClient.
    EXPECT_EQ(PrefService::INITIALIZATION_STATUS_SUCCESS,
              pref_service_->GetInitializationStatus());

    // TODO(jessicag): Confirm registration of metrics providers.
    // TODO(jessicag): MetricsService initializes recording state.
    // TODO(jessicag): Confirm MetricsService::Start() is called.
  }

  void TearDown() override {
    // Trigger BlimpMetricsServiceClientTest destructor.
    client_.reset();
    ASSERT_EQ(nullptr, client_.get());
    base::RunLoop().RunUntilIdle();

    // PrefService unaffected
    EXPECT_EQ(PrefService::INITIALIZATION_STATUS_SUCCESS,
              pref_service_->GetInitializationStatus());

    // Clean up static variable in MetricService to avoid impacting other tests.
    metrics::MetricsService::SetExecutionPhase(
        metrics::MetricsService::UNINITIALIZED_PHASE,
        pref_service_.get());
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<content::NotificationService> notification_service_;
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry_;
  std::unique_ptr<PrefService> pref_service_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  std::unique_ptr<BlimpMetricsServiceClient> client_;

  DISALLOW_COPY_AND_ASSIGN(BlimpMetricsServiceClientTest);
};

TEST_F(BlimpMetricsServiceClientTest, CtorDtor) {
  // Confirm construction and destruction work as expected.
}

}  // namespace engine
}  // namespace blimp
