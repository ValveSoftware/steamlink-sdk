// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/browser_sync/browser/abstract_profile_sync_service_test.h"
#include "components/browser_sync/browser/test_profile_sync_service.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/history_backend_notifier.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/typed_url_data_type_controller.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_driver/data_type_manager_impl.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/test/data_type_error_handler_mock.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/protocol/typed_url_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using browser_sync::TypedUrlDataTypeController;
using history::HistoryBackend;
using history::HistoryBackendNotifier;
using history::TypedUrlSyncableService;
using testing::DoAll;
using testing::Return;
using testing::SetArgumentPointee;
using testing::_;

namespace {

const char kDummySavingBrowserHistoryDisabled[] = "dummyPref";

// Visits with this timestamp are treated as expired.
static const int EXPIRED_VISIT = -1;

ACTION(ReturnNewDataTypeManager) {
  return new sync_driver::DataTypeManagerImpl(arg0, arg1, arg2, arg3, arg4);
}

class HistoryBackendMock : public HistoryBackend {
 public:
  HistoryBackendMock()
      : HistoryBackend(nullptr, nullptr, base::ThreadTaskRunnerHandle::Get()) {}
  bool IsExpiredVisitTime(const base::Time& time) override {
    return time.ToInternalValue() == EXPIRED_VISIT;
  }
  MOCK_METHOD1(GetAllTypedURLs, bool(history::URLRows* entries));
  MOCK_METHOD3(GetMostRecentVisitsForURL, bool(history::URLID id,
                                               int max_visits,
                                               history::VisitVector* visits));
  MOCK_METHOD2(UpdateURL, bool(history::URLID id, const history::URLRow& url));
  MOCK_METHOD3(AddVisits, bool(const GURL& url,
                               const std::vector<history::VisitInfo>& visits,
                               history::VisitSource visit_source));
  MOCK_METHOD2(GetURL, bool(const GURL& url_id, history::URLRow* url_row));
  MOCK_METHOD2(SetPageTitle, void(const GURL& url,
                                  const base::string16& title));
  MOCK_METHOD1(DeleteURL, void(const GURL& url));

 private:
  friend class ProfileSyncServiceTypedUrlTest;

  virtual ~HistoryBackendMock() {}
};

class HistoryServiceMock : public history::HistoryService {
 public:
  HistoryServiceMock() : history::HistoryService(), backend_(nullptr) {}

  base::CancelableTaskTracker::TaskId ScheduleDBTask(
      std::unique_ptr<history::HistoryDBTask> task,
      base::CancelableTaskTracker* tracker) override {
    // Explicitly copy out the raw pointer -- compilers might decide to
    // evaluate task.release() before the arguments for the first Bind().
    history::HistoryDBTask* task_raw = task.get();
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::Bind(&HistoryServiceMock::RunTaskOnDBThread,
                   base::Unretained(this), task_raw),
        base::Bind(&base::DeletePointer<history::HistoryDBTask>,
                   task.release()));
    return base::CancelableTaskTracker::kBadTaskId;  // unused
  }

  ~HistoryServiceMock() override {}

  void set_task_runner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    DCHECK(task_runner.get());
    task_runner_ = task_runner;
  }

  void set_backend(scoped_refptr<history::HistoryBackend> backend) {
    backend_ = backend;
  }

 private:
  void RunTaskOnDBThread(history::HistoryDBTask* task) {
    EXPECT_TRUE(task->RunOnDBThread(backend_.get(), NULL));
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<history::HistoryBackend> backend_;
};

class TestTypedUrlSyncableService : public TypedUrlSyncableService {
  // TODO(gangwu): remove TestProfileSyncService or even remove whole test
  // suite, and make sure typed_url_syncable_service_unittest.cc and the various
  // typed url integration tests.
 public:
  explicit TestTypedUrlSyncableService(history::HistoryBackend* history_backend)
      : TypedUrlSyncableService(history_backend) {}

  static void WriteToSyncNode(const history::URLRow& url,
                              const history::VisitVector& visits,
                              syncer::WriteNode* node) {
    sync_pb::TypedUrlSpecifics typed_url;
    WriteToTypedUrlSpecifics(url, visits, &typed_url);
    node->SetTypedUrlSpecifics(typed_url);
  }

 protected:
  // Don't clear error stats - that way we can verify their values in our
  // tests.
  void ClearErrorStats() override {}
};

class ProfileSyncServiceTypedUrlTest : public AbstractProfileSyncServiceTest {
 public:
  void AddTypedUrlSyncNode(const history::URLRow& url,
                           const history::VisitVector& visits) {
    syncer::WriteTransaction trans(FROM_HERE, sync_service()->GetUserShare());

    syncer::WriteNode node(&trans);
    std::string tag = url.url().spec();
    syncer::WriteNode::InitUniqueByCreationResult result =
        node.InitUniqueByCreation(syncer::TYPED_URLS, tag);
    ASSERT_EQ(syncer::WriteNode::INIT_SUCCESS, result);
    TestTypedUrlSyncableService::WriteToSyncNode(url, visits, &node);
  }

 protected:
  ProfileSyncServiceTypedUrlTest() {
    profile_sync_service_bundle()
        ->pref_service()
        ->registry()
        ->RegisterBooleanPref(kDummySavingBrowserHistoryDisabled, false);

    data_type_thread()->Start();
    base::RunLoop run_loop;
    data_type_thread()->task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::Bind(&ProfileSyncServiceTypedUrlTest::CreateHistoryService,
                   base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();
    history_service_ = base::WrapUnique(new HistoryServiceMock);
    history_service_->set_task_runner(data_type_thread()->task_runner());
    history_service_->set_backend(history_backend_);

    browser_sync::ProfileSyncServiceBundle::SyncClientBuilder builder(
        profile_sync_service_bundle());
    builder.SetHistoryService(history_service_.get());
    builder.SetSyncServiceCallback(GetSyncServiceCallback());
    builder.SetSyncableServiceCallback(
        base::Bind(&ProfileSyncServiceTypedUrlTest::GetSyncableServiceForType,
                   base::Unretained(this)));
    builder.set_activate_model_creation();
    sync_client_ = builder.Build();
  }

  void CreateHistoryService() {
    history_backend_ = new HistoryBackendMock();
    syncable_service_ = base::WrapUnique(
        new TestTypedUrlSyncableService(history_backend_.get()));
  }

  void DeleteSyncableService() {
    syncable_service_.reset();
    history_backend_ = nullptr;
  }

  ~ProfileSyncServiceTypedUrlTest() override {
    history_service_->Shutdown();

    // Request stop to get deletion tasks related to the HistoryService posted
    // on the Sync thread. It is important to not Shutdown at this moment,
    // because after shutdown the Sync thread is not returned to the sync
    // service, so we could not get the thread's message loop to wait for the
    // deletions to be finished.
    sync_service()->RequestStop(sync_driver::SyncService::CLEAR_DATA);
    // Spin the sync thread.
    {
      base::RunLoop run_loop;
      sync_service()->GetSyncLoopForTest()->task_runner()->PostTaskAndReply(
          FROM_HERE, base::Bind(&base::DoNothing), run_loop.QuitClosure());
      run_loop.Run();
    }

    // Spin the loop again for deletion tasks posted from the Sync thread.
    base::RunLoop().RunUntilIdle();

    {
      base::RunLoop run_loop;
      data_type_thread()->task_runner()->PostTaskAndReply(
          FROM_HERE,
          base::Bind(&ProfileSyncServiceTypedUrlTest::DeleteSyncableService,
                     base::Unretained(this)),
          run_loop.QuitClosure());
      run_loop.Run();
    }
  }

  TypedUrlSyncableService* StartSyncService(const base::Closure& callback) {
    if (!sync_service()) {
      std::string account_id =
          profile_sync_service_bundle()->account_tracker()->SeedAccountInfo(
              "gaia_id", "test");
      SigninManagerBase* signin =
          profile_sync_service_bundle()->signin_manager();
      signin->SetAuthenticatedAccountInfo("gaia_id", "test");
      CreateSyncService(std::move(sync_client_), callback);
      data_type_controller = new TypedUrlDataTypeController(
          base::ThreadTaskRunnerHandle::Get(), base::Bind(&base::DoNothing),
          sync_service()->GetSyncClient(), kDummySavingBrowserHistoryDisabled);
      EXPECT_CALL(*profile_sync_service_bundle()->component_factory(),
                  CreateDataTypeManager(_, _, _, _, _))
          .WillOnce(ReturnNewDataTypeManager());

      profile_sync_service_bundle()->auth_service()->UpdateCredentials(
          account_id, "oauth2_login_token");

      sync_service()->RegisterDataTypeController(data_type_controller);

      sync_service()->Initialize();
      base::RunLoop().Run();
    }
    return syncable_service_.get();
  }

  void GetTypedUrlsFromSyncDB(history::URLRows* urls) {
    urls->clear();
    syncer::ReadTransaction trans(FROM_HERE, sync_service()->GetUserShare());
    syncer::ReadNode typed_url_root(&trans);
    if (typed_url_root.InitTypeRoot(syncer::TYPED_URLS) !=
        syncer::BaseNode::INIT_OK)
      return;

    int64_t child_id = typed_url_root.GetFirstChildId();
    while (child_id != syncer::kInvalidId) {
      syncer::ReadNode child_node(&trans);
      if (child_node.InitByIdLookup(child_id) != syncer::BaseNode::INIT_OK)
        return;

      const sync_pb::TypedUrlSpecifics& typed_url(
          child_node.GetTypedUrlSpecifics());
      history::URLRow new_url(GURL(typed_url.url()));

      new_url.set_title(base::UTF8ToUTF16(typed_url.title()));
      DCHECK(typed_url.visits_size());
      DCHECK_EQ(typed_url.visits_size(), typed_url.visit_transitions_size());
      new_url.set_last_visit(base::Time::FromInternalValue(
          typed_url.visits(typed_url.visits_size() - 1)));
      new_url.set_hidden(typed_url.hidden());

      urls->push_back(new_url);
      child_id = child_node.GetSuccessorId();
    }
  }

  void SetIdleChangeProcessorExpectations() {
    EXPECT_CALL((history_backend()), SetPageTitle(_, _)).Times(0);
    EXPECT_CALL((history_backend()), UpdateURL(_, _)).Times(0);
    EXPECT_CALL((history_backend()), GetURL(_, _)).Times(0);
    EXPECT_CALL((history_backend()), DeleteURL(_)).Times(0);
  }

  void SendNotification(const base::Closure& task) {
    data_type_thread()->task_runner()->PostTaskAndReply(
        FROM_HERE, task,
        base::Bind(&base::MessageLoop::QuitNow,
                   base::Unretained(base::MessageLoop::current())));
    base::RunLoop().Run();
  }

  void SendNotificationURLVisited(ui::PageTransition transition,
                                  const history::URLRow& row) {
    base::Time visit_time;
    history::RedirectList redirects;
    SendNotification(
        base::Bind(&HistoryBackendNotifier::NotifyURLVisited,
                   base::Unretained(history_backend_.get()),
                   transition,
                   row,
                   redirects,
                   visit_time));
  }

  void SendNotificationURLsModified(const history::URLRows& rows) {
    SendNotification(base::Bind(&HistoryBackendNotifier::NotifyURLsModified,
                                base::Unretained(history_backend_.get()),
                                rows));
  }

  void SendNotificationURLsDeleted(bool all_history,
                                   bool expired,
                                   const history::URLRows& deleted_rows,
                                   const std::set<GURL>& favicon_urls) {
    SendNotification(base::Bind(&HistoryBackendNotifier::NotifyURLsDeleted,
                                base::Unretained(history_backend_.get()),
                                all_history, expired, deleted_rows,
                                favicon_urls));
  }

  static bool URLsEqual(const history::URLRow& lhs,
                        const history::URLRow& rhs) {
    // Only verify the fields we explicitly sync (i.e. don't verify typed_count
    // or visit_count because we rely on the history DB to manage those values
    // and they are left unchanged by HistoryBackendMock).
    return (lhs.url().spec().compare(rhs.url().spec()) == 0) &&
           (lhs.title().compare(rhs.title()) == 0) &&
           (lhs.last_visit() == rhs.last_visit()) &&
           (lhs.hidden() == rhs.hidden());
  }

  static history::URLRow MakeTypedUrlEntry(const char* url,
                                           const char* title,
                                           int typed_count,
                                           int64_t last_visit,
                                           bool hidden,
                                           history::VisitVector* visits) {
    // Give each URL a unique ID, to mimic the behavior of the real database.
    static int unique_url_id = 0;
    GURL gurl(url);
    history::URLRow history_url(gurl, ++unique_url_id);
    history_url.set_title(base::UTF8ToUTF16(title));
    history_url.set_typed_count(typed_count);
    history_url.set_last_visit(
        base::Time::FromInternalValue(last_visit));
    history_url.set_hidden(hidden);
    visits->push_back(history::VisitRow(
        history_url.id(), history_url.last_visit(), 0,
        ui::PAGE_TRANSITION_TYPED, 0));
    history_url.set_visit_count(visits->size());
    return history_url;
  }

  base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) {
    DCHECK_EQ(syncer::TYPED_URLS, type);
    return syncable_service_->AsWeakPtr();
  }

  HistoryBackendMock& history_backend() { return *history_backend_.get(); }

 private:
  scoped_refptr<HistoryBackendMock> history_backend_;
  std::unique_ptr<HistoryServiceMock> history_service_;
  syncer::DataTypeErrorHandlerMock error_handler_;
  TypedUrlDataTypeController* data_type_controller;
  std::unique_ptr<TestTypedUrlSyncableService> syncable_service_;
  std::unique_ptr<sync_driver::FakeSyncClient> sync_client_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceTypedUrlTest);
};

void AddTypedUrlEntries(ProfileSyncServiceTypedUrlTest* test,
                        const history::URLRows& entries) {
  test->CreateRoot(syncer::TYPED_URLS);
  for (size_t i = 0; i < entries.size(); ++i) {
    history::VisitVector visits;
    visits.push_back(history::VisitRow(
        entries[i].id(), entries[i].last_visit(), 0,
        ui::PageTransitionFromInt(0), 0));
    test->AddTypedUrlSyncNode(entries[i], visits);
  }
}

}  // namespace

TEST_F(ProfileSyncServiceTypedUrlTest, EmptyNativeEmptySync) {
  EXPECT_CALL((history_backend()), GetAllTypedURLs(_)).WillOnce(Return(true));
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, syncer::TYPED_URLS);
  TypedUrlSyncableService* syncable_service =
      StartSyncService(create_root.callback());
  history::URLRows sync_entries;
  GetTypedUrlsFromSyncDB(&sync_entries);
  EXPECT_EQ(0U, sync_entries.size());
  ASSERT_EQ(0, syncable_service->GetErrorPercentage());
}

TEST_F(ProfileSyncServiceTypedUrlTest, HasNativeEmptySync) {
  history::URLRows entries;
  history::VisitVector visits;
  entries.push_back(MakeTypedUrlEntry("http://foo.com", "bar",
                                      2, 15, false, &visits));

  EXPECT_CALL((history_backend()), GetAllTypedURLs(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(entries), Return(true)));
  EXPECT_CALL((history_backend()), GetMostRecentVisitsForURL(_, _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(visits), Return(true)));
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, syncer::TYPED_URLS);
  TypedUrlSyncableService* syncable_service =
      StartSyncService(create_root.callback());
  history::URLRows sync_entries;
  GetTypedUrlsFromSyncDB(&sync_entries);
  ASSERT_EQ(1U, sync_entries.size());
  EXPECT_TRUE(URLsEqual(entries[0], sync_entries[0]));
  ASSERT_EQ(0, syncable_service->GetErrorPercentage());
}

TEST_F(ProfileSyncServiceTypedUrlTest, HasNativeErrorReadingVisits) {
  history::URLRows entries;
  history::VisitVector visits;
  history::URLRow native_entry1(MakeTypedUrlEntry("http://foo.com", "bar",
                                                  2, 15, false, &visits));
  history::URLRow native_entry2(MakeTypedUrlEntry("http://foo2.com", "bar",
                                                  3, 15, false, &visits));
  entries.push_back(native_entry1);
  entries.push_back(native_entry2);
  EXPECT_CALL((history_backend()), GetAllTypedURLs(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(entries), Return(true)));
  // Return an error from GetMostRecentVisitsForURL() for the second URL.
  EXPECT_CALL((history_backend()),
              GetMostRecentVisitsForURL(native_entry1.id(), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL((history_backend()),
              GetMostRecentVisitsForURL(native_entry2.id(), _, _))
      .WillRepeatedly(Return(false));
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, syncer::TYPED_URLS);
  StartSyncService(create_root.callback());
  history::URLRows sync_entries;
  GetTypedUrlsFromSyncDB(&sync_entries);
  ASSERT_EQ(1U, sync_entries.size());
  EXPECT_TRUE(URLsEqual(native_entry1, sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, HasNativeWithBlankEmptySync) {
  std::vector<history::URLRow> entries;
  history::VisitVector visits;
  // Add an empty URL.
  entries.push_back(MakeTypedUrlEntry("", "bar",
                                      2, 15, false, &visits));
  entries.push_back(MakeTypedUrlEntry("http://foo.com", "bar",
                                      2, 15, false, &visits));
  EXPECT_CALL((history_backend()), GetAllTypedURLs(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(entries), Return(true)));
  EXPECT_CALL((history_backend()), GetMostRecentVisitsForURL(_, _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(visits), Return(true)));
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, syncer::TYPED_URLS);
  StartSyncService(create_root.callback());
  std::vector<history::URLRow> sync_entries;
  GetTypedUrlsFromSyncDB(&sync_entries);
  // The empty URL should be ignored.
  ASSERT_EQ(1U, sync_entries.size());
  EXPECT_TRUE(URLsEqual(entries[1], sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, HasNativeHasSyncNoMerge) {
  history::VisitVector native_visits;
  history::VisitVector sync_visits;
  history::URLRow native_entry(MakeTypedUrlEntry("http://native.com", "entry",
                                                 2, 15, false, &native_visits));
  history::URLRow sync_entry(MakeTypedUrlEntry("http://sync.com", "entry",
                                               3, 16, false, &sync_visits));

  history::URLRows native_entries;
  native_entries.push_back(native_entry);
  EXPECT_CALL((history_backend()), GetAllTypedURLs(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));
  EXPECT_CALL((history_backend()), GetMostRecentVisitsForURL(_, _, _))
      .WillRepeatedly(
          DoAll(SetArgumentPointee<2>(native_visits), Return(true)));
  EXPECT_CALL((history_backend()), AddVisits(_, _, history::SOURCE_SYNCED))
      .WillRepeatedly(Return(true));

  history::URLRows sync_entries;
  sync_entries.push_back(sync_entry);

  EXPECT_CALL((history_backend()), UpdateURL(_, _))
      .WillRepeatedly(Return(true));
  StartSyncService(base::Bind(&AddTypedUrlEntries, this, sync_entries));

  std::map<std::string, history::URLRow> expected;
  expected[native_entry.url().spec()] = native_entry;
  expected[sync_entry.url().spec()] = sync_entry;

  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);

  EXPECT_TRUE(new_sync_entries.size() == expected.size());
  for (history::URLRows::iterator entry = new_sync_entries.begin();
       entry != new_sync_entries.end(); ++entry) {
    EXPECT_TRUE(URLsEqual(expected[entry->url().spec()], *entry));
  }
}

TEST_F(ProfileSyncServiceTypedUrlTest, EmptyNativeExpiredSync) {
  history::VisitVector sync_visits;
  history::URLRow sync_entry(MakeTypedUrlEntry("http://sync.com", "entry",
                                               3, EXPIRED_VISIT, false,
                                               &sync_visits));
  history::URLRows sync_entries;
  sync_entries.push_back(sync_entry);

  // Since all our URLs are expired, no backend calls to add new URLs will be
  // made.
  EXPECT_CALL((history_backend()), GetAllTypedURLs(_)).WillOnce(Return(true));
  SetIdleChangeProcessorExpectations();

  StartSyncService(base::Bind(&AddTypedUrlEntries, this, sync_entries));
}

TEST_F(ProfileSyncServiceTypedUrlTest, HasNativeHasSyncMerge) {
  history::VisitVector native_visits;
  history::URLRow native_entry(MakeTypedUrlEntry("http://native.com", "entry",
                                                 2, 15, false, &native_visits));
  history::VisitVector sync_visits;
  history::URLRow sync_entry(MakeTypedUrlEntry("http://native.com", "name",
                                               1, 17, false, &sync_visits));
  history::VisitVector merged_visits;
  merged_visits.push_back(history::VisitRow(
      sync_entry.id(), base::Time::FromInternalValue(15), 0,
      ui::PageTransitionFromInt(0), 0));

  history::URLRow merged_entry(MakeTypedUrlEntry("http://native.com", "name",
                                                 2, 17, false, &merged_visits));

  history::URLRows native_entries;
  native_entries.push_back(native_entry);
  EXPECT_CALL((history_backend()), GetAllTypedURLs(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));
  EXPECT_CALL((history_backend()), GetMostRecentVisitsForURL(_, _, _))
      .WillRepeatedly(
          DoAll(SetArgumentPointee<2>(native_visits), Return(true)));
  EXPECT_CALL((history_backend()), AddVisits(_, _, history::SOURCE_SYNCED))
      .WillRepeatedly(Return(true));

  history::URLRows sync_entries;
  sync_entries.push_back(sync_entry);

  EXPECT_CALL((history_backend()), UpdateURL(_, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL((history_backend()), SetPageTitle(_, _)).WillRepeatedly(Return());
  StartSyncService(base::Bind(&AddTypedUrlEntries, this, sync_entries));

  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(merged_entry, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, HasNativeWithErrorHasSyncMerge) {
  history::VisitVector native_visits;
  history::URLRow native_entry(MakeTypedUrlEntry("http://native.com", "native",
                                                 2, 15, false, &native_visits));
  history::VisitVector sync_visits;
  history::URLRow sync_entry(MakeTypedUrlEntry("http://native.com", "sync",
                                               1, 17, false, &sync_visits));

  history::URLRows native_entries;
  native_entries.push_back(native_entry);
  EXPECT_CALL((history_backend()), GetAllTypedURLs(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));
  // Return an error getting the visits for the native URL.
  EXPECT_CALL((history_backend()), GetMostRecentVisitsForURL(_, _, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL((history_backend()), GetURL(_, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(native_entry), Return(true)));
  EXPECT_CALL((history_backend()), AddVisits(_, _, history::SOURCE_SYNCED))
      .WillRepeatedly(Return(true));

  history::URLRows sync_entries;
  sync_entries.push_back(sync_entry);

  EXPECT_CALL((history_backend()), UpdateURL(_, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL((history_backend()), SetPageTitle(_, _)).WillRepeatedly(Return());
  StartSyncService(base::Bind(&AddTypedUrlEntries, this, sync_entries));

  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(sync_entry, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserChangeAdd) {
  history::VisitVector added_visits;
  history::URLRow added_entry(MakeTypedUrlEntry("http://added.com", "entry",
                                                2, 15, false, &added_visits));

  EXPECT_CALL((history_backend()), GetAllTypedURLs(_)).WillOnce(Return(true));
  EXPECT_CALL((history_backend()), GetMostRecentVisitsForURL(_, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(added_visits), Return(true)));

  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, syncer::TYPED_URLS);
  StartSyncService(create_root.callback());

  history::URLRows changed_urls;
  changed_urls.push_back(added_entry);
  SendNotificationURLsModified(changed_urls);

  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(added_entry, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserChangeAddWithBlank) {
  history::VisitVector added_visits;
  history::URLRow empty_entry(MakeTypedUrlEntry("", "entry",
                                                2, 15, false, &added_visits));
  history::URLRow added_entry(MakeTypedUrlEntry("http://added.com", "entry",
                                                2, 15, false, &added_visits));

  EXPECT_CALL((history_backend()), GetAllTypedURLs(_)).WillOnce(Return(true));
  EXPECT_CALL((history_backend()), GetMostRecentVisitsForURL(_, _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(added_visits), Return(true)));

  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, syncer::TYPED_URLS);
  StartSyncService(create_root.callback());

  history::URLRows changed_urls;
  changed_urls.push_back(empty_entry);
  changed_urls.push_back(added_entry);
  SendNotificationURLsModified(changed_urls);

  std::vector<history::URLRow> new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(added_entry, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserChangeUpdate) {
  history::VisitVector original_visits;
  history::URLRow original_entry(MakeTypedUrlEntry("http://mine.com", "entry",
                                                   2, 15, false,
                                                   &original_visits));
  history::URLRows original_entries;
  original_entries.push_back(original_entry);

  EXPECT_CALL((history_backend()), GetAllTypedURLs(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL((history_backend()), GetMostRecentVisitsForURL(_, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(original_visits), Return(true)));
  CreateRootHelper create_root(this, syncer::TYPED_URLS);
  StartSyncService(create_root.callback());

  history::VisitVector updated_visits;
  history::URLRow updated_entry(MakeTypedUrlEntry("http://mine.com", "entry",
                                                  7, 17, false,
                                                  &updated_visits));
  EXPECT_CALL((history_backend()), GetMostRecentVisitsForURL(_, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(updated_visits), Return(true)));

  history::URLRows changed_urls;
  changed_urls.push_back(updated_entry);
  SendNotificationURLsModified(changed_urls);

  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(updated_entry, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserChangeAddFromVisit) {
  history::VisitVector added_visits;
  history::URLRow added_entry(MakeTypedUrlEntry("http://added.com", "entry",
                                                2, 15, false, &added_visits));

  EXPECT_CALL((history_backend()), GetAllTypedURLs(_)).WillOnce(Return(true));
  EXPECT_CALL((history_backend()), GetMostRecentVisitsForURL(_, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(added_visits), Return(true)));

  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, syncer::TYPED_URLS);
  StartSyncService(create_root.callback());

  SendNotificationURLVisited(ui::PAGE_TRANSITION_TYPED, added_entry);

  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(added_entry, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserChangeUpdateFromVisit) {
  history::VisitVector original_visits;
  history::URLRow original_entry(MakeTypedUrlEntry("http://mine.com", "entry",
                                                   2, 15, false,
                                                   &original_visits));
  history::URLRows original_entries;
  original_entries.push_back(original_entry);

  EXPECT_CALL((history_backend()), GetAllTypedURLs(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL((history_backend()), GetMostRecentVisitsForURL(_, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(original_visits), Return(true)));
  CreateRootHelper create_root(this, syncer::TYPED_URLS);
  StartSyncService(create_root.callback());

  history::VisitVector updated_visits;
  history::URLRow updated_entry(MakeTypedUrlEntry("http://mine.com", "entry",
                                                  7, 17, false,
                                                  &updated_visits));
  EXPECT_CALL((history_backend()), GetMostRecentVisitsForURL(_, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(updated_visits), Return(true)));

  SendNotificationURLVisited(ui::PAGE_TRANSITION_TYPED, updated_entry);

  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(updated_entry, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserIgnoreChangeUpdateFromVisit) {
  history::VisitVector original_visits;
  history::URLRow original_entry(MakeTypedUrlEntry("http://mine.com", "entry",
                                                   2, 15, false,
                                                   &original_visits));
  history::URLRows original_entries;
  original_entries.push_back(original_entry);

  EXPECT_CALL((history_backend()), GetAllTypedURLs(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL((history_backend()), GetMostRecentVisitsForURL(_, _, _))
      .WillRepeatedly(
          DoAll(SetArgumentPointee<2>(original_visits), Return(true)));
  CreateRootHelper create_root(this, syncer::TYPED_URLS);
  StartSyncService(create_root.callback());
  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(original_entry, new_sync_entries[0]));

  history::VisitVector updated_visits;
  history::URLRow updated_entry(MakeTypedUrlEntry("http://mine.com", "entry",
                                                  7, 15, false,
                                                  &updated_visits));

  // Should ignore this change because it's not TYPED.
  SendNotificationURLVisited(ui::PAGE_TRANSITION_RELOAD, updated_entry);
  GetTypedUrlsFromSyncDB(&new_sync_entries);

  // Should be no changes to the sync DB from this notification.
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(original_entry, new_sync_entries[0]));

  // Now, try updating it with a large number of visits not divisible by 10
  // (should ignore this visit).
  history::URLRow twelve_visits(MakeTypedUrlEntry("http://mine.com", "entry",
                                                  12, 15, false,
                                                  &updated_visits));
  SendNotificationURLVisited(ui::PAGE_TRANSITION_TYPED, twelve_visits);
  GetTypedUrlsFromSyncDB(&new_sync_entries);

  // Should be no changes to the sync DB from this notification.
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(original_entry, new_sync_entries[0]));

  // Now, try updating it with a large number of visits that is divisible by 10
  // (should *not* be ignored).
  history::URLRow twenty_visits(MakeTypedUrlEntry("http://mine.com", "entry",
                                                  20, 15, false,
                                                  &updated_visits));
  SendNotificationURLVisited(ui::PAGE_TRANSITION_TYPED, twenty_visits);
  GetTypedUrlsFromSyncDB(&new_sync_entries);

  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(twenty_visits, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserChangeRemove) {
  history::VisitVector original_visits1;
  history::URLRow original_entry1(MakeTypedUrlEntry("http://mine.com", "entry",
                                                    2, 15, false,
                                                    &original_visits1));
  history::VisitVector original_visits2;
  history::URLRow original_entry2(MakeTypedUrlEntry("http://mine2.com",
                                                    "entry2",
                                                    3, 15, false,
                                                    &original_visits2));
  history::URLRows original_entries;
  original_entries.push_back(original_entry1);
  original_entries.push_back(original_entry2);

  EXPECT_CALL((history_backend()), GetAllTypedURLs(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL((history_backend()), GetMostRecentVisitsForURL(_, _, _))
      .WillRepeatedly(
          DoAll(SetArgumentPointee<2>(original_visits1), Return(true)));
  CreateRootHelper create_root(this, syncer::TYPED_URLS);
  StartSyncService(create_root.callback());

  history::URLRows rows;
  rows.push_back(history::URLRow(GURL("http://mine.com")));
  SendNotificationURLsDeleted(false, false, rows, std::set<GURL>());
  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(original_entry2, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserChangeRemoveExpired) {
  history::VisitVector original_visits1;
  history::URLRow original_entry1(MakeTypedUrlEntry("http://mine.com", "entry",
                                                    2, 15, false,
                                                    &original_visits1));
  history::VisitVector original_visits2;
  history::URLRow original_entry2(MakeTypedUrlEntry("http://mine2.com",
                                                    "entry2",
                                                    3, 15, false,
                                                    &original_visits2));
  history::URLRows original_entries;
  original_entries.push_back(original_entry1);
  original_entries.push_back(original_entry2);

  EXPECT_CALL((history_backend()), GetAllTypedURLs(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL((history_backend()), GetMostRecentVisitsForURL(_, _, _))
      .WillRepeatedly(
          DoAll(SetArgumentPointee<2>(original_visits1), Return(true)));
  CreateRootHelper create_root(this, syncer::TYPED_URLS);
  StartSyncService(create_root.callback());

  // Setting expired=true should cause the sync code to ignore this deletion.
  history::URLRows rows;
  rows.push_back(history::URLRow(GURL("http://mine.com")));
  SendNotificationURLsDeleted(false, true, rows, std::set<GURL>());
  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  // Both URLs should still be there.
  ASSERT_EQ(2U, new_sync_entries.size());
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserChangeRemoveAll) {
  history::VisitVector original_visits1;
  history::URLRow original_entry1(MakeTypedUrlEntry("http://mine.com", "entry",
                                                    2, 15, false,
                                                    &original_visits1));
  history::VisitVector original_visits2;
  history::URLRow original_entry2(MakeTypedUrlEntry("http://mine2.com",
                                                    "entry2",
                                                    3, 15, false,
                                                    &original_visits2));
  history::URLRows original_entries;
  original_entries.push_back(original_entry1);
  original_entries.push_back(original_entry2);

  EXPECT_CALL((history_backend()), GetAllTypedURLs(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL((history_backend()), GetMostRecentVisitsForURL(_, _, _))
      .WillRepeatedly(
          DoAll(SetArgumentPointee<2>(original_visits1), Return(true)));
  CreateRootHelper create_root(this, syncer::TYPED_URLS);
  StartSyncService(create_root.callback());

  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(2U, new_sync_entries.size());

  SendNotificationURLsDeleted(true, false, history::URLRows(),
                              std::set<GURL>());

  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(0U, new_sync_entries.size());
}

TEST_F(ProfileSyncServiceTypedUrlTest, FailWriteToHistoryBackend) {
  history::VisitVector native_visits;
  history::VisitVector sync_visits;
  history::URLRow native_entry(MakeTypedUrlEntry("http://native.com", "entry",
                                                 2, 15, false, &native_visits));
  history::URLRow sync_entry(MakeTypedUrlEntry("http://sync.com", "entry",
                                               3, 16, false, &sync_visits));

  history::URLRows native_entries;
  native_entries.push_back(native_entry);
  EXPECT_CALL((history_backend()), GetAllTypedURLs(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));
  EXPECT_CALL((history_backend()), GetURL(_, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(native_entry), Return(false)));
  EXPECT_CALL((history_backend()), GetMostRecentVisitsForURL(_, _, _))
      .WillRepeatedly(
          DoAll(SetArgumentPointee<2>(native_visits), Return(true)));
  EXPECT_CALL((history_backend()), AddVisits(_, _, history::SOURCE_SYNCED))
      .WillRepeatedly(Return(false));

  history::URLRows sync_entries;
  sync_entries.push_back(sync_entry);

  EXPECT_CALL((history_backend()), UpdateURL(_, _))
      .WillRepeatedly(Return(false));
  TypedUrlSyncableService* syncable_service =
      StartSyncService(base::Bind(&AddTypedUrlEntries, this, sync_entries));
  // Errors writing to the DB should be recorded, but should not cause an
  // unrecoverable error.
  ASSERT_FALSE(sync_service()->data_type_status_table().GetFailedTypes().Has(
      syncer::TYPED_URLS));
  // Some calls should have succeeded, so the error percentage should be
  // somewhere > 0 and < 100.
  ASSERT_NE(0, syncable_service->GetErrorPercentage());
  ASSERT_NE(100, syncable_service->GetErrorPercentage());
}

TEST_F(ProfileSyncServiceTypedUrlTest, FailToGetTypedURLs) {
  history::VisitVector native_visits;
  history::VisitVector sync_visits;
  history::URLRow native_entry(MakeTypedUrlEntry("http://native.com", "entry",
                                                 2, 15, false, &native_visits));
  history::URLRow sync_entry(MakeTypedUrlEntry("http://sync.com", "entry",
                                               3, 16, false, &sync_visits));

  history::URLRows native_entries;
  native_entries.push_back(native_entry);
  EXPECT_CALL((history_backend()), GetAllTypedURLs(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(false)));

  history::URLRows sync_entries;
  sync_entries.push_back(sync_entry);

  StartSyncService(base::Bind(&AddTypedUrlEntries, this, sync_entries));
  // Errors getting typed URLs will cause an unrecoverable error (since we can
  // do *nothing* in that case).
  ASSERT_TRUE(sync_service()->data_type_status_table().GetFailedTypes().Has(
      syncer::TYPED_URLS));
  ASSERT_EQ(1u,
            sync_service()->data_type_status_table().GetFailedTypes().Size());
  // Can't check GetErrorPercentage(), because generating an unrecoverable
  // error will free the model associator.
}

TEST_F(ProfileSyncServiceTypedUrlTest, IgnoreLocalFileURL) {
  history::VisitVector original_visits;
  // Create http and file url.
  history::URLRow url_entry(MakeTypedUrlEntry("http://yey.com",
                                              "yey", 12, 15, false,
                                              &original_visits));
  history::URLRow file_entry(MakeTypedUrlEntry("file:///kitty.jpg",
                                               "kitteh", 12, 15, false,
                                               &original_visits));

  history::URLRows original_entries;
  original_entries.push_back(url_entry);
  original_entries.push_back(file_entry);

  EXPECT_CALL((history_backend()), GetAllTypedURLs(_))
      .WillRepeatedly(
          DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL((history_backend()), GetMostRecentVisitsForURL(_, _, _))
      .WillRepeatedly(
          DoAll(SetArgumentPointee<2>(original_visits), Return(true)));
  CreateRootHelper create_root(this, syncer::TYPED_URLS);
  StartSyncService(create_root.callback());

  history::VisitVector updated_visits;
  // Create updates for the previous urls + a new file one.
  history::URLRow updated_url_entry(MakeTypedUrlEntry("http://yey.com",
                                                      "yey", 20, 15, false,
                                                      &updated_visits));
  history::URLRow updated_file_entry(MakeTypedUrlEntry("file:///cat.jpg",
                                                       "cat", 20, 15, false,
                                                       &updated_visits));
  history::URLRow new_file_entry(MakeTypedUrlEntry("file:///dog.jpg",
                                                   "dog", 20, 15, false,
                                                   &updated_visits));

  history::URLRows changed_urls;
  changed_urls.push_back(updated_url_entry);
  changed_urls.push_back(updated_file_entry);
  changed_urls.push_back(new_file_entry);
  SendNotificationURLsModified(changed_urls);

  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);

  // We should ignore the local file urls (existing and updated),
  // and only be left with the updated http url.
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(updated_url_entry, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, IgnoreLocalhostURL) {
  history::VisitVector original_visits;
  // Create http and localhost url.
  history::URLRow url_entry(MakeTypedUrlEntry("http://yey.com",
                                              "yey", 12, 15, false,
                                              &original_visits));
  history::URLRow localhost_entry(MakeTypedUrlEntry("http://localhost",
                                              "localhost", 12, 15, false,
                                              &original_visits));

  history::URLRows original_entries;
  original_entries.push_back(url_entry);
  original_entries.push_back(localhost_entry);

  EXPECT_CALL((history_backend()), GetAllTypedURLs(_))
      .WillRepeatedly(
          DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL((history_backend()), GetMostRecentVisitsForURL(_, _, _))
      .WillRepeatedly(
          DoAll(SetArgumentPointee<2>(original_visits), Return(true)));
  CreateRootHelper create_root(this, syncer::TYPED_URLS);
  StartSyncService(create_root.callback());

  history::VisitVector updated_visits;
  // Update the previous entries and add a new localhost.
  history::URLRow updated_url_entry(MakeTypedUrlEntry("http://yey.com",
                                                  "yey", 20, 15, false,
                                                  &updated_visits));
  history::URLRow updated_localhost_entry(MakeTypedUrlEntry(
                                                  "http://localhost:80",
                                                  "localhost", 20, 15, false,
                                                  &original_visits));
  history::URLRow localhost_ip_entry(MakeTypedUrlEntry("http://127.0.0.1",
                                                  "localhost", 12, 15, false,
                                                  &original_visits));

  history::URLRows changed_urls;
  changed_urls.push_back(updated_url_entry);
  changed_urls.push_back(updated_localhost_entry);
  changed_urls.push_back(localhost_ip_entry);
  SendNotificationURLsModified(changed_urls);

  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);

  // We should ignore the localhost urls and left only with http url.
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(updated_url_entry, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, IgnoreModificationWithoutValidVisit) {
  EXPECT_CALL((history_backend()), GetAllTypedURLs(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL((history_backend()), GetMostRecentVisitsForURL(_, _, _))
      .WillRepeatedly(Return(true));

  CreateRootHelper create_root(this, syncer::TYPED_URLS);
  StartSyncService(create_root.callback());

  history::VisitVector updated_visits;
  history::URLRow updated_url_entry(MakeTypedUrlEntry("http://yey.com",
                                                  "yey", 20, 0, false,
                                                  &updated_visits));

  history::URLRows changed_urls;
  changed_urls.push_back(updated_url_entry);
  SendNotificationURLsModified(changed_urls);

  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);

  // The change should be ignored.
  ASSERT_EQ(0U, new_sync_entries.size());
}
