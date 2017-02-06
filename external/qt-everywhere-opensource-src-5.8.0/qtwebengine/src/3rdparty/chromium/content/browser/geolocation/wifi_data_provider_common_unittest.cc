// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/wifi_data_provider_common.h"

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/geolocation/wifi_data_provider_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::DoDefault;
using testing::Invoke;
using testing::Return;

namespace content {

class MockWlanApi : public WifiDataProviderCommon::WlanApiInterface {
 public:
  MockWlanApi() : calls_(0), bool_return_(true) {
    ANNOTATE_BENIGN_RACE(&calls_, "This is a test-only data race on a counter");
    ON_CALL(*this, GetAccessPointData(_))
        .WillByDefault(Invoke(this, &MockWlanApi::GetAccessPointDataInternal));
  }

  MOCK_METHOD1(GetAccessPointData, bool(WifiData::AccessPointDataSet* data));

  int calls_;
  bool bool_return_;
  WifiData::AccessPointDataSet data_out_;

 private:
  bool GetAccessPointDataInternal(WifiData::AccessPointDataSet* data) {
    ++calls_;
    *data = data_out_;
    return bool_return_;
  }
};

class MockPollingPolicy : public WifiPollingPolicy {
 public:
  MockPollingPolicy() {
    ON_CALL(*this,PollingInterval())
        .WillByDefault(Return(1));
    ON_CALL(*this,NoWifiInterval())
        .WillByDefault(Return(1));
  }

  MOCK_METHOD0(PollingInterval, int());
  MOCK_METHOD0(NoWifiInterval, int());

  virtual void UpdatePollingInterval(bool) {}
};

class WifiDataProviderCommonWithMock : public WifiDataProviderCommon {
 public:
  WifiDataProviderCommonWithMock()
      : new_wlan_api_(new MockWlanApi),
        new_polling_policy_(new MockPollingPolicy) {}

  // WifiDataProviderCommon
  WlanApiInterface* NewWlanApi() override {
    CHECK(new_wlan_api_ != NULL);
    return new_wlan_api_.release();
  }
  WifiPollingPolicy* NewPollingPolicy() override {
    CHECK(new_polling_policy_ != NULL);
    return new_polling_policy_.release();
  }

  std::unique_ptr<MockWlanApi> new_wlan_api_;
  std::unique_ptr<MockPollingPolicy> new_polling_policy_;

 private:
  ~WifiDataProviderCommonWithMock() override {}

  DISALLOW_COPY_AND_ASSIGN(WifiDataProviderCommonWithMock);
};

WifiDataProvider* CreateWifiDataProviderCommonWithMock() {
  return new WifiDataProviderCommonWithMock;
}

// Main test fixture
class GeolocationWifiDataProviderCommonTest : public testing::Test {
 public:
  GeolocationWifiDataProviderCommonTest()
      : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        wifi_data_callback_(
            base::Bind(&GeolocationWifiDataProviderCommonTest::OnWifiDataUpdate,
                       base::Unretained(this))) {}

  void SetUp() override {
    provider_ = new WifiDataProviderCommonWithMock;
    wlan_api_ = provider_->new_wlan_api_.get();
    polling_policy_ = provider_->new_polling_policy_.get();
    provider_->AddCallback(&wifi_data_callback_);
  }

  void TearDown() override {
    provider_->RemoveCallback(&wifi_data_callback_);
    provider_->StopDataProvider();
    provider_ = NULL;
  }

  void OnWifiDataUpdate() {
    // Callbacks must run on the originating thread.
    EXPECT_TRUE(main_task_runner_->BelongsToCurrentThread());
    run_loop_->Quit();
  }

  void RunLoop() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

 protected:
  TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  std::unique_ptr<base::RunLoop> run_loop_;
  WifiDataProviderManager::WifiDataUpdateCallback wifi_data_callback_;
  scoped_refptr<WifiDataProviderCommonWithMock> provider_;
  MockWlanApi* wlan_api_;
  MockPollingPolicy* polling_policy_;
};

TEST_F(GeolocationWifiDataProviderCommonTest, CreateDestroy) {
  // Test fixture members were SetUp correctly.
  EXPECT_TRUE(main_task_runner_->BelongsToCurrentThread());
  EXPECT_TRUE(NULL != provider_.get());
  EXPECT_TRUE(NULL != wlan_api_);
}

TEST_F(GeolocationWifiDataProviderCommonTest, RunNormal) {
  EXPECT_CALL(*wlan_api_, GetAccessPointData(_))
      .Times(AtLeast(1));
  EXPECT_CALL(*polling_policy_, PollingInterval())
      .Times(AtLeast(1));
  provider_->StartDataProvider();
  RunLoop();
  SUCCEED();
}

TEST_F(GeolocationWifiDataProviderCommonTest, NoWifi) {
  EXPECT_CALL(*polling_policy_, NoWifiInterval())
      .Times(AtLeast(1));
  EXPECT_CALL(*wlan_api_, GetAccessPointData(_))
      .WillRepeatedly(Return(false));
  provider_->StartDataProvider();
  RunLoop();
}

TEST_F(GeolocationWifiDataProviderCommonTest, IntermittentWifi) {
  EXPECT_CALL(*polling_policy_, PollingInterval())
      .Times(AtLeast(1));
  EXPECT_CALL(*polling_policy_, NoWifiInterval())
      .Times(1);
  EXPECT_CALL(*wlan_api_, GetAccessPointData(_))
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillRepeatedly(DoDefault());

  AccessPointData single_access_point;
  single_access_point.channel = 2;
  single_access_point.mac_address = 3;
  single_access_point.radio_signal_strength = 4;
  single_access_point.signal_to_noise = 5;
  single_access_point.ssid = base::ASCIIToUTF16("foossid");
  wlan_api_->data_out_.insert(single_access_point);

  provider_->StartDataProvider();
  RunLoop();
  RunLoop();
}

#if defined(OS_MACOSX)
#define MAYBE_DoAnEmptyScan DISABLED_DoAnEmptyScan
#else
#define MAYBE_DoAnEmptyScan DoAnEmptyScan
#endif
TEST_F(GeolocationWifiDataProviderCommonTest, MAYBE_DoAnEmptyScan) {
  EXPECT_CALL(*wlan_api_, GetAccessPointData(_))
      .Times(AtLeast(1));
  EXPECT_CALL(*polling_policy_, PollingInterval())
      .Times(AtLeast(1));
  provider_->StartDataProvider();
  RunLoop();
  EXPECT_EQ(wlan_api_->calls_, 1);
  WifiData data;
  EXPECT_TRUE(provider_->GetData(&data));
  EXPECT_EQ(0, static_cast<int>(data.access_point_data.size()));
}

#if defined(OS_MACOSX)
#define MAYBE_DoScanWithResults DISABLED_DoScanWithResults
#else
#define MAYBE_DoScanWithResults DoScanWithResults
#endif
TEST_F(GeolocationWifiDataProviderCommonTest, MAYBE_DoScanWithResults) {
  EXPECT_CALL(*wlan_api_, GetAccessPointData(_))
      .Times(AtLeast(1));
  EXPECT_CALL(*polling_policy_, PollingInterval())
      .Times(AtLeast(1));
  AccessPointData single_access_point;
  single_access_point.channel = 2;
  single_access_point.mac_address = 3;
  single_access_point.radio_signal_strength = 4;
  single_access_point.signal_to_noise = 5;
  single_access_point.ssid = base::ASCIIToUTF16("foossid");
  wlan_api_->data_out_.insert(single_access_point);

  provider_->StartDataProvider();
  RunLoop();
  EXPECT_EQ(wlan_api_->calls_, 1);
  WifiData data;
  EXPECT_TRUE(provider_->GetData(&data));
  EXPECT_EQ(1, static_cast<int>(data.access_point_data.size()));
  EXPECT_EQ(single_access_point.ssid, data.access_point_data.begin()->ssid);
}

TEST_F(GeolocationWifiDataProviderCommonTest, RegisterUnregister) {
  WifiDataProviderManager::SetFactoryForTesting(
      CreateWifiDataProviderCommonWithMock);
  WifiDataProviderManager::Register(&wifi_data_callback_);
  RunLoop();
  WifiDataProviderManager::Unregister(&wifi_data_callback_);
  WifiDataProviderManager::ResetFactoryForTesting();
}

}  // namespace content
