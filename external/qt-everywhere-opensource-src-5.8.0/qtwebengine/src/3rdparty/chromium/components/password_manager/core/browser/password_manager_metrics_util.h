// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_

#include <stddef.h>

#include <string>

namespace password_manager {

namespace metrics_util {

// Metrics: "PasswordManager.InfoBarResponse"
enum ResponseType {
  NO_RESPONSE = 0,
  REMEMBER_PASSWORD,
  NEVER_REMEMBER_PASSWORD,
  INFOBAR_DISMISSED,
  NUM_RESPONSE_TYPES,
};

// Metrics: "PasswordBubble.DisplayDisposition"
enum UIDisplayDisposition {
  AUTOMATIC_WITH_PASSWORD_PENDING = 0,
  MANUAL_WITH_PASSWORD_PENDING,
  MANUAL_MANAGE_PASSWORDS,
  MANUAL_BLACKLISTED_OBSOLETE,  // obsolete.
  AUTOMATIC_GENERATED_PASSWORD_CONFIRMATION,
  AUTOMATIC_CREDENTIAL_REQUEST_OBSOLETE,  // obsolete
  AUTOMATIC_SIGNIN_TOAST,
  MANUAL_WITH_PASSWORD_PENDING_UPDATE,
  AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE,
  NUM_DISPLAY_DISPOSITIONS
};

// Metrics: "PasswordManager.UIDismissalReason"
enum UIDismissalReason {
  // We use this to mean both "Bubble lost focus" and "No interaction with the
  // infobar", depending on which experiment is active.
  NO_DIRECT_INTERACTION = 0,
  CLICKED_SAVE,
  CLICKED_CANCEL,
  CLICKED_NEVER,
  CLICKED_MANAGE,
  CLICKED_DONE,
  CLICKED_UNBLACKLIST_OBSOLETE,  // obsolete.
  CLICKED_OK,
  CLICKED_CREDENTIAL_OBSOLETE,  // obsolete.
  AUTO_SIGNIN_TOAST_TIMEOUT,
  AUTO_SIGNIN_TOAST_CLICKED_OBSOLETE,  // obsolete.
  CLICKED_BRAND_NAME,
  NUM_UI_RESPONSES,
};

enum FormDeserializationStatus {
  LOGIN_DATABASE_SUCCESS,
  LOGIN_DATABASE_FAILURE,
  LIBSECRET_SUCCESS,
  LIBSECRET_FAILURE,
  GNOME_SUCCESS,
  GNOME_FAILURE,
  NUM_DESERIALIZATION_STATUSES
};

// Metrics: "PasswordManager.PasswordSyncState"
enum PasswordSyncState {
  SYNCING_OK,
  NOT_SYNCING_FAILED_READ,
  NOT_SYNCING_DUPLICATE_TAGS,
  NOT_SYNCING_SERVER_ERROR,
  NUM_SYNC_STATES
};

// Metrics: "PasswordGeneration.SubmissionEvent"
enum PasswordSubmissionEvent {
  PASSWORD_SUBMITTED,
  PASSWORD_SUBMISSION_FAILED,
  PASSWORD_NOT_SUBMITTED,
  PASSWORD_OVERRIDDEN,
  PASSWORD_USED,
  GENERATED_PASSWORD_FORCE_SAVED,
  SUBMISSION_EVENT_ENUM_COUNT
};

enum UpdatePasswordSubmissionEvent {
  NO_ACCOUNTS_CLICKED_UPDATE,
  NO_ACCOUNTS_CLICKED_NOPE,
  NO_ACCOUNTS_NO_INTERACTION,
  ONE_ACCOUNT_CLICKED_UPDATE,
  ONE_ACCOUNT_CLICKED_NOPE,
  ONE_ACCOUNT_NO_INTERACTION,
  MULTIPLE_ACCOUNTS_CLICKED_UPDATE,
  MULTIPLE_ACCOUNTS_CLICKED_NOPE,
  MULTIPLE_ACCOUNTS_NO_INTERACTION,
  PASSWORD_OVERRIDDEN_CLICKED_UPDATE,
  PASSWORD_OVERRIDDEN_CLICKED_NOPE,
  PASSWORD_OVERRIDDEN_NO_INTERACTION,
  UPDATE_PASSWORD_EVENT_COUNT,

  NO_UPDATE_SUBMISSION
};

enum MultiAccountUpdateBubbleUserAction {
  DEFAULT_ACCOUNT_MATCHED_BY_PASSWORD_USER_CHANGED,
  DEFAULT_ACCOUNT_MATCHED_BY_PASSWORD_USER_NOT_CHANGED,
  DEFAULT_ACCOUNT_MATCHED_BY_PASSWORD_USER_REJECTED_UPDATE,
  DEFAULT_ACCOUNT_PREFERRED_USER_CHANGED,
  DEFAULT_ACCOUNT_PREFERRED_USER_NOT_CHANGED,
  DEFAULT_ACCOUNT_PREFERRED_USER_REJECTED_UPDATE,
  DEFAULT_ACCOUNT_FIRST_USER_CHANGED,
  DEFAULT_ACCOUNT_FIRST_USER_NOT_CHANGED,
  DEFAULT_ACCOUNT_FIRST_USER_REJECTED_UPDATE,
  MULTI_ACCOUNT_UPDATE_BUBBLE_USER_ACTION_COUNT
};

enum AutoSigninPromoUserAction {
  AUTO_SIGNIN_NO_ACTION,
  AUTO_SIGNIN_TURN_OFF,
  AUTO_SIGNIN_OK_GOT_IT,
  AUTO_SIGNIN_PROMO_ACTION_COUNT
};

enum AccountChooserUserAction {
  ACCOUNT_CHOOSER_DISMISSED,
  ACCOUNT_CHOOSER_CREDENTIAL_CHOSEN,
  ACCOUNT_CHOOSER_SIGN_IN,
  ACCOUNT_CHOOSER_ACTION_COUNT
};

enum SyncSignInUserAction {
  CHROME_SIGNIN_DISMISSED,
  CHROME_SIGNIN_OK,
  CHROME_SIGNIN_CANCEL,
  CHROME_SIGNIN_ACTION_COUNT
};

enum AccountChooserUsabilityMetric {
  ACCOUNT_CHOOSER_LOOKS_OK,
  ACCOUNT_CHOOSER_EMPTY_USERNAME,
  ACCOUNT_CHOOSER_DUPLICATES,
  ACCOUNT_CHOOSER_EMPTY_USERNAME_AND_DUPLICATES,
  ACCOUNT_CHOOSER_USABILITY_COUNT,
};

// A version of the UMA_HISTOGRAM_BOOLEAN macro that allows the |name|
// to vary over the program's runtime.
void LogUMAHistogramBoolean(const std::string& name, bool sample);

// Log the |reason| a user dismissed the password manager UI.
void LogUIDismissalReason(UIDismissalReason reason);

// Log the appropriate display disposition.
void LogUIDisplayDisposition(UIDisplayDisposition disposition);

// Log if a saved FormData was deserialized correctly.
void LogFormDataDeserializationStatus(FormDeserializationStatus status);

// When a credential was filled, log whether it came from an Android app.
void LogFilledCredentialIsFromAndroidApp(bool from_android);

// Log what's preventing passwords from syncing.
void LogPasswordSyncState(PasswordSyncState state);

// Log submission events related to generation.
void LogPasswordGenerationSubmissionEvent(PasswordSubmissionEvent event);

// Log when password generation is available for a particular form.
void LogPasswordGenerationAvailableSubmissionEvent(
    PasswordSubmissionEvent event);

// Log submission events related to password update.
void LogUpdatePasswordSubmissionEvent(UpdatePasswordSubmissionEvent event);

// Log a user action on showing an update password bubble with multiple
// accounts.
void LogMultiAccountUpdateBubbleUserAction(
    MultiAccountUpdateBubbleUserAction action);

// Log a user action on showing the autosignin first run experience.
void LogAutoSigninPromoUserAction(AutoSigninPromoUserAction action);

// Log a user action on showing the account chooser for one or many accounts.
void LogAccountChooserUserActionOneAccount(AccountChooserUserAction action);
void LogAccountChooserUserActionManyAccounts(AccountChooserUserAction action);

// Log a user action on showing the Chrome sign in promo.
void LogAutoSigninPromoUserAction(SyncSignInUserAction action);

// Log if the account chooser has empty username or duplicate usernames.
void LogAccountChooserUsability(AccountChooserUsabilityMetric usability);

}  // namespace metrics_util

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_
