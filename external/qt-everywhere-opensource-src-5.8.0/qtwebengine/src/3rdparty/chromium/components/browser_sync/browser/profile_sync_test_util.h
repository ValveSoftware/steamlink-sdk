// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_BROWSER_PROFILE_SYNC_TEST_UTIL_H_
#define COMPONENTS_BROWSER_SYNC_BROWSER_PROFILE_SYNC_TEST_UTIL_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "base/time/time.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_driver/fake_sync_client.h"
#include "components/sync_driver/sync_api_component_factory_mock.h"
#include "components/sync_sessions/fake_sync_sessions_client.h"
#include "components/syncable_prefs/testing_pref_service_syncable.h"

namespace base {
class Time;
class TimeDelta;
}

namespace history {
class HistoryService;
}

namespace net {
class URLRequestContextGetter;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace browser_sync {

// An empty syncer::NetworkTimeUpdateCallback. Used in various tests to
// instantiate ProfileSyncService.
void EmptyNetworkTimeUpdate(const base::Time&,
                            const base::TimeDelta&,
                            const base::TimeDelta&);

// Call this to register preferences needed for ProfileSyncService creation.
void RegisterPrefsForProfileSyncService(
    user_prefs::PrefRegistrySyncable* registry);

// Aggregate this class to get all necessary support for creating a
// ProfileSyncService in tests. The test still needs to have its own
// MessageLoop, though.
class ProfileSyncServiceBundle {
 public:
#if defined(OS_CHROMEOS)
  typedef FakeSigninManagerBase FakeSigninManagerType;
#else
  typedef FakeSigninManager FakeSigninManagerType;
#endif

  ProfileSyncServiceBundle();

  ~ProfileSyncServiceBundle();

  // Builders

  // Builds a child of FakeSyncClient which overrides some of the client's
  // accessors to return objects from the bundle.
  class SyncClientBuilder {
   public:
    // Construct the builder and associate with the |bundle| to source objects
    // from.
    explicit SyncClientBuilder(ProfileSyncServiceBundle* bundle);

    ~SyncClientBuilder();

    void SetPersonalDataManager(
        autofill::PersonalDataManager* personal_data_manager);

    // The client will call this callback to produce the SyncableService
    // specific to |type|.
    void SetSyncableServiceCallback(
        const base::Callback<base::WeakPtr<syncer::SyncableService>(
            syncer::ModelType type)>& get_syncable_service_callback);

    // The client will call this callback to produce the SyncService for the
    // current Profile.
    void SetSyncServiceCallback(const base::Callback<sync_driver::SyncService*(
                                    void)>& get_sync_service_callback);

    void SetHistoryService(history::HistoryService* history_service);

    void SetBookmarkModelCallback(
        const base::Callback<bookmarks::BookmarkModel*(void)>&
            get_bookmark_model_callback);

    void set_activate_model_creation() { activate_model_creation_ = true; }

    std::unique_ptr<sync_driver::FakeSyncClient> Build();

   private:
    // Associated bundle to source objects from.
    ProfileSyncServiceBundle* const bundle_;

    autofill::PersonalDataManager* personal_data_manager_;
    base::Callback<base::WeakPtr<syncer::SyncableService>(
        syncer::ModelType type)>
        get_syncable_service_callback_;
    base::Callback<sync_driver::SyncService*(void)> get_sync_service_callback_;
    history::HistoryService* history_service_ = nullptr;
    base::Callback<bookmarks::BookmarkModel*(void)>
        get_bookmark_model_callback_;
    // If set, the built client will be able to build some ModelSafeWorker
    // instances.
    bool activate_model_creation_ = false;

    DISALLOW_COPY_AND_ASSIGN(SyncClientBuilder);
  };

  // Creates an InitParams instance with the specified |start_behavior| and
  // |sync_client|, and fills the rest with dummy values and objects owned by
  // the bundle.
  ProfileSyncService::InitParams CreateBasicInitParams(
      ProfileSyncService::StartBehavior start_behavior,
      std::unique_ptr<sync_driver::SyncClient> sync_client);

  // Accessors

  net::URLRequestContextGetter* url_request_context() {
    return url_request_context_.get();
  }

  syncable_prefs::TestingPrefServiceSyncable* pref_service() {
    return &pref_service_;
  }

  FakeProfileOAuth2TokenService* auth_service() { return &auth_service_; }

  FakeSigninManagerType* signin_manager() { return &signin_manager_; }

  AccountTrackerService* account_tracker() { return &account_tracker_; }

  SyncApiComponentFactoryMock* component_factory() {
    return &component_factory_;
  }

  sync_sessions::FakeSyncSessionsClient* sync_sessions_client() {
    return &sync_sessions_client_;
  }

  invalidation::FakeInvalidationService* fake_invalidation_service() {
    return &fake_invalidation_service_;
  }

  base::SingleThreadTaskRunner* db_thread() { return db_thread_.get(); }

  void set_db_thread(
      const scoped_refptr<base::SingleThreadTaskRunner>& db_thread) {
    db_thread_ = db_thread;
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> db_thread_;
  base::SequencedWorkerPoolOwner worker_pool_owner_;
  syncable_prefs::TestingPrefServiceSyncable pref_service_;
  TestSigninClient signin_client_;
  AccountTrackerService account_tracker_;
  FakeSigninManagerType signin_manager_;
  FakeProfileOAuth2TokenService auth_service_;
  SyncApiComponentFactoryMock component_factory_;
  sync_sessions::FakeSyncSessionsClient sync_sessions_client_;
  invalidation::FakeInvalidationService fake_invalidation_service_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceBundle);
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_BROWSER_PROFILE_SYNC_TEST_UTIL_H_
