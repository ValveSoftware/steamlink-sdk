// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_BUBBLE_EXPERIMENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_BUBBLE_EXPERIMENT_H_

class PrefRegistrySimple;
class PrefService;

namespace sync_driver {
class SyncService;
}

namespace password_bubble_experiment {

extern const char kBrandingExperimentName[];
extern const char kChromeSignInPasswordPromoExperimentName[];
extern const char kChromeSignInPasswordPromoThresholdParam[];
extern const char kSmartBubbleExperimentName[];
extern const char kSmartBubbleThresholdParam[];
extern const char kSmartLockBrandingGroupName[];
extern const char kSmartLockBrandingSavePromptOnlyGroupName[];

// Registers prefs which controls appearance of the first run experience for the
// Smart Lock UI, namely was first run experience shown for save prompt or auto
// sign-in prompt.
void RegisterPrefs(PrefRegistrySimple* registry);

// Returns the number of times the "Save password" bubble can be dismissed by
// user before it's not shown automatically.
int GetSmartBubbleDismissalThreshold();

// A Smart Lock user is a sync user without a custom passphrase.
bool IsSmartLockUser(const sync_driver::SyncService* sync_service);

enum class SmartLockBranding { NONE, FULL, SAVE_PROMPT_ONLY };

// If the user is not a Smart Lock user, returns NONE. For Smart Lock users:
// * returns NONE if the password manager should not be referred to as Smart
//   Lock anywhere;
// * returns FULL, if it should be referred to as Smart Lock everywhere;
// * returns SAVE_PROMPT_ONLY if it only should be referred to as Smart Lock in
//   the save password bubble.
SmartLockBranding GetSmartLockBrandingState(
    const sync_driver::SyncService* sync_service);

// Convenience function for checking whether the result of
// GetSmartLockBrandingState is SmartLockBranding::FULL.
bool IsSmartLockBrandingEnabled(const sync_driver::SyncService* sync_service);

// Convenience function for checking whether the result of
// GetSmartLockBrandingState is not equal to SmartLockBranding::NONE.
bool IsSmartLockBrandingSavePromptEnabled(
    const sync_driver::SyncService* sync_service);

// Returns true if save prompt should contain first run experience.
bool ShouldShowSavePromptFirstRunExperience(
    const sync_driver::SyncService* sync_service,
    PrefService* prefs);

// Sets appropriate value to the preference which controls appearance of the
// first run experience for the save prompt.
void RecordSavePromptFirstRunExperienceWasShown(PrefService* prefs);

// Returns true if first run experience for auto sign-in prompt should be shown.
bool ShouldShowAutoSignInPromptFirstRunExperience(PrefService* prefs);

// Sets appropriate value to the preference which controls appearance of the
// first run experience for the auto sign-in prompt.
void RecordAutoSignInPromptFirstRunExperienceWasShown(PrefService* prefs);

// Turns off the auto signin experience setting.
void TurnOffAutoSignin(PrefService* prefs);

// Returns true if the Chrome Sign In promo should be shown.
bool ShouldShowChromeSignInPasswordPromo(
    PrefService* prefs,
    const sync_driver::SyncService* sync_service);

}  // namespace password_bubble_experiment

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_BUBBLE_EXPERIMENT_H_
