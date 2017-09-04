// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_settings_migration_experiment.h"

#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"

namespace password_manager {

bool IsSettingsMigrationActive() {
  const char kFieldTrialName[] = "PasswordManagerSettingsMigration";
  const char kEnabledGroupNamePrefix[] = "Enable";
  return base::StartsWith(base::FieldTrialList::FindFullName(kFieldTrialName),
                          kEnabledGroupNamePrefix,
                          base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace password_manager
