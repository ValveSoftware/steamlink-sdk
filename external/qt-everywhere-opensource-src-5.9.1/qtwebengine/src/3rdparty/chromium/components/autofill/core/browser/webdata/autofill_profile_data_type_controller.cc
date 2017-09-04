// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_profile_data_type_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/syncable_service.h"

using autofill::AutofillWebDataService;

namespace browser_sync {

AutofillProfileDataTypeController::AutofillProfileDataTypeController(
    const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
    const base::Closure& dump_stack,
    syncer::SyncClient* sync_client,
    const scoped_refptr<autofill::AutofillWebDataService>& web_data_service)
    : NonUIDataTypeController(syncer::AUTOFILL_PROFILE,
                              dump_stack,
                              sync_client),
      db_thread_(db_thread),
      sync_client_(sync_client),
      web_data_service_(web_data_service),
      callback_registered_(false) {}

syncer::ModelSafeGroup AutofillProfileDataTypeController::model_safe_group()
    const {
  return syncer::GROUP_DB;
}

void AutofillProfileDataTypeController::WebDatabaseLoaded() {
  DCHECK(CalledOnValidThread());
  OnModelLoaded();
}

void AutofillProfileDataTypeController::OnPersonalDataChanged() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state(), MODEL_STARTING);

  sync_client_->GetPersonalDataManager()->RemoveObserver(this);

  if (!web_data_service_)
    return;

  if (web_data_service_->IsDatabaseLoaded()) {
    OnModelLoaded();
  } else if (!callback_registered_) {
    web_data_service_->RegisterDBLoadedCallback(
        base::Bind(&AutofillProfileDataTypeController::WebDatabaseLoaded,
                   base::AsWeakPtr(this)));
    callback_registered_ = true;
  }
}

AutofillProfileDataTypeController::~AutofillProfileDataTypeController() {}

bool AutofillProfileDataTypeController::PostTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK(CalledOnValidThread());
  return db_thread_->PostTask(from_here, task);
}

bool AutofillProfileDataTypeController::StartModels() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state(), MODEL_STARTING);
  // Waiting for the personal data is subtle:  we do this as the PDM resets
  // its cache of unique IDs once it gets loaded. If we were to proceed with
  // association, the local ids in the mappings would wind up colliding.
  autofill::PersonalDataManager* personal_data =
      sync_client_->GetPersonalDataManager();
  if (!personal_data->IsDataLoaded()) {
    personal_data->AddObserver(this);
    return false;
  }

  if (!web_data_service_)
    return false;

  if (web_data_service_->IsDatabaseLoaded())
    return true;

  if (!callback_registered_) {
    web_data_service_->RegisterDBLoadedCallback(
        base::Bind(&AutofillProfileDataTypeController::WebDatabaseLoaded,
                   base::AsWeakPtr(this)));
    callback_registered_ = true;
  }

  return false;
}

void AutofillProfileDataTypeController::StopModels() {
  DCHECK(CalledOnValidThread());
  sync_client_->GetPersonalDataManager()->RemoveObserver(this);
}

}  // namespace browser_sync
