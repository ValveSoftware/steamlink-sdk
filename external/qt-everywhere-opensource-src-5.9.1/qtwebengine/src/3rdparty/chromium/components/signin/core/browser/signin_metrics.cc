// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_metrics.h"

#include <limits.h>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"

namespace signin_metrics {

// Helper method to determine which |DifferentPrimaryAccounts| applies.
DifferentPrimaryAccounts ComparePrimaryAccounts(bool primary_accounts_same,
                                                int pre_count_gaia_cookies) {
  if (primary_accounts_same)
    return ACCOUNTS_SAME;
  if (pre_count_gaia_cookies == 0)
    return NO_COOKIE_PRESENT;
  return COOKIE_AND_TOKEN_PRIMARIES_DIFFERENT;
}

void LogSigninAccessPointStarted(AccessPoint access_point) {
  UMA_HISTOGRAM_ENUMERATION("Signin.SigninStartedAccessPoint",
                            static_cast<int>(access_point),
                            static_cast<int>(AccessPoint::ACCESS_POINT_MAX));
}

void LogSigninAccessPointCompleted(AccessPoint access_point) {
  UMA_HISTOGRAM_ENUMERATION("Signin.SigninCompletedAccessPoint",
                            static_cast<int>(access_point),
                            static_cast<int>(AccessPoint::ACCESS_POINT_MAX));
}

void LogSigninReason(Reason reason) {
  UMA_HISTOGRAM_ENUMERATION("Signin.SigninReason", static_cast<int>(reason),
                            static_cast<int>(Reason::REASON_MAX));
}

void LogSigninAccountReconciliation(int total_number_accounts,
                                    int count_added_to_cookie_jar,
                                    int count_removed_from_cookie_jar,
                                    bool primary_accounts_same,
                                    bool is_first_reconcile,
                                    int pre_count_gaia_cookies) {
  UMA_HISTOGRAM_COUNTS_100("Profile.NumberOfAccountsPerProfile",
                           total_number_accounts);
  // We want to include zeroes in the counts of added or removed accounts to
  // easily capture _relatively_ how often we merge accounts.
  if (is_first_reconcile) {
    UMA_HISTOGRAM_COUNTS_100("Signin.Reconciler.AddedToCookieJar.FirstRun",
                             count_added_to_cookie_jar);
    UMA_HISTOGRAM_COUNTS_100("Signin.Reconciler.RemovedFromCookieJar.FirstRun",
                             count_removed_from_cookie_jar);
    UMA_HISTOGRAM_ENUMERATION(
        "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
        ComparePrimaryAccounts(primary_accounts_same, pre_count_gaia_cookies),
        NUM_DIFFERENT_PRIMARY_ACCOUNT_METRICS);
  } else {
    UMA_HISTOGRAM_COUNTS_100("Signin.Reconciler.AddedToCookieJar.SubsequentRun",
                             count_added_to_cookie_jar);
    UMA_HISTOGRAM_COUNTS_100(
        "Signin.Reconciler.RemovedFromCookieJar.SubsequentRun",
        count_removed_from_cookie_jar);
    UMA_HISTOGRAM_ENUMERATION(
        "Signin.Reconciler.DifferentPrimaryAccounts.SubsequentRun",
        ComparePrimaryAccounts(primary_accounts_same, pre_count_gaia_cookies),
        NUM_DIFFERENT_PRIMARY_ACCOUNT_METRICS);
  }
}

void LogSigninAccountReconciliationDuration(base::TimeDelta duration,
                                            bool successful) {
  if (successful) {
    UMA_HISTOGRAM_TIMES("Signin.Reconciler.Duration.Success", duration);
  } else {
    UMA_HISTOGRAM_TIMES("Signin.Reconciler.Duration.Failure", duration);
  }
}

void LogSigninProfile(bool is_first_run, base::Time install_date) {
  // Track whether or not the user signed in during the first run of Chrome.
  UMA_HISTOGRAM_BOOLEAN("Signin.DuringFirstRun", is_first_run);

  // Determine how much time passed since install when this profile was signed
  // in.
  base::TimeDelta elapsed_time = base::Time::Now() - install_date;
  UMA_HISTOGRAM_COUNTS("Signin.ElapsedTimeFromInstallToSignin",
                       elapsed_time.InMinutes());
}

void LogSigninAddAccount() {
  // Account signin may fail for a wide variety of reasons. There is no
  // explicit false, but one can compare this value with the various UI
  // flows that lead to account sign-in, and deduce that the difference
  // counts the failures.
  UMA_HISTOGRAM_BOOLEAN("Signin.AddAccount", true);
}

void LogSignout(ProfileSignout source_metric, SignoutDelete delete_metric) {
  UMA_HISTOGRAM_ENUMERATION("Signin.SignoutProfile", source_metric,
                            NUM_PROFILE_SIGNOUT_METRICS);
  if (delete_metric != SignoutDelete::IGNORE_METRIC) {
    UMA_HISTOGRAM_BOOLEAN(
        "Signin.SignoutDeleteProfile",
        delete_metric == SignoutDelete::DELETED ? true : false);
  }
}

void LogExternalCcResultFetches(
    bool fetches_completed,
    const base::TimeDelta& time_to_check_connections) {
  UMA_HISTOGRAM_BOOLEAN("Signin.Reconciler.AllExternalCcResultCompleted",
                        fetches_completed);
  if (fetches_completed) {
    UMA_HISTOGRAM_TIMES(
        "Signin.Reconciler.ExternalCcResultTime.Completed",
        time_to_check_connections);
  } else {
    UMA_HISTOGRAM_TIMES(
        "Signin.Reconciler.ExternalCcResultTime.NotCompleted",
        time_to_check_connections);
  }
}

void LogAuthError(GoogleServiceAuthError::State auth_error) {
  UMA_HISTOGRAM_ENUMERATION("Signin.AuthError", auth_error,
      GoogleServiceAuthError::State::NUM_STATES);
}

void LogSigninConfirmHistogramValue(int action) {
  UMA_HISTOGRAM_ENUMERATION("Signin.OneClickConfirmation", action,
                            signin_metrics::HISTOGRAM_CONFIRM_MAX);
}

void LogXDevicePromoEligible(CrossDevicePromoEligibility metric) {
  UMA_HISTOGRAM_ENUMERATION(
      "Signin.XDevicePromo.Eligibility", metric,
      NUM_CROSS_DEVICE_PROMO_ELIGIBILITY_METRICS);
}

void LogXDevicePromoInitialized(CrossDevicePromoInitialized metric) {
  UMA_HISTOGRAM_ENUMERATION(
      "Signin.XDevicePromo.Initialized", metric,
      NUM_CROSS_DEVICE_PROMO_INITIALIZED_METRICS);
}

void LogBrowsingSessionDuration(const base::Time& previous_activity_time) {
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Signin.XDevicePromo.BrowsingSessionDuration",
      (base::Time::Now() - previous_activity_time).InMinutes(), 1,
      base::TimeDelta::FromDays(30).InMinutes(), 50);
}

void LogAccountReconcilorStateOnGaiaResponse(AccountReconcilorState state) {
  UMA_HISTOGRAM_ENUMERATION("Signin.AccountReconcilorState.OnGaiaResponse",
                            state, ACCOUNT_RECONCILOR_HISTOGRAM_COUNT);
}

void LogAccountEquality(AccountEquality equality) {
  UMA_HISTOGRAM_ENUMERATION("Signin.AccountEquality",
                            static_cast<int>(equality),
                            static_cast<int>(AccountEquality::HISTOGRAM_COUNT));
}

void LogCookieJarStableAge(const base::TimeDelta stable_age,
                           const ReportingType type) {
  INVESTIGATOR_HISTOGRAM_CUSTOM_COUNTS(
      "Signin.CookieJar.StableAge", type, stable_age.InSeconds(), 1,
      base::TimeDelta::FromDays(365).InSeconds(), 100);
}

void LogCookieJarCounts(const int signed_in,
                        const int signed_out,
                        const int total,
                        const ReportingType type) {
  INVESTIGATOR_HISTOGRAM_CUSTOM_COUNTS("Signin.CookieJar.SignedInCount", type,
                                       signed_in, 1, 10, 10);
  INVESTIGATOR_HISTOGRAM_CUSTOM_COUNTS("Signin.CookieJar.SignedOutCount", type,
                                       signed_out, 1, 10, 10);
  INVESTIGATOR_HISTOGRAM_CUSTOM_COUNTS("Signin.CookieJar.TotalCount", type,
                                       total, 1, 10, 10);
}

void LogAccountRelation(const AccountRelation relation,
                        const ReportingType type) {
  INVESTIGATOR_HISTOGRAM_ENUMERATION(
      "Signin.CookieJar.ChromeAccountRelation", type,
      static_cast<int>(relation),
      static_cast<int>(AccountRelation::HISTOGRAM_COUNT));
}

void LogIsShared(const bool is_shared, const ReportingType type) {
  INVESTIGATOR_HISTOGRAM_BOOLEAN("Signin.IsShared", type, is_shared);
}

}  // namespace signin_metrics
