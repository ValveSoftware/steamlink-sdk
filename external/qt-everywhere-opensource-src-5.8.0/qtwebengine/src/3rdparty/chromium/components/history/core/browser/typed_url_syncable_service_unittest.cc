// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/typed_url_syncable_service.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/in_memory_history_backend.h"
#include "components/history/core/test/test_history_database.h"
#include "sync/api/fake_sync_change_processor.h"
#include "sync/api/sync_change_processor_wrapper_for_test.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/internal_api/public/attachments/attachment_service_proxy_for_test.h"
#include "sync/protocol/sync.pb.h"
#include "sync/protocol/typed_url_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using history::HistoryBackend;
using history::URLID;
using history::URLRow;
using history::URLRows;
using history::VisitRow;
using history::VisitVector;

namespace history {

namespace {

// Constants used to limit size of visits processed. See
// equivalent constants in typed_url_syncable_service.cc for descriptions.
const int kMaxTypedUrlVisits = 100;
const int kVisitThrottleThreshold = 10;
const int kVisitThrottleMultiple = 10;

// Visits with this timestamp are treated as expired.
const int kExpiredVisit = -1;

// Helper constants for tests.
const char kTitle[] = "pie";
const char kTitle2[] = "pie2";
const char kURL[] = "http://pie.com/";

bool URLsEqual(URLRow& row, sync_pb::TypedUrlSpecifics& specifics) {
  return ((row.url().spec().compare(specifics.url()) == 0) &&
          (base::UTF16ToUTF8(row.title()).compare(specifics.title()) == 0) &&
          (row.hidden() == specifics.hidden()));
}

bool URLsEqual(history::URLRow& lhs, history::URLRow& rhs) {
  // Only compare synced fields (ignore typed_count and visit_count as those
  // are maintained by the history subsystem).
  return (lhs.url().spec().compare(rhs.url().spec()) == 0) &&
         (lhs.title().compare(rhs.title()) == 0) &&
         (lhs.hidden() == rhs.hidden());
}

void AddNewestVisit(ui::PageTransition transition,
                    int64_t visit_time,
                    URLRow* url,
                    VisitVector* visits) {
  base::Time time = base::Time::FromInternalValue(visit_time);
  visits->insert(visits->begin(), VisitRow(url->id(), time, 0, transition, 0));

  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED)) {
    url->set_typed_count(url->typed_count() + 1);
  }

  url->set_last_visit(time);
  url->set_visit_count(visits->size());
}

void AddOldestVisit(ui::PageTransition transition,
                    int64_t visit_time,
                    URLRow* url,
                    VisitVector* visits) {
  base::Time time = base::Time::FromInternalValue(visit_time);
  visits->push_back(VisitRow(url->id(), time, 0, transition, 0));

  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED)) {
    url->set_typed_count(url->typed_count() + 1);
  }

  url->set_visit_count(visits->size());
}

// Create a new row object and the typed visit çorresponding with the time at
// |last_visit| in the |visits| vector.
URLRow MakeTypedUrlRow(const std::string& url,
                       const std::string& title,
                       int typed_count,
                       int64_t last_visit,
                       bool hidden,
                       VisitVector* visits) {
  // Give each URL a unique ID, to mimic the behavior of the real database.
  GURL gurl(url);
  URLRow history_url(gurl);
  history_url.set_title(base::UTF8ToUTF16(title));
  history_url.set_typed_count(typed_count);
  history_url.set_hidden(hidden);

  base::Time last_visit_time = base::Time::FromInternalValue(last_visit);
  history_url.set_last_visit(last_visit_time);

  if (typed_count > 0) {
    // Add a typed visit for time |last_visit|.
    visits->push_back(VisitRow(history_url.id(), last_visit_time, 0,
                               ui::PAGE_TRANSITION_TYPED, 0));
  } else {
    // Add a non-typed visit for time |last_visit|.
    visits->push_back(VisitRow(history_url.id(), last_visit_time, 0,
                               ui::PAGE_TRANSITION_RELOAD, 0));
  }

  history_url.set_visit_count(visits->size());
  return history_url;
}

static sync_pb::TypedUrlSpecifics MakeTypedUrlSpecifics(const char* url,
                                                        const char* title,
                                                        int64_t last_visit,
                                                        bool hidden) {
  sync_pb::TypedUrlSpecifics typed_url;
  typed_url.set_url(url);
  typed_url.set_title(title);
  typed_url.set_hidden(hidden);
  typed_url.add_visits(last_visit);
  typed_url.add_visit_transitions(ui::PAGE_TRANSITION_TYPED);
  return typed_url;
}

class TestHistoryBackend;

class TestHistoryBackendDelegate : public HistoryBackend::Delegate {
 public:
  TestHistoryBackendDelegate() {}

  void NotifyProfileError(sql::InitStatus init_status) override {}
  void SetInMemoryBackend(
      std::unique_ptr<InMemoryHistoryBackend> backend) override{};
  void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                             const GURL& icon_url) override{};
  void NotifyURLVisited(ui::PageTransition transition,
                        const URLRow& row,
                        const RedirectList& redirects,
                        base::Time visit_time) override{};
  void NotifyURLsModified(const URLRows& changed_urls) override{};
  void NotifyURLsDeleted(bool all_history,
                         bool expired,
                         const URLRows& deleted_rows,
                         const std::set<GURL>& favicon_urls) override{};
  void NotifyKeywordSearchTermUpdated(const URLRow& row,
                                      KeywordID keyword_id,
                                      const base::string16& term) override{};
  void NotifyKeywordSearchTermDeleted(URLID url_id) override{};
  void DBLoaded() override{};

 private:
  DISALLOW_COPY_AND_ASSIGN(TestHistoryBackendDelegate);
};

class TestHistoryBackend : public HistoryBackend {
 public:
  TestHistoryBackend()
      : HistoryBackend(new TestHistoryBackendDelegate(),
                       nullptr,
                       base::ThreadTaskRunnerHandle::Get()) {}

  bool IsExpiredVisitTime(const base::Time& time) override {
    return time.ToInternalValue() == kExpiredVisit;
  }

  URLID GetIdByUrl(const GURL& gurl) {
    return db()->GetRowForURL(gurl, nullptr);
  }

  void SetVisitsForUrl(URLRow& new_url, const VisitVector visits) {
    std::vector<history::VisitInfo> added_visits;
    URLRows new_urls;
    DeleteURL(new_url.url());
    for (const auto& visit : visits) {
      added_visits.push_back(
          history::VisitInfo(visit.visit_time, visit.transition));
    }
    new_urls.push_back(new_url);
    AddPagesWithDetails(new_urls, history::SOURCE_SYNCED);
    AddVisits(new_url.url(), added_visits, history::SOURCE_SYNCED);
    new_url.set_id(GetIdByUrl(new_url.url()));
  }

 private:
  ~TestHistoryBackend() override {}
};

}  // namespace

class TypedUrlSyncableServiceTest : public testing::Test {
 public:
  TypedUrlSyncableServiceTest() : typed_url_sync_service_(NULL) {}
  ~TypedUrlSyncableServiceTest() override {}

  void SetUp() override {
    fake_history_backend_ = new TestHistoryBackend();
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
    fake_history_backend_->Init(false,
        TestHistoryDatabaseParamsForPath(test_dir_.path()));
    typed_url_sync_service_ =
        fake_history_backend_->GetTypedUrlSyncableService();
    fake_change_processor_.reset(new syncer::FakeSyncChangeProcessor);
  }

  // Starts sync for |typed_url_sync_service_| with |initial_data| as the
  // initial sync data.
  void StartSyncing(const syncer::SyncDataList& initial_data);

  // Builds a set of url rows and visit vectors based on |num_typed_urls| and
  // |num_reload_urls|, and |urls|. The rows are stored into |rows|, the visit
  // vectors in |visit_vectors|, and the changes are pushed into the history
  // backend.
  // Returns true if sync receives the proper number of changes, false
  // otherwise.
  bool BuildAndPushLocalChanges(unsigned int num_typed_urls,
                                unsigned int num_reload_urls,
                                const std::vector<std::string>& urls,
                                URLRows* rows,
                                std::vector<VisitVector>* visit_vectors);

  // Fills |urls| with the set of synced urls within |typed_url_sync_service_|.
  void GetSyncedUrls(std::set<GURL>* urls) const;

  // Create and apply a change for url and its visits into history backend.
  VisitVector ApplyUrlAndVisitsChange(
      const std::string& url,
      const std::string& title,
      int typed_count,
      int64_t last_visit,
      bool hidden,
      syncer::SyncChange::SyncChangeType change_type);

  // Add typed_url_sync_service_ to fake_history_backend_'s observer's list.
  void AddObserver();

  // Fills |specifics| with the sync data for |url| and |visits|.
  static void WriteToTypedUrlSpecifics(const URLRow& url,
                                       const VisitVector& visits,
                                       sync_pb::TypedUrlSpecifics* specifics);

  // Helper to call TypedUrlSyncableService's MergeURLs method.
  static TypedUrlSyncableService::MergeResult MergeUrls(
      const sync_pb::TypedUrlSpecifics& typed_url,
      const history::URLRow& url,
      history::VisitVector* visits,
      history::URLRow* new_url,
      std::vector<history::VisitInfo>* new_visits);

  // Helper to call TypedUrlSyncableService's DiffVisits method.
  static void DiffVisits(const history::VisitVector& history_visits,
                         const sync_pb::TypedUrlSpecifics& sync_specifics,
                         std::vector<history::VisitInfo>* new_visits,
                         history::VisitVector* removed_visits);

  // Create a new row associated with a specific visit's time.
  static history::VisitRow CreateVisit(ui::PageTransition type,
                                       int64_t timestamp);

  static const TypedUrlSyncableService::MergeResult DIFF_NONE =
      TypedUrlSyncableService::DIFF_NONE;
  static const TypedUrlSyncableService::MergeResult DIFF_UPDATE_NODE =
      TypedUrlSyncableService::DIFF_UPDATE_NODE;
  static const TypedUrlSyncableService::MergeResult DIFF_LOCAL_ROW_CHANGED =
      TypedUrlSyncableService::DIFF_LOCAL_ROW_CHANGED;
  static const TypedUrlSyncableService::MergeResult DIFF_LOCAL_VISITS_ADDED =
      TypedUrlSyncableService::DIFF_LOCAL_VISITS_ADDED;

 protected:
  base::MessageLoop message_loop_;
  base::ScopedTempDir test_dir_;
  scoped_refptr<TestHistoryBackend> fake_history_backend_;
  TypedUrlSyncableService* typed_url_sync_service_;
  std::unique_ptr<syncer::FakeSyncChangeProcessor> fake_change_processor_;
};

void TypedUrlSyncableServiceTest::StartSyncing(
    const syncer::SyncDataList& initial_data) {
  DCHECK(fake_change_processor_.get());

  // Set change processor.
  syncer::SyncMergeResult result =
      typed_url_sync_service_->MergeDataAndStartSyncing(
          syncer::TYPED_URLS, initial_data,
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor_.get())),
          std::unique_ptr<syncer::SyncErrorFactory>(
              new syncer::SyncErrorFactoryMock()));
  typed_url_sync_service_->history_backend_observer_.RemoveAll();
  EXPECT_FALSE(result.error().IsSet()) << result.error().message();
}

bool TypedUrlSyncableServiceTest::BuildAndPushLocalChanges(
    unsigned int num_typed_urls,
    unsigned int num_reload_urls,
    const std::vector<std::string>& urls,
    URLRows* rows,
    std::vector<VisitVector>* visit_vectors) {
  unsigned int total_urls = num_typed_urls + num_reload_urls;
  DCHECK(urls.size() >= total_urls);
  if (!typed_url_sync_service_)
    return false;

  if (total_urls) {
    // Create new URL rows, populate the mock backend with its visits, and
    // send to the sync service.
    URLRows changed_urls;

    for (unsigned int i = 0; i < total_urls; ++i) {
      int typed = i < num_typed_urls ? 1 : 0;
      VisitVector visits;
      visit_vectors->push_back(visits);
      rows->push_back(MakeTypedUrlRow(urls[i], kTitle, typed, i + 3, false,
                                      &visit_vectors->back()));
      fake_history_backend_->SetVisitsForUrl(rows->back(),
                                             visit_vectors->back());
      changed_urls.push_back(rows->back());
    }

    typed_url_sync_service_->OnURLsModified(fake_history_backend_.get(),
                                            changed_urls);
  }

  // Check that communication with sync was successful.
  if (num_typed_urls != fake_change_processor_->changes().size())
    return false;
  return true;
}

void TypedUrlSyncableServiceTest::GetSyncedUrls(std::set<GURL>* urls) const {
  return typed_url_sync_service_->GetSyncedUrls(urls);
}

VisitVector TypedUrlSyncableServiceTest::ApplyUrlAndVisitsChange(
    const std::string& url,
    const std::string& title,
    int typed_count,
    int64_t last_visit,
    bool hidden,
    syncer::SyncChange::SyncChangeType change_type) {
  VisitVector visits;
  URLRow row =
      MakeTypedUrlRow(url, title, typed_count, last_visit, hidden, &visits);
  syncer::SyncChangeList change_list;
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url_specifics =
      entity_specifics.mutable_typed_url();
  WriteToTypedUrlSpecifics(row, visits, typed_url_specifics);
  syncer::SyncData sync_data = syncer::SyncData::CreateRemoteData(
      1, entity_specifics, base::Time(), syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create());
  syncer::SyncChange sync_change(FROM_HERE, change_type, sync_data);
  change_list.push_back(sync_change);
  typed_url_sync_service_->ProcessSyncChanges(FROM_HERE, change_list);
  return visits;
}

void TypedUrlSyncableServiceTest::AddObserver() {
  typed_url_sync_service_->history_backend_observer_.Add(
      fake_history_backend_.get());
}

// Static.
void TypedUrlSyncableServiceTest::WriteToTypedUrlSpecifics(
    const URLRow& url,
    const VisitVector& visits,
    sync_pb::TypedUrlSpecifics* specifics) {
  TypedUrlSyncableService::WriteToTypedUrlSpecifics(url, visits, specifics);
}

// Static.
TypedUrlSyncableService::MergeResult TypedUrlSyncableServiceTest::MergeUrls(
    const sync_pb::TypedUrlSpecifics& typed_url,
    const history::URLRow& url,
    history::VisitVector* visits,
    history::URLRow* new_url,
    std::vector<history::VisitInfo>* new_visits) {
  return TypedUrlSyncableService::MergeUrls(typed_url, url, visits, new_url,
                                            new_visits);
}

// Static.
void TypedUrlSyncableServiceTest::DiffVisits(
    const history::VisitVector& history_visits,
    const sync_pb::TypedUrlSpecifics& sync_specifics,
    std::vector<history::VisitInfo>* new_visits,
    history::VisitVector* removed_visits) {
  TypedUrlSyncableService::DiffVisits(history_visits, sync_specifics,
                                      new_visits, removed_visits);
}

// Static.
history::VisitRow TypedUrlSyncableServiceTest::CreateVisit(
    ui::PageTransition type,
    int64_t timestamp) {
  return history::VisitRow(0, base::Time::FromInternalValue(timestamp), 0, type,
                           0);
}

// Create a local typed URL with one TYPED visit after sync has started. Check
// that sync is sent an ADD change for the new URL.
TEST_F(TypedUrlSyncableServiceTest, AddLocalTypedUrl) {
  // Create a local typed URL (simulate a typed visit) that is not already
  // in sync. Check that sync is sent an ADD change for the existing URL.
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(BuildAndPushLocalChanges(1, 0, urls, &url_rows, &visit_vectors));

  URLRow url_row = url_rows.front();
  VisitVector visits = visit_vectors.front();

  // Check change processor.
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  ASSERT_EQ(1U, changes.size());
  ASSERT_TRUE(changes[0].IsValid());
  EXPECT_EQ(syncer::TYPED_URLS, changes[0].sync_data().GetDataType());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, changes[0].change_type());

  // Get typed url specifics.
  sync_pb::TypedUrlSpecifics url_specifics =
      changes[0].sync_data().GetSpecifics().typed_url();

  EXPECT_TRUE(URLsEqual(url_row, url_specifics));
  ASSERT_EQ(1, url_specifics.visits_size());
  ASSERT_EQ(static_cast<const int>(visits.size()), url_specifics.visits_size());
  EXPECT_EQ(visits[0].visit_time.ToInternalValue(), url_specifics.visits(0));
  EXPECT_EQ(static_cast<const int>(visits[0].transition),
            url_specifics.visit_transitions(0));

  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
  EXPECT_EQ(1U, sync_state.size());
  EXPECT_TRUE(sync_state.end() != sync_state.find(url_row.url()));
}

// Update a local typed URL that is already synced. Check that sync is sent an
// UPDATE for the existing url, but RELOAD visits aren't synced.
TEST_F(TypedUrlSyncableServiceTest, UpdateLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(BuildAndPushLocalChanges(1, 0, urls, &url_rows, &visit_vectors));
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  changes.clear();

  // Update the URL row, adding another typed visit to the visit vector.
  URLRow url_row = url_rows.front();
  VisitVector visits = visit_vectors.front();

  URLRows changed_urls;
  AddNewestVisit(ui::PAGE_TRANSITION_TYPED, 7, &url_row, &visits);
  AddNewestVisit(ui::PAGE_TRANSITION_RELOAD, 8, &url_row, &visits);
  AddNewestVisit(ui::PAGE_TRANSITION_LINK, 9, &url_row, &visits);
  fake_history_backend_->SetVisitsForUrl(url_row, visits);
  changed_urls.push_back(url_row);

  // Notify typed url sync service of the update.
  typed_url_sync_service_->OnURLsModified(fake_history_backend_.get(),
                                          changed_urls);

  ASSERT_EQ(1U, changes.size());
  ASSERT_TRUE(changes[0].IsValid());
  EXPECT_EQ(syncer::TYPED_URLS, changes[0].sync_data().GetDataType());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, changes[0].change_type());

  sync_pb::TypedUrlSpecifics url_specifics =
      changes[0].sync_data().GetSpecifics().typed_url();

  EXPECT_TRUE(URLsEqual(url_row, url_specifics));
  ASSERT_EQ(3, url_specifics.visits_size());
  ASSERT_EQ(static_cast<const int>(visits.size()) - 1,
            url_specifics.visits_size());

  // Check that each visit has been translated/communicated correctly.
  // Note that the specifics record visits in chronological order, and the
  // visits from the db are in reverse chronological order.
  EXPECT_EQ(visits[0].visit_time.ToInternalValue(), url_specifics.visits(2));
  EXPECT_EQ(static_cast<const int>(visits[0].transition),
            url_specifics.visit_transitions(2));
  EXPECT_EQ(visits[2].visit_time.ToInternalValue(), url_specifics.visits(1));
  EXPECT_EQ(static_cast<const int>(visits[3].transition),
            url_specifics.visit_transitions(1));
  EXPECT_EQ(visits[3].visit_time.ToInternalValue(), url_specifics.visits(0));
  EXPECT_EQ(static_cast<const int>(visits[3].transition),
            url_specifics.visit_transitions(0));

  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
  EXPECT_EQ(1U, sync_state.size());
  EXPECT_TRUE(sync_state.end() != sync_state.find(url_row.url()));
}

// Append a RELOAD visit to a typed url that is already synced. Check that sync
// does not receive any updates.
TEST_F(TypedUrlSyncableServiceTest, ReloadVisitLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(BuildAndPushLocalChanges(1, 0, urls, &url_rows, &visit_vectors));
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  changes.clear();

  // Update the URL row, adding another typed visit to the visit vector.
  URLRow url_row = url_rows.front();
  VisitVector visits = visit_vectors.front();

  URLRows changed_urls;
  AddNewestVisit(ui::PAGE_TRANSITION_RELOAD, 7, &url_row, &visits);
  fake_history_backend_->SetVisitsForUrl(url_row, visits);
  changed_urls.push_back(url_row);

  // Notify typed url sync service of the update.
  typed_url_sync_service_->OnURLVisited(
      fake_history_backend_.get(), ui::PAGE_TRANSITION_RELOAD, url_row,
      RedirectList(), base::Time::FromInternalValue(7));

  ASSERT_EQ(0U, changes.size());

  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
  EXPECT_EQ(1U, sync_state.size());
  EXPECT_TRUE(sync_state.end() != sync_state.find(url_row.url()));
}

// Appends a LINK visit to an existing typed url. Check that sync does not
// receive any changes.
TEST_F(TypedUrlSyncableServiceTest, LinkVisitLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(BuildAndPushLocalChanges(1, 0, urls, &url_rows, &visit_vectors));
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  changes.clear();

  URLRow url_row = url_rows.front();
  VisitVector visits = visit_vectors.front();

  // Update the URL row, adding a non-typed visit to the visit vector.
  AddNewestVisit(ui::PAGE_TRANSITION_LINK, 6, &url_row, &visits);
  fake_history_backend_->SetVisitsForUrl(url_row, visits);

  ui::PageTransition transition = ui::PAGE_TRANSITION_LINK;
  // Notify typed url sync service of non-typed visit, expect no change.
  typed_url_sync_service_->OnURLVisited(fake_history_backend_.get(), transition,
                                        url_row, RedirectList(),
                                        base::Time::FromInternalValue(6));
  ASSERT_EQ(0u, changes.size());
}

// Appends a series of LINK visits followed by a TYPED one to an existing typed
// url. Check that sync receives an UPDATE with the newest visit data.
TEST_F(TypedUrlSyncableServiceTest, TypedVisitLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(BuildAndPushLocalChanges(1, 0, urls, &url_rows, &visit_vectors));
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  changes.clear();

  URLRow url_row = url_rows.front();
  VisitVector visits = visit_vectors.front();

  // Update the URL row, adding another typed visit to the visit vector.
  AddOldestVisit(ui::PAGE_TRANSITION_LINK, 1, &url_row, &visits);
  AddNewestVisit(ui::PAGE_TRANSITION_LINK, 6, &url_row, &visits);
  AddNewestVisit(ui::PAGE_TRANSITION_TYPED, 7, &url_row, &visits);
  fake_history_backend_->SetVisitsForUrl(url_row, visits);

  // Notify typed url sync service of typed visit.
  ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;
  typed_url_sync_service_->OnURLVisited(fake_history_backend_.get(), transition,
                                        url_row, RedirectList(),
                                        base::Time::Now());

  ASSERT_EQ(1U, changes.size());
  ASSERT_TRUE(changes[0].IsValid());
  EXPECT_EQ(syncer::TYPED_URLS, changes[0].sync_data().GetDataType());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, changes[0].change_type());

  sync_pb::TypedUrlSpecifics url_specifics =
      changes[0].sync_data().GetSpecifics().typed_url();

  EXPECT_TRUE(URLsEqual(url_row, url_specifics));
  ASSERT_EQ(4u, visits.size());
  EXPECT_EQ(static_cast<const int>(visits.size()), url_specifics.visits_size());

  // Check that each visit has been translated/communicated correctly.
  // Note that the specifics record visits in chronological order, and the
  // visits from the db are in reverse chronological order.
  int r = url_specifics.visits_size() - 1;
  for (int i = 0; i < url_specifics.visits_size(); ++i, --r) {
    EXPECT_EQ(visits[i].visit_time.ToInternalValue(), url_specifics.visits(r));
    EXPECT_EQ(static_cast<const int>(visits[i].transition),
              url_specifics.visit_transitions(r));
  }

  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
  EXPECT_EQ(1U, sync_state.size());
  EXPECT_TRUE(sync_state.end() != sync_state.find(url_row.url()));
}

// Delete several (but not all) local typed urls. Check that sync receives the
// DELETE changes, and the non-deleted urls remain synced.
TEST_F(TypedUrlSyncableServiceTest, DeleteLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back("http://pie.com/");
  urls.push_back("http://cake.com/");
  urls.push_back("http://google.com/");
  urls.push_back("http://foo.com/");
  urls.push_back("http://bar.com/");

  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(BuildAndPushLocalChanges(4, 1, urls, &url_rows, &visit_vectors));
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  changes.clear();

  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
  EXPECT_EQ(4u, sync_state.size());

  // Simulate visit expiry of typed visit, no syncing is done
  // This is to test that sync relies on the in-memory cache to know
  // which urls were typed and synced, and should be deleted.
  url_rows[0].set_typed_count(0);
  VisitVector visits;
  fake_history_backend_->SetVisitsForUrl(url_rows[0], visits);

  // Delete some urls from backend and create deleted row vector.
  URLRows rows;
  for (size_t i = 0; i < 3u; ++i) {
    fake_history_backend_->DeleteURL(url_rows[i].url());
    rows.push_back(url_rows[i]);
  }

  // Notify typed url sync service.
  typed_url_sync_service_->OnURLsDeleted(fake_history_backend_.get(), false,
                                         false, rows, std::set<GURL>());

  ASSERT_EQ(3u, changes.size());
  for (size_t i = 0; i < changes.size(); ++i) {
    ASSERT_TRUE(changes[i].IsValid());
    ASSERT_EQ(syncer::TYPED_URLS, changes[i].sync_data().GetDataType());
    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, changes[i].change_type());
    sync_pb::TypedUrlSpecifics url_specifics =
        changes[i].sync_data().GetSpecifics().typed_url();
    EXPECT_EQ(url_rows[i].url().spec(), url_specifics.url());
  }

  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state_deleted;
  GetSyncedUrls(&sync_state_deleted);
  ASSERT_EQ(1u, sync_state_deleted.size());
  EXPECT_TRUE(sync_state_deleted.end() !=
              sync_state_deleted.find(url_rows[3].url()));
}

// Delete all local typed urls. Check that sync receives them all the DELETE
// changes, and that the sync state afterwards is empty.
TEST_F(TypedUrlSyncableServiceTest, DeleteAllLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back("http://pie.com/");
  urls.push_back("http://cake.com/");
  urls.push_back("http://google.com/");
  urls.push_back("http://foo.com/");
  urls.push_back("http://bar.com/");

  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(BuildAndPushLocalChanges(4, 1, urls, &url_rows, &visit_vectors));
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  changes.clear();

  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_EQ(4u, sync_state.size());

  // Delete urls from backend.
  for (size_t i = 0; i < 4u; ++ i) {
    fake_history_backend_->DeleteURL(url_rows[i].url());
  }
  // Delete urls with |all_history| flag set.
  bool all_history = true;

  // Notify typed url sync service.
  typed_url_sync_service_->OnURLsDeleted(fake_history_backend_.get(),
                                         all_history, false, URLRows(),
                                         std::set<GURL>());

  ASSERT_EQ(4u, changes.size());
  for (size_t i = 0; i < changes.size(); ++i) {
    ASSERT_TRUE(changes[i].IsValid());
    ASSERT_EQ(syncer::TYPED_URLS, changes[i].sync_data().GetDataType());
    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, changes[i].change_type());
  }
  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state_deleted;
  GetSyncedUrls(&sync_state_deleted);
  EXPECT_TRUE(sync_state_deleted.empty());
}

// Saturate the visits for a typed url with both TYPED and LINK navigations.
// Check that no more than kMaxTypedURLVisits are synced, and that LINK visits
// are dropped rather than TYPED ones.
TEST_F(TypedUrlSyncableServiceTest, MaxVisitLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(BuildAndPushLocalChanges(0, 1, urls, &url_rows, &visit_vectors));

  URLRow url_row = url_rows.front();
  VisitVector visits;

  // Add |kMaxTypedUrlVisits| + 10 visits to the url. The 10 oldest
  // non-typed visits are expected to be skipped.
  int i = 1;
  for (; i <= kMaxTypedUrlVisits - 20; ++i)
    AddNewestVisit(ui::PAGE_TRANSITION_TYPED, i, &url_row, &visits);
  for (; i <= kMaxTypedUrlVisits; ++i)
    AddNewestVisit(ui::PAGE_TRANSITION_LINK, i, &url_row, &visits);
  for (; i <= kMaxTypedUrlVisits + 10; ++i)
    AddNewestVisit(ui::PAGE_TRANSITION_TYPED, i, &url_row, &visits);

  fake_history_backend_->SetVisitsForUrl(url_row, visits);

  // Notify typed url sync service of typed visit.
  ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;
  typed_url_sync_service_->OnURLVisited(fake_history_backend_.get(), transition,
                                        url_row, RedirectList(),
                                        base::Time::Now());

  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  ASSERT_EQ(1U, changes.size());
  ASSERT_TRUE(changes[0].IsValid());
  ASSERT_EQ(syncer::SyncChange::ACTION_ADD, changes[0].change_type());
  sync_pb::TypedUrlSpecifics url_specifics =
      changes[0].sync_data().GetSpecifics().typed_url();
  ASSERT_EQ(kMaxTypedUrlVisits, url_specifics.visits_size());

  // Check that each visit has been translated/communicated correctly.
  // Note that the specifics records visits in chronological order, and the
  // visits from the db are in reverse chronological order.
  int num_typed_visits_synced = 0;
  int num_other_visits_synced = 0;
  int r = url_specifics.visits_size() - 1;
  for (int i = 0; i < url_specifics.visits_size(); ++i, --r) {
    if (url_specifics.visit_transitions(i) ==
        static_cast<int32_t>(ui::PAGE_TRANSITION_TYPED)) {
      ++num_typed_visits_synced;
    } else {
      ++num_other_visits_synced;
    }
  }
  EXPECT_EQ(kMaxTypedUrlVisits - 10, num_typed_visits_synced);
  EXPECT_EQ(10, num_other_visits_synced);
}

// Add enough visits to trigger throttling of updates to a typed url. Check that
// sync does not receive an update until the proper throttle interval has been
// reached.
TEST_F(TypedUrlSyncableServiceTest, ThrottleVisitLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(BuildAndPushLocalChanges(0, 1, urls, &url_rows, &visit_vectors));

  URLRow url_row = url_rows.front();
  VisitVector visits;

  // Add enough visits to the url so that typed count is above the throttle
  // limit, and not right on the interval that gets synced.
  int i = 1;
  for (; i < kVisitThrottleThreshold + kVisitThrottleMultiple / 2; ++i)
    AddNewestVisit(ui::PAGE_TRANSITION_TYPED, i, &url_row, &visits);
  fake_history_backend_->SetVisitsForUrl(url_row, visits);

  // Notify typed url sync service of typed visit.
  ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;
  typed_url_sync_service_->OnURLVisited(fake_history_backend_.get(), transition,
                                        url_row, RedirectList(),
                                        base::Time::Now());

  // Should throttle, so sync and local cache should not update.
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  ASSERT_EQ(0u, changes.size());
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_TRUE(sync_state.empty());

  for (; i % kVisitThrottleMultiple != 1; ++i)
    AddNewestVisit(ui::PAGE_TRANSITION_TYPED, i, &url_row, &visits);
  --i;  // Account for the increment before the condition ends.
  fake_history_backend_->SetVisitsForUrl(url_row, visits);

  // Notify typed url sync service of typed visit.
  typed_url_sync_service_->OnURLVisited(fake_history_backend_.get(), transition,
                                        url_row, RedirectList(),
                                        base::Time::Now());

  ASSERT_EQ(1u, changes.size());
  ASSERT_TRUE(changes[0].IsValid());
  ASSERT_EQ(syncer::SyncChange::ACTION_ADD, changes[0].change_type());
  sync_pb::TypedUrlSpecifics url_specifics =
      changes[0].sync_data().GetSpecifics().typed_url();
  ASSERT_EQ(i, url_specifics.visits_size());

  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
}

// Add a typed url locally and one to sync with the same data. Starting sync
// should result in no changes.
TEST_F(TypedUrlSyncableServiceTest, MergeUrlNoChange) {
  // Add a url to backend.
  VisitVector visits;
  URLRow row = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits);
  fake_history_backend_->SetVisitsForUrl(row, visits);

  // Create the same data in sync.
  syncer::SyncDataList initial_sync_data;
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();
  WriteToTypedUrlSpecifics(row, visits, typed_url);
  syncer::SyncData data =
      syncer::SyncData::CreateLocalData(kURL, kTitle, entity_specifics);

  initial_sync_data.push_back(data);
  StartSyncing(initial_sync_data);
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  EXPECT_TRUE(changes.empty());

  // Check that the local cache was is still correct.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
  EXPECT_EQ(sync_state.count(row.url()), 1U);
}

// Starting sync with no sync data should just push the local url to sync.
TEST_F(TypedUrlSyncableServiceTest, MergeUrlEmptySync) {
  // Add a url to backend.
  VisitVector visits;
  URLRow row = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits);
  fake_history_backend_->SetVisitsForUrl(row, visits);

  StartSyncing(syncer::SyncDataList());

  // Check that the local cache is still correct.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
  EXPECT_EQ(sync_state.count(row.url()), 1U);

  // Check that the server was updated correctly.
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  ASSERT_EQ(1U, changes.size());
  ASSERT_TRUE(changes[0].IsValid());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, changes[0].change_type());
  sync_pb::TypedUrlSpecifics url_specifics =
      changes[0].sync_data().GetSpecifics().typed_url();
  ASSERT_EQ(1, url_specifics.visits_size());
  EXPECT_EQ(3, url_specifics.visits(0));
  ASSERT_EQ(1, url_specifics.visit_transitions_size());
  EXPECT_EQ(static_cast<const int>(visits[0].transition),
            url_specifics.visit_transitions(0));
}

// Starting sync with no local data should just push the synced url into the
// backend.
TEST_F(TypedUrlSyncableServiceTest, MergeUrlEmptyLocal) {
  // Create the sync data.
  VisitVector visits;
  URLRow row = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits);
  syncer::SyncDataList initial_sync_data;
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();
  WriteToTypedUrlSpecifics(row, visits, typed_url);
  syncer::SyncData data =
      syncer::SyncData::CreateLocalData(kURL, kTitle, entity_specifics);

  initial_sync_data.push_back(data);
  StartSyncing(initial_sync_data);
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  EXPECT_TRUE(changes.empty());

  // Check that the local cache is updated correctly.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
  EXPECT_EQ(sync_state.count(row.url()), 1U);

  // Check that the backend was updated correctly.
  VisitVector all_visits;
  base::Time server_time = base::Time::FromInternalValue(3);
  URLID url_id = fake_history_backend_->GetIdByUrl(GURL(kURL));
  ASSERT_NE(0, url_id);
  fake_history_backend_->GetVisitsForURL(url_id, &all_visits);
  ASSERT_EQ(1U, all_visits.size());
  EXPECT_EQ(server_time, all_visits[0].visit_time);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      all_visits[0].transition, visits[0].transition));
}

// Add a url to the local and sync data before sync begins, with the sync data
// having more recent visits. Check that starting sync updates the backend
// with the sync visit, while the older local visit is not pushed to sync.
// The title should be updated to the sync version due to the more recent
// timestamp.
TEST_F(TypedUrlSyncableServiceTest, MergeUrlOldLocal) {
  // Add a url to backend.
  VisitVector visits;
  URLRow local_row = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits);
  fake_history_backend_->SetVisitsForUrl(local_row, visits);

  // Create sync data for the same url with a more recent visit.
  VisitVector server_visits;
  URLRow server_row =
      MakeTypedUrlRow(kURL, kTitle2, 1, 6, false, &server_visits);
  server_row.set_id(fake_history_backend_->GetIdByUrl(GURL(kURL)));
  syncer::SyncDataList initial_sync_data;
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();
  WriteToTypedUrlSpecifics(server_row, server_visits, typed_url);
  syncer::SyncData data =
      syncer::SyncData::CreateLocalData(kURL, kTitle2, entity_specifics);

  initial_sync_data.push_back(data);
  StartSyncing(initial_sync_data);

  // Check that the local cache was updated correctly.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());

  // Check that the backend was updated correctly.
  VisitVector all_visits;
  base::Time server_time = base::Time::FromInternalValue(6);
  URLID url_id = fake_history_backend_->GetIdByUrl(GURL(kURL));
  ASSERT_NE(0, url_id);
  fake_history_backend_->GetVisitsForURL(url_id, &all_visits);
  ASSERT_EQ(2U, all_visits.size());
  EXPECT_EQ(server_time, all_visits.back().visit_time);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      all_visits.back().transition, server_visits[0].transition));
  URLRow url_row;
  EXPECT_TRUE(fake_history_backend_->GetURL(GURL(kURL), &url_row));
  EXPECT_EQ(kTitle2, base::UTF16ToUTF8(url_row.title()));

  // Check that the server was updated correctly.
  // The local history visit should not be added to sync because it is older
  // than sync's oldest visit.
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  ASSERT_EQ(1U, changes.size());
  ASSERT_TRUE(changes[0].IsValid());
  sync_pb::TypedUrlSpecifics url_specifics =
      changes[0].sync_data().GetSpecifics().typed_url();
  ASSERT_EQ(1, url_specifics.visits_size());
  EXPECT_EQ(6, url_specifics.visits(0));
  ASSERT_EQ(1, url_specifics.visit_transitions_size());
  EXPECT_EQ(static_cast<const int>(visits[0].transition),
            url_specifics.visit_transitions(0));
}

// Add a url to the local and sync data before sync begins, with the local data
// having more recent visits. Check that starting sync updates the sync
// with the local visits, while the older sync visit is not pushed to the
// backend. Sync's title should be updated to the local version due to the more
// recent timestamp.
TEST_F(TypedUrlSyncableServiceTest, MergeUrlOldSync) {
  // Add a url to backend.
  VisitVector visits;
  URLRow local_row = MakeTypedUrlRow(kURL, kTitle2, 1, 3, false, &visits);
  fake_history_backend_->SetVisitsForUrl(local_row, visits);

  // Create sync data for the same url with an older visit.
  VisitVector server_visits;
  URLRow server_row =
      MakeTypedUrlRow(kURL, kTitle, 1, 2, false, &server_visits);
  syncer::SyncDataList initial_sync_data;
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();
  WriteToTypedUrlSpecifics(server_row, server_visits, typed_url);
  syncer::SyncData data =
      syncer::SyncData::CreateLocalData(kURL, kTitle, entity_specifics);

  initial_sync_data.push_back(data);
  StartSyncing(initial_sync_data);

  // Check that the local cache was updated correctly.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());

  // Check that the backend was not updated.
  VisitVector all_visits;
  base::Time local_visit_time = base::Time::FromInternalValue(3);
  URLID url_id = fake_history_backend_->GetIdByUrl(GURL(kURL));
  ASSERT_NE(0, url_id);
  fake_history_backend_->GetVisitsForURL(url_id, &all_visits);
  ASSERT_EQ(1U, all_visits.size());
  EXPECT_EQ(local_visit_time, all_visits[0].visit_time);

  // Check that the server was updated correctly.
  // The local history visit should not be added to sync because it is older
  // than sync's oldest visit.
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  ASSERT_EQ(1U, changes.size());
  ASSERT_TRUE(changes[0].IsValid());
  sync_pb::TypedUrlSpecifics url_specifics =
      changes[0].sync_data().GetSpecifics().typed_url();
  ASSERT_EQ(1, url_specifics.visits_size());
  EXPECT_EQ(3, url_specifics.visits(0));
  EXPECT_EQ(kTitle2, url_specifics.title());
  ASSERT_EQ(1, url_specifics.visit_transitions_size());
  EXPECT_EQ(static_cast<const int>(visits[0].transition),
            url_specifics.visit_transitions(0));
}

// Create a remote typed URL and visit, then send to syncable service after sync
// has started. Check that local DB is received the new URL and visit.
TEST_F(TypedUrlSyncableServiceTest, AddUrlAndVisits) {
  StartSyncing(syncer::SyncDataList());
  VisitVector visits = ApplyUrlAndVisitsChange(kURL, kTitle, 1, 3, false,
                                               syncer::SyncChange::ACTION_ADD);

  base::Time visit_time = base::Time::FromInternalValue(3);
  VisitVector all_visits;
  URLID url_id = fake_history_backend_->GetIdByUrl(GURL(kURL));
  ASSERT_NE(0, url_id);
  fake_history_backend_->GetVisitsForURL(url_id, &all_visits);
  EXPECT_EQ(1U, all_visits.size());
  EXPECT_EQ(visit_time, all_visits[0].visit_time);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      all_visits[0].transition, visits[0].transition));
  URLRow url_row;
  EXPECT_TRUE(fake_history_backend_->GetURL(GURL(kURL), &url_row));
  EXPECT_EQ(kTitle, base::UTF16ToUTF8(url_row.title()));
}

// Update a remote typed URL and create a new visit that is already synced, then
// send the update to syncable service. Check that local DB is received an
// UPDATE for the existing url and new visit.
TEST_F(TypedUrlSyncableServiceTest, UpdateUrlAndVisits) {
  StartSyncing(syncer::SyncDataList());

  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_EQ(0U, sync_state.size());

  VisitVector visits = ApplyUrlAndVisitsChange(kURL, kTitle, 1, 3, false,
                                               syncer::SyncChange::ACTION_ADD);
  base::Time visit_time = base::Time::FromInternalValue(3);
  VisitVector all_visits;
  URLRow url_row;

  URLID url_id = fake_history_backend_->GetIdByUrl(GURL(kURL));
  ASSERT_NE(0, url_id);

  fake_history_backend_->GetVisitsForURL(url_id, &all_visits);
  sync_state.clear();
  GetSyncedUrls(&sync_state);
  EXPECT_EQ(1U, all_visits.size());
  EXPECT_EQ(visit_time, all_visits[0].visit_time);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      all_visits[0].transition, visits[0].transition));
  EXPECT_EQ(1U, sync_state.size());
  EXPECT_TRUE(fake_history_backend_->GetURL(GURL(kURL), &url_row));
  EXPECT_EQ(kTitle, base::UTF16ToUTF8(url_row.title()));

  VisitVector new_visits = ApplyUrlAndVisitsChange(
      kURL, kTitle2, 2, 6, false, syncer::SyncChange::ACTION_UPDATE);

  sync_state.clear();
  GetSyncedUrls(&sync_state);
  EXPECT_EQ(1U, sync_state.size());
  base::Time new_visit_time = base::Time::FromInternalValue(6);
  url_id = fake_history_backend_->GetIdByUrl(GURL(kURL));
  ASSERT_NE(0, url_id);
  fake_history_backend_->GetVisitsForURL(url_id, &all_visits);

  EXPECT_EQ(2U, all_visits.size());
  EXPECT_EQ(new_visit_time, all_visits.back().visit_time);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      all_visits.back().transition, new_visits[0].transition));
  EXPECT_TRUE(fake_history_backend_->GetURL(GURL(kURL), &url_row));
  EXPECT_EQ(kTitle2, base::UTF16ToUTF8(url_row.title()));
}

// Delete a typed urls which already synced. Check that local DB receives the
// DELETE changes.
TEST_F(TypedUrlSyncableServiceTest, DeleteUrlAndVisits) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  std::set<GURL> sync_state;
  urls.push_back(kURL);

  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(BuildAndPushLocalChanges(1, 0, urls, &url_rows, &visit_vectors));
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  changes.clear();

  base::Time visit_time = base::Time::FromInternalValue(3);
  VisitVector all_visits;
  URLID url_id = fake_history_backend_->GetIdByUrl(GURL(kURL));
  ASSERT_NE(0, url_id);
  fake_history_backend_->GetVisitsForURL(url_id, &all_visits);
  EXPECT_EQ(1U, all_visits.size());
  EXPECT_EQ(visit_time, all_visits[0].visit_time);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      all_visits[0].transition, visit_vectors[0][0].transition));
  URLRow url_row;
  EXPECT_TRUE(fake_history_backend_->GetURL(GURL(kURL), &url_row));
  EXPECT_EQ(kTitle, base::UTF16ToUTF8(url_row.title()));
  GetSyncedUrls(&sync_state);
  EXPECT_EQ(1U, sync_state.size());

  // Add observer back to check if TypedUrlSyncableService receive delete
  // changes back from fake_history_backend_.
  AddObserver();

  ApplyUrlAndVisitsChange(kURL, kTitle, 1, 3, false,
                          syncer::SyncChange::ACTION_DELETE);

  sync_state.clear();
  GetSyncedUrls(&sync_state);
  EXPECT_EQ(0U, sync_state.size());
  EXPECT_FALSE(fake_history_backend_->GetURL(GURL(kURL), &url_row));
  url_id = fake_history_backend_->GetIdByUrl(GURL(kURL));
  ASSERT_EQ(0, url_id);

  // Check TypedUrlSyncableService did not receive update since the update is
  // trigered by it.
  ASSERT_EQ(0u, changes.size());
}

// Create two set of visits for history DB and sync DB, two same set of visits
// are same. Check DiffVisits will return empty set of diff visits.
TEST_F(TypedUrlSyncableServiceTest, DiffVisitsSame) {
  history::VisitVector old_visits;
  sync_pb::TypedUrlSpecifics new_url;

  const int64_t visits[] = {1024, 2065, 65534, 1237684};

  for (int64_t visit : visits) {
    old_visits.push_back(history::VisitRow(0,
                                           base::Time::FromInternalValue(visit),
                                           0, ui::PAGE_TRANSITION_TYPED, 0));
    new_url.add_visits(visit);
    new_url.add_visit_transitions(ui::PAGE_TRANSITION_TYPED);
  }

  std::vector<history::VisitInfo> new_visits;
  history::VisitVector removed_visits;

  DiffVisits(old_visits, new_url, &new_visits, &removed_visits);
  EXPECT_TRUE(new_visits.empty());
  EXPECT_TRUE(removed_visits.empty());
}

// Create two set of visits for history DB and sync DB. Check DiffVisits will
// return correct set of diff visits.
TEST_F(TypedUrlSyncableServiceTest, DiffVisitsRemove) {
  history::VisitVector old_visits;
  sync_pb::TypedUrlSpecifics new_url;

  const int64_t visits_left[] = {1,    2,     1024,    1500,   2065,
                                 6000, 65534, 1237684, 2237684};
  const int64_t visits_right[] = {1024, 2065, 65534, 1237684};

  // DiffVisits will not remove the first visit, because we never delete visits
  // from the start of the array (since those visits can get truncated by the
  // size-limiting code).
  const int64_t visits_removed[] = {1500, 6000, 2237684};

  for (int64_t visit : visits_left) {
    old_visits.push_back(history::VisitRow(0,
                                           base::Time::FromInternalValue(visit),
                                           0, ui::PAGE_TRANSITION_TYPED, 0));
  }

  for (int64_t visit : visits_right) {
    new_url.add_visits(visit);
    new_url.add_visit_transitions(ui::PAGE_TRANSITION_TYPED);
  }

  std::vector<history::VisitInfo> new_visits;
  history::VisitVector removed_visits;

  DiffVisits(old_visits, new_url, &new_visits, &removed_visits);
  EXPECT_TRUE(new_visits.empty());
  ASSERT_EQ(removed_visits.size(), arraysize(visits_removed));
  for (size_t i = 0; i < arraysize(visits_removed); ++i) {
    EXPECT_EQ(removed_visits[i].visit_time.ToInternalValue(),
              visits_removed[i]);
  }
}

// Create two set of visits for history DB and sync DB. Check DiffVisits will
// return correct set of diff visits.
TEST_F(TypedUrlSyncableServiceTest, DiffVisitsAdd) {
  history::VisitVector old_visits;
  sync_pb::TypedUrlSpecifics new_url;

  const int64_t visits_left[] = {1024, 2065, 65534, 1237684};
  const int64_t visits_right[] = {1,    1024,  1500,    2065,
                                  6000, 65534, 1237684, 2237684};

  const int64_t visits_added[] = {1, 1500, 6000, 2237684};

  for (int64_t visit : visits_left) {
    old_visits.push_back(history::VisitRow(0,
                                           base::Time::FromInternalValue(visit),
                                           0, ui::PAGE_TRANSITION_TYPED, 0));
  }

  for (int64_t visit : visits_right) {
    new_url.add_visits(visit);
    new_url.add_visit_transitions(ui::PAGE_TRANSITION_TYPED);
  }

  std::vector<history::VisitInfo> new_visits;
  history::VisitVector removed_visits;

  DiffVisits(old_visits, new_url, &new_visits, &removed_visits);
  EXPECT_TRUE(removed_visits.empty());
  ASSERT_TRUE(new_visits.size() == arraysize(visits_added));
  for (size_t i = 0; i < arraysize(visits_added); ++i) {
    EXPECT_EQ(new_visits[i].first.ToInternalValue(), visits_added[i]);
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        new_visits[i].second, ui::PAGE_TRANSITION_TYPED));
  }
}

// Create three visits, check RELOAD visit is removed by
// WriteToTypedUrlSpecifics so it won't apply to sync DB.
TEST_F(TypedUrlSyncableServiceTest, WriteTypedUrlSpecifics) {
  history::VisitVector visits;
  visits.push_back(CreateVisit(ui::PAGE_TRANSITION_TYPED, 1));
  visits.push_back(CreateVisit(ui::PAGE_TRANSITION_RELOAD, 2));
  visits.push_back(CreateVisit(ui::PAGE_TRANSITION_LINK, 3));

  history::URLRow url(MakeTypedUrlRow(kURL, kTitle, 0, 100, false, &visits));
  sync_pb::TypedUrlSpecifics typed_url;
  WriteToTypedUrlSpecifics(url, visits, &typed_url);
  // RELOAD visits should be removed.
  EXPECT_EQ(2, typed_url.visits_size());
  EXPECT_EQ(typed_url.visit_transitions_size(), typed_url.visits_size());
  EXPECT_EQ(1, typed_url.visits(0));
  EXPECT_EQ(3, typed_url.visits(1));
  EXPECT_EQ(static_cast<int32_t>(ui::PAGE_TRANSITION_TYPED),
            typed_url.visit_transitions(0));
  EXPECT_EQ(static_cast<int32_t>(ui::PAGE_TRANSITION_LINK),
            typed_url.visit_transitions(1));
}

// Create 101 visits, check WriteToTypedUrlSpecifics will only keep 100 visits.
TEST_F(TypedUrlSyncableServiceTest, TooManyVisits) {
  history::VisitVector visits;
  int64_t timestamp = 1000;
  visits.push_back(CreateVisit(ui::PAGE_TRANSITION_TYPED, timestamp++));
  for (int i = 0; i < 100; ++i) {
    visits.push_back(CreateVisit(ui::PAGE_TRANSITION_LINK, timestamp++));
  }
  history::URLRow url(
      MakeTypedUrlRow(kURL, kTitle, 0, timestamp++, false, &visits));
  sync_pb::TypedUrlSpecifics typed_url;
  WriteToTypedUrlSpecifics(url, visits, &typed_url);
  // # visits should be capped at 100.
  EXPECT_EQ(100, typed_url.visits_size());
  EXPECT_EQ(typed_url.visit_transitions_size(), typed_url.visits_size());
  EXPECT_EQ(1000, typed_url.visits(0));
  // Visit with timestamp of 1001 should be omitted since we should have
  // skipped that visit to stay under the cap.
  EXPECT_EQ(1002, typed_url.visits(1));
  EXPECT_EQ(static_cast<int32_t>(ui::PAGE_TRANSITION_TYPED),
            typed_url.visit_transitions(0));
  EXPECT_EQ(static_cast<int32_t>(ui::PAGE_TRANSITION_LINK),
            typed_url.visit_transitions(1));
}

// Create 306 visits, check WriteToTypedUrlSpecifics will only keep 100 typed
// visits.
TEST_F(TypedUrlSyncableServiceTest, TooManyTypedVisits) {
  history::VisitVector visits;
  int64_t timestamp = 1000;
  for (int i = 0; i < 102; ++i) {
    visits.push_back(CreateVisit(ui::PAGE_TRANSITION_TYPED, timestamp++));
    visits.push_back(CreateVisit(ui::PAGE_TRANSITION_LINK, timestamp++));
    visits.push_back(CreateVisit(ui::PAGE_TRANSITION_RELOAD, timestamp++));
  }
  history::URLRow url(
      MakeTypedUrlRow(kURL, kTitle, 0, timestamp++, false, &visits));
  sync_pb::TypedUrlSpecifics typed_url;
  WriteToTypedUrlSpecifics(url, visits, &typed_url);
  // # visits should be capped at 100.
  EXPECT_EQ(100, typed_url.visits_size());
  EXPECT_EQ(typed_url.visit_transitions_size(), typed_url.visits_size());
  // First two typed visits should be skipped.
  EXPECT_EQ(1006, typed_url.visits(0));

  // Ensure there are no non-typed visits since that's all that should fit.
  for (int i = 0; i < typed_url.visits_size(); ++i) {
    EXPECT_EQ(static_cast<int32_t>(ui::PAGE_TRANSITION_TYPED),
              typed_url.visit_transitions(i));
  }
}

// Create a typed url without visit, check WriteToTypedUrlSpecifics will create
// a RELOAD visit for it.
TEST_F(TypedUrlSyncableServiceTest, NoTypedVisits) {
  history::VisitVector visits;
  history::URLRow url(MakeTypedUrlRow(kURL, kTitle, 0, 1000, false, &visits));
  sync_pb::TypedUrlSpecifics typed_url;
  WriteToTypedUrlSpecifics(url, visits, &typed_url);
  // URLs with no typed URL visits should be translated to a URL with one
  // reload visit.
  EXPECT_EQ(1, typed_url.visits_size());
  EXPECT_EQ(typed_url.visit_transitions_size(), typed_url.visits_size());

  EXPECT_EQ(1000, typed_url.visits(0));
  EXPECT_EQ(static_cast<int32_t>(ui::PAGE_TRANSITION_RELOAD),
            typed_url.visit_transitions(0));
}

TEST_F(TypedUrlSyncableServiceTest, MergeUrls) {
  history::VisitVector visits1;
  history::URLRow row1(MakeTypedUrlRow(kURL, kTitle, 2, 3, false, &visits1));
  sync_pb::TypedUrlSpecifics specs1(
      MakeTypedUrlSpecifics(kURL, kTitle, 3, false));
  history::URLRow new_row1((GURL(kURL)));
  std::vector<history::VisitInfo> new_visits1;
  EXPECT_TRUE(TypedUrlSyncableServiceTest::MergeUrls(specs1, row1, &visits1,
                                                     &new_row1, &new_visits1) ==
              TypedUrlSyncableServiceTest::DIFF_NONE);

  history::VisitVector visits2;
  history::URLRow row2(MakeTypedUrlRow(kURL, kTitle, 2, 3, false, &visits2));
  sync_pb::TypedUrlSpecifics specs2(
      MakeTypedUrlSpecifics(kURL, kTitle, 3, true));
  history::VisitVector expected_visits2;
  history::URLRow expected2(
      MakeTypedUrlRow(kURL, kTitle, 2, 3, true, &expected_visits2));
  history::URLRow new_row2((GURL(kURL)));
  std::vector<history::VisitInfo> new_visits2;
  EXPECT_TRUE(TypedUrlSyncableServiceTest::MergeUrls(specs2, row2, &visits2,
                                                     &new_row2, &new_visits2) ==
              TypedUrlSyncableServiceTest::DIFF_LOCAL_ROW_CHANGED);
  EXPECT_TRUE(URLsEqual(new_row2, expected2));

  history::VisitVector visits3;
  history::URLRow row3(MakeTypedUrlRow(kURL, kTitle, 2, 3, false, &visits3));
  sync_pb::TypedUrlSpecifics specs3(
      MakeTypedUrlSpecifics(kURL, kTitle2, 3, true));
  history::VisitVector expected_visits3;
  history::URLRow expected3(
      MakeTypedUrlRow(kURL, kTitle2, 2, 3, true, &expected_visits3));
  history::URLRow new_row3((GURL(kURL)));
  std::vector<history::VisitInfo> new_visits3;
  EXPECT_EQ(TypedUrlSyncableServiceTest::DIFF_LOCAL_ROW_CHANGED |
                TypedUrlSyncableServiceTest::DIFF_NONE,
            TypedUrlSyncableServiceTest::MergeUrls(specs3, row3, &visits3,
                                                   &new_row3, &new_visits3));
  EXPECT_TRUE(URLsEqual(new_row3, expected3));

  // Create one node in history DB with timestamp of 3, and one node in sync
  // DB with timestamp of 4. Result should contain one new item (4).
  history::VisitVector visits4;
  history::URLRow row4(MakeTypedUrlRow(kURL, kTitle, 2, 3, false, &visits4));
  sync_pb::TypedUrlSpecifics specs4(
      MakeTypedUrlSpecifics(kURL, kTitle2, 4, false));
  history::VisitVector expected_visits4;
  history::URLRow expected4(
      MakeTypedUrlRow(kURL, kTitle2, 2, 4, false, &expected_visits4));
  history::URLRow new_row4((GURL(kURL)));
  std::vector<history::VisitInfo> new_visits4;
  EXPECT_EQ(TypedUrlSyncableServiceTest::DIFF_UPDATE_NODE |
                TypedUrlSyncableServiceTest::DIFF_LOCAL_ROW_CHANGED |
                TypedUrlSyncableServiceTest::DIFF_LOCAL_VISITS_ADDED,
            TypedUrlSyncableServiceTest::MergeUrls(specs4, row4, &visits4,
                                                   &new_row4, &new_visits4));
  EXPECT_EQ(1U, new_visits4.size());
  EXPECT_EQ(specs4.visits(0), new_visits4[0].first.ToInternalValue());
  EXPECT_TRUE(URLsEqual(new_row4, expected4));
  EXPECT_EQ(2U, visits4.size());

  history::VisitVector visits5;
  history::URLRow row5(MakeTypedUrlRow(kURL, kTitle, 1, 4, false, &visits5));
  sync_pb::TypedUrlSpecifics specs5(
      MakeTypedUrlSpecifics(kURL, kTitle, 3, false));
  history::VisitVector expected_visits5;
  history::URLRow expected5(
      MakeTypedUrlRow(kURL, kTitle, 2, 3, false, &expected_visits5));
  history::URLRow new_row5((GURL(kURL)));
  std::vector<history::VisitInfo> new_visits5;

  // UPDATE_NODE should be set because row5 has a newer last_visit timestamp.
  EXPECT_EQ(TypedUrlSyncableServiceTest::DIFF_UPDATE_NODE |
                TypedUrlSyncableServiceTest::DIFF_NONE,
            TypedUrlSyncableServiceTest::MergeUrls(specs5, row5, &visits5,
                                                   &new_row5, &new_visits5));
  EXPECT_TRUE(URLsEqual(new_row5, expected5));
  EXPECT_EQ(0U, new_visits5.size());
}

TEST_F(TypedUrlSyncableServiceTest, MergeUrlsAfterExpiration) {
  // Tests to ensure that we don't resurrect expired URLs (URLs that have been
  // deleted from the history DB but still exist in the sync DB).

  // First, create a history row that has two visits, with timestamps 2 and 3.
  history::VisitVector(history_visits);
  history_visits.push_back(history::VisitRow(
      0, base::Time::FromInternalValue(2), 0, ui::PAGE_TRANSITION_TYPED, 0));
  history::URLRow history_url(
      MakeTypedUrlRow(kURL, kTitle, 2, 3, false, &history_visits));

  // Now, create a sync node with visits at timestamps 1, 2, 3, 4.
  sync_pb::TypedUrlSpecifics node(
      MakeTypedUrlSpecifics(kURL, kTitle, 1, false));
  node.add_visits(2);
  node.add_visits(3);
  node.add_visits(4);
  node.add_visit_transitions(2);
  node.add_visit_transitions(3);
  node.add_visit_transitions(4);
  history::URLRow new_history_url(history_url.url());
  std::vector<history::VisitInfo> new_visits;
  EXPECT_EQ(
      TypedUrlSyncableServiceTest::DIFF_NONE |
          TypedUrlSyncableServiceTest::DIFF_LOCAL_VISITS_ADDED,
      TypedUrlSyncableServiceTest::MergeUrls(node, history_url, &history_visits,
                                             &new_history_url, &new_visits));
  EXPECT_TRUE(URLsEqual(history_url, new_history_url));
  EXPECT_EQ(1U, new_visits.size());
  EXPECT_EQ(4U, new_visits[0].first.ToInternalValue());
  // We should not sync the visit with timestamp #1 since it is earlier than
  // any other visit for this URL in the history DB. But we should sync visit
  // #4.
  EXPECT_EQ(3U, history_visits.size());
  EXPECT_EQ(2U, history_visits[0].visit_time.ToInternalValue());
  EXPECT_EQ(3U, history_visits[1].visit_time.ToInternalValue());
  EXPECT_EQ(4U, history_visits[2].visit_time.ToInternalValue());
}

}  // namespace history
