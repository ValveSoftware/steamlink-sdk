// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_DATA_TYPE_CONTROLLER_H__
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_DATA_TYPE_CONTROLLER_H__

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/sync/driver/non_ui_data_type_controller.h"

namespace autofill {
class AutofillWebDataService;
}  // namespace autofill

namespace browser_sync {

// A class that manages the startup and shutdown of autofill sync.
class AutofillDataTypeController : public syncer::NonUIDataTypeController {
 public:
  // |dump_stack| is called when an unrecoverable error occurs.
  AutofillDataTypeController(
      const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
      const base::Closure& dump_stack,
      syncer::SyncClient* sync_client,
      const scoped_refptr<autofill::AutofillWebDataService>& web_data_service);
  ~AutofillDataTypeController() override;

  // NonUIDataTypeController implementation.
  syncer::ModelSafeGroup model_safe_group() const override;

 protected:
  // NonUIDataTypeController implementation.
  bool PostTaskOnBackendThread(const tracked_objects::Location& from_here,
                               const base::Closure& task) override;
  bool StartModels() override;

 private:
  friend class AutofillDataTypeControllerTest;
  FRIEND_TEST_ALL_PREFIXES(AutofillDataTypeControllerTest, StartWDSReady);
  FRIEND_TEST_ALL_PREFIXES(AutofillDataTypeControllerTest, StartWDSNotReady);

  // Callback once WebDatabase has loaded.
  void WebDatabaseLoaded();

  // A reference to the DB thread's task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> db_thread_;

  // A reference to the AutofillWebDataService for this controller.
  scoped_refptr<autofill::AutofillWebDataService> web_data_service_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDataTypeController);
};

}  // namespace browser_sync

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_DATA_TYPE_CONTROLLER_H__
