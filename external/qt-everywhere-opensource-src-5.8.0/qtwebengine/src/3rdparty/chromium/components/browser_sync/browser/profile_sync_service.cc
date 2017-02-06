// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/browser/profile_sync_service.h"

#include <stddef.h>
#include <cstddef>
#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/browser_sync/common/browser_sync_switches.h"
#include "components/history/core/browser/typed_url_data_type_controller.h"
#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/json_pref_store.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_driver/backend_migrator.h"
#include "components/sync_driver/change_processor.h"
#include "components/sync_driver/data_type_controller.h"
#include "components/sync_driver/device_info.h"
#include "components/sync_driver/device_info_service.h"
#include "components/sync_driver/device_info_sync_service.h"
#include "components/sync_driver/device_info_tracker.h"
#include "components/sync_driver/glue/chrome_report_unrecoverable_error.h"
#include "components/sync_driver/glue/sync_backend_host.h"
#include "components/sync_driver/glue/sync_backend_host_impl.h"
#include "components/sync_driver/pref_names.h"
#include "components/sync_driver/signin_manager_wrapper.h"
#include "components/sync_driver/sync_api_component_factory.h"
#include "components/sync_driver/sync_client.h"
#include "components/sync_driver/sync_driver_switches.h"
#include "components/sync_driver/sync_error_controller.h"
#include "components/sync_driver/sync_stopped_reporter.h"
#include "components/sync_driver/sync_type_preference_provider.h"
#include "components/sync_driver/sync_util.h"
#include "components/sync_driver/system_encryptor.h"
#include "components/sync_driver/user_selectable_sync_type.h"
#include "components/sync_sessions/favicon_cache.h"
#include "components/sync_sessions/session_data_type_controller.h"
#include "components/sync_sessions/sessions_sync_manager.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "components/version_info/version_info_values.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context_getter.h"
#include "sync/api/model_type_store.h"
#include "sync/api/sync_error.h"
#include "sync/internal_api/public/base/stop_source.h"
#include "sync/internal_api/public/configure_reason.h"
#include "sync/internal_api/public/http_bridge_network_resources.h"
#include "sync/internal_api/public/network_resources.h"
#include "sync/internal_api/public/sessions/model_neutral_state.h"
#include "sync/internal_api/public/sessions/type_debug_info_observer.h"
#include "sync/internal_api/public/shared_model_type_processor.h"
#include "sync/internal_api/public/shutdown_reason.h"
#include "sync/internal_api/public/sync_encryption_handler.h"
#include "sync/internal_api/public/util/experiments.h"
#include "sync/internal_api/public/util/sync_db_util.h"
#include "sync/internal_api/public/util/sync_string_conversions.h"
#include "sync/js/js_event_details.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/directory.h"
#include "sync/util/cryptographer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

#if defined(OS_ANDROID)
#include "sync/internal_api/public/read_transaction.h"
#endif

using browser_sync::SessionsSyncManager;
using browser_sync::SyncBackendHost;
using sync_driver::ChangeProcessor;
using sync_driver::DataTypeController;
using sync_driver::DataTypeManager;
using sync_driver::DataTypeStatusTable;
using sync_driver::DeviceInfoSyncService;
using sync_driver_v2::DeviceInfoService;
using syncer::ModelType;
using syncer::ModelTypeSet;
using syncer::JsBackend;
using syncer::JsController;
using syncer::JsEventDetails;
using syncer::JsEventHandler;
using syncer::ModelSafeRoutingInfo;
using syncer::SyncCredentials;
using syncer::SyncProtocolError;
using syncer::WeakHandle;
using syncer_v2::ModelTypeStore;
using syncer_v2::SharedModelTypeProcessor;

typedef GoogleServiceAuthError AuthError;

const char kSyncUnrecoverableErrorHistogram[] =
    "Sync.UnrecoverableErrors";

const net::BackoffEntry::Policy kRequestAccessTokenBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    2000,

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.2,  // 20%

    // Maximum amount of time we are willing to delay our request in ms.
    // TODO(pavely): crbug.com/246686 ProfileSyncService should retry
    // RequestAccessToken on connection state change after backoff
    1000 * 3600 * 4,  // 4 hours.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

static const base::FilePath::CharType kSyncDataFolderName[] =
    FILE_PATH_LITERAL("Sync Data");
static const base::FilePath::CharType kLevelDBFolderName[] =
    FILE_PATH_LITERAL("LevelDB");

namespace {

// Perform the actual sync data folder deletion.
// This should only be called on the sync thread.
void DeleteSyncDataFolder(const base::FilePath& directory_path) {
  if (base::DirectoryExists(directory_path)) {
    if (!base::DeleteFile(directory_path, true))
      LOG(DFATAL) << "Could not delete the Sync Data folder.";
  }
}

}  // namespace

bool ShouldShowActionOnUI(
    const syncer::SyncProtocolError& error) {
  return (error.action != syncer::UNKNOWN_ACTION &&
          error.action != syncer::DISABLE_SYNC_ON_CLIENT &&
          error.action != syncer::STOP_SYNC_FOR_DISABLED_ACCOUNT &&
          error.action != syncer::RESET_LOCAL_SYNC_DATA);
}

ProfileSyncService::InitParams::InitParams() = default;
ProfileSyncService::InitParams::~InitParams() = default;
ProfileSyncService::InitParams::InitParams(InitParams&& other)  // NOLINT
    : sync_client(std::move(other.sync_client)),
      signin_wrapper(std::move(other.signin_wrapper)),
      oauth2_token_service(other.oauth2_token_service),
      gaia_cookie_manager_service(other.gaia_cookie_manager_service),
      start_behavior(other.start_behavior),
      network_time_update_callback(
          std::move(other.network_time_update_callback)),
      base_directory(std::move(other.base_directory)),
      url_request_context(std::move(other.url_request_context)),
      debug_identifier(std::move(other.debug_identifier)),
      channel(other.channel),
      db_thread(std::move(other.db_thread)),
      file_thread(std::move(other.file_thread)),
      blocking_pool(other.blocking_pool) {}

ProfileSyncService::ProfileSyncService(InitParams init_params)
    : OAuth2TokenService::Consumer("sync"),
      last_auth_error_(AuthError::AuthErrorNone()),
      passphrase_required_reason_(syncer::REASON_PASSPHRASE_NOT_REQUIRED),
      sync_client_(std::move(init_params.sync_client)),
      sync_prefs_(sync_client_->GetPrefService()),
      sync_service_url_(
          GetSyncServiceURL(*base::CommandLine::ForCurrentProcess(),
                            init_params.channel)),
      network_time_update_callback_(
          std::move(init_params.network_time_update_callback)),
      base_directory_(init_params.base_directory),
      url_request_context_(init_params.url_request_context),
      debug_identifier_(std::move(init_params.debug_identifier)),
      channel_(init_params.channel),
      db_thread_(init_params.db_thread),
      file_thread_(init_params.file_thread),
      blocking_pool_(init_params.blocking_pool),
      is_first_time_sync_configure_(false),
      backend_initialized_(false),
      sync_disabled_by_admin_(false),
      is_auth_in_progress_(false),
      signin_(std::move(init_params.signin_wrapper)),
      unrecoverable_error_reason_(ERROR_REASON_UNSET),
      expect_sync_configuration_aborted_(false),
      encrypted_types_(syncer::SyncEncryptionHandler::SensitiveTypes()),
      encrypt_everything_allowed_(true),
      encrypt_everything_(false),
      encryption_pending_(false),
      configure_status_(DataTypeManager::UNKNOWN),
      oauth2_token_service_(init_params.oauth2_token_service),
      request_access_token_backoff_(&kRequestAccessTokenBackoffPolicy),
      connection_status_(syncer::CONNECTION_NOT_ATTEMPTED),
      last_get_token_error_(GoogleServiceAuthError::AuthErrorNone()),
      gaia_cookie_manager_service_(init_params.gaia_cookie_manager_service),
      network_resources_(new syncer::HttpBridgeNetworkResources),
      start_behavior_(init_params.start_behavior),
      directory_path_(
          base_directory_.Append(base::FilePath(kSyncDataFolderName))),
      catch_up_configure_in_progress_(false),
      passphrase_prompt_triggered_by_version_(false),
      sync_enabled_weak_factory_(this),
      weak_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(sync_client_);
  std::string last_version = sync_prefs_.GetLastRunVersion();
  std::string current_version = PRODUCT_VERSION;
  sync_prefs_.SetLastRunVersion(current_version);

  // Check for a major version change. Note that the versions have format
  // MAJOR.MINOR.BUILD.PATCH.
  if (last_version.substr(0, last_version.find('.')) !=
      current_version.substr(0, current_version.find('.'))) {
    passphrase_prompt_triggered_by_version_ = true;
  }
}

ProfileSyncService::~ProfileSyncService() {
  if (gaia_cookie_manager_service_)
    gaia_cookie_manager_service_->RemoveObserver(this);
  sync_prefs_.RemoveSyncPrefObserver(this);
  // Shutdown() should have been called before destruction.
  CHECK(!backend_initialized_);
}

bool ProfileSyncService::CanSyncStart() const {
  return IsSyncAllowed() && IsSyncRequested() && IsSignedIn();
}

void ProfileSyncService::Initialize() {
  sync_client_->Initialize();

  // We don't pass StartupController an Unretained reference to future-proof
  // against the controller impl changing to post tasks.
  startup_controller_.reset(new browser_sync::StartupController(
      &sync_prefs_,
      base::Bind(&ProfileSyncService::CanBackendStart, base::Unretained(this)),
      base::Bind(&ProfileSyncService::StartUpSlowBackendComponents,
                 weak_factory_.GetWeakPtr())));
  std::unique_ptr<browser_sync::LocalSessionEventRouter> router(
      sync_client_->GetSyncSessionsClient()->GetLocalSessionEventRouter());
  local_device_ = sync_client_->GetSyncApiComponentFactory()
                      ->CreateLocalDeviceInfoProvider();
  sync_stopped_reporter_.reset(new browser_sync::SyncStoppedReporter(
      sync_service_url_, local_device_->GetSyncUserAgent(),
      url_request_context_,
      browser_sync::SyncStoppedReporter::ResultCallback()));
  sessions_sync_manager_.reset(new SessionsSyncManager(
      sync_client_->GetSyncSessionsClient(), &sync_prefs_, local_device_.get(),
      std::move(router),
      base::Bind(&ProfileSyncService::NotifyForeignSessionUpdated,
                 sync_enabled_weak_factory_.GetWeakPtr()),
      base::Bind(&ProfileSyncService::TriggerRefresh,
                 sync_enabled_weak_factory_.GetWeakPtr(),
                 syncer::ModelTypeSet(syncer::SESSIONS))));

  if (channel_ == version_info::Channel::UNKNOWN &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSyncEnableUSSDeviceInfo)) {
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
        blocking_pool_->GetSequencedTaskRunnerWithShutdownBehavior(
            blocking_pool_->GetSequenceToken(),
            base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));
    // TODO(skym): Stop creating leveldb files when signed out.
    // TODO(skym): Verify using AsUTF8Unsafe is okay here. Should work as long
    // as the Local State file is guaranteed to be UTF-8.
    device_info_service_.reset(new DeviceInfoService(
        local_device_.get(),
        base::Bind(&ModelTypeStore::CreateStore, syncer::DEVICE_INFO,
                   directory_path_.Append(base::FilePath(kLevelDBFolderName))
                       .AsUTF8Unsafe(),
                   blocking_task_runner),
        base::Bind(&SharedModelTypeProcessor::CreateAsChangeProcessor)));
  } else {
    device_info_sync_service_.reset(
        new DeviceInfoSyncService(local_device_.get()));
  }

  sync_driver::SyncApiComponentFactory::RegisterDataTypesMethod
      register_platform_types_callback =
          sync_client_->GetRegisterPlatformTypesCallback();
  sync_client_->GetSyncApiComponentFactory()->RegisterDataTypes(
      this, register_platform_types_callback);

  if (gaia_cookie_manager_service_)
    gaia_cookie_manager_service_->AddObserver(this);

  // We clear this here (vs Shutdown) because we want to remember that an error
  // happened on shutdown so we can display details (message, location) about it
  // in about:sync.
  ClearStaleErrors();

  sync_prefs_.AddSyncPrefObserver(this);

  SyncInitialState sync_state = CAN_START;
  if (!IsSignedIn()) {
    sync_state = NOT_SIGNED_IN;
  } else if (IsManaged()) {
    sync_state = IS_MANAGED;
  } else if (!IsSyncAllowedByPlatform()) {
    // This case should currently never be hit, as Android's master sync isn't
    // plumbed into PSS until after this function. See http://crbug.com/568771.
    sync_state = NOT_ALLOWED_BY_PLATFORM;
  } else if (!IsSyncRequested()) {
    if (IsFirstSetupComplete()) {
      sync_state = NOT_REQUESTED;
    } else {
      sync_state = NOT_REQUESTED_NOT_SETUP;
    }
  } else if (!IsFirstSetupComplete()) {
    sync_state = NEEDS_CONFIRMATION;
  }
  UMA_HISTOGRAM_ENUMERATION("Sync.InitialState", sync_state,
                            SYNC_INITIAL_STATE_LIMIT);

  // If sync isn't allowed, the only thing to do is to turn it off.
  if (!IsSyncAllowed()) {
    // Only clear data if disallowed by policy.
    RequestStop(IsManaged() ? CLEAR_DATA : KEEP_DATA);
    return;
  }

  RegisterAuthNotifications();

  if (!IsSignedIn()) {
    // Clean up in case of previous crash during signout.
    StopImpl(CLEAR_DATA);
  }

#if defined(OS_CHROMEOS)
  std::string bootstrap_token = sync_prefs_.GetEncryptionBootstrapToken();
  if (bootstrap_token.empty()) {
    sync_prefs_.SetEncryptionBootstrapToken(
        sync_prefs_.GetSpareBootstrapToken());
  }
#endif

#if !defined(OS_ANDROID)
  DCHECK(sync_error_controller_ == NULL)
      << "Initialize() called more than once.";
  sync_error_controller_.reset(new SyncErrorController(this));
  AddObserver(sync_error_controller_.get());
#endif

  memory_pressure_listener_.reset(new base::MemoryPressureListener(
      base::Bind(&ProfileSyncService::OnMemoryPressure,
                 sync_enabled_weak_factory_.GetWeakPtr())));
  startup_controller_->Reset(GetRegisteredDataTypes());

  // Auto-start means means the first time the profile starts up, sync should
  // start up immediately.
  if (start_behavior_ == AUTO_START && IsSyncRequested() &&
      !IsFirstSetupComplete()) {
    startup_controller_->TryStartImmediately();
  } else {
    startup_controller_->TryStart();
  }
}

void ProfileSyncService::StartSyncingWithServer() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (base::FeatureList::IsEnabled(
          switches::kSyncClearDataOnPassphraseEncryption) &&
      sync_prefs_.GetPassphraseEncryptionTransitionInProgress()) {
    BeginConfigureCatchUpBeforeClear();
    return;
  }

  if (backend_)
    backend_->StartSyncingWithServer();
}

void ProfileSyncService::RegisterAuthNotifications() {
  oauth2_token_service_->AddObserver(this);
  if (signin())
    signin()->AddObserver(this);
}

void ProfileSyncService::UnregisterAuthNotifications() {
  if (signin())
    signin()->RemoveObserver(this);
  oauth2_token_service_->RemoveObserver(this);
}

void ProfileSyncService::RegisterDataTypeController(
    sync_driver::DataTypeController* data_type_controller) {
  DCHECK_EQ(data_type_controllers_.count(data_type_controller->type()), 0U);
  data_type_controllers_[data_type_controller->type()] = data_type_controller;
}

bool ProfileSyncService::IsDataTypeControllerRunning(
    syncer::ModelType type) const {
  DataTypeController::TypeMap::const_iterator iter =
      data_type_controllers_.find(type);
  if (iter == data_type_controllers_.end()) {
    return false;
  }
  return iter->second->state() == DataTypeController::RUNNING;
}

sync_driver::OpenTabsUIDelegate* ProfileSyncService::GetOpenTabsUIDelegate() {
  if (!IsDataTypeControllerRunning(syncer::SESSIONS))
    return NULL;
  return sessions_sync_manager_.get();
}

browser_sync::FaviconCache* ProfileSyncService::GetFaviconCache() {
  return sessions_sync_manager_->GetFaviconCache();
}

sync_driver::DeviceInfoTracker* ProfileSyncService::GetDeviceInfoTracker()
    const {
  // One of the two should always be non-null after initialization is done.
  if (device_info_service_) {
    return device_info_service_.get();
  } else {
    return device_info_sync_service_.get();
  }
}

sync_driver::LocalDeviceInfoProvider*
ProfileSyncService::GetLocalDeviceInfoProvider() const {
  return local_device_.get();
}

void ProfileSyncService::GetDataTypeControllerStates(
  DataTypeController::StateMap* state_map) const {
  for (DataTypeController::TypeMap::const_iterator iter =
           data_type_controllers_.begin();
       iter != data_type_controllers_.end(); ++iter)
      (*state_map)[iter->first] = iter->second.get()->state();
}

void ProfileSyncService::OnSessionRestoreComplete() {
  DataTypeController::TypeMap::const_iterator iter =
      data_type_controllers_.find(syncer::SESSIONS);
  if (iter == data_type_controllers_.end()) {
    return;
  }
  DCHECK(iter->second);

  static_cast<browser_sync::SessionDataTypeController*>(iter->second.get())
      ->OnSessionRestoreComplete();
}

SyncCredentials ProfileSyncService::GetCredentials() {
  SyncCredentials credentials;
  credentials.account_id = signin_->GetAccountIdToUse();
  DCHECK(!credentials.account_id.empty());
  credentials.email = signin_->GetEffectiveUsername();
  credentials.sync_token = access_token_;

  if (credentials.sync_token.empty())
    credentials.sync_token = "credentials_lost";

  credentials.scope_set.insert(signin_->GetSyncScopeToUse());

  return credentials;
}

bool ProfileSyncService::ShouldDeleteSyncFolder() {
  return !IsFirstSetupComplete();
}

void ProfileSyncService::InitializeBackend(bool delete_stale_data) {
  if (!backend_) {
    NOTREACHED();
    return;
  }

  SyncCredentials credentials = GetCredentials();

  if (delete_stale_data)
    ClearStaleErrors();

  SyncBackendHost::HttpPostProviderFactoryGetter
      http_post_provider_factory_getter =
          base::Bind(&syncer::NetworkResources::GetHttpPostProviderFactory,
                     base::Unretained(network_resources_.get()),
                     url_request_context_,
                     network_time_update_callback_);

  backend_->Initialize(
      this, std::move(sync_thread_), db_thread_, file_thread_,
      GetJsEventHandler(), sync_service_url_, local_device_->GetSyncUserAgent(),
      credentials, delete_stale_data,
      std::unique_ptr<syncer::SyncManagerFactory>(
          new syncer::SyncManagerFactory()),
      MakeWeakHandle(sync_enabled_weak_factory_.GetWeakPtr()),
      base::Bind(browser_sync::ChromeReportUnrecoverableError, channel_),
      http_post_provider_factory_getter, std::move(saved_nigori_state_));
}

bool ProfileSyncService::IsEncryptedDatatypeEnabled() const {
  if (encryption_pending())
    return true;
  const syncer::ModelTypeSet preferred_types = GetPreferredDataTypes();
  const syncer::ModelTypeSet encrypted_types = GetEncryptedDataTypes();
  DCHECK(encrypted_types.Has(syncer::PASSWORDS));
  return !Intersection(preferred_types, encrypted_types).Empty();
}

void ProfileSyncService::OnProtocolEvent(
    const syncer::ProtocolEvent& event) {
  FOR_EACH_OBSERVER(browser_sync::ProtocolEventObserver,
                    protocol_event_observers_,
                    OnProtocolEvent(event));
}

void ProfileSyncService::OnDirectoryTypeCommitCounterUpdated(
    syncer::ModelType type,
    const syncer::CommitCounters& counters) {
  FOR_EACH_OBSERVER(syncer::TypeDebugInfoObserver,
                    type_debug_info_observers_,
                    OnCommitCountersUpdated(type, counters));
}

void ProfileSyncService::OnDirectoryTypeUpdateCounterUpdated(
    syncer::ModelType type,
    const syncer::UpdateCounters& counters) {
  FOR_EACH_OBSERVER(syncer::TypeDebugInfoObserver,
                    type_debug_info_observers_,
                    OnUpdateCountersUpdated(type, counters));
}

void ProfileSyncService::OnDirectoryTypeStatusCounterUpdated(
    syncer::ModelType type,
    const syncer::StatusCounters& counters) {
  FOR_EACH_OBSERVER(syncer::TypeDebugInfoObserver,
                    type_debug_info_observers_,
                    OnStatusCountersUpdated(type, counters));
}

void ProfileSyncService::OnDataTypeRequestsSyncStartup(
    syncer::ModelType type) {
  DCHECK(syncer::UserTypes().Has(type));

  if (!GetPreferredDataTypes().Has(type)) {
    // We can get here as datatype SyncableServices are typically wired up
    // to the native datatype even if sync isn't enabled.
    DVLOG(1) << "Dropping sync startup request because type "
             << syncer::ModelTypeToString(type) << "not enabled.";
    return;
  }

  // If this is a data type change after a major version update, reset the
  // passphrase prompted state and notify observers.
  if (IsPassphraseRequired() && passphrase_prompt_triggered_by_version_) {
    // The major version has changed and a local syncable change was made.
    // Reset the passphrase prompt state.
    passphrase_prompt_triggered_by_version_ = false;
    sync_prefs_.SetPassphrasePrompted(false);
    NotifyObservers();
  }

  if (backend_.get()) {
    DVLOG(1) << "A data type requested sync startup, but it looks like "
                "something else beat it to the punch.";
    return;
  }

  startup_controller_->OnDataTypeRequestsSyncStartup(type);
}

void ProfileSyncService::StartUpSlowBackendComponents() {
  invalidation::InvalidationService* invalidator =
      sync_client_->GetInvalidationService();

  backend_.reset(
      sync_client_->GetSyncApiComponentFactory()->CreateSyncBackendHost(
          debug_identifier_, invalidator, sync_prefs_.AsWeakPtr(),
          directory_path_));

  // Initialize the backend.  Every time we start up a new SyncBackendHost,
  // we'll want to start from a fresh SyncDB, so delete any old one that might
  // be there.
  InitializeBackend(ShouldDeleteSyncFolder());

  UpdateFirstSyncTimePref();

  ReportPreviousSessionMemoryWarningCount();
}

void ProfileSyncService::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK_EQ(access_token_request_.get(), request);
  access_token_request_.reset();
  access_token_ = access_token;
  token_receive_time_ = base::Time::Now();
  last_get_token_error_ = GoogleServiceAuthError::AuthErrorNone();

  if (sync_prefs_.SyncHasAuthError()) {
    sync_prefs_.SetSyncAuthError(false);
  }

  if (HasSyncingBackend())
    backend_->UpdateCredentials(GetCredentials());
  else
    startup_controller_->TryStart();
}

void ProfileSyncService::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(access_token_request_.get(), request);
  DCHECK_NE(error.state(), GoogleServiceAuthError::NONE);
  access_token_request_.reset();
  last_get_token_error_ = error;
  switch (error.state()) {
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::REQUEST_CANCELED:
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE: {
      // Transient error. Retry after some time.
      request_access_token_backoff_.InformOfRequest(false);
      next_token_request_time_ = base::Time::Now() +
          request_access_token_backoff_.GetTimeUntilRelease();
      request_access_token_retry_timer_.Start(
          FROM_HERE, request_access_token_backoff_.GetTimeUntilRelease(),
          base::Bind(&ProfileSyncService::RequestAccessToken,
                     sync_enabled_weak_factory_.GetWeakPtr()));
      NotifyObservers();
      break;
    }
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS: {
      if (!sync_prefs_.SyncHasAuthError()) {
        sync_prefs_.SetSyncAuthError(true);
        UMA_HISTOGRAM_ENUMERATION("Sync.SyncAuthError",
                                  AUTH_ERROR_ENCOUNTERED,
                                  AUTH_ERROR_LIMIT);
      }
      // Fallthrough.
    }
    default: {
      if (error.state() != GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS) {
        LOG(ERROR) << "Unexpected persistent error: " << error.ToString();
      }
      // Show error to user.
      UpdateAuthErrorState(error);
    }
  }
}

void ProfileSyncService::OnRefreshTokenAvailable(
    const std::string& account_id) {
  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422460 ProfileSyncService::OnRefreshTokenAvailable"));

  if (account_id == signin_->GetAccountIdToUse())
    OnRefreshTokensLoaded();
}

void ProfileSyncService::OnRefreshTokenRevoked(
    const std::string& account_id) {
  if (account_id == signin_->GetAccountIdToUse()) {
    access_token_.clear();
    UpdateAuthErrorState(
        GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
  }
}

void ProfileSyncService::OnRefreshTokensLoaded() {
  // This notification gets fired when OAuth2TokenService loads the tokens
  // from storage.
  // Initialize the backend if sync is enabled. If the sync token was
  // not loaded, GetCredentials() will generate invalid credentials to
  // cause the backend to generate an auth error (crbug.com/121755).
  if (HasSyncingBackend()) {
    RequestAccessToken();
  } else {
    startup_controller_->TryStart();
  }
}

void ProfileSyncService::Shutdown() {
  UnregisterAuthNotifications();

  ShutdownImpl(syncer::BROWSER_SHUTDOWN);
  if (sync_error_controller_) {
    // Destroy the SyncErrorController when the service shuts down for good.
    RemoveObserver(sync_error_controller_.get());
    sync_error_controller_.reset();
  }

  if (sync_thread_)
    sync_thread_->Stop();
}

void ProfileSyncService::ShutdownImpl(syncer::ShutdownReason reason) {
  if (!backend_) {
    if (reason == syncer::ShutdownReason::DISABLE_SYNC && sync_thread_) {
      // If the backend is already shut down when a DISABLE_SYNC happens,
      // the data directory needs to be cleaned up here.
      sync_thread_->task_runner()->PostTask(
          FROM_HERE, base::Bind(&DeleteSyncDataFolder, directory_path_));
    }
    return;
  }

  if (reason == syncer::ShutdownReason::STOP_SYNC
      || reason == syncer::ShutdownReason::DISABLE_SYNC) {
    RemoveClientFromServer();
  }

  // First, we spin down the backend to stop change processing as soon as
  // possible.
  base::Time shutdown_start_time = base::Time::Now();
  backend_->StopSyncingForShutdown();

  // Stop all data type controllers, if needed.  Note that until Stop
  // completes, it is possible in theory to have a ChangeProcessor apply a
  // change from a native model.  In that case, it will get applied to the sync
  // database (which doesn't get destroyed until we destroy the backend below)
  // as an unsynced change.  That will be persisted, and committed on restart.
  if (data_type_manager_) {
    if (data_type_manager_->state() != DataTypeManager::STOPPED) {
      // When aborting as part of shutdown, we should expect an aborted sync
      // configure result, else we'll dcheck when we try to read the sync error.
      expect_sync_configuration_aborted_ = true;
      data_type_manager_->Stop();
    }
    data_type_manager_.reset();
  }

  // Shutdown the migrator before the backend to ensure it doesn't pull a null
  // snapshot.
  migrator_.reset();
  sync_js_controller_.AttachJsBackend(WeakHandle<syncer::JsBackend>());

  // Move aside the backend so nobody else tries to use it while we are
  // shutting it down.
  std::unique_ptr<SyncBackendHost> doomed_backend(backend_.release());
  if (doomed_backend) {
    sync_thread_ = doomed_backend->Shutdown(reason);
    doomed_backend.reset();
  }
  base::TimeDelta shutdown_time = base::Time::Now() - shutdown_start_time;
  UMA_HISTOGRAM_TIMES("Sync.Shutdown.BackendDestroyedTime", shutdown_time);

  sync_enabled_weak_factory_.InvalidateWeakPtrs();

  startup_controller_->Reset(GetRegisteredDataTypes());

  // If the sync DB is getting destroyed, the local DeviceInfo is no longer
  // valid and should be cleared from the cache.
  if (reason == syncer::ShutdownReason::DISABLE_SYNC) {
    local_device_->Clear();
  }

  // Clear various flags.
  expect_sync_configuration_aborted_ = false;
  is_auth_in_progress_ = false;
  backend_initialized_ = false;
  cached_passphrase_.clear();
  encryption_pending_ = false;
  encrypt_everything_ = false;
  encrypted_types_ = syncer::SyncEncryptionHandler::SensitiveTypes();
  passphrase_required_reason_ = syncer::REASON_PASSPHRASE_NOT_REQUIRED;
  catch_up_configure_in_progress_ = false;
  access_token_.clear();
  request_access_token_retry_timer_.Stop();
  // Revert to "no auth error".
  if (last_auth_error_.state() != GoogleServiceAuthError::NONE)
    UpdateAuthErrorState(GoogleServiceAuthError::AuthErrorNone());

  NotifyObservers();

  // Mark this as a clean shutdown(without crash).
  sync_prefs_.SetCleanShutdown(true);
}

void ProfileSyncService::StopImpl(SyncStopDataFate data_fate) {
  switch (data_fate) {
    case KEEP_DATA:
      // TODO(maxbogue): Investigate whether this logic can/should be moved
      // into ShutdownImpl or SyncBackendHost itself.
      if (HasSyncingBackend()) {
        backend_->UnregisterInvalidationIds();
      }
      ShutdownImpl(syncer::STOP_SYNC);
      break;
    case CLEAR_DATA:
      // Clear prefs (including SyncSetupHasCompleted) before shutting down so
      // PSS clients don't think we're set up while we're shutting down.
      sync_prefs_.ClearPreferences();
      ClearUnrecoverableError();
      ShutdownImpl(syncer::DISABLE_SYNC);
      break;
  }
}

bool ProfileSyncService::IsFirstSetupComplete() const {
  return sync_prefs_.IsFirstSetupComplete();
}

void ProfileSyncService::SetFirstSetupComplete() {
  sync_prefs_.SetFirstSetupComplete();
  if (IsBackendInitialized()) {
    ReconfigureDatatypeManager();
  }
}

void ProfileSyncService::UpdateLastSyncedTime() {
  sync_prefs_.SetLastSyncedTime(base::Time::Now());
}

void ProfileSyncService::NotifyObservers() {
  FOR_EACH_OBSERVER(sync_driver::SyncServiceObserver, observers_,
                    OnStateChanged());
}

void ProfileSyncService::NotifySyncCycleCompleted() {
  FOR_EACH_OBSERVER(sync_driver::SyncServiceObserver, observers_,
                    OnSyncCycleCompleted());
}

void ProfileSyncService::NotifyForeignSessionUpdated() {
  FOR_EACH_OBSERVER(sync_driver::SyncServiceObserver, observers_,
                    OnForeignSessionUpdated());
}

void ProfileSyncService::ClearStaleErrors() {
  ClearUnrecoverableError();
  last_actionable_error_ = SyncProtocolError();
  // Clear the data type errors as well.
  if (data_type_manager_.get())
    data_type_manager_->ResetDataTypeErrors();
}

void ProfileSyncService::ClearUnrecoverableError() {
  unrecoverable_error_reason_ = ERROR_REASON_UNSET;
  unrecoverable_error_message_.clear();
  unrecoverable_error_location_ = tracked_objects::Location();
}

// An invariant has been violated.  Transition to an error state where we try
// to do as little work as possible, to avoid further corruption or crashes.
void ProfileSyncService::OnUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  // Unrecoverable errors that arrive via the syncer::UnrecoverableErrorHandler
  // interface are assumed to originate within the syncer.
  unrecoverable_error_reason_ = ERROR_REASON_SYNCER;
  OnUnrecoverableErrorImpl(from_here, message, true);
}

void ProfileSyncService::OnUnrecoverableErrorImpl(
    const tracked_objects::Location& from_here,
    const std::string& message,
    bool delete_sync_database) {
  DCHECK(HasUnrecoverableError());
  unrecoverable_error_message_ = message;
  unrecoverable_error_location_ = from_here;

  UMA_HISTOGRAM_ENUMERATION(kSyncUnrecoverableErrorHistogram,
                            unrecoverable_error_reason_,
                            ERROR_REASON_LIMIT);
  std::string location;
  from_here.Write(true, true, &location);
  LOG(ERROR)
      << "Unrecoverable error detected at " << location
      << " -- ProfileSyncService unusable: " << message;

  // Shut all data types down.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ProfileSyncService::ShutdownImpl,
                            sync_enabled_weak_factory_.GetWeakPtr(),
                            delete_sync_database ? syncer::DISABLE_SYNC
                                                 : syncer::STOP_SYNC));
}

void ProfileSyncService::ReenableDatatype(syncer::ModelType type) {
  if (!backend_initialized_ || !data_type_manager_)
    return;
  data_type_manager_->ReenableType(type);
}

void ProfileSyncService::UpdateBackendInitUMA(bool success) {
  is_first_time_sync_configure_ = !IsFirstSetupComplete();

  if (is_first_time_sync_configure_) {
    UMA_HISTOGRAM_BOOLEAN("Sync.BackendInitializeFirstTimeSuccess", success);
  } else {
    UMA_HISTOGRAM_BOOLEAN("Sync.BackendInitializeRestoreSuccess", success);
  }

  base::Time on_backend_initialized_time = base::Time::Now();
  base::TimeDelta delta = on_backend_initialized_time -
      startup_controller_->start_backend_time();
  if (is_first_time_sync_configure_) {
    UMA_HISTOGRAM_LONG_TIMES("Sync.BackendInitializeFirstTime", delta);
  } else {
    UMA_HISTOGRAM_LONG_TIMES("Sync.BackendInitializeRestoreTime", delta);
  }
}

void ProfileSyncService::PostBackendInitialization() {
  if (protocol_event_observers_.might_have_observers()) {
    backend_->RequestBufferedProtocolEventsAndEnableForwarding();
  }

  if (type_debug_info_observers_.might_have_observers()) {
    backend_->EnableDirectoryTypeDebugInfoForwarding();
  }

  // If we have a cached passphrase use it to decrypt/encrypt data now that the
  // backend is initialized. We want to call this before notifying observers in
  // case this operation affects the "passphrase required" status.
  ConsumeCachedPassphraseIfPossible();

  // The very first time the backend initializes is effectively the first time
  // we can say we successfully "synced".  LastSyncedTime will only be null in
  // this case, because the pref wasn't restored on StartUp.
  if (sync_prefs_.GetLastSyncedTime().is_null()) {
    UpdateLastSyncedTime();
  }

  // Auto-start means IsFirstSetupComplete gets set automatically.
  if (start_behavior_ == AUTO_START && !IsFirstSetupComplete()) {
    // This will trigger a configure if it completes setup.
    SetFirstSetupComplete();
  } else if (CanConfigureDataTypes()) {
    ConfigureDataTypeManager();
  }

  // Check for a cookie jar mismatch.
  std::vector<gaia::ListedAccount> accounts;
  std::vector<gaia::ListedAccount> signed_out_accounts;
  GoogleServiceAuthError error(GoogleServiceAuthError::NONE);
  if (gaia_cookie_manager_service_ &&
      gaia_cookie_manager_service_->ListAccounts(
          &accounts, &signed_out_accounts)) {
    OnGaiaAccountsInCookieUpdated(accounts, signed_out_accounts, error);
  }

  NotifyObservers();
}

void ProfileSyncService::OnBackendInitialized(
    const syncer::WeakHandle<syncer::JsBackend>& js_backend,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    const std::string& cache_guid,
    bool success) {
  UpdateBackendInitUMA(success);

  if (!success) {
    // Something went unexpectedly wrong.  Play it safe: stop syncing at once
    // and surface error UI to alert the user sync has stopped.
    // Keep the directory around for now so that on restart we will retry
    // again and potentially succeed in presence of transient file IO failures
    // or permissions issues, etc.
    //
    // TODO(rlarocque): Consider making this UnrecoverableError less special.
    // Unlike every other UnrecoverableError, it does not delete our sync data.
    // This exception made sense at the time it was implemented, but our new
    // directory corruption recovery mechanism makes it obsolete.  By the time
    // we get here, we will have already tried and failed to delete the
    // directory.  It would be no big deal if we tried to delete it again.
    OnInternalUnrecoverableError(FROM_HERE,
                                 "BackendInitialize failure",
                                 false,
                                 ERROR_REASON_BACKEND_INIT_FAILURE);
    return;
  }

  backend_initialized_ = true;

  sync_js_controller_.AttachJsBackend(js_backend);
  debug_info_listener_ = debug_info_listener;

  SigninClient* signin_client = signin_->GetOriginal()->signin_client();
  DCHECK(signin_client);
  std::string signin_scoped_device_id =
      signin_client->GetSigninScopedDeviceId();

  // Initialize local device info.
  local_device_->Initialize(cache_guid, signin_scoped_device_id,
                            blocking_pool_);

  PostBackendInitialization();
}

void ProfileSyncService::OnSyncCycleCompleted() {
  UpdateLastSyncedTime();
  const syncer::sessions::SyncSessionSnapshot snapshot =
      GetLastSessionSnapshot();
  if (IsDataTypeControllerRunning(syncer::SESSIONS) &&
      snapshot.model_neutral_state().get_updates_request_types.Has(
          syncer::SESSIONS) &&
      !syncer::sessions::HasSyncerError(snapshot.model_neutral_state())) {
    // Trigger garbage collection of old sessions now that we've downloaded
    // any new session data.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&SessionsSyncManager::DoGarbageCollection,
                              base::AsWeakPtr(sessions_sync_manager_.get())));
  }
  DVLOG(2) << "Notifying observers sync cycle completed";
  NotifySyncCycleCompleted();
}

void ProfileSyncService::OnExperimentsChanged(
    const syncer::Experiments& experiments) {
  if (current_experiments_.Matches(experiments))
    return;

  current_experiments_ = experiments;

  sync_client_->GetPrefService()->SetBoolean(
      invalidation::prefs::kInvalidationServiceUseGCMChannel,
      experiments.gcm_invalidations_enabled);
}

void ProfileSyncService::UpdateAuthErrorState(const AuthError& error) {
  is_auth_in_progress_ = false;
  last_auth_error_ = error;

  NotifyObservers();
}

namespace {

AuthError ConnectionStatusToAuthError(
    syncer::ConnectionStatus status) {
  switch (status) {
    case syncer::CONNECTION_OK:
      return AuthError::AuthErrorNone();
      break;
    case syncer::CONNECTION_AUTH_ERROR:
      return AuthError(AuthError::INVALID_GAIA_CREDENTIALS);
      break;
    case syncer::CONNECTION_SERVER_ERROR:
      return AuthError(AuthError::CONNECTION_FAILED);
      break;
    default:
      NOTREACHED();
      return AuthError(AuthError::CONNECTION_FAILED);
  }
}

}  // namespace

void ProfileSyncService::OnConnectionStatusChange(
    syncer::ConnectionStatus status) {
  connection_status_update_time_ = base::Time::Now();
  connection_status_ = status;
  if (status == syncer::CONNECTION_AUTH_ERROR) {
    // Sync server returned error indicating that access token is invalid. It
    // could be either expired or access is revoked. Let's request another
    // access token and if access is revoked then request for token will fail
    // with corresponding error. If access token is repeatedly reported
    // invalid, there may be some issues with server, e.g. authentication
    // state is inconsistent on sync and token server. In that case, we
    // backoff token requests exponentially to avoid hammering token server
    // too much and to avoid getting same token due to token server's caching
    // policy. |request_access_token_retry_timer_| is used to backoff request
    // triggered by both auth error and failure talking to GAIA server.
    // Therefore, we're likely to reach the backoff ceiling more quickly than
    // you would expect from looking at the BackoffPolicy if both types of
    // errors happen. We shouldn't receive two errors back-to-back without
    // attempting a token/sync request in between, thus crank up request delay
    // unnecessary. This is because we won't make a sync request if we hit an
    // error until GAIA succeeds at sending a new token, and we won't request
    // a new token unless sync reports a token failure. But to be safe, don't
    // schedule request if this happens.
    if (request_access_token_retry_timer_.IsRunning()) {
      // The timer to perform a request later is already running; nothing
      // further needs to be done at this point.
    } else if (request_access_token_backoff_.failure_count() == 0) {
      // First time request without delay. Currently invalid token is used
      // to initialize sync backend and we'll always end up here. We don't
      // want to delay initialization.
      request_access_token_backoff_.InformOfRequest(false);
      RequestAccessToken();
    } else  {
      request_access_token_backoff_.InformOfRequest(false);
      request_access_token_retry_timer_.Start(
          FROM_HERE, request_access_token_backoff_.GetTimeUntilRelease(),
          base::Bind(&ProfileSyncService::RequestAccessToken,
                     sync_enabled_weak_factory_.GetWeakPtr()));
    }
  } else {
    // Reset backoff time after successful connection.
    if (status == syncer::CONNECTION_OK) {
      // Request shouldn't be scheduled at this time. But if it is, it's
      // possible that sync flips between OK and auth error states rapidly,
      // thus hammers token server. To be safe, only reset backoff delay when
      // no scheduled request.
      if (!request_access_token_retry_timer_.IsRunning()) {
        request_access_token_backoff_.Reset();
      }
    }

    const GoogleServiceAuthError auth_error =
        ConnectionStatusToAuthError(status);
    DVLOG(1) << "Connection status change: " << auth_error.ToString();
    UpdateAuthErrorState(auth_error);
  }
}

void ProfileSyncService::OnPassphraseRequired(
    syncer::PassphraseRequiredReason reason,
    const sync_pb::EncryptedData& pending_keys) {
  DCHECK(backend_.get());
  DCHECK(backend_->IsNigoriEnabled());

  // TODO(lipalani) : add this check to other locations as well.
  if (HasUnrecoverableError()) {
    // When unrecoverable error is detected we post a task to shutdown the
    // backend. The task might not have executed yet.
    return;
  }

  DVLOG(1) << "Passphrase required with reason: "
           << syncer::PassphraseRequiredReasonToString(reason);
  passphrase_required_reason_ = reason;

  // TODO(stanisc): http://crbug.com/351005: Does this support USS types?
  const syncer::ModelTypeSet types = GetPreferredDataTypes();
  if (data_type_manager_) {
    // Reconfigure without the encrypted types (excluded implicitly via the
    // failed datatypes handler).
    data_type_manager_->Configure(types, syncer::CONFIGURE_REASON_CRYPTO);
  }

  // Notify observers that the passphrase status may have changed.
  NotifyObservers();
}

void ProfileSyncService::OnPassphraseAccepted() {
  DVLOG(1) << "Received OnPassphraseAccepted.";

  // If the pending keys were resolved via keystore, it's possible we never
  // consumed our cached passphrase. Clear it now.
  if (!cached_passphrase_.empty())
    cached_passphrase_.clear();

  // Reset passphrase_required_reason_ since we know we no longer require the
  // passphrase. We do this here rather than down in ResolvePassphraseRequired()
  // because that can be called by OnPassphraseRequired() if no encrypted data
  // types are enabled, and we don't want to clobber the true passphrase error.
  passphrase_required_reason_ = syncer::REASON_PASSPHRASE_NOT_REQUIRED;

  // Make sure the data types that depend on the passphrase are started at
  // this time.
  // TODO(stanisc): http://crbug.com/351005: Does this support USS types?
  const syncer::ModelTypeSet types = GetPreferredDataTypes();
  if (data_type_manager_) {
    // Re-enable any encrypted types if necessary.
    data_type_manager_->Configure(types, syncer::CONFIGURE_REASON_CRYPTO);
  }

  NotifyObservers();
}

void ProfileSyncService::OnEncryptedTypesChanged(
    syncer::ModelTypeSet encrypted_types,
    bool encrypt_everything) {
  encrypted_types_ = encrypted_types;
  encrypt_everything_ = encrypt_everything;
  DCHECK(encrypt_everything_allowed_ || !encrypt_everything_);
  DVLOG(1) << "Encrypted types changed to "
           << syncer::ModelTypeSetToString(encrypted_types_)
           << " (encrypt everything is set to "
           << (encrypt_everything_ ? "true" : "false") << ")";
  DCHECK(encrypted_types_.Has(syncer::PASSWORDS));

  NotifyObservers();
}

void ProfileSyncService::OnEncryptionComplete() {
  DVLOG(1) << "Encryption complete";
  if (encryption_pending_ && encrypt_everything_) {
    encryption_pending_ = false;
    // This is to nudge the integration tests when encryption is
    // finished.
    NotifyObservers();
  }
}

void ProfileSyncService::OnMigrationNeededForTypes(
    syncer::ModelTypeSet types) {
  DCHECK(backend_initialized_);
  DCHECK(data_type_manager_.get());

  // Migrator must be valid, because we don't sync until it is created and this
  // callback originates from a sync cycle.
  migrator_->MigrateTypes(types);
}

void ProfileSyncService::OnActionableError(const SyncProtocolError& error) {
  last_actionable_error_ = error;
  DCHECK_NE(last_actionable_error_.action,
            syncer::UNKNOWN_ACTION);
  switch (error.action) {
    case syncer::UPGRADE_CLIENT:
    case syncer::CLEAR_USER_DATA_AND_RESYNC:
    case syncer::ENABLE_SYNC_ON_ACCOUNT:
    case syncer::STOP_AND_RESTART_SYNC:
      // TODO(lipalani) : if setup in progress we want to display these
      // actions in the popup. The current experience might not be optimal for
      // the user. We just dismiss the dialog.
      if (startup_controller_->IsSetupInProgress()) {
        RequestStop(CLEAR_DATA);
        expect_sync_configuration_aborted_ = true;
      }
      // Trigger an unrecoverable error to stop syncing.
      OnInternalUnrecoverableError(FROM_HERE,
                                   last_actionable_error_.error_description,
                                   true,
                                   ERROR_REASON_ACTIONABLE_ERROR);
      break;
    case syncer::DISABLE_SYNC_ON_CLIENT:
      if (error.error_type == syncer::NOT_MY_BIRTHDAY) {
        UMA_HISTOGRAM_ENUMERATION("Sync.StopSource", syncer::BIRTHDAY_ERROR,
                                  syncer::STOP_SOURCE_LIMIT);
      }
      RequestStop(CLEAR_DATA);
#if !defined(OS_CHROMEOS)
      // On every platform except ChromeOS, sign out the user after a dashboard
      // clear.
      static_cast<SigninManager*>(signin_->GetOriginal())
          ->SignOut(signin_metrics::SERVER_FORCED_DISABLE,
                    signin_metrics::SignoutDelete::IGNORE_METRIC);
#endif
      break;
    case syncer::STOP_SYNC_FOR_DISABLED_ACCOUNT:
      // Sync disabled by domain admin. we should stop syncing until next
      // restart.
      sync_disabled_by_admin_ = true;
      ShutdownImpl(syncer::DISABLE_SYNC);
      break;
    case syncer::RESET_LOCAL_SYNC_DATA:
      ShutdownImpl(syncer::DISABLE_SYNC);
      startup_controller_->TryStart();
      break;
    default:
      NOTREACHED();
  }
  NotifyObservers();
}

void ProfileSyncService::OnLocalSetPassphraseEncryption(
    const syncer::SyncEncryptionHandler::NigoriState& nigori_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!base::FeatureList::IsEnabled(
          switches::kSyncClearDataOnPassphraseEncryption))
    return;

  // At this point the user has set a custom passphrase and we have received the
  // updated nigori state. Time to cache the nigori state, and catch up the
  // active data types.
  sync_prefs_.SetSavedNigoriStateForPassphraseEncryptionTransition(
      nigori_state);
  sync_prefs_.SetPassphraseEncryptionTransitionInProgress(true);
  BeginConfigureCatchUpBeforeClear();
}

void ProfileSyncService::BeginConfigureCatchUpBeforeClear() {
  DCHECK(data_type_manager_);
  DCHECK(!saved_nigori_state_);
  saved_nigori_state_ =
      sync_prefs_.GetSavedNigoriStateForPassphraseEncryptionTransition();
  const syncer::ModelTypeSet types = GetActiveDataTypes();
  catch_up_configure_in_progress_ = true;
  data_type_manager_->Configure(types, syncer::CONFIGURE_REASON_CATCH_UP);
}

void ProfileSyncService::ClearAndRestartSyncForPassphraseEncryption() {
  DCHECK(thread_checker_.CalledOnValidThread());
  backend_->ClearServerData(
      base::Bind(&ProfileSyncService::OnClearServerDataDone,
                 sync_enabled_weak_factory_.GetWeakPtr()));
}

void ProfileSyncService::OnClearServerDataDone() {
  DCHECK(sync_prefs_.GetPassphraseEncryptionTransitionInProgress());
  sync_prefs_.SetPassphraseEncryptionTransitionInProgress(false);

  // Call to ClearServerData generates new keystore key on the server. This
  // makes keystore bootstrap token invalid. Let's clear it from preferences.
  sync_prefs_.SetKeystoreEncryptionBootstrapToken(std::string());

  // Shutdown sync, delete the Directory, then restart, restoring the cached
  // nigori state.
  ShutdownImpl(syncer::DISABLE_SYNC);
  startup_controller_->TryStart();
}

void ProfileSyncService::OnConfigureDone(
    const DataTypeManager::ConfigureResult& result) {
  configure_status_ = result.status;
  data_type_status_table_ = result.data_type_status_table;

  // We should have cleared our cached passphrase before we get here (in
  // OnBackendInitialized()).
  DCHECK(cached_passphrase_.empty());

  if (!sync_configure_start_time_.is_null()) {
    if (result.status == DataTypeManager::OK) {
      base::Time sync_configure_stop_time = base::Time::Now();
      base::TimeDelta delta = sync_configure_stop_time -
          sync_configure_start_time_;
      if (is_first_time_sync_configure_) {
        UMA_HISTOGRAM_LONG_TIMES("Sync.ServiceInitialConfigureTime", delta);
      } else {
        UMA_HISTOGRAM_LONG_TIMES("Sync.ServiceSubsequentConfigureTime",
                                  delta);
      }
    }
    sync_configure_start_time_ = base::Time();
  }

  // Notify listeners that configuration is done.
  FOR_EACH_OBSERVER(sync_driver::SyncServiceObserver, observers_,
                    OnSyncConfigurationCompleted());

  DVLOG(1) << "PSS OnConfigureDone called with status: " << configure_status_;
  // The possible status values:
  //    ABORT - Configuration was aborted. This is not an error, if
  //            initiated by user.
  //    OK - Some or all types succeeded.
  //    Everything else is an UnrecoverableError. So treat it as such.

  // First handle the abort case.
  if (configure_status_ == DataTypeManager::ABORTED &&
      expect_sync_configuration_aborted_) {
    DVLOG(0) << "ProfileSyncService::Observe Sync Configure aborted";
    expect_sync_configuration_aborted_ = false;
    return;
  }

  // Handle unrecoverable error.
  if (configure_status_ != DataTypeManager::OK) {
    // Something catastrophic had happened. We should only have one
    // error representing it.
    syncer::SyncError error =
        data_type_status_table_.GetUnrecoverableError();
    DCHECK(error.IsSet());
    std::string message =
        "Sync configuration failed with status " +
        DataTypeManager::ConfigureStatusToString(configure_status_) +
        " caused by " +
        syncer::ModelTypeSetToString(
            data_type_status_table_.GetUnrecoverableErrorTypes()) +
        ": " + error.message();
    LOG(ERROR) << "ProfileSyncService error: " << message;
    OnInternalUnrecoverableError(error.location(),
                                 message,
                                 true,
                                 ERROR_REASON_CONFIGURATION_FAILURE);
    return;
  }

  DCHECK_EQ(DataTypeManager::OK, configure_status_);

  // We should never get in a state where we have no encrypted datatypes
  // enabled, and yet we still think we require a passphrase for decryption.
  DCHECK(!(IsPassphraseRequiredForDecryption() &&
           !IsEncryptedDatatypeEnabled()));

  // This must be done before we start syncing with the server to avoid
  // sending unencrypted data up on a first time sync.
  if (encryption_pending_)
    backend_->EnableEncryptEverything();
  NotifyObservers();

  if (migrator_.get() &&
      migrator_->state() != browser_sync::BackendMigrator::IDLE) {
    // Migration in progress.  Let the migrator know we just finished
    // configuring something.  It will be up to the migrator to call
    // StartSyncingWithServer() if migration is now finished.
    migrator_->OnConfigureDone(result);
    return;
  }

  if (catch_up_configure_in_progress_) {
    catch_up_configure_in_progress_ = false;
    ClearAndRestartSyncForPassphraseEncryption();
    return;
  }

  StartSyncingWithServer();
}

void ProfileSyncService::OnConfigureStart() {
  sync_configure_start_time_ = base::Time::Now();
  NotifyObservers();
}

ProfileSyncService::SyncStatusSummary
      ProfileSyncService::QuerySyncStatusSummary() {
  if (HasUnrecoverableError()) {
    return UNRECOVERABLE_ERROR;
  } else if (!backend_) {
    return NOT_ENABLED;
  } else if (backend_.get() && !IsFirstSetupComplete()) {
    return SETUP_INCOMPLETE;
  } else if (backend_ && IsFirstSetupComplete() && data_type_manager_ &&
             data_type_manager_->state() == DataTypeManager::STOPPED) {
    return DATATYPES_NOT_INITIALIZED;
  } else if (IsSyncActive()) {
    return INITIALIZED;
  }
  return UNKNOWN_ERROR;
}

std::string ProfileSyncService::QuerySyncStatusSummaryString() {
  SyncStatusSummary status = QuerySyncStatusSummary();

  std::string config_status_str =
      configure_status_ != DataTypeManager::UNKNOWN ?
          DataTypeManager::ConfigureStatusToString(configure_status_) : "";

  switch (status) {
    case UNRECOVERABLE_ERROR:
      return "Unrecoverable error detected";
    case NOT_ENABLED:
      return "Syncing not enabled";
    case SETUP_INCOMPLETE:
      return "First time sync setup incomplete";
    case DATATYPES_NOT_INITIALIZED:
      return "Datatypes not fully initialized";
    case INITIALIZED:
      return "Sync service initialized";
    default:
      return "Status unknown: Internal error?";
  }
}

std::string ProfileSyncService::GetBackendInitializationStateString() const {
  return startup_controller_->GetBackendInitializationStateString();
}

bool ProfileSyncService::IsSetupInProgress() const {
  return startup_controller_->IsSetupInProgress();
}

bool ProfileSyncService::QueryDetailedSyncStatus(
    SyncBackendHost::Status* result) {
  if (backend_.get() && backend_initialized_) {
    *result = backend_->GetDetailedStatus();
    return true;
  } else {
    SyncBackendHost::Status status;
    status.sync_protocol_error = last_actionable_error_;
    *result = status;
    return false;
  }
}

const AuthError& ProfileSyncService::GetAuthError() const {
  return last_auth_error_;
}

bool ProfileSyncService::CanConfigureDataTypes() const {
  return IsFirstSetupComplete() && !IsSetupInProgress();
}

bool ProfileSyncService::IsFirstSetupInProgress() const {
  return !IsFirstSetupComplete() && startup_controller_->IsSetupInProgress();
}

std::unique_ptr<sync_driver::SyncSetupInProgressHandle>
ProfileSyncService::GetSetupInProgressHandle() {
  if (++outstanding_setup_in_progress_handles_ == 1) {
    DCHECK(!startup_controller_->IsSetupInProgress());
    startup_controller_->SetSetupInProgress(true);

    NotifyObservers();
  }

  return std::unique_ptr<sync_driver::SyncSetupInProgressHandle>(
      new sync_driver::SyncSetupInProgressHandle(
          base::Bind(&ProfileSyncService::OnSetupInProgressHandleDestroyed,
                     weak_factory_.GetWeakPtr())));
}

bool ProfileSyncService::IsSyncAllowed() const {
  return IsSyncAllowedByFlag() && !IsManaged() && IsSyncAllowedByPlatform();
}

bool ProfileSyncService::IsSyncActive() const {
  return backend_initialized_ && data_type_manager_ &&
         data_type_manager_->state() != DataTypeManager::STOPPED;
}

void ProfileSyncService::TriggerRefresh(const syncer::ModelTypeSet& types) {
  if (backend_initialized_)
    backend_->TriggerRefresh(types);
}

bool ProfileSyncService::IsSignedIn() const {
  // Sync is logged in if there is a non-empty effective account id.
  return !signin_->GetAccountIdToUse().empty();
}

bool ProfileSyncService::CanBackendStart() const {
  return CanSyncStart() && oauth2_token_service_ &&
         oauth2_token_service_->RefreshTokenIsAvailable(
             signin_->GetAccountIdToUse());
}

bool ProfileSyncService::IsBackendInitialized() const {
  return backend_initialized_;
}

bool ProfileSyncService::ConfigurationDone() const {
  return data_type_manager_ &&
         data_type_manager_->state() == DataTypeManager::CONFIGURED;
}

bool ProfileSyncService::waiting_for_auth() const {
  return is_auth_in_progress_;
}

const syncer::Experiments& ProfileSyncService::current_experiments() const {
  return current_experiments_;
}

bool ProfileSyncService::HasUnrecoverableError() const {
  return unrecoverable_error_reason_ != ERROR_REASON_UNSET;
}

bool ProfileSyncService::IsPassphraseRequired() const {
  return passphrase_required_reason_ !=
      syncer::REASON_PASSPHRASE_NOT_REQUIRED;
}

bool ProfileSyncService::IsPassphraseRequiredForDecryption() const {
  // If there is an encrypted datatype enabled and we don't have the proper
  // passphrase, we must prompt the user for a passphrase. The only way for the
  // user to avoid entering their passphrase is to disable the encrypted types.
  return IsEncryptedDatatypeEnabled() && IsPassphraseRequired();
}

base::string16 ProfileSyncService::GetLastSyncedTimeString() const {
  const base::Time last_synced_time = sync_prefs_.GetLastSyncedTime();
  if (last_synced_time.is_null())
    return l10n_util::GetStringUTF16(IDS_SYNC_TIME_NEVER);

  base::TimeDelta time_since_last_sync = base::Time::Now() - last_synced_time;

  if (time_since_last_sync < base::TimeDelta::FromMinutes(1))
    return l10n_util::GetStringUTF16(IDS_SYNC_TIME_JUST_NOW);

  return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                ui::TimeFormat::LENGTH_SHORT,
                                time_since_last_sync);
}

void ProfileSyncService::UpdateSelectedTypesHistogram(
    bool sync_everything, const syncer::ModelTypeSet chosen_types) const {
  if (!IsFirstSetupComplete() ||
      sync_everything != sync_prefs_.HasKeepEverythingSynced()) {
    UMA_HISTOGRAM_BOOLEAN("Sync.SyncEverything", sync_everything);
  }

  // Only log the data types that are shown in the sync settings ui.
  // Note: the order of these types must match the ordering of
  // the respective types in ModelType
  const sync_driver::user_selectable_type::UserSelectableSyncType
      user_selectable_types[] = {
    sync_driver::user_selectable_type::BOOKMARKS,
    sync_driver::user_selectable_type::PREFERENCES,
    sync_driver::user_selectable_type::PASSWORDS,
    sync_driver::user_selectable_type::AUTOFILL,
    sync_driver::user_selectable_type::THEMES,
    sync_driver::user_selectable_type::TYPED_URLS,
    sync_driver::user_selectable_type::EXTENSIONS,
    sync_driver::user_selectable_type::APPS,
    sync_driver::user_selectable_type::PROXY_TABS,
  };

  static_assert(37 == syncer::MODEL_TYPE_COUNT,
                "custom config histogram must be updated");

  if (!sync_everything) {
    const syncer::ModelTypeSet current_types = GetPreferredDataTypes();

    syncer::ModelTypeSet type_set = syncer::UserSelectableTypes();
    syncer::ModelTypeSet::Iterator it = type_set.First();

    DCHECK_EQ(arraysize(user_selectable_types), type_set.Size());

    for (size_t i = 0; i < arraysize(user_selectable_types) && it.Good();
         ++i, it.Inc()) {
      const syncer::ModelType type = it.Get();
      if (chosen_types.Has(type) &&
          (!IsFirstSetupComplete() || !current_types.Has(type))) {
        // Selected type has changed - log it.
        UMA_HISTOGRAM_ENUMERATION(
            "Sync.CustomSync",
            user_selectable_types[i],
            sync_driver::user_selectable_type::SELECTABLE_DATATYPE_COUNT + 1);
      }
    }
  }
}

#if defined(OS_CHROMEOS)
void ProfileSyncService::RefreshSpareBootstrapToken(
    const std::string& passphrase) {
  sync_driver::SystemEncryptor encryptor;
  syncer::Cryptographer temp_cryptographer(&encryptor);
  // The first 2 params (hostname and username) doesn't have any effect here.
  syncer::KeyParams key_params = {"localhost", "dummy", passphrase};

  std::string bootstrap_token;
  if (!temp_cryptographer.AddKey(key_params)) {
    NOTREACHED() << "Failed to add key to cryptographer.";
  }
  temp_cryptographer.GetBootstrapToken(&bootstrap_token);
  sync_prefs_.SetSpareBootstrapToken(bootstrap_token);
}
#endif

void ProfileSyncService::OnUserChoseDatatypes(
    bool sync_everything,
    syncer::ModelTypeSet chosen_types) {
  DCHECK(syncer::UserSelectableTypes().HasAll(chosen_types));

  if (!backend_.get() && !HasUnrecoverableError()) {
    NOTREACHED();
    return;
  }

  UpdateSelectedTypesHistogram(sync_everything, chosen_types);
  sync_prefs_.SetKeepEverythingSynced(sync_everything);

  if (data_type_manager_)
    data_type_manager_->ResetDataTypeErrors();
  ChangePreferredDataTypes(chosen_types);
}

void ProfileSyncService::ChangePreferredDataTypes(
    syncer::ModelTypeSet preferred_types) {

  DVLOG(1) << "ChangePreferredDataTypes invoked";
  const syncer::ModelTypeSet registered_types = GetRegisteredDataTypes();
  // Will only enable those types that are registered and preferred.
  sync_prefs_.SetPreferredDataTypes(registered_types, preferred_types);

  // Now reconfigure the DTM.
  ReconfigureDatatypeManager();
}

syncer::ModelTypeSet ProfileSyncService::GetActiveDataTypes() const {
  if (!IsSyncActive() || !ConfigurationDone())
    return syncer::ModelTypeSet();
  const syncer::ModelTypeSet preferred_types = GetPreferredDataTypes();
  const syncer::ModelTypeSet failed_types =
      data_type_status_table_.GetFailedTypes();
  return Difference(preferred_types, failed_types);
}

sync_driver::SyncClient* ProfileSyncService::GetSyncClient() const {
  return sync_client_.get();
}

syncer::ModelTypeSet ProfileSyncService::GetPreferredDataTypes() const {
  const syncer::ModelTypeSet registered_types = GetRegisteredDataTypes();
  const syncer::ModelTypeSet preferred_types =
      Union(sync_prefs_.GetPreferredDataTypes(registered_types),
            syncer::ControlTypes());
  const syncer::ModelTypeSet enforced_types =
      Intersection(GetDataTypesFromPreferenceProviders(), registered_types);
  return Union(preferred_types, enforced_types);
}

syncer::ModelTypeSet ProfileSyncService::GetForcedDataTypes() const {
  // TODO(treib,zea): When SyncPrefs also implements SyncTypePreferenceProvider,
  // we'll need another way to distinguish user-choosable types from
  // programmatically-enabled types.
  return GetDataTypesFromPreferenceProviders();
}

syncer::ModelTypeSet ProfileSyncService::GetRegisteredDataTypes() const {
  syncer::ModelTypeSet registered_types;
  // The data_type_controllers_ are determined by command-line flags;
  // that's effectively what controls the values returned here.
  for (DataTypeController::TypeMap::const_iterator it =
           data_type_controllers_.begin();
       it != data_type_controllers_.end(); ++it) {
    registered_types.Put(it->first);
  }
  return registered_types;
}

bool ProfileSyncService::IsUsingSecondaryPassphrase() const {
  syncer::PassphraseType passphrase_type = GetPassphraseType();
  return passphrase_type == syncer::FROZEN_IMPLICIT_PASSPHRASE ||
         passphrase_type == syncer::CUSTOM_PASSPHRASE;
}

std::string ProfileSyncService::GetCustomPassphraseKey() const {
  sync_driver::SystemEncryptor encryptor;
  syncer::Cryptographer cryptographer(&encryptor);
  cryptographer.Bootstrap(sync_prefs_.GetEncryptionBootstrapToken());
  return cryptographer.GetDefaultNigoriKeyData();
}

syncer::PassphraseType ProfileSyncService::GetPassphraseType() const {
  return backend_->GetPassphraseType();
}

base::Time ProfileSyncService::GetExplicitPassphraseTime() const {
  return backend_->GetExplicitPassphraseTime();
}

bool ProfileSyncService::IsCryptographerReady(
    const syncer::BaseTransaction* trans) const {
  return backend_.get() && backend_->IsCryptographerReady(trans);
}

void ProfileSyncService::SetPlatformSyncAllowedProvider(
    const PlatformSyncAllowedProvider& platform_sync_allowed_provider) {
  platform_sync_allowed_provider_ = platform_sync_allowed_provider;
}

void ProfileSyncService::ConfigureDataTypeManager() {
  // Don't configure datatypes if the setup UI is still on the screen - this
  // is to help multi-screen setting UIs (like iOS) where they don't want to
  // start syncing data until the user is done configuring encryption options,
  // etc. ReconfigureDatatypeManager() will get called again once the UI calls
  // SetSetupInProgress(false).
  if (!CanConfigureDataTypes()) {
    // If we can't configure the data type manager yet, we should still notify
    // observers. This is to support multiple setup UIs being open at once.
    NotifyObservers();
    return;
  }

  bool restart = false;
  if (!data_type_manager_) {
    restart = true;
    data_type_manager_.reset(
        sync_client_->GetSyncApiComponentFactory()->CreateDataTypeManager(
            debug_info_listener_, &data_type_controllers_, this, backend_.get(),
            this));

    // We create the migrator at the same time.
    migrator_.reset(new browser_sync::BackendMigrator(
        debug_identifier_, GetUserShare(), this, data_type_manager_.get(),
        base::Bind(&ProfileSyncService::StartSyncingWithServer,
                   base::Unretained(this))));
  }

  syncer::ModelTypeSet types;
  syncer::ConfigureReason reason = syncer::CONFIGURE_REASON_UNKNOWN;
  types = GetPreferredDataTypes();
  if (restart) {
    // Datatype downloads on restart are generally due to newly supported
    // datatypes (although it's also possible we're picking up where a failed
    // previous configuration left off).
    // TODO(sync): consider detecting configuration recovery and setting
    // the reason here appropriately.
    reason = syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE;
  } else {
    // The user initiated a reconfiguration (either to add or remove types).
    reason = syncer::CONFIGURE_REASON_RECONFIGURATION;
  }

  data_type_manager_->Configure(types, reason);
}

syncer::UserShare* ProfileSyncService::GetUserShare() const {
  if (backend_.get() && backend_initialized_) {
    return backend_->GetUserShare();
  }
  NOTREACHED();
  return NULL;
}

syncer::sessions::SyncSessionSnapshot
ProfileSyncService::GetLastSessionSnapshot() const {
  if (backend_)
    return backend_->GetLastSessionSnapshot();
  return syncer::sessions::SyncSessionSnapshot();
}

bool ProfileSyncService::HasUnsyncedItems() const {
  if (HasSyncingBackend() && backend_initialized_) {
    return backend_->HasUnsyncedItems();
  }
  NOTREACHED();
  return false;
}

browser_sync::BackendMigrator*
ProfileSyncService::GetBackendMigratorForTest() {
  return migrator_.get();
}

void ProfileSyncService::GetModelSafeRoutingInfo(
    syncer::ModelSafeRoutingInfo* out) const {
  if (backend_.get() && backend_initialized_) {
    backend_->GetModelSafeRoutingInfo(out);
  } else {
    NOTREACHED();
  }
}

base::Value* ProfileSyncService::GetTypeStatusMap() const {
  std::unique_ptr<base::ListValue> result(new base::ListValue());

  if (!backend_.get() || !backend_initialized_) {
    return result.release();
  }

  DataTypeStatusTable::TypeErrorMap error_map =
      data_type_status_table_.GetAllErrors();
  ModelTypeSet active_types;
  ModelTypeSet passive_types;
  ModelSafeRoutingInfo routing_info;
  backend_->GetModelSafeRoutingInfo(&routing_info);
  for (ModelSafeRoutingInfo::const_iterator it = routing_info.begin();
       it != routing_info.end(); ++it) {
    if (it->second == syncer::GROUP_PASSIVE) {
      passive_types.Put(it->first);
    } else {
      active_types.Put(it->first);
    }
  }

  SyncBackendHost::Status detailed_status = backend_->GetDetailedStatus();
  ModelTypeSet& throttled_types(detailed_status.throttled_types);
  ModelTypeSet registered = GetRegisteredDataTypes();
  std::unique_ptr<base::DictionaryValue> type_status_header(
      new base::DictionaryValue());

  type_status_header->SetString("name", "Model Type");
  type_status_header->SetString("status", "header");
  type_status_header->SetString("value", "Group Type");
  type_status_header->SetString("num_entries", "Total Entries");
  type_status_header->SetString("num_live", "Live Entries");
  result->Append(std::move(type_status_header));

  std::unique_ptr<base::DictionaryValue> type_status;
  for (ModelTypeSet::Iterator it = registered.First(); it.Good(); it.Inc()) {
    ModelType type = it.Get();

    type_status.reset(new base::DictionaryValue());
    type_status->SetString("name", ModelTypeToString(type));

    if (error_map.find(type) != error_map.end()) {
      const syncer::SyncError &error = error_map.find(type)->second;
      DCHECK(error.IsSet());
      switch (error.GetSeverity()) {
        case syncer::SyncError::SYNC_ERROR_SEVERITY_ERROR: {
            std::string error_text = "Error: " + error.location().ToString() +
                ", " + error.GetMessagePrefix() + error.message();
            type_status->SetString("status", "error");
            type_status->SetString("value", error_text);
          }
          break;
        case syncer::SyncError::SYNC_ERROR_SEVERITY_INFO:
          type_status->SetString("status", "disabled");
          type_status->SetString("value", error.message());
          break;
        default:
          NOTREACHED() << "Unexpected error severity.";
          break;
      }
    } else if (syncer::IsProxyType(type) && passive_types.Has(type)) {
      // Show a proxy type in "ok" state unless it is disabled by user.
      DCHECK(!throttled_types.Has(type));
      type_status->SetString("status", "ok");
      type_status->SetString("value", "Passive");
    } else if (throttled_types.Has(type) && passive_types.Has(type)) {
      type_status->SetString("status", "warning");
      type_status->SetString("value", "Passive, Throttled");
    } else if (passive_types.Has(type)) {
      type_status->SetString("status", "warning");
      type_status->SetString("value", "Passive");
    } else if (throttled_types.Has(type)) {
      type_status->SetString("status", "warning");
      type_status->SetString("value", "Throttled");
    } else if (active_types.Has(type)) {
      type_status->SetString("status", "ok");
      type_status->SetString("value", "Active: " +
                             ModelSafeGroupToString(routing_info[type]));
    } else {
      type_status->SetString("status", "warning");
      type_status->SetString("value", "Disabled by User");
    }

    int live_count = detailed_status.num_entries_by_type[type] -
        detailed_status.num_to_delete_entries_by_type[type];
    type_status->SetInteger("num_entries",
                            detailed_status.num_entries_by_type[type]);
    type_status->SetInteger("num_live", live_count);

    result->Append(std::move(type_status));
  }
  return result.release();
}

void ProfileSyncService::ConsumeCachedPassphraseIfPossible() {
  // If no cached passphrase, or sync backend hasn't started up yet, just exit.
  // If the backend isn't running yet, OnBackendInitialized() will call this
  // method again after the backend starts up.
  if (cached_passphrase_.empty() || !IsBackendInitialized())
    return;

  // Backend is up and running, so we can consume the cached passphrase.
  std::string passphrase = cached_passphrase_;
  cached_passphrase_.clear();

  // If we need a passphrase to decrypt data, try the cached passphrase.
  if (passphrase_required_reason() == syncer::REASON_DECRYPTION) {
    if (SetDecryptionPassphrase(passphrase)) {
      DVLOG(1) << "Cached passphrase successfully decrypted pending keys";
      return;
    }
  }

  // If we get here, we don't have pending keys (or at least, the passphrase
  // doesn't decrypt them) - just try to re-encrypt using the encryption
  // passphrase.
  if (!IsUsingSecondaryPassphrase())
    SetEncryptionPassphrase(passphrase, IMPLICIT);
}

void ProfileSyncService::RequestAccessToken() {
  // Only one active request at a time.
  if (access_token_request_ != NULL)
    return;
  request_access_token_retry_timer_.Stop();
  OAuth2TokenService::ScopeSet oauth2_scopes;
  oauth2_scopes.insert(signin_->GetSyncScopeToUse());

  // Invalidate previous token, otherwise token service will return the same
  // token again.
  const std::string& account_id = signin_->GetAccountIdToUse();
  if (!access_token_.empty()) {
    oauth2_token_service_->InvalidateAccessToken(account_id, oauth2_scopes,
                                                 access_token_);
  }

  access_token_.clear();

  token_request_time_ = base::Time::Now();
  token_receive_time_ = base::Time();
  next_token_request_time_ = base::Time();
  access_token_request_ =
      oauth2_token_service_->StartRequest(account_id, oauth2_scopes, this);
}

void ProfileSyncService::SetEncryptionPassphrase(const std::string& passphrase,
                                                 PassphraseType type) {
  // This should only be called when the backend has been initialized.
  DCHECK(IsBackendInitialized());
  DCHECK(!(type == IMPLICIT && IsUsingSecondaryPassphrase())) <<
      "Data is already encrypted using an explicit passphrase";
  DCHECK(!(type == EXPLICIT &&
           passphrase_required_reason_ == syncer::REASON_DECRYPTION)) <<
         "Can not set explicit passphrase when decryption is needed.";

  DVLOG(1) << "Setting " << (type == EXPLICIT ? "explicit" : "implicit")
           << " passphrase for encryption.";
  if (passphrase_required_reason_ == syncer::REASON_ENCRYPTION) {
    // REASON_ENCRYPTION implies that the cryptographer does not have pending
    // keys. Hence, as long as we're not trying to do an invalid passphrase
    // change (e.g. explicit -> explicit or explicit -> implicit), we know this
    // will succeed. If for some reason a new encryption key arrives via
    // sync later, the SBH will trigger another OnPassphraseRequired().
    passphrase_required_reason_ = syncer::REASON_PASSPHRASE_NOT_REQUIRED;
    NotifyObservers();
  }
  backend_->SetEncryptionPassphrase(passphrase, type == EXPLICIT);
}

bool ProfileSyncService::SetDecryptionPassphrase(
    const std::string& passphrase) {
  if (IsPassphraseRequired()) {
    DVLOG(1) << "Setting passphrase for decryption.";
    bool result = backend_->SetDecryptionPassphrase(passphrase);
    UMA_HISTOGRAM_BOOLEAN("Sync.PassphraseDecryptionSucceeded", result);
    return result;
  } else {
    NOTREACHED() << "SetDecryptionPassphrase must not be called when "
                    "IsPassphraseRequired() is false.";
    return false;
  }
}

bool ProfileSyncService::IsEncryptEverythingAllowed() const {
  return encrypt_everything_allowed_;
}

void ProfileSyncService::SetEncryptEverythingAllowed(bool allowed) {
  DCHECK(allowed || !IsBackendInitialized() || !IsEncryptEverythingEnabled());
  encrypt_everything_allowed_ = allowed;
}

void ProfileSyncService::EnableEncryptEverything() {
  DCHECK(IsEncryptEverythingAllowed());

  // Tests override IsBackendInitialized() to always return true, so we
  // must check that instead of |backend_initialized_|.
  // TODO(akalin): Fix the above. :/
  DCHECK(IsBackendInitialized());
  // TODO(atwilson): Persist the encryption_pending_ flag to address the various
  // problems around cancelling encryption in the background (crbug.com/119649).
  if (!encrypt_everything_)
    encryption_pending_ = true;
}

bool ProfileSyncService::encryption_pending() const {
  // We may be called during the setup process before we're
  // initialized (via IsEncryptedDatatypeEnabled and
  // IsPassphraseRequiredForDecryption).
  return encryption_pending_;
}

bool ProfileSyncService::IsEncryptEverythingEnabled() const {
  DCHECK(backend_initialized_);
  return encrypt_everything_ || encryption_pending_;
}

syncer::ModelTypeSet ProfileSyncService::GetEncryptedDataTypes() const {
  DCHECK(encrypted_types_.Has(syncer::PASSWORDS));
  // We may be called during the setup process before we're
  // initialized.  In this case, we default to the sensitive types.
  return encrypted_types_;
}

void ProfileSyncService::OnSyncManagedPrefChange(bool is_sync_managed) {
  if (is_sync_managed) {
    StopImpl(CLEAR_DATA);
  } else {
    // Sync is no longer disabled by policy. Try starting it up if appropriate.
    startup_controller_->TryStart();
  }
}

void ProfileSyncService::GoogleSigninSucceeded(const std::string& account_id,
                                               const std::string& username,
                                               const std::string& password) {
  if (IsSyncRequested() && !password.empty()) {
    cached_passphrase_ = password;
    // Try to consume the passphrase we just cached. If the sync backend
    // is not running yet, the passphrase will remain cached until the
    // backend starts up.
    ConsumeCachedPassphraseIfPossible();
  }
#if defined(OS_CHROMEOS)
  RefreshSpareBootstrapToken(password);
#endif
  if (!IsBackendInitialized() || GetAuthError().state() != AuthError::NONE) {
    // Track the fact that we're still waiting for auth to complete.
    is_auth_in_progress_ = true;
  }
}

void ProfileSyncService::GoogleSignedOut(const std::string& account_id,
                                         const std::string& username) {
  sync_disabled_by_admin_ = false;
  UMA_HISTOGRAM_ENUMERATION("Sync.StopSource", syncer::SIGN_OUT,
                            syncer::STOP_SOURCE_LIMIT);
  RequestStop(CLEAR_DATA);
}

void ProfileSyncService::OnGaiaAccountsInCookieUpdated(
    const std::vector<gaia::ListedAccount>& accounts,
    const std::vector<gaia::ListedAccount>& signed_out_accounts,
    const GoogleServiceAuthError& error) {
  if (!IsBackendInitialized())
    return;

  bool cookie_jar_mismatch = true;
  bool cookie_jar_empty = accounts.size() == 0;
  std::string account_id = signin_->GetAccountIdToUse();

  // Iterate through list of accounts, looking for current sync account.
  for (const auto& account : accounts) {
    if (account.id == account_id) {
      cookie_jar_mismatch = false;
      break;
    }
  }

  DVLOG(1) << "Cookie jar mismatch: " << cookie_jar_mismatch;
  DVLOG(1) << "Cookie jar empty: " << cookie_jar_empty;
  backend_->OnCookieJarChanged(cookie_jar_mismatch, cookie_jar_empty);
}

void ProfileSyncService::AddObserver(
    sync_driver::SyncServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void ProfileSyncService::RemoveObserver(
    sync_driver::SyncServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ProfileSyncService::AddProtocolEventObserver(
    browser_sync::ProtocolEventObserver* observer) {
  protocol_event_observers_.AddObserver(observer);
  if (HasSyncingBackend()) {
    backend_->RequestBufferedProtocolEventsAndEnableForwarding();
  }
}

void ProfileSyncService::RemoveProtocolEventObserver(
    browser_sync::ProtocolEventObserver* observer) {
  protocol_event_observers_.RemoveObserver(observer);
  if (HasSyncingBackend() &&
      !protocol_event_observers_.might_have_observers()) {
    backend_->DisableProtocolEventForwarding();
  }
}

void ProfileSyncService::AddTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* type_debug_info_observer) {
  type_debug_info_observers_.AddObserver(type_debug_info_observer);
  if (type_debug_info_observers_.might_have_observers() &&
      backend_initialized_) {
    backend_->EnableDirectoryTypeDebugInfoForwarding();
  }
}

void ProfileSyncService::RemoveTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* type_debug_info_observer) {
  type_debug_info_observers_.RemoveObserver(type_debug_info_observer);
  if (!type_debug_info_observers_.might_have_observers() &&
      backend_initialized_) {
    backend_->DisableDirectoryTypeDebugInfoForwarding();
  }
}

void ProfileSyncService::AddPreferenceProvider(
    SyncTypePreferenceProvider* provider) {
  DCHECK(!HasPreferenceProvider(provider))
      << "Providers may only be added once!";
  preference_providers_.insert(provider);
}

void ProfileSyncService::RemovePreferenceProvider(
    SyncTypePreferenceProvider* provider) {
  DCHECK(HasPreferenceProvider(provider))
      << "Only providers that have been added before can be removed!";
  preference_providers_.erase(provider);
}

bool ProfileSyncService::HasPreferenceProvider(
    SyncTypePreferenceProvider* provider) const {
  return preference_providers_.count(provider) > 0;
}

namespace {

class GetAllNodesRequestHelper
    : public base::RefCountedThreadSafe<GetAllNodesRequestHelper> {
 public:
  GetAllNodesRequestHelper(
      syncer::ModelTypeSet requested_types,
      const base::Callback<void(std::unique_ptr<base::ListValue>)>& callback);

  void OnReceivedNodesForTypes(
      const std::vector<syncer::ModelType>& types,
      ScopedVector<base::ListValue> scoped_node_lists);

 private:
  friend class base::RefCountedThreadSafe<GetAllNodesRequestHelper>;
  virtual ~GetAllNodesRequestHelper();

  std::unique_ptr<base::ListValue> result_accumulator_;

  syncer::ModelTypeSet awaiting_types_;
  base::Callback<void(std::unique_ptr<base::ListValue>)> callback_;
};

GetAllNodesRequestHelper::GetAllNodesRequestHelper(
    syncer::ModelTypeSet requested_types,
    const base::Callback<void(std::unique_ptr<base::ListValue>)>& callback)
    : result_accumulator_(new base::ListValue()),
      awaiting_types_(requested_types),
      callback_(callback) {}

GetAllNodesRequestHelper::~GetAllNodesRequestHelper() {
  if (!awaiting_types_.Empty()) {
    DLOG(WARNING)
        << "GetAllNodesRequest deleted before request was fulfilled.  "
        << "Missing types are: " << ModelTypeSetToString(awaiting_types_);
  }
}

// Called when the set of nodes for a type or set of types has been returned.
//
// The nodes for several types can be returned at the same time by specifying
// their types in the |types| array, and putting their results at the
// correspnding indices in the |scoped_node_lists|.
void GetAllNodesRequestHelper::OnReceivedNodesForTypes(
    const std::vector<syncer::ModelType>& types,
    ScopedVector<base::ListValue> scoped_node_lists) {
  DCHECK_EQ(types.size(), scoped_node_lists.size());

  // Take unsafe ownership of the node list.
  std::vector<base::ListValue*> node_lists;
  scoped_node_lists.release(&node_lists);

  for (size_t i = 0; i < node_lists.size() && i < types.size(); ++i) {
    const ModelType type = types[i];
    base::ListValue* node_list = node_lists[i];

    // Add these results to our list.
    std::unique_ptr<base::DictionaryValue> type_dict(
        new base::DictionaryValue());
    type_dict->SetString("type", ModelTypeToString(type));
    type_dict->Set("nodes", node_list);
    result_accumulator_->Append(std::move(type_dict));

    // Remember that this part of the request is satisfied.
    awaiting_types_.Remove(type);
  }

  if (awaiting_types_.Empty()) {
    callback_.Run(std::move(result_accumulator_));
    callback_.Reset();
  }
}

}  // namespace

void ProfileSyncService::GetAllNodes(
    const base::Callback<void(std::unique_ptr<base::ListValue>)>& callback) {
  // TODO(stanisc): crbug.com/328606: Make this work for USS datatypes.
  ModelTypeSet all_types = GetRegisteredDataTypes();
  all_types.PutAll(syncer::ControlTypes());
  scoped_refptr<GetAllNodesRequestHelper> helper =
      new GetAllNodesRequestHelper(all_types, callback);

  if (!backend_initialized_) {
    // If there's no backend available to fulfill the request, handle it here.
    ScopedVector<base::ListValue> empty_results;
    std::vector<ModelType> type_vector;
    for (ModelTypeSet::Iterator it = all_types.First(); it.Good(); it.Inc()) {
      type_vector.push_back(it.Get());
      empty_results.push_back(new base::ListValue());
    }
    helper->OnReceivedNodesForTypes(type_vector, std::move(empty_results));
  } else {
    backend_->GetAllNodesForTypes(
        all_types,
        base::Bind(&GetAllNodesRequestHelper::OnReceivedNodesForTypes, helper));
  }
}

bool ProfileSyncService::HasObserver(
    const sync_driver::SyncServiceObserver* observer) const {
  return observers_.HasObserver(observer);
}

base::WeakPtr<syncer::JsController> ProfileSyncService::GetJsController() {
  return sync_js_controller_.AsWeakPtr();
}

void ProfileSyncService::SyncEvent(SyncEventCodes code) {
  UMA_HISTOGRAM_ENUMERATION("Sync.EventCodes", code, MAX_SYNC_EVENT_CODE);
}

// static
bool ProfileSyncService::IsSyncAllowedByFlag() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSync);
}

bool ProfileSyncService::IsSyncAllowedByPlatform() const {
  return platform_sync_allowed_provider_.is_null() ||
         platform_sync_allowed_provider_.Run();
}

bool ProfileSyncService::IsManaged() const {
  return sync_prefs_.IsManaged() || sync_disabled_by_admin_;
}

void ProfileSyncService::RequestStop(SyncStopDataFate data_fate) {
  sync_prefs_.SetSyncRequested(false);
  StopImpl(data_fate);
}

bool ProfileSyncService::IsSyncRequested() const {
  return sync_prefs_.IsSyncRequested();
}

SigninManagerBase* ProfileSyncService::signin() const {
  if (!signin_)
    return NULL;
  return signin_->GetOriginal();
}

void ProfileSyncService::RequestStart() {
  if (!IsSyncAllowed()) {
    // Sync cannot be requested if it's not allowed.
    return;
  }
  DCHECK(sync_client_);
  if (!IsSyncRequested()) {
    sync_prefs_.SetSyncRequested(true);
    NotifyObservers();
  }
  startup_controller_->TryStartImmediately();
}

void ProfileSyncService::ReconfigureDatatypeManager() {
  // If we haven't initialized yet, don't configure the DTM as it could cause
  // association to start before a Directory has even been created.
  if (backend_initialized_) {
    DCHECK(backend_.get());
    ConfigureDataTypeManager();
  } else if (HasUnrecoverableError()) {
    // There is nothing more to configure. So inform the listeners,
    NotifyObservers();

    DVLOG(1) << "ConfigureDataTypeManager not invoked because of an "
             << "Unrecoverable error.";
  } else {
    DVLOG(0) << "ConfigureDataTypeManager not invoked because backend is not "
             << "initialized";
  }
}

syncer::ModelTypeSet ProfileSyncService::GetDataTypesFromPreferenceProviders()
    const {
  syncer::ModelTypeSet types;
  for (std::set<SyncTypePreferenceProvider*>::const_iterator it =
           preference_providers_.begin();
       it != preference_providers_.end();
       ++it) {
    types.PutAll((*it)->GetPreferredDataTypes());
  }
  return types;
}

const DataTypeStatusTable& ProfileSyncService::data_type_status_table()
    const {
  return data_type_status_table_;
}

void ProfileSyncService::OnInternalUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message,
    bool delete_sync_database,
    UnrecoverableErrorReason reason) {
  DCHECK(!HasUnrecoverableError());
  unrecoverable_error_reason_ = reason;
  OnUnrecoverableErrorImpl(from_here, message, delete_sync_database);
}

bool ProfileSyncService::IsRetryingAccessTokenFetchForTest() const {
  return request_access_token_retry_timer_.IsRunning();
}

std::string ProfileSyncService::GetAccessTokenForTest() const {
  return access_token_;
}

WeakHandle<syncer::JsEventHandler> ProfileSyncService::GetJsEventHandler() {
  return MakeWeakHandle(sync_js_controller_.AsWeakPtr());
}

syncer::SyncableService* ProfileSyncService::GetSessionsSyncableService() {
  return sessions_sync_manager_.get();
}

syncer::SyncableService* ProfileSyncService::GetDeviceInfoSyncableService() {
  return device_info_sync_service_.get();
}

syncer_v2::ModelTypeService* ProfileSyncService::GetDeviceInfoService() {
  return device_info_service_.get();
}

sync_driver::SyncService::SyncTokenStatus
ProfileSyncService::GetSyncTokenStatus() const {
  SyncTokenStatus status;
  status.connection_status_update_time = connection_status_update_time_;
  status.connection_status = connection_status_;
  status.token_request_time = token_request_time_;
  status.token_receive_time = token_receive_time_;
  status.last_get_token_error = last_get_token_error_;
  if (request_access_token_retry_timer_.IsRunning())
    status.next_token_request_time = next_token_request_time_;
  return status;
}

void ProfileSyncService::OverrideNetworkResourcesForTest(
    std::unique_ptr<syncer::NetworkResources> network_resources) {
  network_resources_ = std::move(network_resources);
}

bool ProfileSyncService::HasSyncingBackend() const {
  return backend_ != NULL;
}

void ProfileSyncService::UpdateFirstSyncTimePref() {
  if (!IsSignedIn()) {
    sync_prefs_.ClearFirstSyncTime();
  } else if (sync_prefs_.GetFirstSyncTime().is_null()) {
    // Set if not set before and it's syncing now.
    sync_prefs_.SetFirstSyncTime(base::Time::Now());
  }
}

void ProfileSyncService::FlushDirectory() const {
  // backend_initialized_ implies backend_ isn't NULL and the manager exists.
  // If sync is not initialized yet, we fail silently.
  if (backend_initialized_)
    backend_->FlushDirectory();
}

base::FilePath ProfileSyncService::GetDirectoryPathForTest() const {
  return directory_path_;
}

base::MessageLoop* ProfileSyncService::GetSyncLoopForTest() const {
  if (sync_thread_) {
    return sync_thread_->message_loop();
  } else if (backend_) {
    return backend_->GetSyncLoopForTesting();
  } else {
    return NULL;
  }
}

void ProfileSyncService::RefreshTypesForTest(syncer::ModelTypeSet types) {
  if (backend_initialized_)
    backend_->RefreshTypesForTest(types);
}

void ProfileSyncService::RemoveClientFromServer() const {
  if (!backend_initialized_) return;
  const std::string cache_guid = local_device_->GetLocalSyncCacheGUID();
  std::string birthday;
  syncer::UserShare* user_share = GetUserShare();
  if (user_share && user_share->directory.get()) {
    birthday = user_share->directory->store_birthday();
  }
  if (!access_token_.empty() && !cache_guid.empty() && !birthday.empty()) {
    sync_stopped_reporter_->ReportSyncStopped(
        access_token_, cache_guid, birthday);
  }
}

void ProfileSyncService::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    sync_prefs_.SetMemoryPressureWarningCount(
        sync_prefs_.GetMemoryPressureWarningCount() + 1);
  }
}

void ProfileSyncService::ReportPreviousSessionMemoryWarningCount() {
  int warning_received = sync_prefs_.GetMemoryPressureWarningCount();

  if (-1 != warning_received) {
    // -1 means it is new client.
    if (!sync_prefs_.DidSyncShutdownCleanly()) {
      UMA_HISTOGRAM_COUNTS("Sync.MemoryPressureWarningBeforeUncleanShutdown",
                           warning_received);
    } else {
      UMA_HISTOGRAM_COUNTS("Sync.MemoryPressureWarningBeforeCleanShutdown",
                           warning_received);
    }
  }
  sync_prefs_.SetMemoryPressureWarningCount(0);
  // Will set to true during a clean shutdown, so crash or something else will
  // remain this as false.
  sync_prefs_.SetCleanShutdown(false);
}

const GURL& ProfileSyncService::sync_service_url() const {
  return sync_service_url_;
}

std::string ProfileSyncService::unrecoverable_error_message() const {
  return unrecoverable_error_message_;
}

tracked_objects::Location ProfileSyncService::unrecoverable_error_location()
    const {
  return unrecoverable_error_location_;
}

void ProfileSyncService::OnSetupInProgressHandleDestroyed() {
  DCHECK_GT(outstanding_setup_in_progress_handles_, 0);

  // Don't re-start Sync until all outstanding handles are destroyed.
  if (--outstanding_setup_in_progress_handles_ != 0)
    return;

  DCHECK(startup_controller_->IsSetupInProgress());
  startup_controller_->SetSetupInProgress(false);

  if (IsBackendInitialized())
    ReconfigureDatatypeManager();
  NotifyObservers();
}
