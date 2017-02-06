// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_DATA_TYPE_CONTROLLER_H__
#define COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_DATA_TYPE_CONTROLLER_H__

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync_driver/non_ui_data_type_controller.h"
#include "components/sync_driver/sync_api_component_factory.h"

namespace history {
class HistoryBackend;
}

namespace browser_sync {

class ControlTask;

// A class that manages the startup and shutdown of typed_url sync.
class TypedUrlDataTypeController : public sync_driver::NonUIDataTypeController {
 public:
  explicit TypedUrlDataTypeController(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      const base::Closure& error_callback,
      sync_driver::SyncClient* sync_client,
      const char* history_disabled_pref_name);

  // NonUIDataTypeController implementation
  syncer::ModelType type() const override;
  syncer::ModelSafeGroup model_safe_group() const override;
  bool ReadyForStart() const override;

  // Invoked on the history thread to set our history backend - must be called
  // before CreateSyncComponents() is invoked.
  void SetBackend(history::HistoryBackend* backend);

 protected:
  // NonUIDataTypeController interface.
  bool PostTaskOnBackendThread(const tracked_objects::Location& from_here,
                               const base::Closure& task) override;

 private:
  ~TypedUrlDataTypeController() override;

  void OnSavingBrowserHistoryDisabledChanged();

  // Name of the pref that indicates whether saving history is disabled.
  const char* history_disabled_pref_name_;

  history::HistoryBackend* backend_;
  PrefChangeRegistrar pref_registrar_;

  // Helper object to make sure we don't leave tasks running on the history
  // thread.
  base::CancelableTaskTracker task_tracker_;

  sync_driver::SyncClient* const sync_client_;

  DISALLOW_COPY_AND_ASSIGN(TypedUrlDataTypeController);
};

}  // namespace browser_sync

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_DATA_TYPE_CONTROLLER_H__
