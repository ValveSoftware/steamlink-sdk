// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_io_data.h"

#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/previews/core/previews_black_list.h"
#include "components/previews/core/previews_black_list_item.h"
#include "components/previews/core/previews_opt_out_store.h"
#include "components/previews/core/previews_ui_service.h"
#include "components/variations/variations_associated_data.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace previews {

namespace {

class TestPreviewsIOData : public PreviewsIOData {
 public:
  TestPreviewsIOData(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
      : PreviewsIOData(io_task_runner, ui_task_runner), initialized_(false) {}
  ~TestPreviewsIOData() override {}

  // Whether Initialize was called.
  bool initialized() { return initialized_; }

 private:
  // Set |initialized_| to true and use base class functionality.
  void InitializeOnIOThread(
      std::unique_ptr<PreviewsOptOutStore> previews_opt_out_store) override {
    initialized_ = true;
    PreviewsIOData::InitializeOnIOThread(std::move(previews_opt_out_store));
  }

  // Whether Initialize was called.
  bool initialized_;
};

void RunLoadCallback(
    LoadBlackListCallback callback,
    std::unique_ptr<BlackListItemMap> black_list_item_map,
    std::unique_ptr<PreviewsBlackListItem> host_indifferent_black_list_item) {
  callback.Run(std::move(black_list_item_map),
               std::move(host_indifferent_black_list_item));
}

class TestPreviewsOptOutStore : public PreviewsOptOutStore {
 public:
  TestPreviewsOptOutStore() {}
  ~TestPreviewsOptOutStore() override {}

 private:
  // PreviewsOptOutStore implementation:
  void AddPreviewNavigation(bool opt_out,
                            const std::string& host_name,
                            PreviewsType type,
                            base::Time now) override {}

  void LoadBlackList(LoadBlackListCallback callback) override {
    std::unique_ptr<BlackListItemMap> black_list_item_map(
        new BlackListItemMap());
    std::unique_ptr<PreviewsBlackListItem> host_indifferent_black_list_item =
        PreviewsBlackList::CreateHostIndifferentBlackListItem();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&RunLoadCallback, callback,
                              base::Passed(&black_list_item_map),
                              base::Passed(&host_indifferent_black_list_item)));
  }

  void ClearBlackList(base::Time begin_time, base::Time end_time) override {}
};

class PreviewsIODataTest : public testing::Test {
 public:
  PreviewsIODataTest() {}

  ~PreviewsIODataTest() override {}

  void set_io_data(std::unique_ptr<TestPreviewsIOData> io_data) {
    io_data_ = std::move(io_data);
  }

  TestPreviewsIOData* io_data() { return io_data_.get(); }

  void set_ui_service(std::unique_ptr<PreviewsUIService> ui_service) {
    ui_service_ = std::move(ui_service);
  }

 protected:
  // Run this test on a single thread.
  base::MessageLoopForIO loop_;

 private:
  std::unique_ptr<TestPreviewsIOData> io_data_;
  std::unique_ptr<PreviewsUIService> ui_service_;
};

}  // namespace

TEST_F(PreviewsIODataTest, TestInitialization) {
  set_io_data(base::MakeUnique<TestPreviewsIOData>(loop_.task_runner(),
                                                   loop_.task_runner()));
  set_ui_service(base::MakeUnique<PreviewsUIService>(
      io_data(), loop_.task_runner(), nullptr));
  base::RunLoop().RunUntilIdle();
  // After the outstanding posted tasks have run, |io_data_| should be fully
  // initialized.
  EXPECT_TRUE(io_data()->initialized());
}

TEST_F(PreviewsIODataTest, TestShouldAllowPreview) {
  // Test most reasons to disallow the blacklist.
  // Excluded values are USER_RECENTLY_OPTED_OUT, USER_BLACKLISTED,
  // HOST_BLACKLISTED. These are internal to the blacklist.
  net::TestURLRequestContext context;
  GURL test_host("http://www.test_host.com");
  std::unique_ptr<net::URLRequest> request =
      context.CreateRequest(test_host, net::DEFAULT_PRIORITY, nullptr);
  set_io_data(base::MakeUnique<TestPreviewsIOData>(loop_.task_runner(),
                                                   loop_.task_runner()));
  base::HistogramTester histogram_tester;

  // If not in the field trial, don't log anything, and return false.
  EXPECT_FALSE(io_data()->ShouldAllowPreview(*request, PreviewsType::OFFLINE));
  histogram_tester.ExpectTotalCount("Previews.EligibilityReason.Offline", 0);

  // Enable Offline previews field trial.
  base::FieldTrialList field_trial_list(nullptr);
  std::map<std::string, std::string> params;
  params["show_offline_pages"] = "true";
  variations::AssociateVariationParams("ClientSidePreviews", "Enabled", params);
  base::FieldTrialList::CreateFieldTrial("ClientSidePreviews", "Enabled");

  // The blacklist is not created yet.
  EXPECT_FALSE(io_data()->ShouldAllowPreview(*request, PreviewsType::OFFLINE));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.Offline",
      static_cast<int>(PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE), 1);

  set_ui_service(base::WrapUnique(
      new PreviewsUIService(io_data(), loop_.task_runner(),
                            base::MakeUnique<TestPreviewsOptOutStore>())));

  base::RunLoop().RunUntilIdle();

  // Return one of the failing statuses from the blacklist; cause the blacklist
  // to not be loaded by clearing the blacklist.
  base::Time now = base::Time::Now();
  io_data()->ClearBlackList(now, now);

  EXPECT_FALSE(io_data()->ShouldAllowPreview(*request, PreviewsType::OFFLINE));
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.Offline",
      static_cast<int>(PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED),
      1);

  // Reload the blacklist. The blacklist should allow all navigations now.
  base::RunLoop().RunUntilIdle();

  // NQE should be null on the context.
  EXPECT_FALSE(io_data()->ShouldAllowPreview(*request, PreviewsType::OFFLINE));
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.Offline",
      static_cast<int>(PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE),
      1);

  net::TestNetworkQualityEstimator network_quality_estimator(params);
  context.set_network_quality_estimator(&network_quality_estimator);

  // Set NQE type to unknown.
  network_quality_estimator.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);

  EXPECT_FALSE(io_data()->ShouldAllowPreview(*request, PreviewsType::OFFLINE));
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.Offline",
      static_cast<int>(PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE),
      2);

  // Set NQE type to fast enough.
  network_quality_estimator.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_2G);
  EXPECT_FALSE(io_data()->ShouldAllowPreview(*request, PreviewsType::OFFLINE));
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.Offline",
      static_cast<int>(PreviewsEligibilityReason::NETWORK_NOT_SLOW), 1);

  // Set NQE type to slow.
  network_quality_estimator.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  EXPECT_TRUE(io_data()->ShouldAllowPreview(*request, PreviewsType::OFFLINE));
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.Offline",
      static_cast<int>(PreviewsEligibilityReason::ALLOWED), 1);

  histogram_tester.ExpectTotalCount("Previews.EligibilityReason.Offline", 6);

  variations::testing::ClearAllVariationParams();
}

}  // namespace previews
