// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_model_impl.h"

#include <stdint.h>
#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/offline_pages/client_namespace_constants.h"
#include "components/offline_pages/client_policy_controller.h"
#include "components/offline_pages/offline_page_client_policy.h"
#include "components/offline_pages/offline_page_feature.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_storage_manager.h"
#include "components/offline_pages/offline_page_test_archiver.h"
#include "components/offline_pages/offline_page_test_store.h"
#include "components/offline_pages/offline_page_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {

namespace {
const char kTestClientNamespace[] = "default";
const char kUserRequestedNamespace[] = "download";
const GURL kTestUrl("http://example.com");
const GURL kTestUrl2("http://other.page.com");
const GURL kTestUrl3("http://test.xyz");
const GURL kTestUrl4("http://page.net");
const GURL kFileUrl("file:///foo");
const GURL kTestUrlWithFragment("http://example.com#frag");
const GURL kTestUrl2WithFragment("http://other.page.com#frag");
const GURL kTestUrl2WithFragment2("http://other.page.com#frag2");
const ClientId kTestClientId1(kTestClientNamespace, "1234");
const ClientId kTestClientId2(kTestClientNamespace, "5678");
const ClientId kTestClientId3(kTestClientNamespace, "42");
const ClientId kTestUserRequestedClientId(kUserRequestedNamespace, "714");
const int64_t kTestFileSize = 876543LL;
const base::string16 kTestTitle = base::UTF8ToUTF16("a title");

bool URLSpecContains(std::string contains_value, const GURL& url) {
  std::string spec = url.spec();
  return spec.find(contains_value) != std::string::npos;
}

}  // namespace

class OfflinePageModelImplTest
    : public testing::Test,
      public OfflinePageModel::Observer,
      public OfflinePageTestArchiver::Observer,
      public base::SupportsWeakPtr<OfflinePageModelImplTest> {
 public:
  OfflinePageModelImplTest();
  ~OfflinePageModelImplTest() override;

  void SetUp() override;
  void TearDown() override;

  // OfflinePageModel::Observer implementation.
  void OfflinePageModelLoaded(OfflinePageModel* model) override;
  void OfflinePageModelChanged(OfflinePageModel* model) override;
  void OfflinePageDeleted(int64_t offline_id,
                          const ClientId& client_id) override;

  // OfflinePageTestArchiver::Observer implementation.
  void SetLastPathCreatedByArchiver(const base::FilePath& file_path) override;

  // OfflinePageModel callbacks.
  void OnSavePageDone(SavePageResult result, int64_t offline_id);
  void OnDeletePageDone(DeletePageResult result);
  void OnCheckPagesExistOfflineDone(const CheckPagesExistOfflineResult& result);
  void OnGetOfflineIdsForClientIdDone(MultipleOfflineIdResult* storage,
                                      const MultipleOfflineIdResult& result);
  void OnGetSingleOfflinePageItemResult(
      std::unique_ptr<OfflinePageItem>* storage,
      const OfflinePageItem* result);
  void OnGetMultipleOfflinePageItemsResult(
      MultipleOfflinePageItemResult* storage,
      const MultipleOfflinePageItemResult& result);
  void OnPagesExpired(bool result);

  // OfflinePageMetadataStore callbacks.
  void OnStoreUpdateDone(bool /* success */);

  std::unique_ptr<OfflinePageTestArchiver> BuildArchiver(
      const GURL& url,
      OfflinePageArchiver::ArchiverResult result);
  std::unique_ptr<OfflinePageMetadataStore> BuildStore();
  std::unique_ptr<OfflinePageModelImpl> BuildModel(
      std::unique_ptr<OfflinePageMetadataStore> store);
  void ResetModel();

  // Utility methods.
  // Runs until all of the tasks that are not delayed are gone from the task
  // queue.
  void PumpLoop();
  // Fast-forwards virtual time by |delta|, causing tasks with a remaining
  // delay less than or equal to |delta| to be executed.
  void FastForwardBy(base::TimeDelta delta);
  void ResetResults();

  OfflinePageTestStore* GetStore();

  MultipleOfflinePageItemResult GetAllPages();
  MultipleOfflinePageItemResult GetPagesByClientIds(
      const std::vector<ClientId>& client_ids);
  void DeletePagesByClientIds(const std::vector<ClientId>& client_ids);

  // Saves the page without waiting for it to finish.
  void SavePageWithArchiverAsync(
      const GURL& url,
      const ClientId& client_id,
      const GURL& original_url,
      std::unique_ptr<OfflinePageArchiver> archiver);

  // All 3 methods below will wait for the save to finish.
  void SavePageWithArchiver(
      const GURL& url,
      const ClientId& client_id,
      std::unique_ptr<OfflinePageArchiver> archiver);
  void SavePageWithArchiverResult(const GURL& url,
                                  const ClientId& client_id,
                                  OfflinePageArchiver::ArchiverResult result);
  // Returns the offline ID of the saved page.
  std::pair<SavePageResult, int64_t> SavePage(const GURL& url,
                                              const ClientId& client_id);

  void DeletePage(int64_t offline_id, const DeletePageCallback& callback) {
    std::vector<int64_t> offline_ids;
    offline_ids.push_back(offline_id);
    model()->DeletePagesByOfflineId(offline_ids, callback);
  }

  bool HasPages(std::string name_space);

  CheckPagesExistOfflineResult CheckPagesExistOffline(std::set<GURL>);

  MultipleOfflineIdResult GetOfflineIdsForClientId(const ClientId& client_id);

  std::unique_ptr<OfflinePageItem> GetPageByOfflineId(int64_t offline_id);

  MultipleOfflinePageItemResult GetPagesByFinalURL(const GURL& url);
  MultipleOfflinePageItemResult GetPagesByAllURLS(const GURL& url);

  OfflinePageModelImpl* model() { return model_.get(); }

  int64_t last_save_offline_id() const { return last_save_offline_id_; }

  SavePageResult last_save_result() const { return last_save_result_; }

  DeletePageResult last_delete_result() const { return last_delete_result_; }

  int64_t last_deleted_offline_id() const { return last_deleted_offline_id_; }

  ClientId last_deleted_client_id() const { return last_deleted_client_id_; }

  const base::FilePath& last_archiver_path() { return last_archiver_path_; }

  int last_cleared_pages_count() const { return last_cleared_pages_count_; }

  DeletePageResult last_clear_page_result() const {
    return last_clear_page_result_;
  }

  bool last_expire_page_result() const { return last_expire_page_result_; }

  const base::HistogramTester& histograms() const { return histogram_tester_; }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<OfflinePageModelImpl> model_;
  SavePageResult last_save_result_;
  int64_t last_save_offline_id_;
  DeletePageResult last_delete_result_;
  base::FilePath last_archiver_path_;
  int64_t last_deleted_offline_id_;
  ClientId last_deleted_client_id_;
  CheckPagesExistOfflineResult last_pages_exist_result_;
  int last_cleared_pages_count_;
  DeletePageResult last_clear_page_result_;
  bool last_expire_page_result_;

  base::HistogramTester histogram_tester_;
};

OfflinePageModelImplTest::OfflinePageModelImplTest()
    : task_runner_(new base::TestMockTimeTaskRunner),
      task_runner_handle_(task_runner_),
      last_save_result_(SavePageResult::CANCELLED),
      last_save_offline_id_(-1),
      last_delete_result_(DeletePageResult::CANCELLED),
      last_deleted_offline_id_(-1) {}

OfflinePageModelImplTest::~OfflinePageModelImplTest() {}

void OfflinePageModelImplTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  model_ = BuildModel(BuildStore());
  model_->AddObserver(this);
  PumpLoop();
}

void OfflinePageModelImplTest::TearDown() {
  model_->RemoveObserver(this);
  model_.reset();
  PumpLoop();
}

void OfflinePageModelImplTest::OfflinePageModelLoaded(OfflinePageModel* model) {
  ASSERT_EQ(model_.get(), model);
}

void OfflinePageModelImplTest::OfflinePageModelChanged(
    OfflinePageModel* model) {
  ASSERT_EQ(model_.get(), model);
}

void OfflinePageModelImplTest::OfflinePageDeleted(int64_t offline_id,
                                                  const ClientId& client_id) {
  last_deleted_offline_id_ = offline_id;
  last_deleted_client_id_ = client_id;
}

void OfflinePageModelImplTest::SetLastPathCreatedByArchiver(
    const base::FilePath& file_path) {
  last_archiver_path_ = file_path;
}

void OfflinePageModelImplTest::OnSavePageDone(SavePageResult result,
                                              int64_t offline_id) {
  last_save_result_ = result;
  last_save_offline_id_ = offline_id;
}

void OfflinePageModelImplTest::OnDeletePageDone(DeletePageResult result) {
  last_delete_result_ = result;
}

void OfflinePageModelImplTest::OnCheckPagesExistOfflineDone(
    const CheckPagesExistOfflineResult& result) {
  last_pages_exist_result_ = result;
}

void OfflinePageModelImplTest::OnStoreUpdateDone(bool /* success - ignored */) {
}

std::unique_ptr<OfflinePageTestArchiver>
OfflinePageModelImplTest::BuildArchiver(
    const GURL& url,
    OfflinePageArchiver::ArchiverResult result) {
  return std::unique_ptr<OfflinePageTestArchiver>(
      new OfflinePageTestArchiver(this, url, result, kTestTitle, kTestFileSize,
                                  base::ThreadTaskRunnerHandle::Get()));
}

std::unique_ptr<OfflinePageMetadataStore>
OfflinePageModelImplTest::BuildStore() {
  return std::unique_ptr<OfflinePageMetadataStore>(
      new OfflinePageTestStore(base::ThreadTaskRunnerHandle::Get()));
}

std::unique_ptr<OfflinePageModelImpl> OfflinePageModelImplTest::BuildModel(
    std::unique_ptr<OfflinePageMetadataStore> store) {
  return std::unique_ptr<OfflinePageModelImpl>(
      new OfflinePageModelImpl(std::move(store), temp_dir_.GetPath(),
                               base::ThreadTaskRunnerHandle::Get()));
}

void OfflinePageModelImplTest::ResetModel() {
  model_->RemoveObserver(this);
  OfflinePageTestStore* old_store = GetStore();
  std::unique_ptr<OfflinePageMetadataStore> new_store(
      new OfflinePageTestStore(*old_store));
  model_ = BuildModel(std::move(new_store));
  model_->AddObserver(this);
  PumpLoop();
}

void OfflinePageModelImplTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void OfflinePageModelImplTest::FastForwardBy(base::TimeDelta delta) {
  task_runner_->FastForwardBy(delta);
}

void OfflinePageModelImplTest::ResetResults() {
  last_save_result_ = SavePageResult::CANCELLED;
  last_delete_result_ = DeletePageResult::CANCELLED;
  last_archiver_path_.clear();
  last_pages_exist_result_.clear();
  last_cleared_pages_count_ = 0;
}

OfflinePageTestStore* OfflinePageModelImplTest::GetStore() {
  return static_cast<OfflinePageTestStore*>(model()->GetStoreForTesting());
}

void OfflinePageModelImplTest::SavePageWithArchiverAsync(
    const GURL& url,
    const ClientId& client_id,
    const GURL& original_url,
    std::unique_ptr<OfflinePageArchiver> archiver) {
  OfflinePageModel::SavePageParams save_page_params;
  save_page_params.url = url;
  save_page_params.client_id = client_id;
  save_page_params.original_url = original_url;
  model()->SavePage(
      save_page_params,
      std::move(archiver),
      base::Bind(&OfflinePageModelImplTest::OnSavePageDone, AsWeakPtr()));
}

void OfflinePageModelImplTest::SavePageWithArchiver(
    const GURL& url,
    const ClientId& client_id,
    std::unique_ptr<OfflinePageArchiver> archiver) {
  SavePageWithArchiverAsync(url, client_id, GURL(), std::move(archiver));
  PumpLoop();
}

void OfflinePageModelImplTest::SavePageWithArchiverResult(
    const GURL& url,
    const ClientId& client_id,
    OfflinePageArchiver::ArchiverResult result) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(url, result));
  SavePageWithArchiverAsync(url, client_id, GURL(), std::move(archiver));
  PumpLoop();
}

std::pair<SavePageResult, int64_t>
OfflinePageModelImplTest::SavePage(const GURL& url, const ClientId& client_id) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      url, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  SavePageWithArchiverAsync(url, client_id, GURL(), std::move(archiver));
  PumpLoop();
  return std::make_pair(last_save_result_, last_save_offline_id_);
}

MultipleOfflinePageItemResult OfflinePageModelImplTest::GetAllPages() {
  MultipleOfflinePageItemResult result;
  model()->GetAllPages(
      base::Bind(&OfflinePageModelImplTest::OnGetMultipleOfflinePageItemsResult,
                 AsWeakPtr(), base::Unretained(&result)));
  PumpLoop();
  return result;
}

MultipleOfflinePageItemResult OfflinePageModelImplTest::GetPagesByClientIds(
    const std::vector<ClientId>& client_ids) {
  MultipleOfflinePageItemResult result;
  model()->GetPagesByClientIds(
      client_ids,
      base::Bind(&OfflinePageModelImplTest::OnGetMultipleOfflinePageItemsResult,
                 AsWeakPtr(), base::Unretained(&result)));
  PumpLoop();
  return result;
}

void OfflinePageModelImplTest::DeletePagesByClientIds(
    const std::vector<ClientId>& client_ids) {
  model()->DeletePagesByClientIds(
      client_ids,
      base::Bind(&OfflinePageModelImplTest::OnDeletePageDone, AsWeakPtr()));
  PumpLoop();
}

CheckPagesExistOfflineResult OfflinePageModelImplTest::CheckPagesExistOffline(
    std::set<GURL> pages) {
  model()->CheckPagesExistOffline(
      pages, base::Bind(&OfflinePageModelImplTest::OnCheckPagesExistOfflineDone,
                        AsWeakPtr()));
  PumpLoop();
  return last_pages_exist_result_;
}

MultipleOfflineIdResult OfflinePageModelImplTest::GetOfflineIdsForClientId(
    const ClientId& client_id) {
  MultipleOfflineIdResult result;
  model()->GetOfflineIdsForClientId(
      client_id,
      base::Bind(&OfflinePageModelImplTest::OnGetOfflineIdsForClientIdDone,
                 AsWeakPtr(), base::Unretained(&result)));
  PumpLoop();
  return result;
}

void OfflinePageModelImplTest::OnGetOfflineIdsForClientIdDone(
    MultipleOfflineIdResult* storage,
    const MultipleOfflineIdResult& result) {
  *storage = result;
}

std::unique_ptr<OfflinePageItem> OfflinePageModelImplTest::GetPageByOfflineId(
    int64_t offline_id) {
  std::unique_ptr<OfflinePageItem> result = nullptr;
  model()->GetPageByOfflineId(
      offline_id,
      base::Bind(&OfflinePageModelImplTest::OnGetSingleOfflinePageItemResult,
                 AsWeakPtr(), base::Unretained(&result)));
  PumpLoop();
  return result;
}

void OfflinePageModelImplTest::OnGetSingleOfflinePageItemResult(
    std::unique_ptr<OfflinePageItem>* storage,
    const OfflinePageItem* result) {
  if (result == nullptr) {
    storage->reset(nullptr);
    return;
  }

  *storage = base::MakeUnique<OfflinePageItem>(*result);
}

void OfflinePageModelImplTest::OnGetMultipleOfflinePageItemsResult(
    MultipleOfflinePageItemResult* storage,
    const MultipleOfflinePageItemResult& result) {
  *storage = result;
}

void OfflinePageModelImplTest::OnPagesExpired(bool result) {
  last_expire_page_result_ = result;
}

MultipleOfflinePageItemResult OfflinePageModelImplTest::GetPagesByFinalURL(
    const GURL& url) {
  MultipleOfflinePageItemResult result;
  model()->GetPagesByURL(
      url,
      OfflinePageModel::URLSearchMode::SEARCH_BY_FINAL_URL_ONLY,
      base::Bind(&OfflinePageModelImplTest::OnGetMultipleOfflinePageItemsResult,
                 AsWeakPtr(), base::Unretained(&result)));
  PumpLoop();
  return result;
}

MultipleOfflinePageItemResult OfflinePageModelImplTest::GetPagesByAllURLS(
    const GURL& url) {
  MultipleOfflinePageItemResult result;
  model()->GetPagesByURL(
      url,
      OfflinePageModel::URLSearchMode::SEARCH_BY_ALL_URLS,
      base::Bind(&OfflinePageModelImplTest::OnGetMultipleOfflinePageItemsResult,
                 AsWeakPtr(), base::Unretained(&result)));
  PumpLoop();
  return result;
}

bool OfflinePageModelImplTest::HasPages(std::string name_space) {
  MultipleOfflinePageItemResult all_pages = GetAllPages();
  for (const auto& page : all_pages) {
    if (page.client_id.name_space == name_space)
      return true;
  }

  return false;
}

TEST_F(OfflinePageModelImplTest, SavePageSuccessful) {
  EXPECT_FALSE(HasPages(kTestClientNamespace));

   std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestUrl, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  SavePageWithArchiverAsync(
      kTestUrl, kTestClientId1, kTestUrl2, std::move(archiver));
  PumpLoop();
  EXPECT_TRUE(HasPages(kTestClientNamespace));

  OfflinePageTestStore* store = GetStore();
  EXPECT_EQ(kTestUrl, store->last_saved_page().url);
  EXPECT_EQ(kTestClientId1.id, store->last_saved_page().client_id.id);
  EXPECT_EQ(kTestClientId1.name_space,
            store->last_saved_page().client_id.name_space);
  // Save last_archiver_path since it will be referred to later.
  base::FilePath archiver_path = last_archiver_path();
  EXPECT_EQ(archiver_path, store->last_saved_page().file_path);
  EXPECT_EQ(kTestFileSize, store->last_saved_page().file_size);
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  ResetResults();

  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();

  ASSERT_EQ(1UL, offline_pages.size());
  EXPECT_EQ(kTestUrl, offline_pages[0].url);
  EXPECT_EQ(kTestClientId1.id, offline_pages[0].client_id.id);
  EXPECT_EQ(kTestClientId1.name_space, offline_pages[0].client_id.name_space);
  EXPECT_EQ(archiver_path, offline_pages[0].file_path);
  EXPECT_EQ(kTestFileSize, offline_pages[0].file_size);
  EXPECT_EQ(0, offline_pages[0].access_count);
  EXPECT_EQ(0, offline_pages[0].flags);
  EXPECT_EQ(kTestTitle, offline_pages[0].title);
  EXPECT_EQ(kTestUrl2, offline_pages[0].original_url);
}

TEST_F(OfflinePageModelImplTest, SavePageOfflineArchiverCancelled) {
  SavePageWithArchiverResult(
      kTestUrl, kTestClientId1,
      OfflinePageArchiver::ArchiverResult::ERROR_CANCELED);
  EXPECT_EQ(SavePageResult::CANCELLED, last_save_result());
}

TEST_F(OfflinePageModelImplTest, SavePageOfflineArchiverDeviceFull) {
  SavePageWithArchiverResult(
      kTestUrl, kTestClientId1,
      OfflinePageArchiver::ArchiverResult::ERROR_DEVICE_FULL);
  EXPECT_EQ(SavePageResult::DEVICE_FULL, last_save_result());
}

TEST_F(OfflinePageModelImplTest, SavePageOfflineArchiverContentUnavailable) {
  SavePageWithArchiverResult(
      kTestUrl, kTestClientId1,
      OfflinePageArchiver::ArchiverResult::ERROR_CONTENT_UNAVAILABLE);
  EXPECT_EQ(SavePageResult::CONTENT_UNAVAILABLE, last_save_result());
}

TEST_F(OfflinePageModelImplTest, SavePageOfflineCreationFailed) {
  SavePageWithArchiverResult(
      kTestUrl, kTestClientId1,
      OfflinePageArchiver::ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED);
  EXPECT_EQ(SavePageResult::ARCHIVE_CREATION_FAILED, last_save_result());
}

TEST_F(OfflinePageModelImplTest, SavePageOfflineArchiverReturnedWrongUrl) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(GURL("http://other.random.url.com"),
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  SavePageWithArchiver(kTestUrl, kTestClientId1, std::move(archiver));
  EXPECT_EQ(SavePageResult::ARCHIVE_CREATION_FAILED, last_save_result());
}

TEST_F(OfflinePageModelImplTest, SavePageOfflineCreationStoreWriteFailure) {
  GetStore()->set_test_scenario(
      OfflinePageTestStore::TestScenario::WRITE_FAILED);
  SavePageWithArchiverResult(
      kTestUrl, kTestClientId1,
      OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED);
  EXPECT_EQ(SavePageResult::STORE_FAILURE, last_save_result());
}

TEST_F(OfflinePageModelImplTest, SavePageLocalFileFailed) {
  // Don't create archiver since it will not be needed for pages that are not
  // going to be saved.
  SavePageWithArchiver(
      kFileUrl, kTestClientId1, std::unique_ptr<OfflinePageTestArchiver>());
  EXPECT_EQ(SavePageResult::SKIPPED, last_save_result());
}

TEST_F(OfflinePageModelImplTest, SavePageOfflineArchiverTwoPages) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestUrl, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  // archiver_ptr will be valid until after first PumpLoop() call after
  // CompleteCreateArchive() is called.
  OfflinePageTestArchiver* archiver_ptr = archiver.get();
  archiver_ptr->set_delayed(true);
  SavePageWithArchiverAsync(
      kTestUrl, kTestClientId1, GURL(), std::move(archiver));
  EXPECT_TRUE(archiver_ptr->create_archive_called());

  // Request to save another page.
  SavePage(kTestUrl2, kTestClientId2);

  OfflinePageTestStore* store = GetStore();

  EXPECT_EQ(kTestUrl2, store->last_saved_page().url);
  EXPECT_EQ(kTestClientId2, store->last_saved_page().client_id);
  base::FilePath archiver_path2 = last_archiver_path();
  EXPECT_EQ(archiver_path2, store->last_saved_page().file_path);
  EXPECT_EQ(kTestFileSize, store->last_saved_page().file_size);
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());

  ResetResults();

  archiver_ptr->CompleteCreateArchive();
  // After this pump loop archiver_ptr is invalid.
  PumpLoop();

  EXPECT_EQ(kTestUrl, store->last_saved_page().url);
  EXPECT_EQ(kTestClientId1, store->last_saved_page().client_id);
  base::FilePath archiver_path = last_archiver_path();
  EXPECT_EQ(archiver_path, store->last_saved_page().file_path);
  EXPECT_EQ(kTestFileSize, store->last_saved_page().file_size);
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());

  ResetResults();

  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();

  EXPECT_EQ(2UL, offline_pages.size());
  // Offline IDs are random, so the order of the pages is also random
  // So load in the right page for the validation below.
  const OfflinePageItem* page1 = nullptr;
  const OfflinePageItem* page2 = nullptr;
  if (offline_pages[0].client_id == kTestClientId1) {
    page1 = &offline_pages[0];
    page2 = &offline_pages[1];
  } else {
    page1 = &offline_pages[1];
    page2 = &offline_pages[0];
  }

  EXPECT_EQ(kTestUrl, page1->url);
  EXPECT_EQ(kTestClientId1, page1->client_id);
  EXPECT_EQ(archiver_path, page1->file_path);
  EXPECT_EQ(kTestFileSize, page1->file_size);
  EXPECT_EQ(0, page1->access_count);
  EXPECT_EQ(0, page1->flags);
  EXPECT_EQ(kTestUrl2, page2->url);
  EXPECT_EQ(kTestClientId2, page2->client_id);
  EXPECT_EQ(archiver_path2, page2->file_path);
  EXPECT_EQ(kTestFileSize, page2->file_size);
  EXPECT_EQ(0, page2->access_count);
  EXPECT_EQ(0, page2->flags);
}

TEST_F(OfflinePageModelImplTest, MarkPageAccessed) {
  SavePage(kTestUrl, kTestClientId1);

  // This will increase access_count by one.
  model()->MarkPageAccessed(last_save_offline_id());
  PumpLoop();

  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();

  EXPECT_EQ(1UL, offline_pages.size());
  EXPECT_EQ(kTestUrl, offline_pages[0].url);
  EXPECT_EQ(kTestClientId1, offline_pages[0].client_id);
  EXPECT_EQ(kTestFileSize, offline_pages[0].file_size);
  EXPECT_EQ(1, offline_pages[0].access_count);
}

TEST_F(OfflinePageModelImplTest, GetAllPagesStoreEmpty) {
  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();

  EXPECT_EQ(0UL, offline_pages.size());
}

TEST_F(OfflinePageModelImplTest, DeletePageSuccessful) {
  OfflinePageTestStore* store = GetStore();

  // Save one page.
  SavePage(kTestUrl, kTestClientId1);
  int64_t offline1 = last_save_offline_id();
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  EXPECT_EQ(1u, store->GetAllPages().size());

  ResetResults();

  // Save another page.
  SavePage(kTestUrl2, kTestClientId2);
  int64_t offline2 = last_save_offline_id();
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  EXPECT_EQ(2u, store->GetAllPages().size());

  ResetResults();

  // Delete one page.
  DeletePage(offline1, base::Bind(&OfflinePageModelImplTest::OnDeletePageDone,
                                  AsWeakPtr()));

  PumpLoop();

  EXPECT_EQ(last_deleted_offline_id(), offline1);
  EXPECT_EQ(last_deleted_client_id(), kTestClientId1);
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  ASSERT_EQ(1u, store->GetAllPages().size());
  EXPECT_EQ(kTestUrl2, store->GetAllPages()[0].url);

  // Delete another page.
  DeletePage(offline2, base::Bind(&OfflinePageModelImplTest::OnDeletePageDone,
                                  AsWeakPtr()));

  ResetResults();

  PumpLoop();

  EXPECT_EQ(last_deleted_offline_id(), offline2);
  EXPECT_EQ(last_deleted_client_id(), kTestClientId2);
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  EXPECT_EQ(0u, store->GetAllPages().size());
}

TEST_F(OfflinePageModelImplTest, DeleteCachedPageByPredicateUserRequested) {
  OfflinePageTestStore* store = GetStore();

  // Save one page.
  SavePage(kTestUrl, kTestClientId1);
  int64_t offline1 = last_save_offline_id();
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  EXPECT_EQ(1u, store->GetAllPages().size());

  ResetResults();

  // Save an user-requested page in same domain.
  SavePage(kTestUrl, kTestUserRequestedClientId);
  int64_t offline2 = last_save_offline_id();
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  EXPECT_EQ(2u, store->GetAllPages().size());

  ResetResults();

  // Delete the second page.
  model()->DeleteCachedPagesByURLPredicate(
      base::Bind(&URLSpecContains, "example.com"),
      base::Bind(&OfflinePageModelImplTest::OnDeletePageDone, AsWeakPtr()));

  PumpLoop();

  EXPECT_EQ(last_deleted_offline_id(), offline1);
  EXPECT_EQ(last_deleted_client_id(), kTestClientId1);
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  ASSERT_EQ(1u, store->GetAllPages().size());
  EXPECT_EQ(kTestUrl, store->GetAllPages()[0].url);
  EXPECT_EQ(offline2, store->GetAllPages()[0].offline_id);
}

TEST_F(OfflinePageModelImplTest, DeleteCachedPageByPredicate) {
  OfflinePageTestStore* store = GetStore();

  // Save one page.
  SavePage(kTestUrl, kTestClientId1);
  int64_t offline1 = last_save_offline_id();
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  EXPECT_EQ(1u, store->GetAllPages().size());

  ResetResults();

  // Save another page.
  SavePage(kTestUrl2, kTestClientId2);
  int64_t offline2 = last_save_offline_id();
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  EXPECT_EQ(2u, store->GetAllPages().size());

  ResetResults();

  // Delete the second page.
  model()->DeleteCachedPagesByURLPredicate(
      base::Bind(&URLSpecContains, "page.com"),
      base::Bind(&OfflinePageModelImplTest::OnDeletePageDone, AsWeakPtr()));

  PumpLoop();

  EXPECT_EQ(last_deleted_offline_id(), offline2);
  EXPECT_EQ(last_deleted_client_id(), kTestClientId2);
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  ASSERT_EQ(1u, store->GetAllPages().size());
  EXPECT_EQ(kTestUrl, store->GetAllPages()[0].url);

  ResetResults();

  // Delete the first page.
  model()->DeleteCachedPagesByURLPredicate(
      base::Bind(&URLSpecContains, "example.com"),
      base::Bind(&OfflinePageModelImplTest::OnDeletePageDone, AsWeakPtr()));

  PumpLoop();

  EXPECT_EQ(last_deleted_offline_id(), offline1);
  EXPECT_EQ(last_deleted_client_id(), kTestClientId1);
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  EXPECT_EQ(0u, store->GetAllPages().size());
}

TEST_F(OfflinePageModelImplTest, DeletePageNotFound) {
  DeletePage(1234LL, base::Bind(&OfflinePageModelImplTest::OnDeletePageDone,
                                AsWeakPtr()));
  PumpLoop();

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());

  ResetResults();

  model()->DeleteCachedPagesByURLPredicate(
      base::Bind(&URLSpecContains, "page.com"),
      base::Bind(&OfflinePageModelImplTest::OnDeletePageDone, AsWeakPtr()));
  PumpLoop();

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
}

TEST_F(OfflinePageModelImplTest, DeletePageStoreFailureOnRemove) {
  // Save a page.
  SavePage(kTestUrl, kTestClientId1);
  int64_t offline_id = last_save_offline_id();
  ResetResults();

  // Try to delete this page.
  GetStore()->set_test_scenario(
      OfflinePageTestStore::TestScenario::REMOVE_FAILED);
  DeletePage(offline_id, base::Bind(&OfflinePageModelImplTest::OnDeletePageDone,
                                    AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(DeletePageResult::STORE_FAILURE, last_delete_result());
}

TEST_F(OfflinePageModelImplTest, DetectThatOfflineCopyIsMissing) {
  // Save a page.
  SavePage(kTestUrl, kTestClientId1);
  int64_t offline_id = last_save_offline_id();

  ResetResults();

  std::unique_ptr<OfflinePageItem> page = GetPageByOfflineId(offline_id);

  // Delete the offline copy of the page.
  base::DeleteFile(page->file_path, false);

  // Resetting the model will cause a consistency check.
  ResetModel();

  PumpLoop();

  // Check if the page has been expired.
  EXPECT_EQ(0UL, GetAllPages().size());
}

TEST_F(OfflinePageModelImplTest, DetectThatOfflineCopyIsMissingAfterLoad) {
  // Save a page.
  SavePage(kTestUrl, kTestClientId1);
  PumpLoop();
  int64_t offline_id = last_save_offline_id();

  ResetResults();

  std::unique_ptr<OfflinePageItem> page = GetPageByOfflineId(offline_id);
  // Delete the offline copy of the page and check the metadata.
  base::DeleteFile(page->file_path, false);
  // Reseting the model should trigger the metadata consistency check as well.
  ResetModel();
  PumpLoop();

  // Check if the page has been expired.
  EXPECT_EQ(0UL, GetAllPages().size());
}

TEST_F(OfflinePageModelImplTest, DetectThatHeadlessPageIsDeleted) {
  // Save a page.
  SavePage(kTestUrl, kTestClientId1);
  PumpLoop();
  int64_t offline_id = last_save_offline_id();

  ResetResults();
  std::unique_ptr<OfflinePageItem> page = GetPageByOfflineId(offline_id);
  base::FilePath path = page->file_path;
  EXPECT_TRUE(base::PathExists(path));
  GetStore()->ClearAllPages();

  EXPECT_TRUE(base::PathExists(path));
  // Since we've manually changed the store, we have to reload the model to
  // actually refresh the in-memory copy in model. Otherwise GetAllPages() would
  // still have the page we saved above.
  ResetModel();
  PumpLoop();

  EXPECT_EQ(0UL, GetAllPages().size());
  EXPECT_FALSE(base::PathExists(path));
}

TEST_F(OfflinePageModelImplTest, DeleteMultiplePages) {
  OfflinePageTestStore* store = GetStore();

  // Save 3 pages.
  SavePage(kTestUrl, kTestClientId1);
  int64_t offline1 = last_save_offline_id();

  SavePage(kTestUrl2, kTestClientId2);
  int64_t offline2 = last_save_offline_id();

  SavePage(kTestUrl3, kTestClientId3);
  PumpLoop();

  EXPECT_EQ(3u, store->GetAllPages().size());

  // Delete multiple pages.
  std::vector<int64_t> ids_to_delete;
  ids_to_delete.push_back(offline2);
  ids_to_delete.push_back(offline1);
  ids_to_delete.push_back(23434LL);  // Non-existent ID.
  model()->DeletePagesByOfflineId(
      ids_to_delete,
      base::Bind(&OfflinePageModelImplTest::OnDeletePageDone, AsWeakPtr()));
  PumpLoop();

  // Success is expected if at least one page is deleted successfully.
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  EXPECT_EQ(1u, store->GetAllPages().size());
}

TEST_F(OfflinePageModelImplTest, GetPageByOfflineId) {
  SavePage(kTestUrl, kTestClientId1);
  int64_t offline1 = last_save_offline_id();
  SavePage(kTestUrl2, kTestClientId2);
  int64_t offline2 = last_save_offline_id();

  std::unique_ptr<OfflinePageItem> page = GetPageByOfflineId(offline1);
  ASSERT_TRUE(page);
  EXPECT_EQ(kTestUrl, page->url);
  EXPECT_EQ(kTestClientId1, page->client_id);
  EXPECT_EQ(kTestFileSize, page->file_size);

  page = GetPageByOfflineId(offline2);
  ASSERT_TRUE(page);
  EXPECT_EQ(kTestUrl2, page->url);
  EXPECT_EQ(kTestClientId2, page->client_id);
  EXPECT_EQ(kTestFileSize, page->file_size);

  page = GetPageByOfflineId(-42);
  EXPECT_FALSE(page);
}

TEST_F(OfflinePageModelImplTest, GetPagesByFinalURL) {
  SavePage(kTestUrl, kTestClientId1);
  SavePage(kTestUrl2, kTestClientId2);

  MultipleOfflinePageItemResult pages = GetPagesByFinalURL(kTestUrl2);
  EXPECT_EQ(1U, pages.size());
  EXPECT_EQ(kTestUrl2, pages[0].url);
  EXPECT_EQ(kTestClientId2, pages[0].client_id);

  pages = GetPagesByFinalURL(kTestUrl);
  EXPECT_EQ(1U, pages.size());
  EXPECT_EQ(kTestUrl, pages[0].url);
  EXPECT_EQ(kTestClientId1, pages[0].client_id);

  pages = GetPagesByFinalURL(GURL("http://foo"));
  EXPECT_EQ(0U, pages.size());
}

TEST_F(OfflinePageModelImplTest, GetPagesByFinalURLWithFragmentStripped) {
  SavePage(kTestUrl, kTestClientId1);
  SavePage(kTestUrl2WithFragment, kTestClientId2);

  MultipleOfflinePageItemResult pages =
      GetPagesByFinalURL(kTestUrlWithFragment);
  EXPECT_EQ(1U, pages.size());
  EXPECT_EQ(kTestUrl, pages[0].url);
  EXPECT_EQ(kTestClientId1, pages[0].client_id);

  pages = GetPagesByFinalURL(kTestUrl2);
  EXPECT_EQ(1U, pages.size());
  EXPECT_EQ(kTestUrl2WithFragment, pages[0].url);
  EXPECT_EQ(kTestClientId2, pages[0].client_id);

  pages = GetPagesByFinalURL(kTestUrl2WithFragment2);
  EXPECT_EQ(1U, pages.size());
  EXPECT_EQ(kTestUrl2WithFragment, pages[0].url);
  EXPECT_EQ(kTestClientId2, pages[0].client_id);
}

TEST_F(OfflinePageModelImplTest, GetPagesByAllURLS) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestUrl, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  SavePageWithArchiverAsync(
      kTestUrl, kTestClientId1, kTestUrl2, std::move(archiver));
  PumpLoop();

  SavePage(kTestUrl2, kTestClientId2);

  MultipleOfflinePageItemResult pages = GetPagesByAllURLS(kTestUrl2);
  ASSERT_EQ(2U, pages.size());
  // Validates the items regardless their order.
  int i = -1;
  if (pages[0].url == kTestUrl2)
    i = 0;
  else if (pages[1].url == kTestUrl2)
    i = 1;
  ASSERT_NE(-1, i);
  EXPECT_EQ(kTestUrl2, pages[i].url);
  EXPECT_EQ(kTestClientId2, pages[i].client_id);
  EXPECT_EQ(GURL(), pages[i].original_url);
  EXPECT_EQ(kTestUrl, pages[1 - i].url);
  EXPECT_EQ(kTestClientId1, pages[1 - i].client_id);
  EXPECT_EQ(kTestUrl2, pages[1 - i].original_url);
}

TEST_F(OfflinePageModelImplTest, CheckPagesExistOffline) {
  SavePage(kTestUrl, kTestClientId1);
  SavePage(kTestUrl2, kTestClientId2);

  // TODO(dewittj): Remove the "Last N" restriction in favor of a better query
  // interface.  See https://crbug.com/622763 for information.
  const ClientId last_n_client_id(kLastNNamespace, "1234");
  SavePage(kTestUrl3, last_n_client_id);

  std::set<GURL> input;
  input.insert(kTestUrl);
  input.insert(kTestUrl2);
  input.insert(kTestUrl3);
  input.insert(kTestUrl4);

  CheckPagesExistOfflineResult existing_pages = CheckPagesExistOffline(input);
  EXPECT_EQ(2U, existing_pages.size());
  EXPECT_NE(existing_pages.end(), existing_pages.find(kTestUrl));
  EXPECT_NE(existing_pages.end(), existing_pages.find(kTestUrl2));
  EXPECT_EQ(existing_pages.end(), existing_pages.find(kTestUrl3));
  EXPECT_EQ(existing_pages.end(), existing_pages.find(kTestUrl4));
}

TEST_F(OfflinePageModelImplTest, CanSaveURL) {
  EXPECT_TRUE(OfflinePageModel::CanSaveURL(GURL("http://foo")));
  EXPECT_TRUE(OfflinePageModel::CanSaveURL(GURL("https://foo")));
  EXPECT_FALSE(OfflinePageModel::CanSaveURL(GURL("file:///foo")));
  EXPECT_FALSE(OfflinePageModel::CanSaveURL(GURL("data:image/png;base64,ab")));
  EXPECT_FALSE(OfflinePageModel::CanSaveURL(GURL("chrome://version")));
  EXPECT_FALSE(OfflinePageModel::CanSaveURL(GURL("chrome-native://newtab/")));
  EXPECT_FALSE(OfflinePageModel::CanSaveURL(GURL("/invalid/url.mhtml")));
}

TEST_F(OfflinePageModelImplTest, SaveRetrieveMultipleClientIds) {
  EXPECT_FALSE(HasPages(kTestClientNamespace));
  SavePage(kTestUrl, kTestClientId1);
  int64_t offline1 = last_save_offline_id();
  EXPECT_TRUE(HasPages(kTestClientNamespace));

  SavePage(kTestUrl2, kTestClientId1);
  int64_t offline2 = last_save_offline_id();

  EXPECT_NE(offline1, offline2);

  const std::vector<int64_t> ids = GetOfflineIdsForClientId(kTestClientId1);

  EXPECT_EQ(2UL, ids.size());

  std::set<int64_t> id_set;
  for (size_t i = 0; i < ids.size(); i++) {
    id_set.insert(ids[i]);
  }

  EXPECT_TRUE(id_set.find(offline1) != id_set.end());
  EXPECT_TRUE(id_set.find(offline2) != id_set.end());
}

TEST_F(OfflinePageModelImplTest, SaveMultiplePagesWithSameURLBySameClientId) {
  EXPECT_FALSE(HasPages(kTestClientNamespace));
  SavePage(kTestUrl, kTestClientId1);
  int64_t offline1 = last_save_offline_id();
  EXPECT_TRUE(HasPages(kTestClientNamespace));

  SavePage(kTestUrl, kTestClientId1);
  int64_t offline2 = last_save_offline_id();

  EXPECT_NE(offline1, offline2);

  const std::vector<int64_t> ids = GetOfflineIdsForClientId(kTestClientId1);

  EXPECT_EQ(1UL, ids.size());

  std::set<int64_t> id_set;
  for (size_t i = 0; i < ids.size(); i++) {
    id_set.insert(ids[i]);
  }

  EXPECT_TRUE(id_set.find(offline2) != id_set.end());
}

TEST_F(OfflinePageModelImplTest, DownloadMetrics) {
  EXPECT_FALSE(HasPages(kUserRequestedNamespace));
  SavePage(kTestUrl, kTestUserRequestedClientId);
  histograms().ExpectUniqueSample(
      "OfflinePages.DownloadSavedPageDuplicateCount", 1, 1);
  FastForwardBy(base::TimeDelta::FromMinutes(1));
  histograms().ExpectTotalCount(
      "OfflinePages.DownloadSavedPageTimeSinceDuplicateSaved", 0);
  SavePage(kTestUrl, kTestUserRequestedClientId);
  histograms().ExpectTotalCount("OfflinePages.DownloadSavedPageDuplicateCount",
                                2);
  histograms().ExpectBucketCount("OfflinePages.DownloadSavedPageDuplicateCount",
                                 2, 1);
  histograms().ExpectBucketCount("OfflinePages.DownloadSavedPageDuplicateCount",
                                 1, 1);
  histograms().ExpectTotalCount(
      "OfflinePages.DownloadSavedPageTimeSinceDuplicateSaved", 1);
  histograms().ExpectTotalCount(
      "OfflinePages.DownloadDeletedPageDuplicateCount", 0);

  // void DeletePage(int64_t offline_id, const DeletePageCallback& callback) {
  const std::vector<int64_t> ids =
      GetOfflineIdsForClientId(kTestUserRequestedClientId);
  ASSERT_EQ(2U, ids.size());

  DeletePage(ids[0], base::Bind(&OfflinePageModelImplTest::OnDeletePageDone,
                                AsWeakPtr()));
  PumpLoop();
  histograms().ExpectUniqueSample(
      "OfflinePages.DownloadDeletedPageDuplicateCount", 2, 1);
  DeletePage(ids[1], base::Bind(&OfflinePageModelImplTest::OnDeletePageDone,
                                AsWeakPtr()));
  PumpLoop();
  // No change when we delete the last page.
  histograms().ExpectTotalCount(
      "OfflinePages.DownloadDeletedPageDuplicateCount", 2);
  histograms().ExpectBucketCount(
      "OfflinePages.DownloadDeletedPageDuplicateCount", 1, 1);
  histograms().ExpectBucketCount(
      "OfflinePages.DownloadDeletedPageDuplicateCount", 2, 1);
}

TEST_F(OfflinePageModelImplTest, ExpirePages) {
  // We will save 3 pages and then expire 2 of them.
  std::pair<SavePageResult, int64_t> saved_pages[3];
  saved_pages[0] = SavePage(kTestUrl, kTestClientId1);
  saved_pages[1] = SavePage(kTestUrl2, kTestClientId2);
  saved_pages[2] = SavePage(kTestUrl3, kTestClientId3);

  for (const auto& save_result : saved_pages) {
    ASSERT_EQ(OfflinePageModel::SavePageResult::SUCCESS,
              std::get<0>(save_result));
  }

  // First two pages will be expired.
  std::vector<int64_t> pages_to_expire = {std::get<1>(saved_pages[0]),
                                          std::get<1>(saved_pages[1])};
  // Pages are marked as expired if they have an expiration_time set.
  base::Time expiration_time =
      base::Time::Now() + base::TimeDelta::FromMinutes(5);

  model()->ExpirePages(
      pages_to_expire, expiration_time,
      base::Bind(&OfflinePageModelImplTest::OnPagesExpired, AsWeakPtr()));
  PumpLoop();

  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();
  for (const auto& offline_page : offline_pages) {
    if (std::find(pages_to_expire.begin(), pages_to_expire.end(),
                  offline_page.offline_id) != pages_to_expire.end()) {
      EXPECT_EQ(expiration_time, offline_page.expiration_time);
      EXPECT_TRUE(offline_page.IsExpired());
    } else {
      EXPECT_EQ(base::Time(), offline_page.expiration_time);
      EXPECT_FALSE(offline_page.IsExpired());
    }
  }
  EXPECT_TRUE(last_expire_page_result());
}

TEST_F(OfflinePageModelImplTest, GetPagesByClientIds) {
  // We will save 3 pages.  One will be expired.
  std::pair<SavePageResult, int64_t> saved_pages[3];
  saved_pages[0] = SavePage(kTestUrl, kTestClientId1);
  saved_pages[1] = SavePage(kTestUrl2, kTestClientId2);
  saved_pages[2] = SavePage(kTestUrl3, kTestClientId3);

  for (const auto& save_result : saved_pages) {
    ASSERT_EQ(OfflinePageModel::SavePageResult::SUCCESS,
              std::get<0>(save_result));
  }

  std::vector<int64_t> pages_to_expire = {std::get<1>(saved_pages[0])};
  // Pages are marked as expired if they have an expiration_time set.
  base::Time expiration_time =
      base::Time::Now() + base::TimeDelta::FromMinutes(5);
  model()->ExpirePages(
      pages_to_expire, expiration_time,
      base::Bind(&OfflinePageModelImplTest::OnPagesExpired, AsWeakPtr()));
  PumpLoop();

  std::vector<ClientId> client_ids = {kTestClientId1, kTestClientId2};
  std::vector<OfflinePageItem> offline_pages = GetPagesByClientIds(client_ids);
  EXPECT_EQ(1U, offline_pages.size());

  const OfflinePageItem& item = offline_pages[0];
  EXPECT_EQ(kTestClientId2.name_space, item.client_id.name_space);
  EXPECT_EQ(kTestClientId2.id, item.client_id.id);
  EXPECT_EQ(kTestUrl2, item.url);
}

TEST_F(OfflinePageModelImplTest, DeletePagesByClientIds) {
  // We will save 3 pages.
  std::pair<SavePageResult, int64_t> saved_pages[3];
  saved_pages[0] = SavePage(kTestUrl, kTestClientId1);
  saved_pages[1] = SavePage(kTestUrl2, kTestClientId2);
  saved_pages[2] = SavePage(kTestUrl3, kTestClientId3);

  for (const auto& save_result : saved_pages) {
    ASSERT_EQ(OfflinePageModel::SavePageResult::SUCCESS,
              std::get<0>(save_result));
  }

  std::vector<ClientId> client_ids = {kTestClientId1, kTestClientId2};
  DeletePagesByClientIds(client_ids);
  std::vector<OfflinePageItem> offline_pages = GetAllPages();
  ASSERT_EQ(1U, offline_pages.size());

  const OfflinePageItem& item = offline_pages[0];
  EXPECT_EQ(kTestClientId3.name_space, item.client_id.name_space);
  EXPECT_EQ(kTestClientId3.id, item.client_id.id);
  EXPECT_EQ(kTestUrl3, item.url);
}

TEST_F(OfflinePageModelImplTest, CustomTabsNamespace) {
  SavePage(kTestUrl, ClientId(kCCTNamespace, "123"));
  std::string histogram_name = "OfflinePages.SavePageResult.";
  histogram_name += kCCTNamespace;

  histograms().ExpectUniqueSample(histogram_name,
                                  static_cast<int>(SavePageResult::SUCCESS), 1);
}

TEST_F(OfflinePageModelImplTest, DownloadNamespace) {
  SavePage(kTestUrl, ClientId(kDownloadNamespace, "123"));
  std::string histogram_name = "OfflinePages.SavePageResult.";
  histogram_name += kDownloadNamespace;

  histograms().ExpectUniqueSample(histogram_name,
                                  static_cast<int>(SavePageResult::SUCCESS), 1);
}

TEST_F(OfflinePageModelImplTest, NewTabPageNamespace) {
  SavePage(kTestUrl, ClientId(kNTPSuggestionsNamespace, "123"));
  std::string histogram_name = "OfflinePages.SavePageResult.";
  histogram_name += kNTPSuggestionsNamespace;

  histograms().ExpectUniqueSample(histogram_name,
                                  static_cast<int>(SavePageResult::SUCCESS), 1);
}

TEST_F(OfflinePageModelImplTest, StoreResetSuccessful) {
  GetStore()->set_test_scenario(
      OfflinePageTestStore::TestScenario::LOAD_FAILED_RESET_SUCCESS);
  ResetModel();

  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();

  EXPECT_TRUE(model()->is_loaded());
  EXPECT_EQ(StoreState::LOADED, GetStore()->state());
  EXPECT_EQ(0UL, offline_pages.size());

  std::pair<SavePageResult, int64_t> result =
      SavePage(kTestUrl, ClientId(kDownloadNamespace, "123"));

  EXPECT_EQ(SavePageResult::SUCCESS, result.first);
}

TEST_F(OfflinePageModelImplTest, StoreResetFailed) {
  GetStore()->set_test_scenario(
      OfflinePageTestStore::TestScenario::LOAD_FAILED_RESET_FAILED);
  ResetModel();

  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();

  EXPECT_TRUE(model()->is_loaded());
  EXPECT_EQ(StoreState::FAILED_RESET, GetStore()->state());
  EXPECT_EQ(0UL, offline_pages.size());

  ResetResults();
  std::pair<SavePageResult, int64_t> result =
      SavePage(kTestUrl, ClientId(kDownloadNamespace, "123"));

  EXPECT_EQ(SavePageResult::STORE_FAILURE, result.first);
}

TEST(CommandLineFlagsTest, OfflineBookmarks) {
  // Disabled by default.
  EXPECT_FALSE(offline_pages::IsOfflineBookmarksEnabled());

  // Check if feature is correctly enabled by command-line flag.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kOfflineBookmarksFeature);
  EXPECT_TRUE(offline_pages::IsOfflineBookmarksEnabled());
}

TEST(CommandLineFlagsTest, OffliningRecentPages) {
  // Enable offline bookmarks feature first.
  // TODO(dimich): once offline pages are enabled by default, remove this.
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list(
      new base::test::ScopedFeatureList);
  scoped_feature_list->InitAndEnableFeature(kOfflineBookmarksFeature);

  // This feature is still disabled by default.
  EXPECT_FALSE(offline_pages::IsOffliningRecentPagesEnabled());

  // Check if feature is correctly enabled by command-line flag.
  scoped_feature_list.reset(new base::test::ScopedFeatureList);
  scoped_feature_list->InitFromCommandLine(
      std::string(kOfflineBookmarksFeature.name) + "," +
          kOffliningRecentPagesFeature.name,
      "");
  EXPECT_TRUE(offline_pages::IsOffliningRecentPagesEnabled());
}

TEST(CommandLineFlagsTest, OfflinePagesSharing) {
  // Enable offline bookmarks feature first.
  // TODO(dimich): once offline pages are enabled by default, remove this.
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list(
      new base::test::ScopedFeatureList);
  scoped_feature_list->InitAndEnableFeature(kOfflineBookmarksFeature);

  // This feature is still disabled by default.
  EXPECT_FALSE(offline_pages::IsOfflinePagesSharingEnabled());

  // Check if feature is correctly enabled by command-line flag.
  scoped_feature_list.reset(new base::test::ScopedFeatureList);
  scoped_feature_list->InitFromCommandLine(
      std::string(kOfflineBookmarksFeature.name) + "," +
          kOfflinePagesSharingFeature.name,
      "");
  EXPECT_TRUE(offline_pages::IsOfflinePagesSharingEnabled());
}

TEST(CommandLineFlagsTest, OfflinePagesSvelteConcurrentLoading) {
  // This feature is disabled by default.
  EXPECT_FALSE(offline_pages::IsOfflinePagesSvelteConcurrentLoadingEnabled());

  // Check if feature is correctly enabled by command-line flag.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kOfflinePagesSvelteConcurrentLoadingFeature);
  EXPECT_TRUE(offline_pages::IsOfflinePagesSvelteConcurrentLoadingEnabled());
}

}  // namespace offline_pages
