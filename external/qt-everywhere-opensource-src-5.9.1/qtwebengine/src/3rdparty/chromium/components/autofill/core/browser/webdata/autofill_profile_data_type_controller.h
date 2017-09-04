// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_DATA_TYPE_CONTROLLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/sync/driver/non_ui_data_type_controller.h"

namespace autofill {
class AutofillWebDataService;
class PersonalDataManager;
}  // namespace autofill

namespace browser_sync {

// Controls syncing of the AUTOFILL_PROFILE data type.
class AutofillProfileDataTypeController
    : public syncer::NonUIDataTypeController,
      public autofill::PersonalDataManagerObserver {
 public:
  // |dump_stack| is called when an unrecoverable error occurs.
  AutofillProfileDataTypeController(
      const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
      const base::Closure& dump_stack,
      syncer::SyncClient* sync_client,
      const scoped_refptr<autofill::AutofillWebDataService>& web_data_service);
  ~AutofillProfileDataTypeController() override;

  // NonUIDataTypeController:
  syncer::ModelSafeGroup model_safe_group() const override;

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override;

 protected:
  // NonUIDataTypeController:
  bool PostTaskOnBackendThread(const tracked_objects::Location& from_here,
                               const base::Closure& task) override;
  bool StartModels() override;
  void StopModels() override;

 private:
  // Callback to notify that WebDatabase has loaded.
  void WebDatabaseLoaded();

  // A reference to the DB thread's task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> db_thread_;

  // A pointer to the sync client.
  syncer::SyncClient* const sync_client_;

  // A reference to the AutofillWebDataService for this controller.
  scoped_refptr<autofill::AutofillWebDataService> web_data_service_;

  // Whether the database loaded callback has been registered.
  bool callback_registered_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileDataTypeController);
};

}  // namespace browser_sync

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_DATA_TYPE_CONTROLLER_H_
