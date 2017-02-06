// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_BROWSER_PROFILE_SYNC_SERVICE_H_
#define COMPONENTS_BROWSER_SYNC_BROWSER_PROFILE_SYNC_SERVICE_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/sync_driver/data_type_controller.h"
#include "components/sync_driver/data_type_manager.h"
#include "components/sync_driver/data_type_manager_observer.h"
#include "components/sync_driver/data_type_status_table.h"
#include "components/sync_driver/glue/sync_backend_host.h"
#include "components/sync_driver/local_device_info_provider.h"
#include "components/sync_driver/protocol_event_observer.h"
#include "components/sync_driver/startup_controller.h"
#include "components/sync_driver/sync_client.h"
#include "components/sync_driver/sync_frontend.h"
#include "components/sync_driver/sync_prefs.h"
#include "components/sync_driver/sync_service.h"
#include "components/sync_driver/sync_stopped_reporter.h"
#include "components/version_info/version_info.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/backoff_entry.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/network_time_update_callback.h"
#include "sync/internal_api/public/shutdown_reason.h"
#include "sync/internal_api/public/sync_manager_factory.h"
#include "sync/internal_api/public/user_share.h"
#include "sync/internal_api/public/util/experiments.h"
#include "sync/internal_api/public/util/unrecoverable_error_handler.h"
#include "sync/js/sync_js_controller.h"
#include "url/gurl.h"

class Profile;
class ProfileOAuth2TokenService;
class SigninManagerWrapper;
class SyncErrorController;
class SyncTypePreferenceProvider;

namespace browser_sync {
class BackendMigrator;
class FaviconCache;
class SessionsSyncManager;
}  // namespace browser_sync

namespace sync_driver {
class DataTypeManager;
class DeviceInfoSyncService;
class DeviceInfoTracker;
class LocalDeviceInfoProvider;
class OpenTabsUIDelegate;
class SyncApiComponentFactory;
class SyncClient;
}  // namespace sync_driver

namespace sync_driver_v2 {
class DeviceInfoService;
}

namespace syncer {
class BaseTransaction;
class NetworkResources;
class TypeDebugInfoObserver;
struct CommitCounters;
struct StatusCounters;
struct SyncCredentials;
struct UpdateCounters;
struct UserShare;
}  // namespace syncer

namespace sync_pb {
class EncryptedData;
}  // namespace sync_pb

// ProfileSyncService is the layer between browser subsystems like bookmarks,
// and the sync backend.  Each subsystem is logically thought of as being
// a sync datatype.
//
// Individual datatypes can, at any point, be in a variety of stages of being
// "enabled".  Here are some specific terms for concepts used in this class:
//
//   'Registered' (feature suppression for a datatype)
//
//      When a datatype is registered, the user has the option of syncing it.
//      The sync opt-in UI will show only registered types; a checkbox should
//      never be shown for an unregistered type, and nor should it ever be
//      synced.
//
//      A datatype is considered registered once RegisterDataTypeController
//      has been called with that datatype's DataTypeController.
//
//   'Preferred' (user preferences and opt-out for a datatype)
//
//      This means the user's opt-in or opt-out preference on a per-datatype
//      basis.  The sync service will try to make active exactly these types.
//      If a user has opted out of syncing a particular datatype, it will
//      be registered, but not preferred.
//
//      This state is controlled by the ConfigurePreferredDataTypes and
//      GetPreferredDataTypes.  They are stored in the preferences system,
//      and persist; though if a datatype is not registered, it cannot
//      be a preferred datatype.
//
//   'Active' (run-time initialization of sync system for a datatype)
//
//      An active datatype is a preferred datatype that is actively being
//      synchronized: the syncer has been instructed to querying the server
//      for this datatype, first-time merges have finished, and there is an
//      actively installed ChangeProcessor that listens for changes to this
//      datatype, propagating such changes into and out of the sync backend
//      as necessary.
//
//      When a datatype is in the process of becoming active, it may be
//      in some intermediate state.  Those finer-grained intermediate states
//      are differentiated by the DataTypeController state.
//
// Sync Configuration:
//
//   Sync configuration is accomplished via the following APIs:
//    * OnUserChoseDatatypes(): Set the data types the user wants to sync.
//    * SetDecryptionPassphrase(): Attempt to decrypt the user's encrypted data
//        using the passed passphrase.
//    * SetEncryptionPassphrase(): Re-encrypt the user's data using the passed
//        passphrase.
//
//   Additionally, the current sync configuration can be fetched by calling
//    * GetRegisteredDataTypes()
//    * GetPreferredDataTypes()
//    * GetActiveDataTypes()
//    * IsUsingSecondaryPassphrase()
//    * IsEncryptEverythingEnabled()
//    * IsPassphraseRequired()/IsPassphraseRequiredForDecryption()
//
//   The "sync everything" state cannot be read from ProfileSyncService, but
//   is instead pulled from SyncPrefs.HasKeepEverythingSynced().
//
// Initial sync setup:
//
//   For privacy reasons, it is usually desirable to avoid syncing any data
//   types until the user has finished setting up sync. There are two APIs
//   that control the initial sync download:
//
//    * SetFirstSetupComplete()
//    * GetSetupInProgressHandle()
//
//   SetFirstSetupComplete() should be called once the user has finished setting
//   up sync at least once on their account. GetSetupInProgressHandle() should
//   be called while the user is actively configuring their account. The handle
//   should be deleted once configuration is complete.
//
//   Once first setup has completed and there are no outstanding
//   setup-in-progress handles, CanConfigureDataTypes() will return true and
//   datatype configuration can begin.
class ProfileSyncService : public sync_driver::SyncService,
                           public sync_driver::SyncFrontend,
                           public sync_driver::SyncPrefObserver,
                           public sync_driver::DataTypeManagerObserver,
                           public syncer::UnrecoverableErrorHandler,
                           public KeyedService,
                           public OAuth2TokenService::Consumer,
                           public OAuth2TokenService::Observer,
                           public SigninManagerBase::Observer,
                           public GaiaCookieManagerService::Observer {
 public:
  typedef browser_sync::SyncBackendHost::Status Status;
  typedef base::Callback<bool(void)> PlatformSyncAllowedProvider;

  enum SyncEventCodes  {
    MIN_SYNC_EVENT_CODE = 0,

    // Events starting the sync service.
    START_FROM_NTP = 1,      // Sync was started from the ad in NTP
    START_FROM_WRENCH = 2,   // Sync was started from the Wrench menu.
    START_FROM_OPTIONS = 3,  // Sync was started from Wrench->Options.
    START_FROM_BOOKMARK_MANAGER = 4,  // Sync was started from Bookmark manager.
    START_FROM_PROFILE_MENU = 5,  // Sync was started from multiprofile menu.
    START_FROM_URL = 6,  // Sync was started from a typed URL.

    // Events regarding cancellation of the signon process of sync.
    CANCEL_FROM_SIGNON_WITHOUT_AUTH = 10,   // Cancelled before submitting
                                            // username and password.
    CANCEL_DURING_SIGNON = 11,              // Cancelled after auth.
    CANCEL_DURING_CONFIGURE = 12,           // Cancelled before choosing data
                                            // types and clicking OK.
    // Events resulting in the stoppage of sync service.
    STOP_FROM_OPTIONS = 20,  // Sync was stopped from Wrench->Options.
    STOP_FROM_ADVANCED_DIALOG = 21,  // Sync was stopped via advanced settings.

    // Miscellaneous events caused by sync service.

    MAX_SYNC_EVENT_CODE
  };

  enum SyncStatusSummary {
    UNRECOVERABLE_ERROR,
    NOT_ENABLED,
    SETUP_INCOMPLETE,
    DATATYPES_NOT_INITIALIZED,
    INITIALIZED,
    UNKNOWN_ERROR,
  };

  // If AUTO_START, sync will set IsFirstSetupComplete() automatically and sync
  // will begin syncing without the user needing to confirm sync settings.
  enum StartBehavior {
    AUTO_START,
    MANUAL_START,
  };

  // Bundles the arguments for ProfileSyncService construction. This is a
  // movable struct. Because of the non-POD data members, it needs out-of-line
  // constructors, so in particular the move constructor needs to be
  // explicitly defined.
  struct InitParams {
    InitParams();
    ~InitParams();
    InitParams(InitParams&& other);  // NOLINT

    std::unique_ptr<sync_driver::SyncClient> sync_client;
    std::unique_ptr<SigninManagerWrapper> signin_wrapper;
    ProfileOAuth2TokenService* oauth2_token_service = nullptr;
    GaiaCookieManagerService* gaia_cookie_manager_service = nullptr;
    StartBehavior start_behavior = MANUAL_START;
    syncer::NetworkTimeUpdateCallback network_time_update_callback;
    base::FilePath base_directory;
    scoped_refptr<net::URLRequestContextGetter> url_request_context;
    std::string debug_identifier;
    version_info::Channel channel = version_info::Channel::UNKNOWN;
    scoped_refptr<base::SingleThreadTaskRunner> db_thread;
    scoped_refptr<base::SingleThreadTaskRunner> file_thread;
    base::SequencedWorkerPool* blocking_pool = nullptr;

   private:
    DISALLOW_COPY_AND_ASSIGN(InitParams);
  };

  explicit ProfileSyncService(InitParams init_params);

  ~ProfileSyncService() override;

  // Initializes the object. This must be called at most once, and
  // immediately after an object of this class is constructed.
  void Initialize();

  // sync_driver::SyncService implementation
  bool IsFirstSetupComplete() const override;
  bool IsSyncAllowed() const override;
  bool IsSyncActive() const override;
  void TriggerRefresh(const syncer::ModelTypeSet& types) override;
  void OnDataTypeRequestsSyncStartup(syncer::ModelType type) override;
  bool CanSyncStart() const override;
  void RequestStop(SyncStopDataFate data_fate) override;
  void RequestStart() override;
  syncer::ModelTypeSet GetActiveDataTypes() const override;
  sync_driver::SyncClient* GetSyncClient() const override;
  syncer::ModelTypeSet GetPreferredDataTypes() const override;
  void OnUserChoseDatatypes(bool sync_everything,
                            syncer::ModelTypeSet chosen_types) override;
  void SetFirstSetupComplete() override;
  bool IsFirstSetupInProgress() const override;
  std::unique_ptr<sync_driver::SyncSetupInProgressHandle>
  GetSetupInProgressHandle() override;
  bool IsSetupInProgress() const override;
  bool ConfigurationDone() const override;
  const GoogleServiceAuthError& GetAuthError() const override;
  bool HasUnrecoverableError() const override;
  bool IsBackendInitialized() const override;
  sync_driver::OpenTabsUIDelegate* GetOpenTabsUIDelegate() override;
  bool IsPassphraseRequiredForDecryption() const override;
  base::Time GetExplicitPassphraseTime() const override;
  bool IsUsingSecondaryPassphrase() const override;
  void EnableEncryptEverything() override;
  bool IsEncryptEverythingEnabled() const override;
  void SetEncryptionPassphrase(const std::string& passphrase,
                               PassphraseType type) override;
  bool SetDecryptionPassphrase(const std::string& passphrase) override
      WARN_UNUSED_RESULT;
  bool IsCryptographerReady(
      const syncer::BaseTransaction* trans) const override;
  syncer::UserShare* GetUserShare() const override;
  sync_driver::LocalDeviceInfoProvider* GetLocalDeviceInfoProvider()
      const override;
  void AddObserver(sync_driver::SyncServiceObserver* observer) override;
  void RemoveObserver(sync_driver::SyncServiceObserver* observer) override;
  bool HasObserver(
      const sync_driver::SyncServiceObserver* observer) const override;
  void RegisterDataTypeController(
      sync_driver::DataTypeController* data_type_controller) override;
  void ReenableDatatype(syncer::ModelType type) override;
  SyncTokenStatus GetSyncTokenStatus() const override;
  std::string QuerySyncStatusSummaryString() override;
  bool QueryDetailedSyncStatus(syncer::SyncStatus* result) override;
  base::string16 GetLastSyncedTimeString() const override;
  std::string GetBackendInitializationStateString() const override;
  syncer::sessions::SyncSessionSnapshot GetLastSessionSnapshot() const override;
  base::Value* GetTypeStatusMap() const override;
  const GURL& sync_service_url() const override;
  std::string unrecoverable_error_message() const override;
  tracked_objects::Location unrecoverable_error_location() const override;
  void AddProtocolEventObserver(
      browser_sync::ProtocolEventObserver* observer) override;
  void RemoveProtocolEventObserver(
      browser_sync::ProtocolEventObserver* observer) override;
  void AddTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  void RemoveTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  base::WeakPtr<syncer::JsController> GetJsController() override;
  void GetAllNodes(const base::Callback<void(std::unique_ptr<base::ListValue>)>&
                       callback) override;

  // Add a sync type preference provider. Each provider may only be added once.
  void AddPreferenceProvider(SyncTypePreferenceProvider* provider);
  // Remove a sync type preference provider. May only be called for providers
  // that have been added. Providers must not remove themselves while being
  // called back.
  void RemovePreferenceProvider(SyncTypePreferenceProvider* provider);
  // Check whether a given sync type preference provider has been added.
  bool HasPreferenceProvider(SyncTypePreferenceProvider* provider) const;

  void RegisterAuthNotifications();
  void UnregisterAuthNotifications();

  // Returns the SyncableService for syncer::SESSIONS.
  virtual syncer::SyncableService* GetSessionsSyncableService();

  // Returns the SyncableService for syncer::DEVICE_INFO.
  virtual syncer::SyncableService* GetDeviceInfoSyncableService();

  // Returns the ModelTypeService for syncer::DEVICE_INFO.
  virtual syncer_v2::ModelTypeService* GetDeviceInfoService();

  // Returns synced devices tracker.
  virtual sync_driver::DeviceInfoTracker* GetDeviceInfoTracker() const;

  // Fills state_map with a map of current data types that are possible to
  // sync, as well as their states.
  void GetDataTypeControllerStates(
      sync_driver::DataTypeController::StateMap* state_map) const;

  // Called when asynchronous session restore has completed.
  void OnSessionRestoreComplete();

  // SyncFrontend implementation.
  void OnBackendInitialized(
      const syncer::WeakHandle<syncer::JsBackend>& js_backend,
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      const std::string& cache_guid,
      bool success) override;
  void OnSyncCycleCompleted() override;
  void OnProtocolEvent(const syncer::ProtocolEvent& event) override;
  void OnDirectoryTypeCommitCounterUpdated(
      syncer::ModelType type,
      const syncer::CommitCounters& counters) override;
  void OnDirectoryTypeUpdateCounterUpdated(
      syncer::ModelType type,
      const syncer::UpdateCounters& counters) override;
  void OnDirectoryTypeStatusCounterUpdated(
      syncer::ModelType type,
      const syncer::StatusCounters& counters) override;
  void OnConnectionStatusChange(syncer::ConnectionStatus status) override;
  void OnPassphraseRequired(
      syncer::PassphraseRequiredReason reason,
      const sync_pb::EncryptedData& pending_keys) override;
  void OnPassphraseAccepted() override;
  void OnEncryptedTypesChanged(syncer::ModelTypeSet encrypted_types,
                               bool encrypt_everything) override;
  void OnEncryptionComplete() override;
  void OnMigrationNeededForTypes(syncer::ModelTypeSet types) override;
  void OnExperimentsChanged(const syncer::Experiments& experiments) override;
  void OnActionableError(const syncer::SyncProtocolError& error) override;
  void OnLocalSetPassphraseEncryption(
      const syncer::SyncEncryptionHandler::NigoriState& nigori_state) override;

  // DataTypeManagerObserver implementation.
  void OnConfigureDone(
      const sync_driver::DataTypeManager::ConfigureResult& result) override;
  void OnConfigureStart() override;

  // DataTypeEncryptionHandler implementation.
  bool IsPassphraseRequired() const override;
  syncer::ModelTypeSet GetEncryptedDataTypes() const override;

  // SigninManagerBase::Observer implementation.
  void GoogleSigninSucceeded(const std::string& account_id,
                             const std::string& username,
                             const std::string& password) override;
  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override;

  // GaiaCookieManagerService::Observer implementation.
  void OnGaiaAccountsInCookieUpdated(
      const std::vector<gaia::ListedAccount>& accounts,
      const std::vector<gaia::ListedAccount>& signed_out_accounts,
      const GoogleServiceAuthError& error) override;

  // Get the sync status code.
  SyncStatusSummary QuerySyncStatusSummary();

  // Reconfigures the data type manager with the latest enabled types.
  // Note: Does not initialize the backend if it is not already initialized.
  // This function needs to be called only after sync has been initialized
  // (i.e.,only for reconfigurations). The reason we don't initialize the
  // backend is because if we had encountered an unrecoverable error we don't
  // want to startup once more.
  // This function is called by |SetSetupInProgress|.
  virtual void ReconfigureDatatypeManager();

  syncer::PassphraseRequiredReason passphrase_required_reason() const {
    return passphrase_required_reason_;
  }

  // Returns true if sync is requested to be running by the user.
  // Note that this does not mean that sync WILL be running; e.g. if
  // IsSyncAllowed() is false then sync won't start, and if the user
  // doesn't confirm their settings (IsFirstSetupComplete), sync will
  // never become active. Use IsSyncActive to see if sync is running.
  virtual bool IsSyncRequested() const;

  // Record stats on various events.
  static void SyncEvent(SyncEventCodes code);

  // Returns whether sync is allowed to run based on command-line switches.
  // Profile::IsSyncAllowed() is probably a better signal than this function.
  // This function can be called from any thread, and the implementation doesn't
  // assume it's running on the UI thread.
  static bool IsSyncAllowedByFlag();

  // Returns whether sync is currently allowed on this platform.
  bool IsSyncAllowedByPlatform() const;

  // Returns whether sync is managed, i.e. controlled by configuration
  // management. If so, the user is not allowed to configure sync.
  virtual bool IsManaged() const;

  // syncer::UnrecoverableErrorHandler implementation.
  void OnUnrecoverableError(const tracked_objects::Location& from_here,
                            const std::string& message) override;

  // The functions below (until ActivateDataType()) should only be
  // called if IsBackendInitialized() is true.

  // TODO(akalin): These two functions are used only by
  // ProfileSyncServiceHarness.  Figure out a different way to expose
  // this info to that class, and remove these functions.

  // Returns whether or not the underlying sync engine has made any
  // local changes to items that have not yet been synced with the
  // server.
  bool HasUnsyncedItems() const;

  // Used by ProfileSyncServiceHarness.  May return NULL.
  browser_sync::BackendMigrator* GetBackendMigratorForTest();

  // Used by tests to inspect interaction with OAuth2TokenService.
  bool IsRetryingAccessTokenFetchForTest() const;

  // Used by tests to inspect the OAuth2 access tokens used by PSS.
  std::string GetAccessTokenForTest() const;

  // TODO(sync): This is only used in tests.  Can we remove it?
  void GetModelSafeRoutingInfo(syncer::ModelSafeRoutingInfo* out) const;

  // SyncPrefObserver implementation.
  void OnSyncManagedPrefChange(bool is_sync_managed) override;

  // Changes which data types we're going to be syncing to |preferred_types|.
  // If it is running, the DataTypeManager will be instructed to reconfigure
  // the sync backend so that exactly these datatypes are actively synced.  See
  // class comment for more on what it means for a datatype to be Preferred.
  virtual void ChangePreferredDataTypes(
      syncer::ModelTypeSet preferred_types);

  // Returns the set of types which are enforced programmatically and can not
  // be disabled by the user.
  virtual syncer::ModelTypeSet GetForcedDataTypes() const;

  // Gets the set of all data types that could be allowed (the set that
  // should be advertised to the user).  These will typically only change
  // via a command-line option.  See class comment for more on what it means
  // for a datatype to be Registered.
  virtual syncer::ModelTypeSet GetRegisteredDataTypes() const;

  // Returns the actual passphrase type being used for encryption.
  virtual syncer::PassphraseType GetPassphraseType() const;

  // Note about setting passphrases: There are different scenarios under which
  // we might want to apply a passphrase. It could be for first-time encryption,
  // re-encryption, or for decryption by clients that sign in at a later time.
  // In addition, encryption can either be done using a custom passphrase, or by
  // reusing the GAIA password. Depending on what is happening in the system,
  // callers should determine which of the two methods below must be used.

  // Returns true if encrypting all the sync data is allowed. If this method
  // returns false, EnableEncryptEverything() should not be called.
  virtual bool IsEncryptEverythingAllowed() const;

  // Sets whether encrypting all the sync data is allowed or not.
  virtual void SetEncryptEverythingAllowed(bool allowed);

  // Returns true if the syncer is waiting for new datatypes to be encrypted.
  virtual bool encryption_pending() const;

  SigninManagerBase* signin() const;

  SyncErrorController* sync_error_controller() {
    return sync_error_controller_.get();
  }

  // TODO(sync): This is only used in tests.  Can we remove it?
  const sync_driver::DataTypeStatusTable& data_type_status_table() const;

  sync_driver::DataTypeManager::ConfigureStatus configure_status() {
    return configure_status_;
  }

  // If true, the ProfileSyncService has detected that a new GAIA signin has
  // succeeded, and is waiting for initialization to complete. This is used by
  // the UI to differentiate between a new auth error (encountered as part of
  // the initialization process) and a pre-existing auth error that just hasn't
  // been cleared yet. Virtual for testing purposes.
  virtual bool waiting_for_auth() const;

  // The set of currently enabled sync experiments.
  const syncer::Experiments& current_experiments() const;

  // OAuth2TokenService::Consumer implementation.
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // OAuth2TokenService::Observer implementation.
  void OnRefreshTokenAvailable(const std::string& account_id) override;
  void OnRefreshTokenRevoked(const std::string& account_id) override;
  void OnRefreshTokensLoaded() override;

  // KeyedService implementation.  This must be called exactly
  // once (before this object is destroyed).
  void Shutdown() override;

  browser_sync::FaviconCache* GetFaviconCache();

  // Overrides the NetworkResources used for Sync connections.
  // This function takes ownership of |network_resources|.
  void OverrideNetworkResourcesForTest(
      std::unique_ptr<syncer::NetworkResources> network_resources);

  virtual bool IsDataTypeControllerRunning(syncer::ModelType type) const;

  // This triggers a Directory::SaveChanges() call on the sync thread.
  // It should be used to persist data to disk when the process might be
  // killed in the near future.
  void FlushDirectory() const;

  // Returns a serialized NigoriKey proto generated from the bootstrap token in
  // SyncPrefs. Will return the empty string if no bootstrap token exists.
  std::string GetCustomPassphraseKey() const;

  // Set the provider for whether sync is currently allowed by the platform.
  void SetPlatformSyncAllowedProvider(
      const PlatformSyncAllowedProvider& platform_sync_allowed_provider);

  // Needed to test whether the directory is deleted properly.
  base::FilePath GetDirectoryPathForTest() const;

  // Sometimes we need to wait for tasks on the sync thread in tests.
  base::MessageLoop* GetSyncLoopForTest() const;

  // Triggers sync cycle with request to update specified |types|.
  void RefreshTypesForTest(syncer::ModelTypeSet types);

 protected:
  // Helper to install and configure a data type manager.
  void ConfigureDataTypeManager();

  // Shuts down the backend sync components.
  // |reason| dictates if syncing is being disabled or not, and whether
  // to claim ownership of sync thread from backend.
  void ShutdownImpl(syncer::ShutdownReason reason);

  // Return SyncCredentials from the OAuth2TokenService.
  syncer::SyncCredentials GetCredentials();

  virtual syncer::WeakHandle<syncer::JsEventHandler> GetJsEventHandler();

  // Helper method for managing encryption UI.
  bool IsEncryptedDatatypeEnabled() const;

  // Helper for OnUnrecoverableError.
  // TODO(tim): Use an enum for |delete_sync_database| here, in ShutdownImpl,
  // and in SyncBackendHost::Shutdown.
  void OnUnrecoverableErrorImpl(
      const tracked_objects::Location& from_here,
      const std::string& message,
      bool delete_sync_database);

  // This is a cache of the last authentication response we received from the
  // sync server. The UI queries this to display appropriate messaging to the
  // user.
  GoogleServiceAuthError last_auth_error_;

  // Our asynchronous backend to communicate with sync components living on
  // other threads.
  std::unique_ptr<browser_sync::SyncBackendHost> backend_;

  // Was the last SYNC_PASSPHRASE_REQUIRED notification sent because it
  // was required for encryption, decryption with a cached passphrase, or
  // because a new passphrase is required?
  syncer::PassphraseRequiredReason passphrase_required_reason_;

 private:
  enum UnrecoverableErrorReason {
    ERROR_REASON_UNSET,
    ERROR_REASON_SYNCER,
    ERROR_REASON_BACKEND_INIT_FAILURE,
    ERROR_REASON_CONFIGURATION_RETRY,
    ERROR_REASON_CONFIGURATION_FAILURE,
    ERROR_REASON_ACTIONABLE_ERROR,
    ERROR_REASON_LIMIT
  };

  enum AuthErrorMetric {
    AUTH_ERROR_ENCOUNTERED,
    AUTH_ERROR_FIXED,
    AUTH_ERROR_LIMIT
  };

  // The initial state of sync, for the Sync.InitialState histogram. Even if
  // this value is CAN_START, sync startup might fail for reasons that we may
  // want to consider logging in the future, such as a passphrase needed for
  // decryption, or the version of Chrome being too old. This enum is used to
  // back a UMA histogram, and should therefore be treated as append-only.
  enum SyncInitialState {
    CAN_START,                // Sync can attempt to start up.
    NOT_SIGNED_IN,            // There is no signed in user.
    NOT_REQUESTED,            // The user turned off sync.
    NOT_REQUESTED_NOT_SETUP,  // The user turned off sync and setup completed
                              // is false. Might indicate a stop-and-clear.
    NEEDS_CONFIRMATION,       // The user must confirm sync settings.
    IS_MANAGED,               // Sync is disallowed by enterprise policy.
    NOT_ALLOWED_BY_PLATFORM,  // Sync is disallowed by the platform.
    SYNC_INITIAL_STATE_LIMIT
  };

  friend class ProfileSyncServicePasswordTest;
  friend class SyncTest;
  friend class TestProfileSyncService;
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceTest, InitialState);

  // Stops the sync engine. Does NOT set IsSyncRequested to false. Use
  // RequestStop for that. |data_fate| controls whether the local sync data is
  // deleted or kept when the engine shuts down.
  void StopImpl(SyncStopDataFate data_fate);

  // Update the last auth error and notify observers of error state.
  void UpdateAuthErrorState(const GoogleServiceAuthError& error);

  // Puts the backend's sync scheduler into NORMAL mode.
  // Called when configuration is complete.
  void StartSyncingWithServer();

  // Called when we've determined that we don't need a passphrase (either
  // because OnPassphraseAccepted() was called, or because we've gotten a
  // OnPassphraseRequired() but no data types are enabled).
  void ResolvePassphraseRequired();

  // During initial signin, ProfileSyncService caches the user's signin
  // passphrase so it can be used to encrypt/decrypt data after sync starts up.
  // This routine is invoked once the backend has started up to use the
  // cached passphrase and clear it out when it is done.
  void ConsumeCachedPassphraseIfPossible();

  // RequestAccessToken initiates RPC to request downscoped access token from
  // refresh token. This happens when a new OAuth2 login token is loaded and
  // when sync server returns AUTH_ERROR which indicates it is time to refresh
  // token.
  virtual void RequestAccessToken();

  // Return true if backend should start from a fresh sync DB.
  bool ShouldDeleteSyncFolder();

  // If |delete_sync_data_folder| is true, then this method will delete all
  // previous "Sync Data" folders. (useful if the folder is partial/corrupt).
  void InitializeBackend(bool delete_sync_data_folder);

  // Initializes the various settings from the command line.
  void InitSettings();

  // Sets the last synced time to the current time.
  void UpdateLastSyncedTime();

  void NotifyObservers();
  void NotifySyncCycleCompleted();
  void NotifyForeignSessionUpdated();

  void ClearStaleErrors();

  void ClearUnrecoverableError();

  // Starts up the backend sync components.
  virtual void StartUpSlowBackendComponents();

  // Collects preferred sync data types from |preference_providers_|.
  syncer::ModelTypeSet GetDataTypesFromPreferenceProviders() const;

  // Called when the user changes the sync configuration, to update the UMA
  // stats.
  void UpdateSelectedTypesHistogram(
      bool sync_everything,
      const syncer::ModelTypeSet chosen_types) const;

#if defined(OS_CHROMEOS)
  // Refresh spare sync bootstrap token for re-enabling the sync service.
  // Called on successful sign-in notifications.
  void RefreshSpareBootstrapToken(const std::string& passphrase);
#endif

  // Internal unrecoverable error handler. Used to track error reason via
  // Sync.UnrecoverableErrors histogram.
  void OnInternalUnrecoverableError(const tracked_objects::Location& from_here,
                                    const std::string& message,
                                    bool delete_sync_database,
                                    UnrecoverableErrorReason reason);

  // Update UMA for syncing backend.
  void UpdateBackendInitUMA(bool success);

  // Various setup following backend initialization, mostly for syncing backend.
  void PostBackendInitialization();

  // Whether sync has been authenticated with an account ID.
  bool IsSignedIn() const;

  // The backend can only start if sync can start and has an auth token. This is
  // different fron CanSyncStart because it represents whether the backend can
  // be started at this moment, whereas CanSyncStart represents whether sync can
  // conceptually start without further user action (acquiring a token is an
  // automatic process).
  bool CanBackendStart() const;

  // True if a syncing backend exists.
  bool HasSyncingBackend() const;

  // Update first sync time stored in preferences
  void UpdateFirstSyncTimePref();

  // Tell the sync server that this client has disabled sync.
  void RemoveClientFromServer() const;

  // Called when the system is under memory pressure.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // Check if previous shutdown is shutdown cleanly.
  void ReportPreviousSessionMemoryWarningCount();

  // After user switches to custom passphrase encryption a set of steps needs to
  // be performed:
  // - Download all latest updates from server (catch up configure).
  // - Clear user data on server.
  // - Clear directory so that data is merged from model types and encrypted.
  // Following three functions perform these steps.

  // Calls data type manager to start catch up configure.
  void BeginConfigureCatchUpBeforeClear();

  // Calls sync backend to send ClearServerDataMessage to server.
  void ClearAndRestartSyncForPassphraseEncryption();

  // Restarts sync clearing directory in the process.
  void OnClearServerDataDone();

  // True if setup has been completed at least once and is not in progress.
  bool CanConfigureDataTypes() const;

  // Called when a SetupInProgressHandle issued by this instance is destroyed.
  virtual void OnSetupInProgressHandleDestroyed();

  // This profile's SyncClient, which abstracts away non-Sync dependencies and
  // the Sync API component factory.
  std::unique_ptr<sync_driver::SyncClient> sync_client_;

  // The class that handles getting, setting, and persisting sync
  // preferences.
  sync_driver::SyncPrefs sync_prefs_;

  // TODO(ncarter): Put this in a profile, once there is UI for it.
  // This specifies where to find the sync server.
  const GURL sync_service_url_;

  // The time that OnConfigureStart is called. This member is zero if
  // OnConfigureStart has not yet been called, and is reset to zero once
  // OnConfigureDone is called.
  base::Time sync_configure_start_time_;

  // Callback to update the network time; used for initializing the backend.
  syncer::NetworkTimeUpdateCallback network_time_update_callback_;

  // The path to the base directory under which sync should store its
  // information.
  base::FilePath base_directory_;

  // The request context in which sync should operate.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_;

  // An identifier representing this instance for debugging purposes.
  std::string debug_identifier_;

  // The product channel of the embedder.
  version_info::Channel channel_;

  // Threading context.
  scoped_refptr<base::SingleThreadTaskRunner> db_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> file_thread_;
  base::SequencedWorkerPool* blocking_pool_;

  // Indicates if this is the first time sync is being configured.  This value
  // is equal to !IsFirstSetupComplete() at the time of OnBackendInitialized().
  bool is_first_time_sync_configure_;

  // Number of UIs currently configuring the Sync service. When this number
  // is decremented back to zero, Sync setup is marked no longer in progress.
  int outstanding_setup_in_progress_handles_ = 0;

  // List of available data type controllers.
  sync_driver::DataTypeController::TypeMap data_type_controllers_;

  // Whether the SyncBackendHost has been initialized.
  bool backend_initialized_;

  // Set when sync receives DISABLED_BY_ADMIN error from server. Prevents
  // ProfileSyncService from starting backend till browser restarted or user
  // signed out.
  bool sync_disabled_by_admin_;

  // Set to true if a signin has completed but we're still waiting for the
  // backend to refresh its credentials.
  bool is_auth_in_progress_;

  // Encapsulates user signin - used to set/get the user's authenticated
  // email address.
  const std::unique_ptr<SigninManagerWrapper> signin_;

  // Information describing an unrecoverable error.
  UnrecoverableErrorReason unrecoverable_error_reason_;
  std::string unrecoverable_error_message_;
  tracked_objects::Location unrecoverable_error_location_;

  // Manages the start and stop of the data types.
  std::unique_ptr<sync_driver::DataTypeManager> data_type_manager_;

  base::ObserverList<sync_driver::SyncServiceObserver> observers_;
  base::ObserverList<browser_sync::ProtocolEventObserver>
      protocol_event_observers_;
  base::ObserverList<syncer::TypeDebugInfoObserver> type_debug_info_observers_;

  std::set<SyncTypePreferenceProvider*> preference_providers_;

  syncer::SyncJsController sync_js_controller_;

  // This allows us to gracefully handle an ABORTED return code from the
  // DataTypeManager in the event that the server informed us to cease and
  // desist syncing immediately.
  bool expect_sync_configuration_aborted_;

  // Sometimes we need to temporarily hold on to a passphrase because we don't
  // yet have a backend to send it to.  This happens during initialization as
  // we don't StartUp until we have a valid token, which happens after valid
  // credentials were provided.
  std::string cached_passphrase_;

  // The current set of encrypted types.  Always a superset of
  // syncer::Cryptographer::SensitiveTypes().
  syncer::ModelTypeSet encrypted_types_;

  // Whether encrypting everything is allowed.
  bool encrypt_everything_allowed_;

  // Whether we want to encrypt everything.
  bool encrypt_everything_;

  // Whether we're waiting for an attempt to encryption all sync data to
  // complete. We track this at this layer in order to allow the user to cancel
  // if they e.g. don't remember their explicit passphrase.
  bool encryption_pending_;

  std::unique_ptr<browser_sync::BackendMigrator> migrator_;

  // This is the last |SyncProtocolError| we received from the server that had
  // an action set on it.
  syncer::SyncProtocolError last_actionable_error_;

  // Exposes sync errors to the UI.
  std::unique_ptr<SyncErrorController> sync_error_controller_;

  // Tracks the set of failed data types (those that encounter an error
  // or must delay loading for some reason).
  sync_driver::DataTypeStatusTable data_type_status_table_;

  sync_driver::DataTypeManager::ConfigureStatus configure_status_;

  // The set of currently enabled sync experiments.
  syncer::Experiments current_experiments_;

  // Sync's internal debug info listener. Used to record datatype configuration
  // and association information.
  syncer::WeakHandle<syncer::DataTypeDebugInfoListener> debug_info_listener_;

  // A thread where all the sync operations happen.
  // OWNERSHIP Notes:
  //     * Created when backend starts for the first time.
  //     * If sync is disabled, PSS claims ownership from backend.
  //     * If sync is reenabled, PSS passes ownership to new backend.
  std::unique_ptr<base::Thread> sync_thread_;

  // ProfileSyncService uses this service to get access tokens.
  ProfileOAuth2TokenService* const oauth2_token_service_;

  // ProfileSyncService needs to remember access token in order to invalidate it
  // with OAuth2TokenService.
  std::string access_token_;

  // ProfileSyncService needs to hold reference to access_token_request_ for
  // the duration of request in order to receive callbacks.
  std::unique_ptr<OAuth2TokenService::Request> access_token_request_;

  // If RequestAccessToken fails with transient error then retry requesting
  // access token with exponential backoff.
  base::OneShotTimer request_access_token_retry_timer_;
  net::BackoffEntry request_access_token_backoff_;

  // States related to sync token and connection.
  base::Time connection_status_update_time_;
  syncer::ConnectionStatus connection_status_;
  base::Time token_request_time_;
  base::Time token_receive_time_;
  GoogleServiceAuthError last_get_token_error_;
  base::Time next_token_request_time_;

  // The gaia cookie manager. Used for monitoring cookie jar changes to detect
  // when the user signs out of the content area.
  GaiaCookieManagerService* const gaia_cookie_manager_service_;

  std::unique_ptr<sync_driver::LocalDeviceInfoProvider> local_device_;

  // Locally owned SyncableService and ModelTypeService implementations.
  std::unique_ptr<browser_sync::SessionsSyncManager> sessions_sync_manager_;
  std::unique_ptr<sync_driver::DeviceInfoSyncService> device_info_sync_service_;
  std::unique_ptr<sync_driver_v2::DeviceInfoService> device_info_service_;

  std::unique_ptr<syncer::NetworkResources> network_resources_;

  StartBehavior start_behavior_;
  std::unique_ptr<browser_sync::StartupController> startup_controller_;

  // The full path to the sync data directory.
  base::FilePath directory_path_;

  std::unique_ptr<browser_sync::SyncStoppedReporter> sync_stopped_reporter_;

  // Listens for the system being under memory pressure.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  // Nigori state after user switching to custom passphrase, saved until
  // transition steps complete. It will be injected into new backend after sync
  // restart.
  std::unique_ptr<syncer::SyncEncryptionHandler::NigoriState>
      saved_nigori_state_;

  // When BeginConfigureCatchUpBeforeClear is called it will set
  // catch_up_configure_in_progress_ to true. This is needed to detect that call
  // to OnConfigureDone originated from BeginConfigureCatchUpBeforeClear and
  // needs to be followed by ClearAndRestartSyncForPassphraseEncryption().
  bool catch_up_configure_in_progress_;

  // Whether the major version has changed since the last time Chrome ran,
  // and therefore a passphrase required state should result in prompting
  // the user. This logic is only enabled on platforms that consume the
  // IsPassphrasePrompted sync preference.
  bool passphrase_prompt_triggered_by_version_;

  // An object that lets us check whether sync is currently allowed on this
  // platform.
  PlatformSyncAllowedProvider platform_sync_allowed_provider_;

  // Used to ensure that certain operations are performed on the thread that
  // this object was created on.
  base::ThreadChecker thread_checker_;

  // This weak factory invalidates its issued pointers when Sync is disabled.
  base::WeakPtrFactory<ProfileSyncService> sync_enabled_weak_factory_;

  base::WeakPtrFactory<ProfileSyncService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncService);
};

bool ShouldShowActionOnUI(
    const syncer::SyncProtocolError& error);


#endif  // COMPONENTS_BROWSER_SYNC_BROWSER_PROFILE_SYNC_SERVICE_H_
