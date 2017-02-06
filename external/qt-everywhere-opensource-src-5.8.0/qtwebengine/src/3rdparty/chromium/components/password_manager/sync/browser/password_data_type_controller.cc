// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/sync/browser/password_data_type_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/sync_driver/sync_client.h"
#include "components/sync_driver/sync_service.h"

namespace browser_sync {

PasswordDataTypeController::PasswordDataTypeController(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    const base::Closure& error_callback,
    sync_driver::SyncClient* sync_client,
    const base::Closure& state_changed_callback,
    const scoped_refptr<password_manager::PasswordStore>& password_store)
    : NonUIDataTypeController(ui_thread, error_callback, sync_client),
      sync_client_(sync_client),
      state_changed_callback_(state_changed_callback),
      password_store_(password_store) {}

syncer::ModelType PasswordDataTypeController::type() const {
  return syncer::PASSWORDS;
}

syncer::ModelSafeGroup PasswordDataTypeController::model_safe_group() const {
  return syncer::GROUP_PASSWORD;
}

PasswordDataTypeController::~PasswordDataTypeController() {}

bool PasswordDataTypeController::PostTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK(ui_thread()->BelongsToCurrentThread());
  if (!password_store_.get())
    return false;
  return password_store_->ScheduleTask(task);
}

bool PasswordDataTypeController::StartModels() {
  DCHECK(ui_thread()->BelongsToCurrentThread());
  DCHECK_EQ(MODEL_STARTING, state());

  sync_client_->GetSyncService()->AddObserver(this);

  OnStateChanged();

  return !!password_store_.get();
}

void PasswordDataTypeController::StopModels() {
  DCHECK(ui_thread()->BelongsToCurrentThread());
  sync_client_->GetSyncService()->RemoveObserver(this);
}

void PasswordDataTypeController::OnStateChanged() {
  state_changed_callback_.Run();
}

}  // namespace browser_sync
