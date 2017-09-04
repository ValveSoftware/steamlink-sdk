// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_

#include "build/build_config.h"

namespace password_manager {
namespace prefs {

// Alphabetical list of preference names specific to the PasswordManager
// component.

// Boolean controlling whether the password manager allows automatic signing in
// through Credential Manager API.
extern const char kCredentialsEnableAutosignin[];

// The value of this preference controls whether the Password Manager will save
// credentials. When it is false, it doesn't ask if you want to save passwords
// but will continue to fill passwords. This preference in a future will
// substitute kPasswordManagerSavingEnabled, currently it's required that values
// of these two preference are in sync with each other.
// TODO(melandory): Preference should also control autofill behavior for the
// passwords.
extern const char kCredentialsEnableService[];

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS) && defined(OS_POSIX)
// The local profile id for this profile.
extern const char kLocalProfileId[];
#endif

#if defined(OS_WIN)
// Whether the password was blank, only valid if OS password was last changed
// on or before the value contained in kOsPasswordLastChanged.
extern const char kOsPasswordBlank[];

// The number of seconds since epoch that the OS password was last changed.
extern const char kOsPasswordLastChanged[];
#endif

#if defined(OS_MACOSX)
// The current status of migrating the passwords from the Keychain to the
// database. Stores a value from MigrationStatus.
extern const char kKeychainMigrationStatus[];
#endif

// Boolean that is true if password saving is on (will record new
// passwords and fill in known passwords). When it is false, it doesn't
// ask if you want to save passwords but will continue to fill passwords.
// Constant name and its value differ because of historical reasons as it
// was not deemed important enough to add migration code just for name
// change.
// See http://crbug.com/392387
extern const char kPasswordManagerSavingEnabled[];

// Boolean that indicated whether first run experience for the auto sign-in
// prompt was shown or not.
extern const char kWasAutoSignInFirstRunExperienceShown[];

// Boolean that indicated whether first run experience for the save prompt was
// shown or not.
extern const char kWasSavePrompFirstRunExperienceShown[];

// Boolean that indicated if user interacted with the Chrome Sign in promo.
extern const char kWasSignInPasswordPromoClicked[];

// Number of times the Chrome Sign in promo popped up.
extern const char kNumberSignInPasswordPromoShown[];

}  // namespace prefs
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_
