// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSION_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_SESSIONS_SESSION_DATA_TYPE_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync_driver/local_device_info_provider.h"
#include "components/sync_driver/ui_data_type_controller.h"

namespace browser_sync {

// Overrides StartModels to avoid sync contention with sessions during
// a session restore operation at startup and to wait for the local
// device info to become available.
class SessionDataTypeController : public sync_driver::UIDataTypeController {
 public:
  SessionDataTypeController(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      const base::Closure& error_callback,
      sync_driver::SyncClient* sync_client,
      sync_driver::LocalDeviceInfoProvider* local_device,
      const char* history_disabled_pref_name);

  // UIDataTypeController interface.
  bool StartModels() override;
  void StopModels() override;
  bool ReadyForStart() const override;

  // Called when asynchronous session restore has completed.
  void OnSessionRestoreComplete();

 protected:
  ~SessionDataTypeController() override;

 private:
  bool IsWaiting();
  void MaybeCompleteLoading();
  void OnLocalDeviceInfoInitialized();
  void OnSavingBrowserHistoryPrefChanged();

  sync_driver::SyncClient* const sync_client_;

  sync_driver::LocalDeviceInfoProvider* const local_device_;
  std::unique_ptr<sync_driver::LocalDeviceInfoProvider::Subscription>
      subscription_;

  // Name of the pref that indicates whether saving history is disabled.
  const char* history_disabled_pref_name_;

  // Flags that indicate the reason for pending loading models.
  bool waiting_on_session_restore_;
  bool waiting_on_local_device_info_;

  PrefChangeRegistrar pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(SessionDataTypeController);
};

}  // namespace browser_sync

#endif  // COMPONENTS_SYNC_SESSIONS_SESSION_DATA_TYPE_CONTROLLER_H_
