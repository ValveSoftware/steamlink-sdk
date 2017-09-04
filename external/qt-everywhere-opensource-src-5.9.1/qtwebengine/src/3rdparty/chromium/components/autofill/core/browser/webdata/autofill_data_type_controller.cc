// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_data_type_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "components/autofill/core/browser/webdata/autocomplete_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/base/experiments.h"
#include "components/sync/model/sync_error.h"

namespace browser_sync {

AutofillDataTypeController::AutofillDataTypeController(
    const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
    const base::Closure& dump_stack,
    syncer::SyncClient* sync_client,
    const scoped_refptr<autofill::AutofillWebDataService>& web_data_service)
    : NonUIDataTypeController(syncer::AUTOFILL, dump_stack, sync_client),
      db_thread_(db_thread),
      web_data_service_(web_data_service) {}

syncer::ModelSafeGroup AutofillDataTypeController::model_safe_group() const {
  return syncer::GROUP_DB;
}

void AutofillDataTypeController::WebDatabaseLoaded() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(MODEL_STARTING, state());

  OnModelLoaded();
}

AutofillDataTypeController::~AutofillDataTypeController() {
  DCHECK(CalledOnValidThread());
}

bool AutofillDataTypeController::PostTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK(CalledOnValidThread());
  return db_thread_->PostTask(from_here, task);
}

bool AutofillDataTypeController::StartModels() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(MODEL_STARTING, state());

  if (!web_data_service_)
    return false;

  if (web_data_service_->IsDatabaseLoaded()) {
    return true;
  } else {
    web_data_service_->RegisterDBLoadedCallback(base::Bind(
        &AutofillDataTypeController::WebDatabaseLoaded, base::AsWeakPtr(this)));
    return false;
  }
}

}  // namespace browser_sync
