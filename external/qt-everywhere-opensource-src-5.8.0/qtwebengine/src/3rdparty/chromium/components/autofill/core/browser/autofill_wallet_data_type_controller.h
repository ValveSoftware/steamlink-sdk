// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_GLUE_AUTOFILL_WALLET_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_DRIVER_GLUE_AUTOFILL_WALLET_DATA_TYPE_CONTROLLER_H_

#include "base/macros.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync_driver/non_ui_data_type_controller.h"

namespace autofill {
class AutofillWebDataService;
}

namespace browser_sync {

// Controls syncing of either AUTOFILL_WALLET or AUTOFILL_WALLET_METADATA.
class AutofillWalletDataTypeController
    : public sync_driver::NonUIDataTypeController {
 public:
  // |model_type| should be either AUTOFILL_WALLET or AUTOFILL_WALLET_METADATA.
  AutofillWalletDataTypeController(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
      const base::Closure& error_callback,
      sync_driver::SyncClient* sync_client,
      syncer::ModelType model_type,
      const scoped_refptr<autofill::AutofillWebDataService>& web_data_service);

  // NonUIDataTypeController implementation.
  syncer::ModelType type() const override;
  syncer::ModelSafeGroup model_safe_group() const override;

 protected:
  ~AutofillWalletDataTypeController() override;

 private:
  // NonUIDataTypeController implementation.
  bool PostTaskOnBackendThread(const tracked_objects::Location& from_here,
                               const base::Closure& task) override;
  bool StartModels() override;
  void StopModels() override;
  bool ReadyForStart() const override;

  // Callback for changes to the autofill pref.
  void OnUserPrefChanged();

  // Returns true if the prefs are set such that wallet sync should be enabled.
  bool IsEnabled();

  // A reference to the UI thread's task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> ui_thread_;

  // A reference to the DB thread's task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> db_thread_;

  // A pointer to the sync client.
  sync_driver::SyncClient* const sync_client_;

  // Whether the database loaded callback has been registered.
  bool callback_registered_;

  // The model type for this DTC; can be AUTOFILL_WALLET_DATA or _METADATA.
  syncer::ModelType model_type_;

  // A reference to the AutofillWebDataService for this controller.
  scoped_refptr<autofill::AutofillWebDataService> web_data_service_;

  // Stores whether we're currently syncing wallet data. This is the last
  // value computed by IsEnabled.
  bool currently_enabled_;

  // Registrar for listening to kAutofillWalletSyncExperimentEnabled status.
  PrefChangeRegistrar pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(AutofillWalletDataTypeController);
};

}  // namespace browser_sync

#endif  // COMPONENTS_SYNC_DRIVER_GLUE_AUTOFILL_WALLET_DATA_TYPE_CONTROLLER_H_
