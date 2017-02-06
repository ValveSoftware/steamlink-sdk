// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_data_type_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "components/autofill/core/browser/webdata/autocomplete_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "sync/api/sync_error.h"
#include "sync/internal_api/public/util/experiments.h"

namespace browser_sync {

AutofillDataTypeController::AutofillDataTypeController(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
    const base::Closure& error_callback,
    sync_driver::SyncClient* sync_client,
    const scoped_refptr<autofill::AutofillWebDataService>& web_data_service)
    : NonUIDataTypeController(ui_thread, error_callback, sync_client),
      db_thread_(db_thread),
      web_data_service_(web_data_service) {}

syncer::ModelType AutofillDataTypeController::type() const {
  return syncer::AUTOFILL;
}

syncer::ModelSafeGroup AutofillDataTypeController::model_safe_group() const {
  return syncer::GROUP_DB;
}

void AutofillDataTypeController::WebDatabaseLoaded() {
  DCHECK(ui_thread()->BelongsToCurrentThread());
  DCHECK_EQ(MODEL_STARTING, state());

  OnModelLoaded();
}

AutofillDataTypeController::~AutofillDataTypeController() {
  DCHECK(ui_thread()->BelongsToCurrentThread());
}

bool AutofillDataTypeController::PostTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK(ui_thread()->BelongsToCurrentThread());
  return db_thread_->PostTask(from_here, task);
}

bool AutofillDataTypeController::StartModels() {
  DCHECK(ui_thread()->BelongsToCurrentThread());
  DCHECK_EQ(MODEL_STARTING, state());

  if (!web_data_service_)
    return false;

  if (web_data_service_->IsDatabaseLoaded()) {
    return true;
  } else {
    web_data_service_->RegisterDBLoadedCallback(
        base::Bind(&AutofillDataTypeController::WebDatabaseLoaded, this));
    return false;
  }
}

}  // namespace browser_sync
