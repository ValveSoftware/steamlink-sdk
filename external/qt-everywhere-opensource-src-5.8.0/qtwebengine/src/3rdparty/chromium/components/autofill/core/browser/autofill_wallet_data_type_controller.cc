// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_wallet_data_type_controller.h"

#include "base/bind.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync_driver/sync_client.h"
#include "components/sync_driver/sync_service.h"
#include "sync/api/sync_error.h"
#include "sync/api/syncable_service.h"

namespace browser_sync {

AutofillWalletDataTypeController::AutofillWalletDataTypeController(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
    const base::Closure& error_callback,
    sync_driver::SyncClient* sync_client,
    syncer::ModelType model_type,
    const scoped_refptr<autofill::AutofillWebDataService>& web_data_service)
    : NonUIDataTypeController(ui_thread, error_callback, sync_client),
      ui_thread_(ui_thread),
      db_thread_(db_thread),
      sync_client_(sync_client),
      callback_registered_(false),
      model_type_(model_type),
      web_data_service_(web_data_service),
      currently_enabled_(IsEnabled()) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  DCHECK(model_type_ == syncer::AUTOFILL_WALLET_DATA ||
         model_type_ == syncer::AUTOFILL_WALLET_METADATA);
  pref_registrar_.Init(sync_client_->GetPrefService());
  pref_registrar_.Add(
      autofill::prefs::kAutofillWalletImportEnabled,
      base::Bind(&AutofillWalletDataTypeController::OnUserPrefChanged,
                 base::Unretained(this)));
}

AutofillWalletDataTypeController::~AutofillWalletDataTypeController() {}

syncer::ModelType AutofillWalletDataTypeController::type() const {
  return model_type_;
}

syncer::ModelSafeGroup AutofillWalletDataTypeController::model_safe_group()
    const {
  return syncer::GROUP_DB;
}

bool AutofillWalletDataTypeController::PostTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  return db_thread_->PostTask(from_here, task);
}

bool AutofillWalletDataTypeController::StartModels() {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  DCHECK_EQ(state(), MODEL_STARTING);

  if (!web_data_service_)
    return false;

  if (web_data_service_->IsDatabaseLoaded())
    return true;

  if (!callback_registered_) {
    web_data_service_->RegisterDBLoadedCallback(
        base::Bind(&AutofillWalletDataTypeController::OnModelLoaded, this));
    callback_registered_ = true;
  }

  return false;
}

void AutofillWalletDataTypeController::StopModels() {
  DCHECK(ui_thread_->BelongsToCurrentThread());

  // This function is called when shutting down (nothing is changing), when
  // sync is disabled completely, or when wallet sync is disabled. In the
  // cases where wallet sync or sync in general is disabled, clear wallet cards
  // and addresses copied from the server. This is different than other sync
  // cases since this type of data reflects what's on the server rather than
  // syncing local data between clients, so this extra step is required.
  sync_driver::SyncService* service = sync_client_->GetSyncService();

  // CanSyncStart indicates if sync is currently enabled at all. The preferred
  // data type indicates if wallet sync data/metadata is enabled, and
  // currently_enabled_ indicates if the other prefs are enabled. All of these
  // have to be enabled to sync wallet data/metadata.
  if (!service->CanSyncStart() ||
      !service->GetPreferredDataTypes().Has(type()) || !currently_enabled_) {
    autofill::PersonalDataManager* pdm = sync_client_->GetPersonalDataManager();
    if (pdm)
      pdm->ClearAllServerData();
  }
}

bool AutofillWalletDataTypeController::ReadyForStart() const {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  return currently_enabled_;
}

void AutofillWalletDataTypeController::OnUserPrefChanged() {
  DCHECK(ui_thread_->BelongsToCurrentThread());

  bool new_enabled = IsEnabled();
  if (currently_enabled_ == new_enabled)
    return;  // No change to sync state.
  currently_enabled_ = new_enabled;

  if (currently_enabled_) {
    // The preference was just enabled. Trigger a reconfiguration. This will do
    // nothing if the type isn't preferred.
    sync_driver::SyncService* sync_service = sync_client_->GetSyncService();
    sync_service->ReenableDatatype(type());
  } else {
    // Post a task to the backend thread to stop the datatype.
    if (state() != NOT_RUNNING && state() != STOPPING) {
      PostTaskOnBackendThread(
          FROM_HERE,
          base::Bind(&DataTypeController::OnSingleDataTypeUnrecoverableError,
                     this,
                     syncer::SyncError(
                         FROM_HERE, syncer::SyncError::DATATYPE_POLICY_ERROR,
                         "Wallet syncing is disabled by policy.", type())));
    }
  }
}

bool AutofillWalletDataTypeController::IsEnabled() {
  DCHECK(ui_thread_->BelongsToCurrentThread());

  // Require the user-visible pref to be enabled to sync Wallet data/metadata.
  PrefService* ps = sync_client_->GetPrefService();
  return ps->GetBoolean(autofill::prefs::kAutofillWalletImportEnabled);
}

}  // namespace browser_sync
