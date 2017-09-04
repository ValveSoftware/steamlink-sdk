// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/downloads/download_ui_adapter.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/offline_pages/client_namespace_constants.h"
#include "components/offline_pages/client_policy_controller.h"
#include "components/offline_pages/stub_offline_page_model.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
// Constants for a test OfflinePageItem.
static const int kTestOfflineId1 = 1;
static const int kTestOfflineId2 = 2;
static const int kTestOfflineId3 = 3;
static const char kTestUrl[] = "http://foo.com/bar.mhtml";
static const char kTestGuid1[] =   "cccccccc-cccc-4ccc-0ccc-ccccccccccc1";
static const char kTestGuid2[] =   "cccccccc-cccc-4ccc-0ccc-ccccccccccc2";
static const char kTestBadGuid[] = "ccccccc-cccc-0ccc-0ccc-ccccccccccc0";
static const ClientId kTestClientIdOtherNamespace(kLastNNamespace, kTestGuid1);
static const ClientId kTestClientIdOtherGuid(kLastNNamespace, kTestBadGuid);
static const ClientId kTestClientId1(kAsyncNamespace, kTestGuid1);
static const ClientId kTestClientId2(kAsyncNamespace, kTestGuid2);
static const base::FilePath kTestFilePath =
    base::FilePath(FILE_PATH_LITERAL("foo/bar.mhtml"));
static const int kFileSize = 1000;
static const base::Time kTestCreationTime = base::Time::Now();
static const base::string16 kTestTitle = base::ASCIIToUTF16("test title");
}  // namespace

// Mock OfflinePageModel for testing the SavePage calls.
class MockOfflinePageModel : public StubOfflinePageModel {
 public:
  MockOfflinePageModel(base::TestMockTimeTaskRunner* task_runner)
      : observer_(nullptr),
        task_runner_(task_runner),
        policy_controller_(new ClientPolicyController()) {
    adapter.reset(new DownloadUIAdapter(this));
    // Add one page.
    OfflinePageItem page(GURL(kTestUrl),
                         kTestOfflineId1,
                         kTestClientId1,
                         kTestFilePath,
                         kFileSize,
                         kTestCreationTime);
    page.title = kTestTitle;
    pages[kTestOfflineId1] = page;
  }

  ~MockOfflinePageModel() override {}

  // OfflinePageModel overrides.
  void AddObserver(Observer* observer) override {
    EXPECT_TRUE(observer != nullptr);
    observer_ = observer;
  }

  void RemoveObserver(Observer* observer) override {
    EXPECT_TRUE(observer != nullptr);
    EXPECT_EQ(observer, observer_);
    observer_ = nullptr;
  }

  // PostTask instead of just running callback to simpulate the real class.
  void GetAllPages(const MultipleOfflinePageItemCallback& callback) override {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&MockOfflinePageModel::GetAllPagesImpl,
        base::Unretained(this), callback));
  }

  void GetAllPagesImpl(const MultipleOfflinePageItemCallback& callback) {
    std::vector<OfflinePageItem> result;
    for (const auto& page : pages)
      result.push_back(page.second);
    callback.Run(result);
  }

  void DeletePageAndNotifyAdapter(const std::string& guid) {
    for(const auto& page : pages) {
      if (page.second.client_id.id == guid) {
        observer_->OfflinePageDeleted(page.second.offline_id,
                                      page.second.client_id);
        pages.erase(page.first);
        return;
      }
    }
  }

  void AddPageAndNotifyAdapter(const OfflinePageItem& page) {
    EXPECT_EQ(pages.end(), pages.find(page.offline_id));
    pages[page.offline_id] = page;
    observer_->OfflinePageModelChanged(this);
  }

  ClientPolicyController* GetPolicyController() override {
    return policy_controller_.get();
  }

  // Normally, OfflinePageModel owns this adapter, so lets test it this way.
  std::unique_ptr<DownloadUIAdapter> adapter;

  std::map<int64_t, OfflinePageItem> pages;

 private:
  OfflinePageModel::Observer* observer_;
  base::TestMockTimeTaskRunner* task_runner_;
  // Normally owned by OfflinePageModel.
  std::unique_ptr<ClientPolicyController> policy_controller_;

  DISALLOW_COPY_AND_ASSIGN(MockOfflinePageModel);
};

class DownloadUIAdapterTest
    : public testing::Test,
      public DownloadUIAdapter::Observer {
 public:
  DownloadUIAdapterTest();
  ~DownloadUIAdapterTest() override;

  // testing::Test
  void SetUp() override;

  // DownloadUIAdapter::Observer
  void ItemsLoaded() override;
  void ItemAdded(const DownloadUIItem& item) override;
  void ItemUpdated(const DownloadUIItem& item) override;
  void ItemDeleted(const std::string& guid) override;

  // Runs until all of the tasks that are not delayed are gone from the task
  // queue.
  void PumpLoop();

  bool items_loaded;
  std::vector<std::string> added_guids, updated_guids, deleted_guids;
  std::unique_ptr<MockOfflinePageModel> model;

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
};

DownloadUIAdapterTest::DownloadUIAdapterTest()
    : items_loaded(false),
      task_runner_(new base::TestMockTimeTaskRunner) {
}

DownloadUIAdapterTest::~DownloadUIAdapterTest() {
}

void DownloadUIAdapterTest::SetUp() {
  model.reset(new MockOfflinePageModel(task_runner_.get()));
  model->adapter->AddObserver(this);
}

void DownloadUIAdapterTest::ItemsLoaded() {
  items_loaded = true;
}

void DownloadUIAdapterTest::ItemAdded(const DownloadUIItem& item) {
  added_guids.push_back(item.guid);
}

void DownloadUIAdapterTest::ItemUpdated(const DownloadUIItem& item) {
  updated_guids.push_back(item.guid);
}

void DownloadUIAdapterTest::ItemDeleted(const std::string& guid) {
  deleted_guids.push_back(guid);
}

void DownloadUIAdapterTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

TEST_F(DownloadUIAdapterTest, InitialLoad) {
  EXPECT_NE(nullptr, model->adapter);
  EXPECT_FALSE(items_loaded);
  PumpLoop();
  EXPECT_TRUE(items_loaded);
  const DownloadUIItem* item = model->adapter->GetItem(kTestGuid1);
  EXPECT_NE(nullptr, item);
}

TEST_F(DownloadUIAdapterTest, InitialItemConversion) {
  EXPECT_EQ(1UL, model->pages.size());
  EXPECT_EQ(kTestGuid1, model->pages[kTestOfflineId1].client_id.id);
  PumpLoop();
  const DownloadUIItem* item = model->adapter->GetItem(kTestGuid1);
  EXPECT_EQ(kTestGuid1, item->guid);
  EXPECT_EQ(kTestUrl, item->url.spec());
  EXPECT_EQ(kTestFilePath, item->target_path);
  EXPECT_EQ(kTestCreationTime, item->start_time);
  EXPECT_EQ(kFileSize, item->total_bytes);
  EXPECT_EQ(kTestTitle, item->title);
}

TEST_F(DownloadUIAdapterTest, ItemDeletedAdded) {
  PumpLoop();
  // Add page, notify adapter.
  OfflinePageItem page(GURL(kTestUrl),
                       kTestOfflineId2,
                       kTestClientId2,
                       base::FilePath(kTestFilePath),
                       kFileSize,
                       kTestCreationTime);
  model->AddPageAndNotifyAdapter(page);
  PumpLoop();
  EXPECT_EQ(1UL, added_guids.size());
  EXPECT_EQ(kTestGuid2, added_guids[0]);
  // Remove pages, notify adapter.
  model->DeletePageAndNotifyAdapter(kTestGuid1);
  model->DeletePageAndNotifyAdapter(kTestGuid2);
  PumpLoop();
  EXPECT_EQ(2UL, deleted_guids.size());
  EXPECT_EQ(kTestGuid1, deleted_guids[0]);
  EXPECT_EQ(kTestGuid2, deleted_guids[1]);
}

TEST_F(DownloadUIAdapterTest, ItemWithWrongNamespace) {
  PumpLoop();
  OfflinePageItem page1(GURL(kTestUrl),
                        kTestOfflineId2,
                        kTestClientIdOtherNamespace,
                        base::FilePath(kTestFilePath),
                        kFileSize,
                        kTestCreationTime);
  model->AddPageAndNotifyAdapter(page1);
  PumpLoop();
  // Should not add the page with wrong namespace.
  EXPECT_EQ(0UL, added_guids.size());

  OfflinePageItem page2(GURL(kTestUrl),
                        kTestOfflineId3,
                        kTestClientIdOtherGuid,
                        base::FilePath(kTestFilePath),
                        kFileSize,
                        kTestCreationTime);
  model->AddPageAndNotifyAdapter(page2);
  PumpLoop();
  // Should not add the page with wrong guid.
  EXPECT_EQ(0UL, added_guids.size());
}

TEST_F(DownloadUIAdapterTest, ItemUpdated) {
  PumpLoop();
  // Clear the initial page and replace it with updated one.
  model->pages.clear();
  // Add page with the same offline_id/guid, notify adapter.
  // This should generate 'updated' notification.
  OfflinePageItem page1(GURL(kTestUrl),
                        kTestOfflineId1,
                        kTestClientId1,
                        base::FilePath(kTestFilePath),
                        kFileSize,
                        kTestCreationTime);
  // Add a new page which did not exist before.
  OfflinePageItem page2(GURL(kTestUrl),
                        kTestOfflineId2,
                        kTestClientId2,
                        base::FilePath(kTestFilePath),
                        kFileSize,
                        kTestCreationTime);
  model->AddPageAndNotifyAdapter(page1);
  model->AddPageAndNotifyAdapter(page2);
  PumpLoop();
  EXPECT_EQ(1UL, added_guids.size());
  EXPECT_EQ(kTestGuid2, added_guids[0]);
  // TODO(dimich): we currently don't report updated items since OPM doesn't
  // have support for that. Add as needed, this will have to be updated when
  // support is added.
  EXPECT_EQ(0UL, updated_guids.size());
}

TEST_F(DownloadUIAdapterTest, NoHangingLoad) {
  EXPECT_NE(nullptr, model->adapter);
  EXPECT_FALSE(items_loaded);
  // Removal of last observer causes cache unload of not-yet-loaded cache.
  model->adapter->RemoveObserver(this);
  // This will complete async fetch of items, but...
  PumpLoop();
  // items should not be loaded when there is no observers!
  EXPECT_FALSE(items_loaded);
  // This should not crash.
  model->adapter->AddObserver(this);
}

}  // namespace offline_pages
