// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/sync/browser/password_manager_setting_migrator_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_manager_settings_migration_experiment.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/sync_driver/sync_service.h"
#include "components/syncable_prefs/pref_service_syncable.h"

namespace {

bool GetBooleanUserOrDefaultPrefValue(PrefService* prefs,
                                      const std::string& name) {
  bool result = false;
  const base::Value* value = prefs->GetUserPrefValue(name);
  if (!value)
    value = prefs->GetDefaultPrefValue(name);
  value->GetAsBoolean(&result);
  return result;
}

void ChangeOnePrefBecauseAnotherPrefHasChanged(
    PrefService* prefs,
    const std::string& other_pref_name,
    const std::string& changed_pref_name) {
  bool changed_pref =
      GetBooleanUserOrDefaultPrefValue(prefs, changed_pref_name);
  bool other_pref = GetBooleanUserOrDefaultPrefValue(prefs, other_pref_name);
  if (changed_pref != other_pref)
    prefs->SetBoolean(other_pref_name, changed_pref);
}

// Change value of both kPasswordManagerSavingEnabled and
// kCredentialsEnableService to the |new_value|.
void UpdatePreferencesValues(PrefService* prefs, bool new_value) {
  prefs->SetBoolean(password_manager::prefs::kPasswordManagerSavingEnabled,
                    new_value);
  prefs->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                    new_value);
}

void SaveCurrentPrefState(PrefService* prefs,
                          bool* new_pref_value,
                          bool* legacy_pref_value) {
  *new_pref_value = GetBooleanUserOrDefaultPrefValue(
      prefs, password_manager::prefs::kCredentialsEnableService);
  *legacy_pref_value = GetBooleanUserOrDefaultPrefValue(
      prefs, password_manager::prefs::kPasswordManagerSavingEnabled);
}

void TrackInitialAndFinalValues(PrefService* prefs,
                                bool initial_new_pref_value,
                                bool initial_legacy_pref_value) {
  bool final_new_pref_value = GetBooleanUserOrDefaultPrefValue(
      prefs, password_manager::prefs::kCredentialsEnableService);
  bool final_legacy_pref_value = GetBooleanUserOrDefaultPrefValue(
      prefs, password_manager::prefs::kPasswordManagerSavingEnabled);
  const int kMaxInitValue = 0x10;
  int value_to_log = 0;
  const int kInitialNewValueMask = 0x8;
  const int kInitialLegacyValueMask = 0x4;
  const int kFinalNewValueMask = 0x2;
  const int kFinalLegacyValueMask = 0x1;
  if (initial_new_pref_value)
    value_to_log |= kInitialNewValueMask;
  if (initial_legacy_pref_value)
    value_to_log |= kInitialLegacyValueMask;
  if (final_new_pref_value)
    value_to_log |= kFinalNewValueMask;
  if (final_legacy_pref_value)
    value_to_log |= kFinalLegacyValueMask;
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.SettingsReconciliation.InitialAndFinalValues",
      value_to_log, kMaxInitValue);
}

}  // namespace

namespace password_manager {

bool PasswordManagerSettingMigratorService::force_disabled_for_testing_ = false;

PasswordManagerSettingMigratorService::PasswordManagerSettingMigratorService(
    syncable_prefs::PrefServiceSyncable* prefs)
    : prefs_(prefs), sync_service_(nullptr) {
  SaveCurrentPrefState(prefs_, &initial_new_pref_value_,
                       &initial_legacy_pref_value_);
}

PasswordManagerSettingMigratorService::
    ~PasswordManagerSettingMigratorService() {}

void PasswordManagerSettingMigratorService::InitializeMigration(
    sync_driver::SyncService* sync_service) {
  if (force_disabled_for_testing_)
    return;
  SaveCurrentPrefState(prefs_, &initial_new_pref_value_,
                       &initial_legacy_pref_value_);
  const int kMaxInitialValues = 4;
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.SettingsReconciliation.InitialValues",
      (static_cast<int>(initial_new_pref_value_) << 1 |
       static_cast<int>(initial_legacy_pref_value_)),
      kMaxInitialValues);
  if (!password_manager::IsSettingsMigrationActive()) {
    return;
  }
  sync_service_ = sync_service;
  if (!sync_service_ || !CanSyncStart()) {
    MigrateOffState(prefs_);
    TrackInitialAndFinalValues(prefs_, initial_new_pref_value_,
                               initial_legacy_pref_value_);
  }
  InitObservers();
}

void PasswordManagerSettingMigratorService::InitObservers() {
  pref_change_registrar_.Init(prefs_);
  pref_change_registrar_.Add(
      password_manager::prefs::kCredentialsEnableService,
      base::Bind(&PasswordManagerSettingMigratorService::
                     OnCredentialsEnableServicePrefChanged,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      password_manager::prefs::kPasswordManagerSavingEnabled,
      base::Bind(&PasswordManagerSettingMigratorService::
                     OnPasswordManagerSavingEnabledPrefChanged,
                 base::Unretained(this)));
  // This causes OnIsSyncingChanged to be called when the value of
  // PrefService::IsSyncing() changes.
  prefs_->AddObserver(this);
}

bool PasswordManagerSettingMigratorService::CanSyncStart() {
  return sync_service_ && sync_service_->CanSyncStart() &&
         syncer::UserSelectableTypes().Has(syncer::PREFERENCES);
}

void PasswordManagerSettingMigratorService::Shutdown() {
  prefs_->RemoveObserver(this);
}

void PasswordManagerSettingMigratorService::
    OnCredentialsEnableServicePrefChanged(
        const std::string& changed_pref_name) {
  sync_data_.push_back(GetBooleanUserOrDefaultPrefValue(
      prefs_, password_manager::prefs::kCredentialsEnableService));
  ChangeOnePrefBecauseAnotherPrefHasChanged(
      prefs_, password_manager::prefs::kPasswordManagerSavingEnabled,
      password_manager::prefs::kCredentialsEnableService);
}

void PasswordManagerSettingMigratorService::
    OnPasswordManagerSavingEnabledPrefChanged(
        const std::string& changed_pref_name) {
  sync_data_.push_back(GetBooleanUserOrDefaultPrefValue(
      prefs_, password_manager::prefs::kPasswordManagerSavingEnabled));
  ChangeOnePrefBecauseAnotherPrefHasChanged(
      prefs_, password_manager::prefs::kCredentialsEnableService,
      password_manager::prefs::kPasswordManagerSavingEnabled);
}

void PasswordManagerSettingMigratorService::OnIsSyncingChanged() {
  if (WasModelAssociationStepPerformed()) {
    // Initial sync has finished.
    MigrateAfterModelAssociation(prefs_);
  }

  if (prefs_->IsSyncing() == prefs_->IsPrioritySyncing()) {
    // Sync is not in model association step.
    SaveCurrentPrefState(prefs_, &initial_new_pref_value_,
                         &initial_legacy_pref_value_);
    sync_data_.clear();
  }
}

bool PasswordManagerSettingMigratorService::WasModelAssociationStepPerformed() {
#if defined(OS_ANDROID) || defined(OS_IOS)
  return prefs_->IsPrioritySyncing();
#else
  return prefs_->IsSyncing() && prefs_->IsPrioritySyncing();
#endif
}

void PasswordManagerSettingMigratorService::MigrateOffState(
    PrefService* prefs) {
  bool new_pref_value = GetBooleanUserOrDefaultPrefValue(
      prefs_, password_manager::prefs::kCredentialsEnableService);
  bool legacy_pref_value = GetBooleanUserOrDefaultPrefValue(
      prefs_, password_manager::prefs::kPasswordManagerSavingEnabled);
  UpdatePreferencesValues(prefs, new_pref_value && legacy_pref_value);
}

void PasswordManagerSettingMigratorService::MigrateAfterModelAssociation(
    PrefService* prefs) {
  if (sync_data_.empty()) {
    MigrateOffState(prefs);
  } else if (sync_data_.size() == 1) {
#if defined(OS_ANDROID) || defined(OS_IOS)
    if (initial_new_pref_value_ != initial_legacy_pref_value_) {
      // Treat special case for mobile clients where only priority pref
      // arrives on the client.
      if (!initial_new_pref_value_ && initial_legacy_pref_value_)
        UpdatePreferencesValues(prefs, true);
      else
        UpdatePreferencesValues(prefs, false);
    } else {
      UpdatePreferencesValues(prefs, sync_data_[0]);
    }
#else
    // Only one value came from sync. This value should be assigned to both
    // preferences.
    UpdatePreferencesValues(prefs, sync_data_[0]);
#endif
  } else {
    bool sync_new_pref_value = sync_data_[0];
    bool sync_legacy_pref_value = sync_data_.back();
    if (sync_legacy_pref_value && sync_new_pref_value) {
      UpdatePreferencesValues(prefs, true);
    } else if (!sync_legacy_pref_value && !sync_new_pref_value) {
      UpdatePreferencesValues(prefs, false);
    } else if (!initial_legacy_pref_value_ && !initial_new_pref_value_) {
      UpdatePreferencesValues(prefs, true);
    } else {
      UpdatePreferencesValues(prefs, false);
    }
  }
  TrackInitialAndFinalValues(prefs, initial_new_pref_value_,
                             initial_legacy_pref_value_);
}

}  // namespace password_manager
