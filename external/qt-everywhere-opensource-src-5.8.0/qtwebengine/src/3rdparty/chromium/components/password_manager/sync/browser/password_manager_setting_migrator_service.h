// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_SYNC_BROWSER_PASSWORD_MANAGER_SETTING_MIGRATOR_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_SYNC_BROWSER_PASSWORD_MANAGER_SETTING_MIGRATOR_SERVICE_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/syncable_prefs/pref_service_syncable_observer.h"

namespace sync_driver {
class SyncService;
}

namespace syncable_prefs {
class PrefServiceSyncable;
}

namespace password_manager {

// Service that is responsible for reconciling the legacy "Offer to save your
// web passwords" setting (henceforth denoted 'L', for legacy) with the new
// "Enable Smart Lock for Passwords" setting (henceforth denoted 'N', for new).
//
// It works as follows.
//
// Users who are not syncing (this is checked on start-up of the service
// calling to SyncService:CanSyncStart()) are migrated on startup of the
// service. These users can be in following states:
//   1. N = 0, L = 0
//   2. N = 1, L = 1
//   3. N = 1, L = 0
//   4. N = 0, L = 1
//
// Nothing should be done in case 1 and 2 since settings are already in sync;
// in case 3,4 we change value of N to 0.
//
// For users who are syncing we save the values for the new and legacy
// preference on service startup (|inital_values_|) and the values that come
// from sync during models association step (|sync_data_|). Propagating change
// of one preference to another preference without having this special treatment
// will not always work. Here is an example which illustrates possible corner
// case: the client executes the migration code for the first time, legacy
// preference has a value 1, new preference was registered for the
// first time and has default value which is also 1, the sync data snapshot
// which user has on a client contains new preference equal to 0 and old
// preference equal to 1, if we blindly propagate these values we first get
// both preferences equal to 0 after priority pref model was associated and
// then both preferences equal to 1 after preferences model was associated.
// But the correct final value in this case is 0.
//
// At the end of model association step we derive the new values for both
// settings using the following table (first column contains local values for
// the preferences, first row contains values which came from sync, where _
// means that the value did not come):
//
//  NL*  0_  1_  _0  _1  00  01  10  11
//
//  00   00  11  00  11  00  11  11  11
//  01   00* 11* x   x   00  00  00  11
//  10   00* 00* x   x   00  00  00  11
//  11   00  11  00  11  00  00  00  11
//
//  *these cases only possible on mobile platforms, where we sync only priority
//  preference data type.
//
// The service observes changes to both preferences (e.g. changes from sync,
// changes from UI) and propagates the change to the other preference if needed.
//
// Note: componetization of this service currently is impossible, because it
// depends on PrefServiceSyncable https://crbug.com/522536.
class PasswordManagerSettingMigratorService
    : public KeyedService,
      public syncable_prefs::PrefServiceSyncableObserver {
 public:
  explicit PasswordManagerSettingMigratorService(
      syncable_prefs::PrefServiceSyncable* prefs);
  ~PasswordManagerSettingMigratorService() override;

  void Shutdown() override;

  // PrefServiceSyncableObserver:
  void OnIsSyncingChanged() override;

  void InitializeMigration(sync_driver::SyncService* sync_service);

  // Only use for testing.
  static void set_force_disabled_for_testing(bool force_disabled) {
    force_disabled_for_testing_ = force_disabled;
  }

 private:
  // Initializes the observers: preferences observers and sync status observers.
  void InitObservers();

  // Returns true if sync is expected to start for the user.
  bool CanSyncStart();

  // Called when the value of the |kCredentialsEnableService| preference
  // changes, and updates the value of |kPasswordManagerSavingEnabled|
  // preference accordingly.
  void OnCredentialsEnableServicePrefChanged(
      const std::string& changed_pref_name);

  // Called when the value of the |kPasswordManagerSavingEnabled| preference
  // changes, and updates the value of |kCredentialsEnableService| preference
  // accordingly.
  void OnPasswordManagerSavingEnabledPrefChanged(
      const std::string& changed_pref_name);

  // Determines if model association step was performed. For desktop platforms,
  // the condition is that for both priority preferences and regular preferences
  // types association step was finished. For mobile platforms, the association
  // only for priority prefs is required.
  bool WasModelAssociationStepPerformed();

  // Turns off one pref if another pref is off.
  void MigrateOffState(PrefService* prefs);

  // Performs a migration after sync associates models. Migration is performed
  // based on initial values for both preferences and values received from
  // sync.
  void MigrateAfterModelAssociation(PrefService* prefs);

  // Contains values which have come from sync during model association step.
  // The vector minimum size is 0 and the vector maximum size is 4:
  //   * sync_data_ has length equal to 0, when no change to the preferences has
  //   came from sync.
  //   * sync_data has length equal to 1, when change to only one preference
  //   came from sync and another pref already had this value, e.g local state
  //   01 and N=1 came from sync.
  //   * sync_data has length equals to 4, changes to both preference has came
  //   from sync, e.g. local state is 11 and 01 came from sync.
  // This way index 0 corresponds to kCredentialsEnableService, last index
  // corresponds to kPasswordManagerSavingEnabled if size of sync_data_ equals
  // to 4, otherwise the vector contains the value only for one preference.
  std::vector<bool> sync_data_;

  // The initial value for kCredentialsEnableService.
  bool initial_new_pref_value_;
  // The initial value for kPasswordManagerSavingEnabled.
  bool initial_legacy_pref_value_;

  syncable_prefs::PrefServiceSyncable* prefs_;
  sync_driver::SyncService* sync_service_;

  PrefChangeRegistrar pref_change_registrar_;

  // If true, the service will refuse to initialize despite Field Trial
  // settings.
  // Default value is false. Only use for testing.
  static bool force_disabled_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerSettingMigratorService);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_SYNC_BROWSER_PASSWORD_MANAGER_SETTING_MIGRATOR_SERVICE_H_
