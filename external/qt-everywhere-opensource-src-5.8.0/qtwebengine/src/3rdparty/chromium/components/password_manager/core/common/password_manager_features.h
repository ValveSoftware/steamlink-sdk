// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_

// This file defines all the base::FeatureList features for the Password Manager
// module.

#include "base/feature_list.h"

namespace password_manager {

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

extern const base::Feature kAffiliationBasedMatching;
extern const base::Feature kDropSyncCredential;
extern const base::Feature kEnableAutomaticPasswordSaving;
extern const base::Feature kEnableManualPasswordGeneration;
extern const base::Feature kEnablePasswordChangeSupport;
extern const base::Feature kEnablePasswordForceSaving;
extern const base::Feature kProtectSyncCredential;
extern const base::Feature kProtectSyncCredentialOnReauth;
extern const base::Feature kPasswordImportExport;
extern const base::Feature kViewPasswords;
extern const base::Feature kFillOnAccountSelect;

}  // namespace features

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_
