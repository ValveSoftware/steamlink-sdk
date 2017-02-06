// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_experiments.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/prefs/pref_service.h"
#include "components/sync_driver/sync_service.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace autofill {

const base::Feature kAutofillProfileCleanup{"AutofillProfileCleanup",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

bool IsAutofillEnabled(const PrefService* pref_service) {
  return pref_service->GetBoolean(prefs::kAutofillEnabled);
}

bool IsInAutofillSuggestionsDisabledExperiment() {
  std::string group_name =
      base::FieldTrialList::FindFullName("AutofillEnabled");
  return group_name == "Disabled";
}

bool IsAutofillProfileCleanupEnabled() {
  return base::FeatureList::IsEnabled(kAutofillProfileCleanup);
}

bool OfferStoreUnmaskedCards() {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // The checkbox can be forced on with a flag, but by default we don't store
  // on Linux due to lack of system keychain integration. See crbug.com/162735
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableOfferStoreUnmaskedWalletCards);
#else
  // Query the field trial before checking command line flags to ensure UMA
  // reports the correct group.
  std::string group_name =
      base::FieldTrialList::FindFullName("OfferStoreUnmaskedWalletCards");

  // The checkbox can be forced on or off with flags.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableOfferStoreUnmaskedWalletCards))
    return true;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableOfferStoreUnmaskedWalletCards))
    return false;

  // Otherwise use the field trial to show the checkbox or not.
  return group_name != "Disabled";
#endif
}

bool IsCreditCardUploadEnabled(const PrefService* pref_service,
                               const sync_driver::SyncService* sync_service,
                               const std::string& user_email) {
  // Query the field trial first to ensure UMA always reports the correct group.
  std::string group_name =
      base::FieldTrialList::FindFullName("OfferUploadCreditCards");

  // Check Autofill sync setting.
  if (!(sync_service && sync_service->CanSyncStart() &&
        sync_service->GetPreferredDataTypes().Has(syncer::AUTOFILL_PROFILE))) {
    return false;
  }

  // Users who have enabled a passphrase have chosen to not make their sync
  // information accessible to Google. Since upload makes credit card data
  // available to other Google systems, disable it for passphrase users.
  // We can't determine the passphrase state until the sync backend is
  // initialized so disable upload if sync is not yet available.
  if (!sync_service->IsBackendInitialized() ||
      sync_service->IsUsingSecondaryPassphrase()) {
    return false;
  }

  // Check Payments integration user setting.
  if (!pref_service->GetBoolean(prefs::kAutofillWalletImportEnabled))
    return false;

  // Check that the user is logged into a supported domain.
  if (user_email.empty())
    return false;
  std::string domain = gaia::ExtractDomainName(user_email);
  if (!(domain == "googlemail.com" || domain == "gmail.com" ||
        domain == "google.com")) {
    return false;
  }

  // With the user settings and logged in state verified, now we can consult the
  // command-line flags and experiment settings.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableOfferUploadCreditCards)) {
    return true;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableOfferUploadCreditCards)) {
    return false;
  }

  return !group_name.empty() && group_name != "Disabled";
}

}  // namespace autofill
