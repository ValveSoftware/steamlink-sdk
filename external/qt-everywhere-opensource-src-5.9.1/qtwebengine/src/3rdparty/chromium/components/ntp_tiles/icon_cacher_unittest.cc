// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/icon_cacher.h"

#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/favicon/core/favicon_client.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/favicon_util.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/image_fetcher/image_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Return;

namespace ntp_tiles {
namespace {

class MockImageFetcher : public image_fetcher::ImageFetcher {
 public:
  MOCK_METHOD1(SetImageFetcherDelegate,
               void(image_fetcher::ImageFetcherDelegate* delegate));
  MOCK_METHOD1(SetDataUseServiceName,
               void(image_fetcher::ImageFetcher::DataUseServiceName name));
  MOCK_METHOD3(StartOrQueueNetworkRequest,
               void(const std::string& id,
                    const GURL& image_url,
                    base::Callback<void(const std::string& id,
                                        const gfx::Image& image)> callback));
};

class IconCacherTest : public ::testing::Test {
 protected:
  IconCacherTest()
      : site_(base::string16(),  // title, unused
              GURL("http://url.google/"),
              GURL("http://url.google/icon.png"),
              GURL("http://url.google/favicon.ico"),
              GURL()),  // thumbnail, unused
        image_fetcher_(new ::testing::StrictMock<MockImageFetcher>),
        favicon_service_(/*favicon_client=*/nullptr, &history_service_) {
    CHECK(history_dir_.CreateUniqueTempDir());
    CHECK(history_service_.Init(
        history::HistoryDatabaseParams(history_dir_.GetPath(), 0, 0)));
  }

  void PreloadIcon(const GURL& url,
                   const GURL& icon_url,
                   favicon_base::IconType icon_type,
                   int width,
                   int height) {
    favicon_service_.SetFavicons(url, icon_url, icon_type,
                                 gfx::test::CreateImage(width, height));
  }

  bool IconIsCachedFor(const GURL& url, favicon_base::IconType icon_type) {
    base::CancelableTaskTracker tracker;
    bool is_cached;
    base::RunLoop loop;
    favicon::GetFaviconImageForPageURL(
        &favicon_service_, url, icon_type,
        base::Bind(
            [](bool* is_cached, base::RunLoop* loop,
               const favicon_base::FaviconImageResult& result) {
              *is_cached = !result.image.IsEmpty();
              loop->Quit();
            },
            &is_cached, &loop),
        &tracker);
    loop.Run();
    return is_cached;
  }

  base::MessageLoop message_loop_;
  PopularSites::Site site_;
  std::unique_ptr<MockImageFetcher> image_fetcher_;
  base::ScopedTempDir history_dir_;
  history::HistoryService history_service_;
  favicon::FaviconService favicon_service_;
};

template <typename Fn>
base::Callback<Fn> BindMockFunction(MockFunction<Fn>* function) {
  return base::Bind(&MockFunction<Fn>::Call, base::Unretained(function));
}

ACTION(FailFetch) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(arg2, arg0, gfx::Image()));
}

ACTION_P2(PassFetch, width, height) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(arg2, arg0, gfx::test::CreateImage(width, height)));
}

ACTION_P(Quit, run_loop) {
  run_loop->Quit();
}

TEST_F(IconCacherTest, LargeCached) {
  MockFunction<void(bool)> done;
  base::RunLoop loop;
  {
    InSequence s;
    EXPECT_CALL(*image_fetcher_,
                SetDataUseServiceName(
                    data_use_measurement::DataUseUserData::NTP_TILES));
    EXPECT_CALL(done, Call(false)).WillOnce(Quit(&loop));
  }
  PreloadIcon(site_.url, site_.large_icon_url, favicon_base::TOUCH_ICON, 128,
              128);

  IconCacher cacher(&favicon_service_, std::move(image_fetcher_));
  cacher.StartFetch(site_, BindMockFunction(&done));
  loop.Run();
  EXPECT_FALSE(IconIsCachedFor(site_.url, favicon_base::FAVICON));
  EXPECT_TRUE(IconIsCachedFor(site_.url, favicon_base::TOUCH_ICON));
}

TEST_F(IconCacherTest, LargeNotCachedAndFetchSucceeded) {
  MockFunction<void(bool)> done;
  base::RunLoop loop;
  {
    InSequence s;
    EXPECT_CALL(*image_fetcher_,
                SetDataUseServiceName(
                    data_use_measurement::DataUseUserData::NTP_TILES));
    EXPECT_CALL(*image_fetcher_,
                StartOrQueueNetworkRequest(_, site_.large_icon_url, _))
        .WillOnce(PassFetch(128, 128));
    EXPECT_CALL(done, Call(true)).WillOnce(Quit(&loop));
  }

  IconCacher cacher(&favicon_service_, std::move(image_fetcher_));
  cacher.StartFetch(site_, BindMockFunction(&done));
  loop.Run();
  EXPECT_FALSE(IconIsCachedFor(site_.url, favicon_base::FAVICON));
  EXPECT_TRUE(IconIsCachedFor(site_.url, favicon_base::TOUCH_ICON));
}

TEST_F(IconCacherTest, SmallNotCachedAndFetchSucceeded) {
  site_.large_icon_url = GURL();

  MockFunction<void(bool)> done;
  base::RunLoop loop;
  {
    InSequence s;
    EXPECT_CALL(*image_fetcher_,
                SetDataUseServiceName(
                    data_use_measurement::DataUseUserData::NTP_TILES));
    EXPECT_CALL(*image_fetcher_,
                StartOrQueueNetworkRequest(_, site_.favicon_url, _))
        .WillOnce(PassFetch(128, 128));
    EXPECT_CALL(done, Call(true)).WillOnce(Quit(&loop));
  }

  IconCacher cacher(&favicon_service_, std::move(image_fetcher_));
  cacher.StartFetch(site_, BindMockFunction(&done));
  loop.Run();
  EXPECT_TRUE(IconIsCachedFor(site_.url, favicon_base::FAVICON));
  EXPECT_FALSE(IconIsCachedFor(site_.url, favicon_base::TOUCH_ICON));
}

TEST_F(IconCacherTest, LargeNotCachedAndFetchFailed) {
  MockFunction<void(bool)> done;
  base::RunLoop loop;
  {
    InSequence s;
    EXPECT_CALL(*image_fetcher_,
                SetDataUseServiceName(
                    data_use_measurement::DataUseUserData::NTP_TILES));
    EXPECT_CALL(*image_fetcher_,
                StartOrQueueNetworkRequest(_, site_.large_icon_url, _))
        .WillOnce(FailFetch());
    EXPECT_CALL(done, Call(false)).WillOnce(Quit(&loop));
  }

  IconCacher cacher(&favicon_service_, std::move(image_fetcher_));
  cacher.StartFetch(site_, BindMockFunction(&done));
  loop.Run();
  EXPECT_FALSE(IconIsCachedFor(site_.url, favicon_base::FAVICON));
  EXPECT_FALSE(IconIsCachedFor(site_.url, favicon_base::TOUCH_ICON));
}

}  // namespace
}  // namespace ntp_tiles
