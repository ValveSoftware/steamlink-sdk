// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
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
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/country_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autocomplete_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_data_type_controller.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_profile_data_type_controller.h"
#include "components/autofill/core/browser/webdata/autofill_profile_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/browser_sync/browser/abstract_profile_sync_service_test.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/browser_sync/browser/test_profile_sync_service.h"
#include "components/sync_driver/data_type_controller.h"
#include "components/sync_driver/data_type_manager_impl.h"
#include "components/sync_driver/sync_api_component_factory_mock.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "components/version_info/version_info.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata_services/web_data_service_test_util.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/data_type_debug_info_listener.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/protocol/autofill_specifics.pb.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::AutocompleteSyncableService;
using autofill::AutofillChange;
using autofill::AutofillChangeList;
using autofill::AutofillEntry;
using autofill::AutofillKey;
using autofill::AutofillProfile;
using autofill::AutofillProfileChange;
using autofill::AutofillProfileSyncableService;
using autofill::AutofillTable;
using autofill::AutofillWebDataService;
using autofill::NAME_FULL;
using autofill::PersonalDataManager;
using autofill::ServerFieldType;
using base::ASCIIToUTF16;
using base::Time;
using base::TimeDelta;
using base::WaitableEvent;
using browser_sync::AutofillDataTypeController;
using browser_sync::AutofillProfileDataTypeController;
using syncer::AUTOFILL;
using syncer::AUTOFILL_PROFILE;
using syncer::BaseNode;
using syncer::syncable::CREATE;
using syncer::syncable::GET_TYPE_ROOT;
using syncer::syncable::MutableEntry;
using syncer::syncable::UNITTEST;
using syncer::syncable::WriterTag;
using syncer::syncable::WriteTransaction;
using testing::_;
using testing::DoAll;
using testing::ElementsAre;
using testing::Not;
using testing::SetArgumentPointee;
using testing::Return;

namespace {

void RegisterAutofillPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(autofill::prefs::kAutofillEnabled, true);
  registry->RegisterBooleanPref(autofill::prefs::kAutofillWalletImportEnabled,
                                true);
  registry->RegisterBooleanPref(autofill::prefs::kAutofillProfileUseDatesFixed,
                                true);
  registry->RegisterIntegerPref(autofill::prefs::kAutofillLastVersionDeduped,
                                atoi(version_info::GetVersionNumber().c_str()));
}

void RunAndSignal(const base::Closure& cb, WaitableEvent* event) {
  cb.Run();
  event->Signal();
}

AutofillEntry MakeAutofillEntry(const char* name,
                                const char* value,
                                int time_shift0,
                                int time_shift1) {
  // Time deep in the past would cause Autocomplete sync to discard the
  // entries.
  static Time base_time = Time::Now().LocalMidnight();

  Time date_created = base_time + TimeDelta::FromSeconds(time_shift0);
  Time date_last_used = date_created;
  if (time_shift1 >= 0)
    date_last_used = base_time + TimeDelta::FromSeconds(time_shift1);
  return AutofillEntry(AutofillKey(ASCIIToUTF16(name), ASCIIToUTF16(value)),
                       date_created, date_last_used);
}

AutofillEntry MakeAutofillEntry(const char* name,
                                const char* value,
                                int time_shift) {
  return MakeAutofillEntry(name, value, time_shift, -1);
}

}  // namespace

class AutofillTableMock : public AutofillTable {
 public:
  AutofillTableMock() {}
  MOCK_METHOD2(RemoveFormElement,
               bool(const base::string16& name,
                    const base::string16& value));  // NOLINT
  MOCK_METHOD1(GetAllAutofillEntries,
               bool(std::vector<AutofillEntry>* entries));  // NOLINT
  MOCK_METHOD4(GetAutofillTimestamps,
               bool(const base::string16& name,  // NOLINT
                    const base::string16& value,
                    Time* date_created,
                    Time* date_last_used));
  MOCK_METHOD1(UpdateAutofillEntries,
               bool(const std::vector<AutofillEntry>&));  // NOLINT
  MOCK_METHOD1(GetAutofillProfiles,
               bool(std::vector<AutofillProfile*>*));  // NOLINT
  MOCK_METHOD1(UpdateAutofillProfile,
               bool(const AutofillProfile&));  // NOLINT
  MOCK_METHOD1(AddAutofillProfile,
               bool(const AutofillProfile&));  // NOLINT
  MOCK_METHOD1(RemoveAutofillProfile,
               bool(const std::string&));  // NOLINT
};

MATCHER_P(MatchProfiles, profile, "") {
  return (profile.Compare(arg) == 0);
}

class WebDatabaseFake : public WebDatabase {
 public:
  explicit WebDatabaseFake(AutofillTable* autofill_table) {
    AddTable(autofill_table);
  }
};

class MockAutofillBackend : public autofill::AutofillWebDataBackend {
 public:
  MockAutofillBackend(
      WebDatabase* web_database,
      const base::Closure& on_changed,
      const base::Callback<void(syncer::ModelType)>& on_sync_started,
      const scoped_refptr<base::SequencedTaskRunner>& ui_thread)
      : web_database_(web_database),
        on_changed_(on_changed),
        on_sync_started_(on_sync_started),
        ui_thread_(ui_thread) {}

  ~MockAutofillBackend() override {}
  WebDatabase* GetDatabase() override { return web_database_; }
  void AddObserver(
      autofill::AutofillWebDataServiceObserverOnDBThread* observer) override {}
  void RemoveObserver(
      autofill::AutofillWebDataServiceObserverOnDBThread* observer) override {}
  void RemoveExpiredFormElements() override {}
  void NotifyOfMultipleAutofillChanges() override {
    DCHECK(!ui_thread_->RunsTasksOnCurrentThread());
    ui_thread_->PostTask(FROM_HERE, on_changed_);
  }
  void NotifyThatSyncHasStarted(syncer::ModelType model_type) override {
    DCHECK(!ui_thread_->RunsTasksOnCurrentThread());
    ui_thread_->PostTask(FROM_HERE, base::Bind(on_sync_started_, model_type));
  }

 private:
  WebDatabase* web_database_;
  base::Closure on_changed_;
  base::Callback<void(syncer::ModelType)> on_sync_started_;
  const scoped_refptr<base::SequencedTaskRunner> ui_thread_;
};

class ProfileSyncServiceAutofillTest;

template<class AutofillProfile>
syncer::ModelType GetModelType() {
  return syncer::UNSPECIFIED;
}

template<>
syncer::ModelType GetModelType<AutofillEntry>() {
  return AUTOFILL;
}

template<>
syncer::ModelType GetModelType<AutofillProfile>() {
  return AUTOFILL_PROFILE;
}

class TokenWebDataServiceFake : public TokenWebData {
 public:
  TokenWebDataServiceFake(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& db_thread)
      : TokenWebData(ui_thread, db_thread) {}

  bool IsDatabaseLoaded() override { return true; }

  AutofillWebDataService::Handle GetAllTokens(
      WebDataServiceConsumer* consumer) override {
    // TODO(tim): It would be nice if WebDataService was injected on
    // construction of ProfileOAuth2TokenService rather than fetched by
    // Initialize so that this isn't necessary (we could pass a NULL service).
    // We currently do return it via EXPECT_CALLs, but without depending on
    // order-of-initialization (which seems way more fragile) we can't tell
    // which component is asking at what time, and some components in these
    // Autofill tests require a WebDataService.
    return 0;
  }

 private:
  ~TokenWebDataServiceFake() override {}

  DISALLOW_COPY_AND_ASSIGN(TokenWebDataServiceFake);
};

class WebDataServiceFake : public AutofillWebDataService {
 public:
  WebDataServiceFake(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& db_thread)
      : AutofillWebDataService(ui_thread, db_thread),
        web_database_(NULL),
        autocomplete_syncable_service_(NULL),
        autofill_profile_syncable_service_(NULL),
        syncable_service_created_or_destroyed_(
            base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED),
        db_thread_(db_thread),
        ui_thread_(ui_thread) {}

  void SetDatabase(WebDatabase* web_database) {
    web_database_ = web_database;
  }

  void StartSyncableService() {
    // The |autofill_profile_syncable_service_| must be constructed on the DB
    // thread.
    const base::Closure& on_changed_callback = base::Bind(
        &WebDataServiceFake::NotifyAutofillMultipleChangedOnUIThread,
        AsWeakPtr());
    const base::Callback<void(syncer::ModelType)> on_sync_started_callback =
        base::Bind(&WebDataServiceFake::NotifySyncStartedOnUIThread,
                   AsWeakPtr());

    db_thread_->PostTask(FROM_HERE,
                         base::Bind(&WebDataServiceFake::CreateSyncableService,
                                    base::Unretained(this), on_changed_callback,
                                    on_sync_started_callback));
    syncable_service_created_or_destroyed_.Wait();
  }

  void ShutdownSyncableService() {
    // The |autofill_profile_syncable_service_| must be destructed on the DB
    // thread.
    db_thread_->PostTask(FROM_HERE,
                         base::Bind(&WebDataServiceFake::DestroySyncableService,
                                    base::Unretained(this)));
    syncable_service_created_or_destroyed_.Wait();
  }

  bool IsDatabaseLoaded() override { return true; }

  WebDatabase* GetDatabase() override { return web_database_; }

  void OnAutofillEntriesChanged(const AutofillChangeList& changes) {
    WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::NOT_SIGNALED);

    base::Closure notify_cb =
        base::Bind(&AutocompleteSyncableService::AutofillEntriesChanged,
                   base::Unretained(autocomplete_syncable_service_),
                   changes);
    db_thread_->PostTask(FROM_HERE,
                         base::Bind(&RunAndSignal, notify_cb, &event));
    event.Wait();
  }

  void OnAutofillProfileChanged(const AutofillProfileChange& changes) {
    WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::NOT_SIGNALED);

    base::Closure notify_cb =
        base::Bind(&AutocompleteSyncableService::AutofillProfileChanged,
                   base::Unretained(autofill_profile_syncable_service_),
                   changes);
    db_thread_->PostTask(FROM_HERE,
                         base::Bind(&RunAndSignal, notify_cb, &event));
    event.Wait();
  }

 private:
  ~WebDataServiceFake() override {}

  void CreateSyncableService(
      const base::Closure& on_changed_callback,
      const base::Callback<void(syncer::ModelType)>& on_sync_started) {
    ASSERT_TRUE(db_thread_->RunsTasksOnCurrentThread());
    // These services are deleted in DestroySyncableService().
    backend_.reset(new MockAutofillBackend(GetDatabase(), on_changed_callback,
                                           on_sync_started, ui_thread_.get()));
    AutocompleteSyncableService::CreateForWebDataServiceAndBackend(
        this, backend_.get());
    AutofillProfileSyncableService::CreateForWebDataServiceAndBackend(
        this, backend_.get(), "en-US");

    autocomplete_syncable_service_ =
        AutocompleteSyncableService::FromWebDataService(this);
    autofill_profile_syncable_service_ =
        AutofillProfileSyncableService::FromWebDataService(this);

    syncable_service_created_or_destroyed_.Signal();
  }

  void DestroySyncableService() {
    ASSERT_TRUE(db_thread_->RunsTasksOnCurrentThread());
    autocomplete_syncable_service_ = NULL;
    autofill_profile_syncable_service_ = NULL;
    backend_.reset();
    syncable_service_created_or_destroyed_.Signal();
  }

  WebDatabase* web_database_;
  AutocompleteSyncableService* autocomplete_syncable_service_;
  AutofillProfileSyncableService* autofill_profile_syncable_service_;
  std::unique_ptr<autofill::AutofillWebDataBackend> backend_;

  WaitableEvent syncable_service_created_or_destroyed_;

  const scoped_refptr<base::SingleThreadTaskRunner> db_thread_;
  const scoped_refptr<base::SingleThreadTaskRunner> ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(WebDataServiceFake);
};

ACTION_P(ReturnNewDataTypeManagerWithDebugListener, debug_listener) {
  return new sync_driver::DataTypeManagerImpl(
      debug_listener,
      arg1,
      arg2,
      arg3,
      arg4);
}

class MockPersonalDataManager : public PersonalDataManager {
 public:
  MockPersonalDataManager() : PersonalDataManager("en-US") {}
  MOCK_CONST_METHOD0(IsDataLoaded, bool());
  MOCK_METHOD0(LoadProfiles, void());
  MOCK_METHOD0(LoadCreditCards, void());
  MOCK_METHOD0(Refresh, void());
};

template <class T> class AddAutofillHelper;

class ProfileSyncServiceAutofillTest
    : public AbstractProfileSyncServiceTest,
      public syncer::DataTypeDebugInfoListener {
 public:
  // DataTypeDebugInfoListener implementation.
  void OnDataTypeConfigureComplete(const std::vector<
      syncer::DataTypeConfigurationStats>& configuration_stats) override {
    ASSERT_EQ(1u, configuration_stats.size());
    association_stats_ = configuration_stats[0].association_stats;
  }

 protected:
  ProfileSyncServiceAutofillTest() : debug_ptr_factory_(this) {
    autofill::CountryNames::SetLocaleString("en-US");
    RegisterAutofillPrefs(
        profile_sync_service_bundle()->pref_service()->registry());

    data_type_thread()->Start();
    profile_sync_service_bundle()->set_db_thread(
        data_type_thread()->task_runner());

    web_database_.reset(new WebDatabaseFake(&autofill_table_));
    web_data_wrapper_ = base::WrapUnique(new MockWebDataServiceWrapper(
        new WebDataServiceFake(base::ThreadTaskRunnerHandle::Get(),
                               data_type_thread()->task_runner()),
        new TokenWebDataServiceFake(base::ThreadTaskRunnerHandle::Get(),
                                    data_type_thread()->task_runner())));
    web_data_service_ = static_cast<WebDataServiceFake*>(
        web_data_wrapper_->GetAutofillWebData().get());
    web_data_service_->SetDatabase(web_database_.get());

    personal_data_manager_ = base::WrapUnique(new MockPersonalDataManager());

    EXPECT_CALL(personal_data_manager(), LoadProfiles());
    EXPECT_CALL(personal_data_manager(), LoadCreditCards());

    personal_data_manager_->Init(
        web_data_service_, profile_sync_service_bundle()->pref_service(),
        profile_sync_service_bundle()->account_tracker(),
        profile_sync_service_bundle()->signin_manager(), false);

    web_data_service_->StartSyncableService();

    browser_sync::ProfileSyncServiceBundle::SyncClientBuilder builder(
        profile_sync_service_bundle());
    builder.SetPersonalDataManager(personal_data_manager_.get());
    builder.SetSyncServiceCallback(GetSyncServiceCallback());
    builder.SetSyncableServiceCallback(
        base::Bind(&ProfileSyncServiceAutofillTest::GetSyncableServiceForType,
                   base::Unretained(this)));
    builder.set_activate_model_creation();
    sync_client_owned_ = builder.Build();
    sync_client_ = sync_client_owned_.get();

    // When UpdateAutofillEntries() is called with an empty list, the return
    // value should be |true|, rather than the default of |false|.
    std::vector<AutofillEntry> empty;
    EXPECT_CALL(autofill_table_, UpdateAutofillEntries(empty))
        .WillRepeatedly(Return(true));
  }

  ~ProfileSyncServiceAutofillTest() override {
    web_data_service_->ShutdownOnUIThread();
    web_data_service_->ShutdownSyncableService();
    web_data_wrapper_->Shutdown();
    web_data_service_ = nullptr;
    web_data_wrapper_.reset();
    web_database_.reset();
    // Shut down the service explicitly before some data members from this
    // test it needs will be deleted.
    sync_service()->Shutdown();
  }

  int GetSyncCount(syncer::ModelType type) {
    syncer::ReadTransaction trans(FROM_HERE, sync_service()->GetUserShare());
    syncer::ReadNode node(&trans);
    if (node.InitTypeRoot(type) != BaseNode::INIT_OK)
      return 0;
    return node.GetTotalNodeCount() - 1;
  }

  void StartSyncService(const base::Closure& callback,
                        bool will_fail_association,
                        syncer::ModelType type) {
    SigninManagerBase* signin = profile_sync_service_bundle()->signin_manager();
    signin->SetAuthenticatedAccountInfo("12345", "test_user@gmail.com");
    CreateSyncService(std::move(sync_client_owned_), callback);

    EXPECT_CALL(*profile_sync_service_bundle()->component_factory(),
                CreateDataTypeManager(_, _, _, _, _))
        .WillOnce(ReturnNewDataTypeManagerWithDebugListener(
            syncer::MakeWeakHandle(debug_ptr_factory_.GetWeakPtr())));

    EXPECT_CALL(personal_data_manager(), IsDataLoaded())
        .WillRepeatedly(Return(true));

    // We need tokens to get the tests going
    profile_sync_service_bundle()->auth_service()->UpdateCredentials(
        signin->GetAuthenticatedAccountId(), "oauth2_login_token");

    sync_service()->RegisterDataTypeController(CreateDataTypeController(type));
    sync_service()->Initialize();
    base::RunLoop().Run();

    // It's possible this test triggered an unrecoverable error, in which case
    // we can't get the sync count.
    if (sync_service()->IsSyncActive()) {
      EXPECT_EQ(GetSyncCount(type),
                association_stats_.num_sync_items_after_association);
    }
    EXPECT_EQ(association_stats_.num_sync_items_after_association,
              association_stats_.num_sync_items_before_association +
              association_stats_.num_sync_items_added -
              association_stats_.num_sync_items_deleted);
  }

  bool AddAutofillSyncNode(const AutofillEntry& entry) {
    syncer::WriteTransaction trans(FROM_HERE, sync_service()->GetUserShare());
    syncer::WriteNode node(&trans);
    std::string tag = AutocompleteSyncableService::KeyToTag(
        base::UTF16ToUTF8(entry.key().name()),
        base::UTF16ToUTF8(entry.key().value()));
    syncer::WriteNode::InitUniqueByCreationResult result =
        node.InitUniqueByCreation(AUTOFILL, tag);
    if (result != syncer::WriteNode::INIT_SUCCESS)
      return false;

    sync_pb::EntitySpecifics specifics;
    AutocompleteSyncableService::WriteAutofillEntry(entry, &specifics);
    node.SetEntitySpecifics(specifics);
    return true;
  }

  bool AddAutofillSyncNode(const AutofillProfile& profile) {
    syncer::WriteTransaction trans(FROM_HERE, sync_service()->GetUserShare());
    syncer::WriteNode node(&trans);
    std::string tag = profile.guid();
    syncer::WriteNode::InitUniqueByCreationResult result =
        node.InitUniqueByCreation(AUTOFILL_PROFILE, tag);
    if (result != syncer::WriteNode::INIT_SUCCESS)
      return false;

    sync_pb::EntitySpecifics specifics;
    AutofillProfileSyncableService::WriteAutofillProfile(profile, &specifics);
    node.SetEntitySpecifics(specifics);
    return true;
  }

  bool GetAutofillEntriesFromSyncDB(std::vector<AutofillEntry>* entries,
                                    std::vector<AutofillProfile>* profiles) {
    syncer::ReadTransaction trans(FROM_HERE, sync_service()->GetUserShare());
    syncer::ReadNode autofill_root(&trans);
    if (autofill_root.InitTypeRoot(AUTOFILL) != BaseNode::INIT_OK) {
      return false;
    }

    int64_t child_id = autofill_root.GetFirstChildId();
    while (child_id != syncer::kInvalidId) {
      syncer::ReadNode child_node(&trans);
      if (child_node.InitByIdLookup(child_id) != BaseNode::INIT_OK)
        return false;

      const sync_pb::AutofillSpecifics& autofill(
          child_node.GetEntitySpecifics().autofill());
      if (autofill.has_value()) {
        AutofillKey key(base::UTF8ToUTF16(autofill.name()),
                        base::UTF8ToUTF16(autofill.value()));
        std::vector<Time> timestamps;
        int timestamps_count = autofill.usage_timestamp_size();
        for (int i = 0; i < timestamps_count; ++i) {
          timestamps.push_back(Time::FromInternalValue(
              autofill.usage_timestamp(i)));
        }
        entries->push_back(
            AutofillEntry(key, timestamps.front(), timestamps.back()));
      } else if (autofill.has_profile()) {
        AutofillProfile p;
        p.set_guid(autofill.profile().guid());
        AutofillProfileSyncableService::OverwriteProfileWithServerData(
            autofill.profile(), &p);
        profiles->push_back(p);
      }
      child_id = child_node.GetSuccessorId();
    }
    return true;
  }

  bool GetAutofillProfilesFromSyncDBUnderProfileNode(
      std::vector<AutofillProfile>* profiles) {
    syncer::ReadTransaction trans(FROM_HERE, sync_service()->GetUserShare());
    syncer::ReadNode autofill_root(&trans);
    if (autofill_root.InitTypeRoot(AUTOFILL_PROFILE) != BaseNode::INIT_OK) {
      return false;
    }

    int64_t child_id = autofill_root.GetFirstChildId();
    while (child_id != syncer::kInvalidId) {
      syncer::ReadNode child_node(&trans);
      if (child_node.InitByIdLookup(child_id) != BaseNode::INIT_OK)
        return false;

      const sync_pb::AutofillProfileSpecifics& autofill(
          child_node.GetEntitySpecifics().autofill_profile());
        AutofillProfile p;
        p.set_guid(autofill.guid());
        AutofillProfileSyncableService::OverwriteProfileWithServerData(autofill,
                                                                       &p);
        profiles->push_back(p);
      child_id = child_node.GetSuccessorId();
    }
    return true;
  }

  void SetIdleChangeProcessorExpectations() {
    EXPECT_CALL(autofill_table_, RemoveFormElement(_, _)).Times(0);
    EXPECT_CALL(autofill_table_, GetAutofillTimestamps(_, _, _, _)).Times(0);

    // Only permit UpdateAutofillEntries() to be called with an empty list.
    std::vector<AutofillEntry> empty;
    EXPECT_CALL(autofill_table_, UpdateAutofillEntries(Not(empty))).Times(0);
  }

  sync_driver::DataTypeController* CreateDataTypeController(
      syncer::ModelType type) {
    DCHECK(type == AUTOFILL || type == AUTOFILL_PROFILE);
    if (type == AUTOFILL) {
      return new AutofillDataTypeController(base::ThreadTaskRunnerHandle::Get(),
                                            data_type_thread()->task_runner(),
                                            base::Bind(&base::DoNothing),
                                            sync_client_, web_data_service_);
    } else {
      return new AutofillProfileDataTypeController(
          base::ThreadTaskRunnerHandle::Get(),
          data_type_thread()->task_runner(), base::Bind(&base::DoNothing),
          sync_client_, web_data_service_);
    }
  }

  AutofillTableMock& autofill_table() { return autofill_table_; }

  MockPersonalDataManager& personal_data_manager() {
    return *personal_data_manager_;
  }

  WebDataServiceFake* web_data_service() { return web_data_service_.get(); }

 private:
  friend class AddAutofillHelper<AutofillEntry>;
  friend class AddAutofillHelper<AutofillProfile>;

  base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) {
    DCHECK(type == AUTOFILL || type == AUTOFILL_PROFILE);
    if (type == AUTOFILL) {
      return AutocompleteSyncableService::FromWebDataService(
                 web_data_service_.get())
          ->AsWeakPtr();
    } else {
      return AutofillProfileSyncableService::FromWebDataService(
                 web_data_service_.get())
          ->AsWeakPtr();
    }
  }

  AutofillTableMock autofill_table_;
  std::unique_ptr<WebDatabaseFake> web_database_;
  std::unique_ptr<MockWebDataServiceWrapper> web_data_wrapper_;
  scoped_refptr<WebDataServiceFake> web_data_service_;
  std::unique_ptr<MockPersonalDataManager> personal_data_manager_;
  syncer::DataTypeAssociationStats association_stats_;
  base::WeakPtrFactory<DataTypeDebugInfoListener> debug_ptr_factory_;
  // |sync_client_owned_| keeps the created client until it is passed to the
  // created ProfileSyncService. |sync_client_| just keeps a weak reference to
  // the client the whole time.
  std::unique_ptr<sync_driver::FakeSyncClient> sync_client_owned_;
  sync_driver::FakeSyncClient* sync_client_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceAutofillTest);
};

template <class T>
class AddAutofillHelper {
 public:
  AddAutofillHelper(ProfileSyncServiceAutofillTest* test,
                    const std::vector<T>& entries)
      : callback_(base::Bind(&AddAutofillHelper::AddAutofillCallback,
                             base::Unretained(this), test, entries)),
        success_(false) {
  }

  const base::Closure& callback() const { return callback_; }
  bool success() { return success_; }

 private:
  void AddAutofillCallback(ProfileSyncServiceAutofillTest* test,
                           const std::vector<T>& entries) {
    if (!test->CreateRoot(GetModelType<T>()))
      return;

    for (size_t i = 0; i < entries.size(); ++i) {
      if (!test->AddAutofillSyncNode(entries[i]))
        return;
    }
    success_ = true;
  }

  base::Closure callback_;
  bool success_;
};

// Overload write transaction to use custom NotifyTransactionComplete
class WriteTransactionTest: public WriteTransaction {
 public:
  WriteTransactionTest(const tracked_objects::Location& from_here,
                       WriterTag writer,
                       syncer::syncable::Directory* directory,
                       WaitableEvent* wait_for_syncapi)
      : WriteTransaction(from_here, writer, directory),
        wait_for_syncapi_(wait_for_syncapi) {}

  void NotifyTransactionComplete(syncer::ModelTypeSet types) override {
    // This is where we differ. Force a thread change here, giving another
    // thread a chance to create a WriteTransaction
    wait_for_syncapi_->Wait();

    WriteTransaction::NotifyTransactionComplete(types);
  }

 private:
  WaitableEvent* const wait_for_syncapi_;
};

// Our fake server updater. Needs the RefCountedThreadSafe inheritance so we can
// post tasks with it.
class FakeServerUpdater : public base::RefCountedThreadSafe<FakeServerUpdater> {
 public:
  FakeServerUpdater(TestProfileSyncService* service,
                    WaitableEvent* wait_for_start,
                    WaitableEvent* wait_for_syncapi,
                    scoped_refptr<base::SequencedTaskRunner> db_thread)
      : entry_(MakeAutofillEntry("0", "0", 0)),
        service_(service),
        wait_for_start_(wait_for_start),
        wait_for_syncapi_(wait_for_syncapi),
        is_finished_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                     base::WaitableEvent::InitialState::NOT_SIGNALED),
        db_thread_(db_thread) {}

  void Update() {
    // This gets called in a modelsafeworker thread.
    ASSERT_TRUE(db_thread_->RunsTasksOnCurrentThread());

    syncer::UserShare* user_share = service_->GetUserShare();
    syncer::syncable::Directory* directory = user_share->directory.get();

    // Create autofill protobuf.
    std::string tag = AutocompleteSyncableService::KeyToTag(
        base::UTF16ToUTF8(entry_.key().name()),
        base::UTF16ToUTF8(entry_.key().value()));
    sync_pb::AutofillSpecifics new_autofill;
    new_autofill.set_name(base::UTF16ToUTF8(entry_.key().name()));
    new_autofill.set_value(base::UTF16ToUTF8(entry_.key().value()));
    new_autofill.add_usage_timestamp(entry_.date_created().ToInternalValue());
    if (entry_.date_created() != entry_.date_last_used()) {
      new_autofill.add_usage_timestamp(
          entry_.date_last_used().ToInternalValue());
    }

    sync_pb::EntitySpecifics entity_specifics;
    entity_specifics.mutable_autofill()->CopyFrom(new_autofill);

    {
      // Tell main thread we've started
      wait_for_start_->Signal();

      // Create write transaction.
      WriteTransactionTest trans(FROM_HERE, UNITTEST, directory,
                                 wait_for_syncapi_);

      // Create actual entry based on autofill protobuf information.
      // Simulates effects of UpdateLocalDataFromServerData
      MutableEntry parent(&trans, GET_TYPE_ROOT, AUTOFILL);
      MutableEntry item(&trans, CREATE, AUTOFILL, parent.GetId(), tag);
      ASSERT_TRUE(item.good());
      item.PutSpecifics(entity_specifics);
      item.PutServerSpecifics(entity_specifics);
      item.PutBaseVersion(1);
      syncer::syncable::Id server_item_id =
          service_->id_factory()->NewServerId();
      item.PutId(server_item_id);
      syncer::syncable::Id new_predecessor;
      ASSERT_TRUE(item.PutPredecessor(new_predecessor));
    }
    DVLOG(1) << "FakeServerUpdater finishing.";
    is_finished_.Signal();
  }

  void CreateNewEntry(const AutofillEntry& entry) {
    entry_ = entry;
    ASSERT_FALSE(db_thread_->RunsTasksOnCurrentThread());
    if (!db_thread_->PostTask(FROM_HERE,
                              base::Bind(&FakeServerUpdater::Update, this))) {
      NOTREACHED() << "Failed to post task to the db thread.";
      return;
    }
  }

  void WaitForUpdateCompletion() {
    is_finished_.Wait();
  }

 private:
  friend class base::RefCountedThreadSafe<FakeServerUpdater>;
  ~FakeServerUpdater() { }

  AutofillEntry entry_;
  TestProfileSyncService* service_;
  WaitableEvent* const wait_for_start_;
  WaitableEvent* const wait_for_syncapi_;
  WaitableEvent is_finished_;
  syncer::syncable::Id parent_id_;
  scoped_refptr<base::SequencedTaskRunner> db_thread_;

  DISALLOW_COPY_AND_ASSIGN(FakeServerUpdater);
};

// TODO(skrul): Test abort startup.
// TODO(skrul): Test processing of cloud changes.
// TODO(tim): Add autofill data type controller test, and a case to cover
//            waiting for the PersonalDataManager.
TEST_F(ProfileSyncServiceAutofillTest, FailModelAssociation) {
  // Don't create the root autofill node so startup fails.
  StartSyncService(base::Closure(), true, AUTOFILL);
  EXPECT_TRUE(sync_service()->HasUnrecoverableError());
}

TEST_F(ProfileSyncServiceAutofillTest, EmptyNativeEmptySync) {
  EXPECT_CALL(autofill_table(), GetAllAutofillEntries(_))
      .WillOnce(Return(true));
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, AUTOFILL);
  EXPECT_CALL(personal_data_manager(), Refresh());
  StartSyncService(create_root.callback(), false, AUTOFILL);
  EXPECT_TRUE(create_root.success());
  std::vector<AutofillEntry> sync_entries;
  std::vector<AutofillProfile> sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&sync_entries, &sync_profiles));
  EXPECT_EQ(0U, sync_entries.size());
  EXPECT_EQ(0U, sync_profiles.size());
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeEntriesEmptySync) {
  std::vector<AutofillEntry> entries;
  entries.push_back(MakeAutofillEntry("foo", "bar", 1));
  EXPECT_CALL(autofill_table(), GetAllAutofillEntries(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(entries), Return(true)));
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, AUTOFILL);
  EXPECT_CALL(personal_data_manager(), Refresh());
  StartSyncService(create_root.callback(), false, AUTOFILL);
  ASSERT_TRUE(create_root.success());
  std::vector<AutofillEntry> sync_entries;
  std::vector<AutofillProfile> sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&sync_entries, &sync_profiles));
  ASSERT_EQ(1U, entries.size());
  EXPECT_TRUE(entries[0] == sync_entries[0]);
  EXPECT_EQ(0U, sync_profiles.size());
}

TEST_F(ProfileSyncServiceAutofillTest, HasProfileEmptySync) {
  std::vector<AutofillProfile*> profiles;
  std::vector<AutofillProfile> expected_profiles;
  // Owned by GetAutofillProfiles caller.
  AutofillProfile* profile0 = new AutofillProfile;
  autofill::test::SetProfileInfoWithGuid(profile0,
      "54B3F9AA-335E-4F71-A27D-719C41564230", "Billing",
      "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");
  profiles.push_back(profile0);
  expected_profiles.push_back(*profile0);
  EXPECT_CALL(autofill_table(), GetAutofillProfiles(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(profiles), Return(true)));
  EXPECT_CALL(personal_data_manager(), Refresh());
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, AUTOFILL_PROFILE);
  StartSyncService(create_root.callback(), false, AUTOFILL_PROFILE);
  ASSERT_TRUE(create_root.success());
  std::vector<AutofillProfile> sync_profiles;
  ASSERT_TRUE(GetAutofillProfilesFromSyncDBUnderProfileNode(&sync_profiles));
  EXPECT_EQ(1U, sync_profiles.size());
  EXPECT_EQ(0, expected_profiles[0].Compare(sync_profiles[0]));
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeWithDuplicatesEmptySync) {
  // There is buggy autofill code that allows duplicate name/value
  // pairs to exist in the database with separate pair_ids.
  std::vector<AutofillEntry> entries;
  entries.push_back(MakeAutofillEntry("foo", "bar", 1));
  entries.push_back(MakeAutofillEntry("dup", "", 2));
  entries.push_back(MakeAutofillEntry("dup", "", 3));
  EXPECT_CALL(autofill_table(), GetAllAutofillEntries(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(entries), Return(true)));
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, AUTOFILL);
  EXPECT_CALL(personal_data_manager(), Refresh());
  StartSyncService(create_root.callback(), false, AUTOFILL);
  ASSERT_TRUE(create_root.success());
  std::vector<AutofillEntry> sync_entries;
  std::vector<AutofillProfile> sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&sync_entries, &sync_profiles));
  EXPECT_EQ(2U, sync_entries.size());
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeHasSyncNoMerge) {
  AutofillEntry native_entry(MakeAutofillEntry("native", "entry", 1));
  AutofillEntry sync_entry(MakeAutofillEntry("sync", "entry", 2));

  std::vector<AutofillEntry> native_entries;
  native_entries.push_back(native_entry);

  EXPECT_CALL(autofill_table(), GetAllAutofillEntries(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));

  std::vector<AutofillEntry> sync_entries;
  sync_entries.push_back(sync_entry);

  AddAutofillHelper<AutofillEntry> add_autofill(this, sync_entries);

  EXPECT_CALL(autofill_table(), UpdateAutofillEntries(ElementsAre(sync_entry)))
      .WillOnce(Return(true));

  EXPECT_CALL(personal_data_manager(), Refresh());
  StartSyncService(add_autofill.callback(), false, AUTOFILL);
  ASSERT_TRUE(add_autofill.success());

  std::set<AutofillEntry> expected_entries;
  expected_entries.insert(native_entry);
  expected_entries.insert(sync_entry);

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&new_sync_entries,
                                           &new_sync_profiles));
  std::set<AutofillEntry> new_sync_entries_set(new_sync_entries.begin(),
                                               new_sync_entries.end());

  EXPECT_TRUE(expected_entries == new_sync_entries_set);
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeHasSyncMergeEntry) {
  AutofillEntry native_entry(MakeAutofillEntry("merge", "entry", 1));
  AutofillEntry sync_entry(MakeAutofillEntry("merge", "entry", 2));
  AutofillEntry merged_entry(MakeAutofillEntry("merge", "entry", 1, 2));

  std::vector<AutofillEntry> native_entries;
  native_entries.push_back(native_entry);
  EXPECT_CALL(autofill_table(), GetAllAutofillEntries(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));

  std::vector<AutofillEntry> sync_entries;
  sync_entries.push_back(sync_entry);
  AddAutofillHelper<AutofillEntry> add_autofill(this, sync_entries);

  EXPECT_CALL(autofill_table(),
              UpdateAutofillEntries(ElementsAre(merged_entry)))
      .WillOnce(Return(true));
  EXPECT_CALL(personal_data_manager(), Refresh());
  StartSyncService(add_autofill.callback(), false, AUTOFILL);
  ASSERT_TRUE(add_autofill.success());

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&new_sync_entries,
                                           &new_sync_profiles));
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(merged_entry == new_sync_entries[0]);
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeHasSyncMergeProfile) {
  AutofillProfile sync_profile;
  autofill::test::SetProfileInfoWithGuid(&sync_profile,
      "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing",
      "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");

  AutofillProfile* native_profile = new AutofillProfile;
  autofill::test::SetProfileInfoWithGuid(native_profile,
      "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549");

  std::vector<AutofillProfile*> native_profiles;
  native_profiles.push_back(native_profile);
  EXPECT_CALL(autofill_table(), GetAutofillProfiles(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(native_profiles), Return(true)));

  std::vector<AutofillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillHelper<AutofillProfile> add_autofill(this, sync_profiles);

  EXPECT_CALL(autofill_table(),
              UpdateAutofillProfile(MatchProfiles(sync_profile)))
      .WillOnce(Return(true));
  EXPECT_CALL(personal_data_manager(), Refresh());
  StartSyncService(add_autofill.callback(), false, AUTOFILL_PROFILE);
  ASSERT_TRUE(add_autofill.success());

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillProfilesFromSyncDBUnderProfileNode(
      &new_sync_profiles));
  ASSERT_EQ(1U, new_sync_profiles.size());
  EXPECT_EQ(0, sync_profile.Compare(new_sync_profiles[0]));
}

// Tests that a sync with a new native profile that matches a more recent new
// sync profile but with less information results in the native profile being
// deleted and replaced by the sync profile with merged usage stats.
TEST_F(
    ProfileSyncServiceAutofillTest,
    HasNativeHasSyncMergeSimilarProfileCombine_SyncHasMoreInfoAndMoreRecent) {
  // Create two almost identical profiles. The GUIDs are different and the
  // native profile has no value for company name.
  AutofillProfile sync_profile;
  autofill::test::SetProfileInfoWithGuid(
      &sync_profile, "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing",
      "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.",
      "unit 5", "Hollywood", "CA", "91601", "US", "12345678910");
  sync_profile.set_use_date(base::Time::FromTimeT(4321));

  AutofillProfile* native_profile = new AutofillProfile;
  autofill::test::SetProfileInfoWithGuid(
      native_profile, "23355099-1170-4B71-8ED4-144470CC9EBF", "Billing",
      "Mitchell", "Morrison", "johnwayne@me.xyz", "", "123 Zoo St.", "unit 5",
      "Hollywood", "CA", "91601", "US", "12345678910");
  native_profile->set_use_date(base::Time::FromTimeT(1234));

  AutofillProfile expected_profile(sync_profile);
  expected_profile.SetRawInfo(NAME_FULL,
                              ASCIIToUTF16("Billing Mitchell Morrison"));
  expected_profile.set_use_count(2);

  std::vector<AutofillProfile*> native_profiles;
  native_profiles.push_back(native_profile);
  EXPECT_CALL(autofill_table(), GetAutofillProfiles(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(native_profiles), Return(true)));
  EXPECT_CALL(autofill_table(),
              AddAutofillProfile(MatchProfiles(expected_profile)))
      .WillOnce(Return(true));
  EXPECT_CALL(autofill_table(),
              RemoveAutofillProfile("23355099-1170-4B71-8ED4-144470CC9EBF"))
      .WillOnce(Return(true));
  std::vector<AutofillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillHelper<AutofillProfile> add_autofill(this, sync_profiles);

  EXPECT_CALL(personal_data_manager(), Refresh());
  // Adds all entries in |sync_profiles| to sync.
  StartSyncService(add_autofill.callback(), false, AUTOFILL_PROFILE);
  ASSERT_TRUE(add_autofill.success());

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(
      GetAutofillProfilesFromSyncDBUnderProfileNode(&new_sync_profiles));
  ASSERT_EQ(1U, new_sync_profiles.size());
  // Check that key fields are the same.
  EXPECT_TRUE(new_sync_profiles[0].IsSubsetOf(sync_profile, "en-US"));
  // Make sure the additional information from the sync profile was kept.
  EXPECT_EQ(ASCIIToUTF16("Fox"),
            new_sync_profiles[0].GetRawInfo(ServerFieldType::COMPANY_NAME));
  // Check that the latest use date is saved.
  EXPECT_EQ(base::Time::FromTimeT(4321), new_sync_profiles[0].use_date());
  // Check that the use counts were added (default value is 1).
  EXPECT_EQ(2U, new_sync_profiles[0].use_count());
}

// Tests that a sync with a new native profile that matches an older new sync
// profile but with less information results in the native profile being deleted
// and replaced by the sync profile with merged usage stats.
TEST_F(ProfileSyncServiceAutofillTest,
       HasNativeHasSyncMergeSimilarProfileCombine_SyncHasMoreInfoAndOlder) {
  // Create two almost identical profiles. The GUIDs are different and the
  // native profile has no value for company name.
  AutofillProfile sync_profile;
  autofill::test::SetProfileInfoWithGuid(
      &sync_profile, "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing",
      "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.",
      "unit 5", "Hollywood", "CA", "91601", "US", "12345678910");
  sync_profile.set_use_date(base::Time::FromTimeT(1234));

  AutofillProfile* native_profile = new AutofillProfile;
  autofill::test::SetProfileInfoWithGuid(
      native_profile, "23355099-1170-4B71-8ED4-144470CC9EBF", "Billing",
      "Mitchell", "Morrison", "johnwayne@me.xyz", "", "123 Zoo St.", "unit 5",
      "Hollywood", "CA", "91601", "US", "12345678910");
  native_profile->set_use_date(base::Time::FromTimeT(4321));

  AutofillProfile expected_profile(sync_profile);
  expected_profile.SetRawInfo(NAME_FULL,
                              ASCIIToUTF16("Billing Mitchell Morrison"));
  expected_profile.set_use_count(2);
  expected_profile.set_use_date(native_profile->use_date());

  std::vector<AutofillProfile*> native_profiles;
  native_profiles.push_back(native_profile);
  EXPECT_CALL(autofill_table(), GetAutofillProfiles(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(native_profiles), Return(true)));
  EXPECT_CALL(autofill_table(),
              AddAutofillProfile(MatchProfiles(expected_profile)))
      .WillOnce(Return(true));
  EXPECT_CALL(autofill_table(),
              RemoveAutofillProfile("23355099-1170-4B71-8ED4-144470CC9EBF"))
      .WillOnce(Return(true));
  std::vector<AutofillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillHelper<AutofillProfile> add_autofill(this, sync_profiles);

  EXPECT_CALL(personal_data_manager(), Refresh());
  // Adds all entries in |sync_profiles| to sync.
  StartSyncService(add_autofill.callback(), false, AUTOFILL_PROFILE);
  ASSERT_TRUE(add_autofill.success());

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(
      GetAutofillProfilesFromSyncDBUnderProfileNode(&new_sync_profiles));
  ASSERT_EQ(1U, new_sync_profiles.size());
  // Check that key fields are the same.
  EXPECT_TRUE(new_sync_profiles[0].IsSubsetOf(sync_profile, "en-US"));
  // Make sure the additional information from the sync profile was kept.
  EXPECT_EQ(ASCIIToUTF16("Fox"),
            new_sync_profiles[0].GetRawInfo(ServerFieldType::COMPANY_NAME));
  // Check that the latest use date is saved.
  EXPECT_EQ(base::Time::FromTimeT(4321), new_sync_profiles[0].use_date());
  // Check that the use counts were added (default value is 1).
  EXPECT_EQ(2U, new_sync_profiles[0].use_count());
}

// Tests that a sync with a new native profile that matches an a new sync
// profile but with more information results in the native profile being deleted
// and replaced by the sync profile with the native profiles additional
// information merged in. The merge should happen even if the sync profile is
// more recent.
TEST_F(ProfileSyncServiceAutofillTest,
       HasNativeHasSyncMergeSimilarProfileCombine_NativeHasMoreInfo) {
  // Create two almost identical profiles. The GUIDs are different and the
  // sync profile has no value for company name.
  AutofillProfile sync_profile;
  autofill::test::SetProfileInfoWithGuid(
      &sync_profile, "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing",
      "Mitchell", "Morrison", "johnwayne@me.xyz", "", "123 Zoo St.", "unit 5",
      "Hollywood", "CA", "91601", "US", "12345678910");
  sync_profile.set_use_date(base::Time::FromTimeT(4321));

  AutofillProfile* native_profile = new AutofillProfile;
  autofill::test::SetProfileInfoWithGuid(
      native_profile, "23355099-1170-4B71-8ED4-144470CC9EBF", "Billing",
      "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.",
      "unit 5", "Hollywood", "CA", "91601", "US", "12345678910");
  native_profile->set_use_date(base::Time::FromTimeT(1234));

  AutofillProfile expected_profile(*native_profile);
  expected_profile.SetRawInfo(NAME_FULL,
                              ASCIIToUTF16("Billing Mitchell Morrison"));
  expected_profile.set_use_date(sync_profile.use_date());
  expected_profile.set_use_count(2);

  std::vector<AutofillProfile*> native_profiles;
  native_profiles.push_back(native_profile);
  EXPECT_CALL(autofill_table(), GetAutofillProfiles(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(native_profiles), Return(true)));
  EXPECT_CALL(autofill_table(),
              AddAutofillProfile(MatchProfiles(expected_profile)))
      .WillOnce(Return(true));
  EXPECT_CALL(autofill_table(),
              RemoveAutofillProfile("23355099-1170-4B71-8ED4-144470CC9EBF"))
      .WillOnce(Return(true));
  std::vector<AutofillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillHelper<AutofillProfile> add_autofill(this, sync_profiles);

  EXPECT_CALL(personal_data_manager(), Refresh());
  // Adds all entries in |sync_profiles| to sync.
  StartSyncService(add_autofill.callback(), false, AUTOFILL_PROFILE);
  ASSERT_TRUE(add_autofill.success());

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(
      GetAutofillProfilesFromSyncDBUnderProfileNode(&new_sync_profiles));
  ASSERT_EQ(1U, new_sync_profiles.size());
  // Check that key fields are the same.
  EXPECT_TRUE(new_sync_profiles[0].IsSubsetOf(expected_profile, "en-US"));
  // Make sure the addtional information of the native profile was saved into
  // the sync profile.
  EXPECT_EQ(ASCIIToUTF16("Fox"),
            new_sync_profiles[0].GetRawInfo(ServerFieldType::COMPANY_NAME));
  // Check that the latest use date is saved.
  EXPECT_EQ(base::Time::FromTimeT(4321), new_sync_profiles[0].use_date());
  // Check that the use counts were added (default value is 1).
  EXPECT_EQ(2U, new_sync_profiles[0].use_count());
}

// Tests that a sync with a new native profile that differ only by name a new
// sync profile results in keeping both profiles.
TEST_F(ProfileSyncServiceAutofillTest, HasNativeHasSync_DifferentPrimaryInfo) {
  AutofillProfile sync_profile;
  autofill::test::SetProfileInfoWithGuid(
      &sync_profile, "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing",
      "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.",
      "unit 5", "Hollywood", "CA", "91601", "US", "12345678910");
  sync_profile.set_use_date(base::Time::FromTimeT(4321));

  AutofillProfile* native_profile = new AutofillProfile;
  autofill::test::SetProfileInfoWithGuid(
      native_profile, "23355099-1170-4B71-8ED4-144470CC9EBF", "Billing", "John",
      "Smith", "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood",
      "CA", "91601", "US", "12345678910");
  native_profile->set_use_date(base::Time::FromTimeT(1234));

  std::vector<AutofillProfile*> native_profiles;
  native_profiles.push_back(native_profile);
  EXPECT_CALL(autofill_table(), GetAutofillProfiles(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(native_profiles), Return(true)));
  EXPECT_CALL(autofill_table(), AddAutofillProfile(MatchProfiles(sync_profile)))
      .WillOnce(Return(true));
  std::vector<AutofillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillHelper<AutofillProfile> add_autofill(this, sync_profiles);

  EXPECT_CALL(personal_data_manager(), Refresh());
  // Adds all entries in |sync_profiles| to sync.
  StartSyncService(add_autofill.callback(), false, AUTOFILL_PROFILE);
  ASSERT_TRUE(add_autofill.success());

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(
      GetAutofillProfilesFromSyncDBUnderProfileNode(&new_sync_profiles));
  // The two profiles should be kept.
  ASSERT_EQ(2U, new_sync_profiles.size());
}

// Tests that a new native profile that is the same as a new sync profile except
// with different GUIDs results in the native profile being deleted and replaced
// by the sync profile.
TEST_F(ProfileSyncServiceAutofillTest, MergeProfileWithDifferentGuid) {
  AutofillProfile sync_profile;

  autofill::test::SetProfileInfoWithGuid(&sync_profile,
      "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing",
      "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");
  sync_profile.set_use_count(20);
  sync_profile.set_use_date(base::Time::FromTimeT(1234));

  std::string native_guid = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";
  AutofillProfile* native_profile = new AutofillProfile;
  autofill::test::SetProfileInfoWithGuid(native_profile,
      native_guid.c_str(), "Billing",
      "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");
  native_profile->set_use_count(5);
  native_profile->set_use_date(base::Time::FromTimeT(4321));

  std::vector<AutofillProfile*> native_profiles;
  native_profiles.push_back(native_profile);
  EXPECT_CALL(autofill_table(), GetAutofillProfiles(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(native_profiles), Return(true)));

  std::vector<AutofillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillHelper<AutofillProfile> add_autofill(this, sync_profiles);

  EXPECT_CALL(autofill_table(), AddAutofillProfile(_)).WillOnce(Return(true));
  EXPECT_CALL(autofill_table(), RemoveAutofillProfile(native_guid))
      .WillOnce(Return(true));
  EXPECT_CALL(personal_data_manager(), Refresh());
  StartSyncService(add_autofill.callback(), false, AUTOFILL_PROFILE);
  ASSERT_TRUE(add_autofill.success());

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillProfilesFromSyncDBUnderProfileNode(
      &new_sync_profiles));
  // Check that the profiles were merged.
  ASSERT_EQ(1U, new_sync_profiles.size());
  EXPECT_EQ(0, sync_profile.Compare(new_sync_profiles[0]));
  // Check that the sync guid was kept.
  EXPECT_EQ(sync_profile.guid(), new_sync_profiles[0].guid());
  // Check that the sync profile use count was kept.
  EXPECT_EQ(20U, new_sync_profiles[0].use_count());
  // Check that the sync profile use date was kept.
  EXPECT_EQ(base::Time::FromTimeT(1234), new_sync_profiles[0].use_date());
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeAddEntry) {
  EXPECT_CALL(autofill_table(), GetAllAutofillEntries(_))
      .WillOnce(Return(true));
  EXPECT_CALL(personal_data_manager(), Refresh());
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, AUTOFILL);
  StartSyncService(create_root.callback(), false, AUTOFILL);
  ASSERT_TRUE(create_root.success());

  AutofillEntry added_entry(MakeAutofillEntry("added", "entry", 1));

  EXPECT_CALL(autofill_table(), GetAutofillTimestamps(_, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(added_entry.date_created()),
                      SetArgumentPointee<3>(added_entry.date_last_used()),
                      Return(true)));

  AutofillChangeList changes;
  changes.push_back(AutofillChange(AutofillChange::ADD, added_entry.key()));

  web_data_service()->OnAutofillEntriesChanged(changes);

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&new_sync_entries,
                                           &new_sync_profiles));
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(added_entry == new_sync_entries[0]);
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeAddProfile) {
  EXPECT_CALL(autofill_table(), GetAutofillProfiles(_)).WillOnce(Return(true));
  EXPECT_CALL(personal_data_manager(), Refresh());
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, AUTOFILL_PROFILE);
  StartSyncService(create_root.callback(), false, AUTOFILL_PROFILE);
  ASSERT_TRUE(create_root.success());

  AutofillProfile added_profile;
  autofill::test::SetProfileInfoWithGuid(&added_profile,
      "D6ADA912-D374-4C0A-917D-F5C8EBE43011", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549");

  AutofillProfileChange change(
      AutofillProfileChange::ADD, added_profile.guid(), &added_profile);
  web_data_service()->OnAutofillProfileChanged(change);

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillProfilesFromSyncDBUnderProfileNode(
      &new_sync_profiles));
  ASSERT_EQ(1U, new_sync_profiles.size());
  EXPECT_EQ(0, added_profile.Compare(new_sync_profiles[0]));
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeUpdateEntry) {
  AutofillEntry original_entry(MakeAutofillEntry("my", "entry", 1));
  std::vector<AutofillEntry> original_entries;
  original_entries.push_back(original_entry);

  EXPECT_CALL(autofill_table(), GetAllAutofillEntries(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL(personal_data_manager(), Refresh());
  CreateRootHelper create_root(this, AUTOFILL);
  StartSyncService(create_root.callback(), false, AUTOFILL);
  ASSERT_TRUE(create_root.success());

  AutofillEntry updated_entry(MakeAutofillEntry("my", "entry", 1, 2));

  EXPECT_CALL(autofill_table(), GetAutofillTimestamps(_, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(updated_entry.date_created()),
                      SetArgumentPointee<3>(updated_entry.date_last_used()),
                      Return(true)));

  AutofillChangeList changes;
  changes.push_back(AutofillChange(AutofillChange::UPDATE,
                                   updated_entry.key()));
  web_data_service()->OnAutofillEntriesChanged(changes);

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&new_sync_entries,
                                           &new_sync_profiles));
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(updated_entry == new_sync_entries[0]);
}


TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeRemoveEntry) {
  AutofillEntry original_entry(MakeAutofillEntry("my", "entry", 1));
  std::vector<AutofillEntry> original_entries;
  original_entries.push_back(original_entry);

  EXPECT_CALL(autofill_table(), GetAllAutofillEntries(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL(personal_data_manager(), Refresh());
  CreateRootHelper create_root(this, AUTOFILL);
  StartSyncService(create_root.callback(), false, AUTOFILL);
  ASSERT_TRUE(create_root.success());

  AutofillChangeList changes;
  changes.push_back(AutofillChange(AutofillChange::REMOVE,
                                   original_entry.key()));
  web_data_service()->OnAutofillEntriesChanged(changes);

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&new_sync_entries,
                                           &new_sync_profiles));
  ASSERT_EQ(0U, new_sync_entries.size());
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeRemoveProfile) {
  AutofillProfile sync_profile;
  autofill::test::SetProfileInfoWithGuid(&sync_profile,
      "3BA5FA1B-1EC4-4BB3-9B57-EC92BE3C1A09", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549");
  AutofillProfile* native_profile = new AutofillProfile;
  autofill::test::SetProfileInfoWithGuid(native_profile,
      "3BA5FA1B-1EC4-4BB3-9B57-EC92BE3C1A09", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549");

  std::vector<AutofillProfile*> native_profiles;
  native_profiles.push_back(native_profile);
  EXPECT_CALL(autofill_table(), GetAutofillProfiles(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(native_profiles), Return(true)));

  std::vector<AutofillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillHelper<AutofillProfile> add_autofill(this, sync_profiles);
  EXPECT_CALL(personal_data_manager(), Refresh());
  StartSyncService(add_autofill.callback(), false, AUTOFILL_PROFILE);
  ASSERT_TRUE(add_autofill.success());

  AutofillProfileChange change(
      AutofillProfileChange::REMOVE, sync_profile.guid(), NULL);
  web_data_service()->OnAutofillProfileChanged(change);

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillProfilesFromSyncDBUnderProfileNode(
      &new_sync_profiles));
  ASSERT_EQ(0U, new_sync_profiles.size());
}

TEST_F(ProfileSyncServiceAutofillTest, ServerChangeRace) {
  // Once for MergeDataAndStartSyncing() and twice for ProcessSyncChanges(), via
  // LoadAutofillData().
  EXPECT_CALL(autofill_table(), GetAllAutofillEntries(_))
      .Times(3)
      .WillRepeatedly(Return(true));
  // On the other hand Autofill and Autocomplete are separated now, so
  // GetAutofillProfiles() should not be called.
  EXPECT_CALL(autofill_table(), GetAutofillProfiles(_)).Times(0);
  EXPECT_CALL(autofill_table(), UpdateAutofillEntries(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(personal_data_manager(), Refresh()).Times(3);
  CreateRootHelper create_root(this, AUTOFILL);
  StartSyncService(create_root.callback(), false, AUTOFILL);
  ASSERT_TRUE(create_root.success());

  std::unique_ptr<WaitableEvent> wait_for_start(
      new WaitableEvent(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::NOT_SIGNALED));
  std::unique_ptr<WaitableEvent> wait_for_syncapi(
      new WaitableEvent(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::NOT_SIGNALED));
  scoped_refptr<FakeServerUpdater> updater(new FakeServerUpdater(
      sync_service(), wait_for_start.get(), wait_for_syncapi.get(),
      data_type_thread()->task_runner()));

  // This server side update will stall waiting for CommitWaiter.
  updater->CreateNewEntry(MakeAutofillEntry("server", "entry", 1));
  wait_for_start->Wait();

  AutofillEntry syncapi_entry(MakeAutofillEntry("syncapi", "entry", 2));
  ASSERT_TRUE(AddAutofillSyncNode(syncapi_entry));
  DVLOG(1) << "Syncapi update finished.";

  // If we reach here, it means syncapi succeeded and we didn't deadlock. Yay!
  // Signal FakeServerUpdater that it can complete.
  wait_for_syncapi->Signal();

  // Make another entry to ensure nothing broke afterwards and wait for finish
  // to clean up.
  updater->WaitForUpdateCompletion();
  updater->CreateNewEntry(MakeAutofillEntry("server2", "entry2", 3));
  updater->WaitForUpdateCompletion();

  // Let callbacks posted on UI thread execute.
  base::RunLoop().RunUntilIdle();

  std::vector<AutofillEntry> sync_entries;
  std::vector<AutofillProfile> sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&sync_entries, &sync_profiles));
  EXPECT_EQ(3U, sync_entries.size());
  EXPECT_EQ(0U, sync_profiles.size());
  for (size_t i = 0; i < sync_entries.size(); i++) {
    DVLOG(1) << "Entry " << i << ": " << sync_entries[i].key().name()
             << ", " << sync_entries[i].key().value();
  }
}
