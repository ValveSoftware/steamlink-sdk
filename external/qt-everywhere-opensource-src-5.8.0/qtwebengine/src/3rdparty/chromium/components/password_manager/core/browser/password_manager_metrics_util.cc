// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_metrics_util.h"

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "url/gurl.h"

using base::ListValue;
using base::FundamentalValue;

namespace password_manager {

namespace metrics_util {

void LogUMAHistogramBoolean(const std::string& name, bool sample) {
  // Note: This leaks memory, which is expected behavior.
  base::HistogramBase* histogram = base::BooleanHistogram::FactoryGet(
      name, base::Histogram::kUmaTargetedHistogramFlag);
  histogram->AddBoolean(sample);
}

void LogUIDismissalReason(UIDismissalReason reason) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.UIDismissalReason",
                            reason,
                            NUM_UI_RESPONSES);
}

void LogUIDisplayDisposition(UIDisplayDisposition disposition) {
  UMA_HISTOGRAM_ENUMERATION("PasswordBubble.DisplayDisposition",
                            disposition,
                            NUM_DISPLAY_DISPOSITIONS);
}

void LogFormDataDeserializationStatus(FormDeserializationStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.FormDataDeserializationStatus",
                            status, NUM_DESERIALIZATION_STATUSES);
}

void LogFilledCredentialIsFromAndroidApp(bool from_android) {
  UMA_HISTOGRAM_BOOLEAN(
      "PasswordManager.FilledCredentialWasFromAndroidApp",
      from_android);
}

void LogPasswordSyncState(PasswordSyncState state) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.PasswordSyncState", state,
                            NUM_SYNC_STATES);
}

void LogPasswordGenerationSubmissionEvent(PasswordSubmissionEvent event) {
  UMA_HISTOGRAM_ENUMERATION("PasswordGeneration.SubmissionEvent", event,
                            SUBMISSION_EVENT_ENUM_COUNT);
}

void LogPasswordGenerationAvailableSubmissionEvent(
    PasswordSubmissionEvent event) {
  UMA_HISTOGRAM_ENUMERATION("PasswordGeneration.SubmissionAvailableEvent",
                            event, SUBMISSION_EVENT_ENUM_COUNT);
}

void LogUpdatePasswordSubmissionEvent(UpdatePasswordSubmissionEvent event) {
  DCHECK_LT(event, UPDATE_PASSWORD_EVENT_COUNT);
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.UpdatePasswordSubmissionEvent",
                            event, UPDATE_PASSWORD_EVENT_COUNT);
}

void LogMultiAccountUpdateBubbleUserAction(
    MultiAccountUpdateBubbleUserAction action) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.MultiAccountPasswordUpdateAction",
                            action,
                            MULTI_ACCOUNT_UPDATE_BUBBLE_USER_ACTION_COUNT);
}

void LogAutoSigninPromoUserAction(AutoSigninPromoUserAction action) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.AutoSigninFirstRunDialog", action,
                            AUTO_SIGNIN_PROMO_ACTION_COUNT);
}

void LogAccountChooserUserActionOneAccount(AccountChooserUserAction action) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.AccountChooserDialogOneAccount",
                            action, ACCOUNT_CHOOSER_ACTION_COUNT);
  // TODO(vasilii): deprecate the histogram when the new ones hit the Stable.
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.AccountChooserDialog", action,
                            ACCOUNT_CHOOSER_ACTION_COUNT);
}

void LogAccountChooserUserActionManyAccounts(AccountChooserUserAction action) {
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.AccountChooserDialogMultipleAccounts", action,
      ACCOUNT_CHOOSER_ACTION_COUNT);
  // TODO(vasilii): deprecate the histogram when the new ones hit the Stable.
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.AccountChooserDialog", action,
                            ACCOUNT_CHOOSER_ACTION_COUNT);
}

void LogAutoSigninPromoUserAction(SyncSignInUserAction action) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.SignInPromo", action,
                            CHROME_SIGNIN_ACTION_COUNT);
}

void LogAccountChooserUsability(AccountChooserUsabilityMetric usability) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.AccountChooserDialogUsability",
                            usability, ACCOUNT_CHOOSER_USABILITY_COUNT);
}

}  // namespace metrics_util

}  // namespace password_manager
