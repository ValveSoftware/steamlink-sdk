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
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
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
const char kTestClientNamespace[] = "CLIENT_NAMESPACE";
const GURL kTestUrl("http://example.com");
const GURL kTestUrl2("http://other.page.com");
const GURL kTestUrl3("http://test.xyz");
const GURL kTestUrl4("http://page.net");
const GURL kFileUrl("file:///foo");
const ClientId kTestClientId1(kTestClientNamespace, "1234");
const ClientId kTestClientId2(kTestClientNamespace, "5678");
const ClientId kTestClientId3(kTestClientNamespace, "42");
const int64_t kTestFileSize = 876543LL;

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
  void OnHasPagesDone(bool result);
  void OnCheckPagesExistOfflineDone(const CheckPagesExistOfflineResult& result);
  void OnClearAllDone();
  void OnGetOfflineIdsForClientIdDone(MultipleOfflineIdResult* storage,
                                      const MultipleOfflineIdResult& result);
  void OnGetSingleOfflinePageItemResult(const OfflinePageItem** storage,
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

  // Returns the offline ID of the saved page.
  std::pair<SavePageResult, int64_t> SavePage(const GURL& url,
                                              ClientId client_id);

  void SavePageWithArchiverResult(const GURL& url,
                                  ClientId client_id,
                                  OfflinePageArchiver::ArchiverResult result);

  void DeletePage(int64_t offline_id, const DeletePageCallback& callback) {
    std::vector<int64_t> offline_ids;
    offline_ids.push_back(offline_id);
    model()->DeletePagesByOfflineId(offline_ids, callback);
  }

  bool HasPages(std::string name_space);

  CheckPagesExistOfflineResult CheckPagesExistOffline(std::set<GURL>);

  MultipleOfflineIdResult GetOfflineIdsForClientId(const ClientId& client_id);

  const OfflinePageItem* GetPageByOfflineId(int64_t offline_id);

  MultipleOfflinePageItemResult GetPagesByOnlineURL(const GURL& offline_url);

  const OfflinePageItem* GetPageByOfflineURL(const GURL& offline_url);

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
  bool last_has_pages_result_;
  CheckPagesExistOfflineResult last_pages_exist_result_;
  int last_cleared_pages_count_;
  DeletePageResult last_clear_page_result_;
  bool last_expire_page_result_;
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

void OfflinePageModelImplTest::OnHasPagesDone(bool result) {
  last_has_pages_result_ = result;
}

void OfflinePageModelImplTest::OnCheckPagesExistOfflineDone(
    const CheckPagesExistOfflineResult& result) {
  last_pages_exist_result_ = result;
}

void OfflinePageModelImplTest::OnClearAllDone() {
  PumpLoop();
}

void OfflinePageModelImplTest::OnStoreUpdateDone(bool /* success - ignored */) {
}

std::unique_ptr<OfflinePageTestArchiver>
OfflinePageModelImplTest::BuildArchiver(
    const GURL& url,
    OfflinePageArchiver::ArchiverResult result) {
  return std::unique_ptr<OfflinePageTestArchiver>(new OfflinePageTestArchiver(
      this, url, result, kTestFileSize, base::ThreadTaskRunnerHandle::Get()));
}

std::unique_ptr<OfflinePageMetadataStore>
OfflinePageModelImplTest::BuildStore() {
  return std::unique_ptr<OfflinePageMetadataStore>(
      new OfflinePageTestStore(base::ThreadTaskRunnerHandle::Get()));
}

std::unique_ptr<OfflinePageModelImpl> OfflinePageModelImplTest::BuildModel(
    std::unique_ptr<OfflinePageMetadataStore> store) {
  return std::unique_ptr<OfflinePageModelImpl>(new OfflinePageModelImpl(
      std::move(store), temp_dir_.path(), base::ThreadTaskRunnerHandle::Get()));
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

std::pair<SavePageResult, int64_t> OfflinePageModelImplTest::SavePage(
    const GURL& url,
    ClientId client_id) {
  SavePageWithArchiverResult(
      url, client_id,
      OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED);
  return std::make_pair(last_save_result_, last_save_offline_id_);
}

void OfflinePageModelImplTest::SavePageWithArchiverResult(
    const GURL& url,
    ClientId client_id,
    OfflinePageArchiver::ArchiverResult result) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(url, result));
  model()->SavePage(
      url, client_id, std::move(archiver),
      base::Bind(&OfflinePageModelImplTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
}

MultipleOfflinePageItemResult OfflinePageModelImplTest::GetAllPages() {
  MultipleOfflinePageItemResult result;
  model()->GetAllPages(
      base::Bind(&OfflinePageModelImplTest::OnGetMultipleOfflinePageItemsResult,
                 AsWeakPtr(), base::Unretained(&result)));
  PumpLoop();
  return result;
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

const OfflinePageItem* OfflinePageModelImplTest::GetPageByOfflineId(
    int64_t offline_id) {
  const OfflinePageItem* result = nullptr;
  model()->GetPageByOfflineId(
      offline_id,
      base::Bind(&OfflinePageModelImplTest::OnGetSingleOfflinePageItemResult,
                 AsWeakPtr(), base::Unretained(&result)));
  PumpLoop();
  return result;
}

const OfflinePageItem* OfflinePageModelImplTest::GetPageByOfflineURL(
    const GURL& offline_url) {
  const OfflinePageItem* result = nullptr;
  model()->GetPageByOfflineURL(
      offline_url,
      base::Bind(&OfflinePageModelImplTest::OnGetSingleOfflinePageItemResult,
                 AsWeakPtr(), base::Unretained(&result)));
  PumpLoop();
  return result;
}

void OfflinePageModelImplTest::OnGetSingleOfflinePageItemResult(
    const OfflinePageItem** storage,
    const OfflinePageItem* result) {
  *storage = result;
}

void OfflinePageModelImplTest::OnGetMultipleOfflinePageItemsResult(
    MultipleOfflinePageItemResult* storage,
    const MultipleOfflinePageItemResult& result) {
  *storage = result;
}

void OfflinePageModelImplTest::OnPagesExpired(bool result) {
  last_expire_page_result_ = result;
}

MultipleOfflinePageItemResult OfflinePageModelImplTest::GetPagesByOnlineURL(
    const GURL& online_url) {
  MultipleOfflinePageItemResult result;
  model()->GetPagesByOnlineURL(
      online_url,
      base::Bind(&OfflinePageModelImplTest::OnGetMultipleOfflinePageItemsResult,
                 AsWeakPtr(), base::Unretained(&result)));
  PumpLoop();
  return result;
}

bool OfflinePageModelImplTest::HasPages(std::string name_space) {
  model()->HasPages(
      name_space,
      base::Bind(&OfflinePageModelImplTest::OnHasPagesDone, AsWeakPtr()));
  PumpLoop();
  return last_has_pages_result_;
}

TEST_F(OfflinePageModelImplTest, SavePageSuccessful) {
  EXPECT_FALSE(HasPages(kTestClientNamespace));
  SavePage(kTestUrl, kTestClientId1);
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

  EXPECT_EQ(1UL, offline_pages.size());
  EXPECT_EQ(kTestUrl, offline_pages[0].url);
  EXPECT_EQ(kTestClientId1.id, offline_pages[0].client_id.id);
  EXPECT_EQ(kTestClientId1.name_space, offline_pages[0].client_id.name_space);
  EXPECT_EQ(archiver_path, offline_pages[0].file_path);
  EXPECT_EQ(kTestFileSize, offline_pages[0].file_size);
  EXPECT_EQ(0, offline_pages[0].access_count);
  EXPECT_EQ(0, offline_pages[0].flags);
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
  model()->SavePage(
      kTestUrl, kTestClientId1, std::move(archiver),
      base::Bind(&OfflinePageModelImplTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(SavePageResult::ARCHIVE_CREATION_FAILED, last_save_result());
}

TEST_F(OfflinePageModelImplTest, SavePageOfflineCreationStoreWriteFailure) {
  GetStore()->set_test_scenario(
      OfflinePageTestStore::TestScenario::WRITE_FAILED);
  SavePage(kTestUrl, kTestClientId1);
  EXPECT_EQ(SavePageResult::STORE_FAILURE, last_save_result());
}

TEST_F(OfflinePageModelImplTest, SavePageLocalFileFailed) {
  // Don't create archiver since it will not be needed for pages that are not
  // going to be saved.
  model()->SavePage(
      kFileUrl, kTestClientId1, std::unique_ptr<OfflinePageTestArchiver>(),
      base::Bind(&OfflinePageModelImplTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(SavePageResult::SKIPPED, last_save_result());
}

TEST_F(OfflinePageModelImplTest, SavePageOfflineArchiverTwoPages) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestUrl, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  // archiver_ptr will be valid until after first PumpLoop() call after
  // CompleteCreateArchive() is called.
  OfflinePageTestArchiver* archiver_ptr = archiver.get();
  archiver_ptr->set_delayed(true);
  model()->SavePage(
      kTestUrl, kTestClientId1, std::move(archiver),
      base::Bind(&OfflinePageModelImplTest::OnSavePageDone, AsWeakPtr()));
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

TEST_F(OfflinePageModelImplTest, GetAllPagesStoreFailure) {
  GetStore()->set_test_scenario(
      OfflinePageTestStore::TestScenario::LOAD_FAILED);
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

TEST_F(OfflinePageModelImplTest, DeletePageByPredicate) {
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
  model()->DeletePagesByURLPredicate(
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
  model()->DeletePagesByURLPredicate(
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

  model()->DeletePagesByURLPredicate(
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

  const OfflinePageItem* page = GetPageByOfflineId(offline_id);

  // Delete the offline copy of the page and check the metadata.
  base::DeleteFile(page->file_path, false);
  model()->CheckMetadataConsistency();
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

  const OfflinePageItem* page = GetPageByOfflineId(offline_id);
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
  const OfflinePageItem* page = GetPageByOfflineId(offline_id);
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
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestUrl, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  model()->SavePage(
      kTestUrl, kTestClientId1, std::move(archiver),
      base::Bind(&OfflinePageModelImplTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
  int64_t offline1 = last_save_offline_id();

  std::unique_ptr<OfflinePageTestArchiver> archiver2(BuildArchiver(
      kTestUrl2, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  model()->SavePage(
      kTestUrl2, kTestClientId2, std::move(archiver2),
      base::Bind(&OfflinePageModelImplTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
  int64_t offline2 = last_save_offline_id();

  std::unique_ptr<OfflinePageTestArchiver> archiver3(BuildArchiver(
      kTestUrl3, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  model()->SavePage(
      kTestUrl3, kTestClientId3, std::move(archiver3),
      base::Bind(&OfflinePageModelImplTest::OnSavePageDone, AsWeakPtr()));
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

  const OfflinePageItem* page = GetPageByOfflineId(offline1);
  EXPECT_TRUE(page);
  EXPECT_EQ(kTestUrl, page->url);
  EXPECT_EQ(kTestClientId1, page->client_id);
  EXPECT_EQ(kTestFileSize, page->file_size);

  page = GetPageByOfflineId(offline2);
  EXPECT_TRUE(page);
  EXPECT_EQ(kTestUrl2, page->url);
  EXPECT_EQ(kTestClientId2, page->client_id);
  EXPECT_EQ(kTestFileSize, page->file_size);

  page = GetPageByOfflineId(-42);
  EXPECT_FALSE(page);
}

TEST_F(OfflinePageModelImplTest, GetPageByOfflineURL) {
  SavePage(kTestUrl, kTestClientId1);
  int64_t offline1 = last_save_offline_id();

  OfflinePageTestStore* store = GetStore();
  GURL offline_url = store->last_saved_page().GetOfflineURL();

  SavePage(kTestUrl2, kTestClientId2);

  GURL offline_url2 = store->last_saved_page().GetOfflineURL();
  int64_t offline2 = last_save_offline_id();

  const OfflinePageItem* page = GetPageByOfflineURL(offline_url2);
  EXPECT_TRUE(page);
  EXPECT_EQ(kTestUrl2, page->url);
  EXPECT_EQ(kTestClientId2, page->client_id);
  EXPECT_EQ(offline2, page->offline_id);

  page = GetPageByOfflineURL(offline_url);
  EXPECT_TRUE(page);
  EXPECT_EQ(kTestUrl, page->url);
  EXPECT_EQ(kTestClientId1, page->client_id);
  EXPECT_EQ(offline1, page->offline_id);

  page = GetPageByOfflineURL(GURL("http://foo"));
  EXPECT_FALSE(page);
}

TEST_F(OfflinePageModelImplTest, GetPagesByOnlineURL) {
  SavePage(kTestUrl, kTestClientId1);
  SavePage(kTestUrl2, kTestClientId2);

  MultipleOfflinePageItemResult pages = GetPagesByOnlineURL(kTestUrl2);
  EXPECT_EQ(1U, pages.size());
  EXPECT_EQ(kTestUrl2, pages[0].url);
  EXPECT_EQ(kTestClientId2, pages[0].client_id);

  pages = GetPagesByOnlineURL(kTestUrl);
  EXPECT_EQ(1U, pages.size());
  EXPECT_EQ(kTestUrl, pages[0].url);
  EXPECT_EQ(kTestClientId1, pages[0].client_id);

  pages = GetPagesByOnlineURL(GURL("http://foo"));
  EXPECT_EQ(0U, pages.size());
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

TEST_F(OfflinePageModelImplTest, ClearAll) {
  SavePage(kTestUrl, kTestClientId1);
  SavePage(kTestUrl2, kTestClientId2);

  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();
  EXPECT_EQ(2UL, offline_pages.size());
  EXPECT_EQ(2UL, GetStore()->GetAllPages().size());
  base::FilePath archiver_path = offline_pages[0].file_path;
  EXPECT_TRUE(base::PathExists(archiver_path));

  // ClearAll should delete all the files and wipe out both cache and store.
  model()->ClearAll(
      base::Bind(&OfflinePageModelImplTest::OnClearAllDone, AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(0UL, GetAllPages().size());
  EXPECT_EQ(0UL, GetStore()->GetAllPages().size());
  EXPECT_FALSE(base::PathExists(archiver_path));

  // The model should reload the store after the reset. All model operations
  // should continue to work.
  SavePage(kTestUrl2, kTestClientId2);
  EXPECT_EQ(1UL, GetAllPages().size());
  EXPECT_EQ(1UL, GetStore()->GetAllPages().size());
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

TEST_F(OfflinePageModelImplTest, GetBestPage) {
  // We will save 3 pages - two for the same URL, and one for a different URL.
  // Correct behavior will pick the most recently saved page for the correct
  // URL.
  std::pair<SavePageResult, int64_t> saved_pages[3];
  saved_pages[0] = SavePage(kTestUrl, kTestClientId1);
  saved_pages[1] = SavePage(kTestUrl, kTestClientId1);
  saved_pages[2] = SavePage(kTestUrl2, kTestClientId2);

  for (const auto& save_result : saved_pages) {
    ASSERT_EQ(OfflinePageModel::SavePageResult::SUCCESS,
              std::get<0>(save_result));
  }

  const OfflinePageItem* offline_page =
      model()->MaybeGetBestPageForOnlineURL(kTestUrl);
  ASSERT_TRUE(nullptr != offline_page);

  EXPECT_EQ(std::get<1>(saved_pages[1]), offline_page->offline_id);
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

TEST(CommandLineFlagsTest, OfflineBookmarks) {
  // Disabled by default.
  EXPECT_FALSE(offline_pages::IsOfflineBookmarksEnabled());

  // Check if feature is correctly enabled by command-line flag.
  base::FeatureList::ClearInstanceForTesting();
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->InitializeFromCommandLine(
      offline_pages::kOfflineBookmarksFeature.name, "");
  base::FeatureList::SetInstance(std::move(feature_list));
  EXPECT_TRUE(offline_pages::IsOfflineBookmarksEnabled());
}

TEST(CommandLineFlagsTest, OffliningRecentPages) {
  // Enable offline bookmarks feature first.
  // TODO(dimich): once offline pages are enabled by default, remove this.
  base::FeatureList::ClearInstanceForTesting();
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->InitializeFromCommandLine(
      offline_pages::kOfflineBookmarksFeature.name, "");
  base::FeatureList::SetInstance(std::move(feature_list));

  // This feature is still disabled by default.
  EXPECT_FALSE(offline_pages::IsOffliningRecentPagesEnabled());

  // Check if feature is correctly enabled by command-line flag.
  base::FeatureList::ClearInstanceForTesting();
  std::unique_ptr<base::FeatureList> feature_list2(new base::FeatureList);
  feature_list2->InitializeFromCommandLine(
      std::string(offline_pages::kOfflineBookmarksFeature.name) + "," +
          offline_pages::kOffliningRecentPagesFeature.name,
      "");
  base::FeatureList::SetInstance(std::move(feature_list2));
  EXPECT_TRUE(offline_pages::IsOffliningRecentPagesEnabled());
}

TEST(CommandLineFlagsTest, OfflinePagesBackgroundLoading) {
  // Enable offline bookmarks feature first.
  // TODO(dimich): once offline pages are enabled by default, remove this.
  base::FeatureList::ClearInstanceForTesting();
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->InitializeFromCommandLine(
      offline_pages::kOfflineBookmarksFeature.name, "");
  base::FeatureList::SetInstance(std::move(feature_list));

  // This feature is still disabled by default.
  EXPECT_FALSE(offline_pages::IsOfflinePagesBackgroundLoadingEnabled());

  // Check if feature is correctly enabled by command-line flag.
  base::FeatureList::ClearInstanceForTesting();
  std::unique_ptr<base::FeatureList> feature_list2(new base::FeatureList);
  feature_list2->InitializeFromCommandLine(
      std::string(offline_pages::kOfflineBookmarksFeature.name) + "," +
          offline_pages::kOfflinePagesBackgroundLoadingFeature.name,
      "");
  base::FeatureList::SetInstance(std::move(feature_list2));
  EXPECT_TRUE(offline_pages::IsOfflinePagesBackgroundLoadingEnabled());
}

}  // namespace offline_pages
