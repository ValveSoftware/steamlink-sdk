// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/downloads/downloads_api.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_service_impl.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/mock_download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using HistoryService = history::HistoryService;
using MockDownloadManager = content::MockDownloadManager;

namespace extensions {

namespace {

// A DownloadService that returns a custom DownloadHistory.
class TestDownloadService : public DownloadServiceImpl {
 public:
  explicit TestDownloadService(Profile* profile)
      : DownloadServiceImpl(profile), profile_(profile) {}
  ~TestDownloadService() override {}

  void set_download_history(std::unique_ptr<DownloadHistory> download_history) {
    download_history_.swap(download_history);
  }

  DownloadHistory* GetDownloadHistory() override {
    return download_history_.get();
  }

  ExtensionDownloadsEventRouter* GetExtensionEventRouter() override {
    if (!router_.get()) {
      router_.reset(new ExtensionDownloadsEventRouter(
          profile_, content::BrowserContext::GetDownloadManager(profile_)));
    }
    return router_.get();
  }

 private:
  std::unique_ptr<DownloadHistory> download_history_;
  std::unique_ptr<ExtensionDownloadsEventRouter> router_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(TestDownloadService);
};

}  // namespace

class DownloadsApiUnitTest : public ExtensionApiUnittest {
 public:
  DownloadsApiUnitTest() {}
  ~DownloadsApiUnitTest() override {}
  void SetUp() override {
    ExtensionApiUnittest::SetUp();

    manager_.reset(new testing::StrictMock<MockDownloadManager>());
    EXPECT_CALL(*manager_, AddObserver(testing::_))
        .WillOnce(testing::SaveArg<0>(&download_history_manager_observer_));
    EXPECT_CALL(*manager_, RemoveObserver(testing::Eq(testing::ByRef(
                               download_history_manager_observer_))))
        .WillOnce(testing::Assign(
            &download_history_manager_observer_,
            static_cast<content::DownloadManager::Observer*>(nullptr)));
    EXPECT_CALL(*manager_, GetAllDownloads(testing::_))
        .Times(testing::AnyNumber());

    std::unique_ptr<HistoryAdapter> history_adapter(new HistoryAdapter);
    std::unique_ptr<DownloadHistory> download_history(
        new DownloadHistory(manager_.get(), std::move(history_adapter)));
    TestDownloadService* download_service = static_cast<TestDownloadService*>(
        DownloadServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), &TestingDownloadServiceFactory));
    ASSERT_TRUE(download_service);
    download_service->set_download_history(std::move(download_history));
  }
  void TearDown() override { ExtensionApiUnittest::TearDown(); }

 private:
  // A private empty history adapter that does nothing on QueryDownloads().
  class HistoryAdapter : public DownloadHistory::HistoryAdapter {
   public:
    HistoryAdapter() : DownloadHistory::HistoryAdapter(nullptr) {}

   private:
    void QueryDownloads(
        const HistoryService::DownloadQueryCallback& callback) override {}
  };

  // Constructs and returns a TestDownloadService.
  static std::unique_ptr<KeyedService> TestingDownloadServiceFactory(
      content::BrowserContext* browser_context);

  std::unique_ptr<MockDownloadManager> manager_;
  content::DownloadManager::Observer* download_history_manager_observer_;

  DISALLOW_COPY_AND_ASSIGN(DownloadsApiUnitTest);
};

// static
std::unique_ptr<KeyedService>
DownloadsApiUnitTest::TestingDownloadServiceFactory(
    content::BrowserContext* browser_context) {
  return base::MakeUnique<TestDownloadService>(
      Profile::FromBrowserContext(browser_context));
}

// Tests that Number/double properties in query are parsed correctly.
// Regression test for https://crbug.com/617435.
TEST_F(DownloadsApiUnitTest, ParseSearchQuery) {
  RunFunction(new DownloadsSearchFunction, "[{\"totalBytesLess\":1}]");
  RunFunction(new DownloadsSearchFunction, "[{\"totalBytesGreater\":2}]");
}

}  // namespace extensions
