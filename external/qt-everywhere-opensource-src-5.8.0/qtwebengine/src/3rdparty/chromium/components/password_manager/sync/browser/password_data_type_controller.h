// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_SYNC_BROWSER_PASSWORD_DATA_TYPE_CONTROLLER_H__
#define COMPONENTS_PASSWORD_MANAGER_SYNC_BROWSER_PASSWORD_DATA_TYPE_CONTROLLER_H__

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/sync_driver/non_ui_data_type_controller.h"
#include "components/sync_driver/sync_service_observer.h"

namespace password_manager {
class PasswordStore;
}

namespace sync_driver {
class SyncClient;
}

namespace browser_sync {

// A class that manages the startup and shutdown of password sync.
class PasswordDataTypeController : public sync_driver::NonUIDataTypeController,
                                   public sync_driver::SyncServiceObserver {
 public:
  PasswordDataTypeController(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      const base::Closure& error_callback,
      sync_driver::SyncClient* sync_client,
      const base::Closure& state_changed_callback,
      const scoped_refptr<password_manager::PasswordStore>& password_store);

  // NonFrontendDataTypeController implementation
  syncer::ModelType type() const override;
  syncer::ModelSafeGroup model_safe_group() const override;

 protected:
  ~PasswordDataTypeController() override;

  // NonUIDataTypeController interface.
  bool PostTaskOnBackendThread(const tracked_objects::Location& from_here,
                               const base::Closure& task) override;
  bool StartModels() override;
  void StopModels() override;

  // sync_driver::SyncServiceObserver:
  void OnStateChanged() override;

 private:
  sync_driver::SyncClient* const sync_client_;
  const base::Closure state_changed_callback_;
  scoped_refptr<password_manager::PasswordStore> password_store_;

  DISALLOW_COPY_AND_ASSIGN(PasswordDataTypeController);
};

}  // namespace browser_sync

#endif  // COMPONENTS_PASSWORD_MANAGER_SYNC_BROWSER_PASSWORD_DATA_TYPE_CONTROLLER_H__
