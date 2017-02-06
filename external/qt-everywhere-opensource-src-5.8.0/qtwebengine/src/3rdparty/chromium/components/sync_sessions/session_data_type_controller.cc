// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_data_type_controller.h"

#include "components/prefs/pref_service.h"
#include "components/sync_driver/sync_client.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_window_delegate.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"

namespace browser_sync {

SessionDataTypeController::SessionDataTypeController(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    const base::Closure& error_callback,
    sync_driver::SyncClient* sync_client,
    sync_driver::LocalDeviceInfoProvider* local_device,
    const char* history_disabled_pref_name)
    : UIDataTypeController(ui_thread,
                           error_callback,
                           syncer::SESSIONS,
                           sync_client),
      sync_client_(sync_client),
      local_device_(local_device),
      history_disabled_pref_name_(history_disabled_pref_name),
      waiting_on_session_restore_(false),
      waiting_on_local_device_info_(false) {
  DCHECK(local_device_);
  pref_registrar_.Init(sync_client_->GetPrefService());
  pref_registrar_.Add(
      history_disabled_pref_name_,
      base::Bind(&SessionDataTypeController::OnSavingBrowserHistoryPrefChanged,
                 base::Unretained(this)));
}

SessionDataTypeController::~SessionDataTypeController() {}

bool SessionDataTypeController::StartModels() {
  DCHECK(ui_thread()->BelongsToCurrentThread());
  browser_sync::SyncedWindowDelegatesGetter* synced_window_getter =
      sync_client_->GetSyncSessionsClient()->GetSyncedWindowDelegatesGetter();
  std::set<const browser_sync::SyncedWindowDelegate*> window =
      synced_window_getter->GetSyncedWindowDelegates();
  for (std::set<const browser_sync::SyncedWindowDelegate*>::const_iterator i =
           window.begin();
       i != window.end(); ++i) {
    if ((*i)->IsSessionRestoreInProgress()) {
      waiting_on_session_restore_ = true;
      break;
    }
  }

  if (!local_device_->GetLocalDeviceInfo()) {
    subscription_ = local_device_->RegisterOnInitializedCallback(base::Bind(
        &SessionDataTypeController::OnLocalDeviceInfoInitialized, this));
    waiting_on_local_device_info_ = true;
  }

  return !IsWaiting();
}

void SessionDataTypeController::StopModels() {
  subscription_.reset();
}

bool SessionDataTypeController::ReadyForStart() const {
  DCHECK(ui_thread()->BelongsToCurrentThread());
  return !sync_client_->GetPrefService()->GetBoolean(
      history_disabled_pref_name_);
}

void SessionDataTypeController::OnSessionRestoreComplete() {
  DCHECK(ui_thread()->BelongsToCurrentThread());
  waiting_on_session_restore_ = false;
  MaybeCompleteLoading();
}

bool SessionDataTypeController::IsWaiting() {
  return waiting_on_session_restore_ || waiting_on_local_device_info_;
}

void SessionDataTypeController::MaybeCompleteLoading() {
  if (state_ == MODEL_STARTING && !IsWaiting()) {
    OnModelLoaded();
  }
}

void SessionDataTypeController::OnLocalDeviceInfoInitialized() {
  DCHECK(ui_thread()->BelongsToCurrentThread());
  subscription_.reset();

  waiting_on_local_device_info_ = false;
  MaybeCompleteLoading();
}

void SessionDataTypeController::OnSavingBrowserHistoryPrefChanged() {
  DCHECK(ui_thread()->BelongsToCurrentThread());
  if (sync_client_->GetPrefService()->GetBoolean(history_disabled_pref_name_)) {
    // If history and tabs persistence is turned off then generate an
    // unrecoverable error. SESSIONS won't be a registered type on the next
    // Chrome restart.
    if (state() != NOT_RUNNING && state() != STOPPING) {
      syncer::SyncError error(
          FROM_HERE, syncer::SyncError::DATATYPE_POLICY_ERROR,
          "History and tab saving is now disabled by policy.",
          syncer::SESSIONS);
      OnSingleDataTypeUnrecoverableError(error);
    }
  }
}

}  // namespace browser_sync
