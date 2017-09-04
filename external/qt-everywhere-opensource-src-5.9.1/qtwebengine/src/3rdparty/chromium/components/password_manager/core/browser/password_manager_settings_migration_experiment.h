// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_SETTINGS_MIGRATOR_EXPERIMENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_SETTINGS_MIGRATOR_EXPERIMENT_H_

namespace password_manager {

// Returns true if settings migration should be active for the user.
bool IsSettingsMigrationActive();

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_SETTINGS_MIGRATOR_EXPERIMENT_H_
