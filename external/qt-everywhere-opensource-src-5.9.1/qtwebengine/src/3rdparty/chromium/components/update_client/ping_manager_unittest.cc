// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/ping_manager.h"

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/url_request_post_interceptor.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace update_client {

class ComponentUpdaterPingManagerTest : public testing::Test {
 public:
  ComponentUpdaterPingManagerTest();
  ~ComponentUpdaterPingManagerTest() override {}

  void RunThreadsUntilIdle();

  // Overrides from testing::Test.
  void SetUp() override;
  void TearDown() override;

 protected:
  scoped_refptr<TestConfigurator> config_;
  std::unique_ptr<PingManager> ping_manager_;

 private:
  base::MessageLoopForIO loop_;
};

ComponentUpdaterPingManagerTest::ComponentUpdaterPingManagerTest() {
}

void ComponentUpdaterPingManagerTest::SetUp() {
  config_ = new TestConfigurator(base::ThreadTaskRunnerHandle::Get(),
                                 base::ThreadTaskRunnerHandle::Get());
  ping_manager_.reset(new PingManager(config_));
}

void ComponentUpdaterPingManagerTest::TearDown() {
  ping_manager_.reset();
  config_ = nullptr;
}

void ComponentUpdaterPingManagerTest::RunThreadsUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

// Test is flaky: http://crbug.com/349547
TEST_F(ComponentUpdaterPingManagerTest, DISABLED_PingManagerTest) {
  std::unique_ptr<InterceptorFactory> interceptor_factory(
      new InterceptorFactory(base::ThreadTaskRunnerHandle::Get()));
  URLRequestPostInterceptor* interceptor =
      interceptor_factory->CreateInterceptor();
  EXPECT_TRUE(interceptor);

  // Test eventresult="1" is sent for successful updates.
  CrxUpdateItem item;
  item.id = "abc";
  item.state = CrxUpdateItem::State::kUpdated;
  item.previous_version = base::Version("1.0");
  item.next_version = base::Version("2.0");

  ping_manager_->SendPing(&item);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
  EXPECT_NE(string::npos,
            interceptor->GetRequests()[0].find(
                "<app appid=\"abc\" version=\"1.0\" nextversion=\"2.0\">"
                "<event eventtype=\"3\" eventresult=\"1\"/></app>"))
      << interceptor->GetRequestsAsString();
  interceptor->Reset();

  // Test eventresult="0" is sent for failed updates.
  item = CrxUpdateItem();
  item.id = "abc";
  item.state = CrxUpdateItem::State::kNoUpdate;
  item.previous_version = base::Version("1.0");
  item.next_version = base::Version("2.0");

  ping_manager_->SendPing(&item);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
  EXPECT_NE(string::npos,
            interceptor->GetRequests()[0].find(
                "<app appid=\"abc\" version=\"1.0\" nextversion=\"2.0\">"
                "<event eventtype=\"3\" eventresult=\"0\"/></app>"))
      << interceptor->GetRequestsAsString();
  interceptor->Reset();

  // Test the error values and the fingerprints.
  item = CrxUpdateItem();
  item.id = "abc";
  item.state = CrxUpdateItem::State::kNoUpdate;
  item.previous_version = base::Version("1.0");
  item.next_version = base::Version("2.0");
  item.previous_fp = "prev fp";
  item.next_fp = "next fp";
  item.error_category = 1;
  item.error_code = 2;
  item.extra_code1 = -1;
  item.diff_error_category = 10;
  item.diff_error_code = 20;
  item.diff_extra_code1 = -10;
  item.diff_update_failed = true;
  item.crx_diffurls.push_back(GURL("http://host/path"));

  ping_manager_->SendPing(&item);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
  EXPECT_NE(string::npos,
            interceptor->GetRequests()[0].find(
                "<app appid=\"abc\" version=\"1.0\" nextversion=\"2.0\">"
                "<event eventtype=\"3\" eventresult=\"0\" errorcat=\"1\" "
                "errorcode=\"2\" extracode1=\"-1\" diffresult=\"0\" "
                "differrorcat=\"10\" "
                "differrorcode=\"20\" diffextracode1=\"-10\" "
                "previousfp=\"prev fp\" nextfp=\"next fp\"/></app>"))
      << interceptor->GetRequestsAsString();
  interceptor->Reset();

  // Test the download metrics.
  item = CrxUpdateItem();
  item.id = "abc";
  item.state = CrxUpdateItem::State::kUpdated;
  item.previous_version = base::Version("1.0");
  item.next_version = base::Version("2.0");

  CrxDownloader::DownloadMetrics download_metrics;
  download_metrics.url = GURL("http://host1/path1");
  download_metrics.downloader = CrxDownloader::DownloadMetrics::kUrlFetcher;
  download_metrics.error = -1;
  download_metrics.downloaded_bytes = 123;
  download_metrics.total_bytes = 456;
  download_metrics.download_time_ms = 987;
  item.download_metrics.push_back(download_metrics);

  download_metrics = CrxDownloader::DownloadMetrics();
  download_metrics.url = GURL("http://host2/path2");
  download_metrics.downloader = CrxDownloader::DownloadMetrics::kBits;
  download_metrics.error = 0;
  download_metrics.downloaded_bytes = 1230;
  download_metrics.total_bytes = 4560;
  download_metrics.download_time_ms = 9870;
  item.download_metrics.push_back(download_metrics);

  ping_manager_->SendPing(&item);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
  EXPECT_NE(
      string::npos,
      interceptor->GetRequests()[0].find(
          "<app appid=\"abc\" version=\"1.0\" nextversion=\"2.0\">"
          "<event eventtype=\"3\" eventresult=\"1\"/>"
          "<event eventtype=\"14\" eventresult=\"0\" downloader=\"direct\" "
          "errorcode=\"-1\" url=\"http://host1/path1\" downloaded=\"123\" "
          "total=\"456\" download_time_ms=\"987\"/>"
          "<event eventtype=\"14\" eventresult=\"1\" downloader=\"bits\" "
          "url=\"http://host2/path2\" downloaded=\"1230\" total=\"4560\" "
          "download_time_ms=\"9870\"/></app>"))
      << interceptor->GetRequestsAsString();
  interceptor->Reset();
}

// Tests that sending the ping fails when the component requires encryption but
// the ping URL is unsecure.
TEST_F(ComponentUpdaterPingManagerTest, PingManagerRequiresEncryptionTest) {
  config_->SetPingUrl(GURL("http:\\foo\bar"));

  {
    CrxUpdateItem item;
    item.component.requires_network_encryption = true;

    EXPECT_FALSE(ping_manager_->SendPing(&item));
  }

  {
    // Tests that the default for |requires_network_encryption| is true.
    CrxUpdateItem item;
    EXPECT_FALSE(ping_manager_->SendPing(&item));
  }
}

}  // namespace update_client
