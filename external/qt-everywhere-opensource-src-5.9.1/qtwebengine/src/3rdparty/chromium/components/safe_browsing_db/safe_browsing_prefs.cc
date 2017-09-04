// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing_db/safe_browsing_prefs.h"

namespace {

// Switch value which force the ScoutGroupSelected pref to true.
const char kForceScoutGroupValueTrue[] = "true";
// Switch value which force the ScoutGroupSelected pref to false.
const char kForceScoutGroupValueFalse[] = "false";

// Name of the Scout Transition UMA metric.
const char kScoutTransitionMetricName[] = "SafeBrowsing.Pref.Scout.Transition";

// Reasons that a state transition for Scout was performed.
// These values are written to logs.  New enum values can be added, but
// existing enums must never be renumbered or deleted and reused.
enum ScoutTransitionReason {
  // Flag forced Scout Group to true
  FORCE_SCOUT_FLAG_TRUE = 0,
  // Flag forced Scout Group to false
  FORCE_SCOUT_FLAG_FALSE = 1,
  // User in OnlyShowScout group, enters Scout Group
  ONLY_SHOW_SCOUT_OPT_IN = 2,
  // User in CanShowScout group, enters Scout Group immediately
  CAN_SHOW_SCOUT_OPT_IN_SCOUT_GROUP_ON = 3,
  // User in CanShowScout group, waiting for interstitial to enter Scout Group
  CAN_SHOW_SCOUT_OPT_IN_WAIT_FOR_INTERSTITIAL = 4,
  // User in CanShowScout group saw the first interstitial and entered the Scout
  // Group
  CAN_SHOW_SCOUT_OPT_IN_SAW_FIRST_INTERSTITIAL = 5,
  // User in Control group
  CONTROL = 6,
  // Rollback: SBER2 on on implies SBER1 can turn on
  ROLLBACK_SBER2_IMPLIES_SBER1 = 7,
  // Rollback: SBER2 off so SBER1 must be turned off
  ROLLBACK_NO_SBER2_SET_SBER1_FALSE = 8,
  // Rollback: SBER2 absent so SBER1 must be cleared
  ROLLBACK_NO_SBER2_CLEAR_SBER1 = 9,
  // New reasons must be added BEFORE MAX_REASONS
  MAX_REASONS
};
}  // namespace

namespace prefs {
const char kSafeBrowsingExtendedReportingEnabled[] =
    "safebrowsing.extended_reporting_enabled";
const char kSafeBrowsingScoutReportingEnabled[] =
    "safebrowsing.scout_reporting_enabled";
const char kSafeBrowsingScoutGroupSelected[] =
    "safebrowsing.scout_group_selected";
}  // namespace prefs

namespace safe_browsing {

const char kSwitchForceScoutGroup[] = "force-scout-group";

const base::Feature kCanShowScoutOptIn{"CanShowScoutOptIn",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kOnlyShowScoutOptIn{"OnlyShowScoutOptIn",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

std::string ChooseOptInTextPreference(
    const PrefService& prefs,
    const std::string& extended_reporting_pref,
    const std::string& scout_pref) {
  return IsScout(prefs) ? scout_pref : extended_reporting_pref;
}

int ChooseOptInTextResource(const PrefService& prefs,
                            int extended_reporting_resource,
                            int scout_resource) {
  return IsScout(prefs) ? scout_resource : extended_reporting_resource;
}

bool ExtendedReportingPrefExists(const PrefService& prefs) {
  return prefs.HasPrefPath(GetExtendedReportingPrefName(prefs));
}

ExtendedReportingLevel GetExtendedReportingLevel(const PrefService& prefs) {
  if (!IsExtendedReportingEnabled(prefs)) {
    return SBER_LEVEL_OFF;
  } else {
    return IsScout(prefs) ? SBER_LEVEL_SCOUT : SBER_LEVEL_LEGACY;
  }
}

const char* GetExtendedReportingPrefName(const PrefService& prefs) {
  // The Scout pref is active if either of the experiment features are on, and
  // ScoutGroupSelected is on as well.
  if ((base::FeatureList::IsEnabled(kCanShowScoutOptIn) ||
       base::FeatureList::IsEnabled(kOnlyShowScoutOptIn)) &&
      prefs.GetBoolean(prefs::kSafeBrowsingScoutGroupSelected)) {
    return prefs::kSafeBrowsingScoutReportingEnabled;
  }

  // ..otherwise, either no experiment is on (ie: the Control group) or
  // ScoutGroupSelected is off. So we use the SBER pref instead.
  return prefs::kSafeBrowsingExtendedReportingEnabled;
}

void InitializeSafeBrowsingPrefs(PrefService* prefs) {
  // First handle forcing kSafeBrowsingScoutGroupSelected pref via cmd-line.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kSwitchForceScoutGroup)) {
    std::string switch_value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            kSwitchForceScoutGroup);
    if (switch_value == kForceScoutGroupValueTrue) {
      prefs->SetBoolean(prefs::kSafeBrowsingScoutGroupSelected, true);
      UMA_HISTOGRAM_ENUMERATION(kScoutTransitionMetricName,
                                FORCE_SCOUT_FLAG_TRUE, MAX_REASONS);
    } else if (switch_value == kForceScoutGroupValueFalse) {
      prefs->SetBoolean(prefs::kSafeBrowsingScoutGroupSelected, false);
      UMA_HISTOGRAM_ENUMERATION(kScoutTransitionMetricName,
                                FORCE_SCOUT_FLAG_FALSE, MAX_REASONS);
    }

    // If the switch is used, don't process the experiment state.
    return;
  }

  // Handle the three possible experiment states.
  if (base::FeatureList::IsEnabled(kOnlyShowScoutOptIn)) {
    // OnlyShowScoutOptIn immediately turns on ScoutGroupSelected pref.
    prefs->SetBoolean(prefs::kSafeBrowsingScoutGroupSelected, true);
    UMA_HISTOGRAM_ENUMERATION(kScoutTransitionMetricName,
                              ONLY_SHOW_SCOUT_OPT_IN, MAX_REASONS);
  } else if (base::FeatureList::IsEnabled(kCanShowScoutOptIn)) {
    // CanShowScoutOptIn will only turn on ScoutGroupSelected pref if the legacy
    // SBER pref is false. Otherwise the legacy SBER pref will stay on and
    // continue to be used until the next security incident, at which point
    // the Scout pref will become the active one.
    if (!prefs->GetBoolean(prefs::kSafeBrowsingExtendedReportingEnabled)) {
      prefs->SetBoolean(prefs::kSafeBrowsingScoutGroupSelected, true);
      UMA_HISTOGRAM_ENUMERATION(kScoutTransitionMetricName,
                                CAN_SHOW_SCOUT_OPT_IN_SCOUT_GROUP_ON,
                                MAX_REASONS);
    } else {
      UMA_HISTOGRAM_ENUMERATION(kScoutTransitionMetricName,
                                CAN_SHOW_SCOUT_OPT_IN_WAIT_FOR_INTERSTITIAL,
                                MAX_REASONS);
    }
  } else {
    // Both experiment features are off, so this is the Control group. We must
    // handle the possibility that the user was previously in an experiment
    // group (above) that was reverted. We want to restore the user to a
    // reasonable state based on the ScoutGroup and ScoutReporting preferences.
    UMA_HISTOGRAM_ENUMERATION(kScoutTransitionMetricName, CONTROL, MAX_REASONS);
    if (prefs->GetBoolean(prefs::kSafeBrowsingScoutReportingEnabled)) {
      // User opted-in to Scout which is broader than legacy Extended Reporting.
      // Opt them in to Extended Reporting.
      prefs->SetBoolean(prefs::kSafeBrowsingExtendedReportingEnabled, true);
      UMA_HISTOGRAM_ENUMERATION(kScoutTransitionMetricName,
                                ROLLBACK_SBER2_IMPLIES_SBER1, MAX_REASONS);
    } else if (prefs->GetBoolean(prefs::kSafeBrowsingScoutGroupSelected)) {
      // User was in the Scout Group (ie: seeing the Scout opt-in text) but did
      // NOT opt-in to Scout. Assume this was a conscious choice and remove
      // their legacy Extended Reporting opt-in as well. The user will have a
      // chance to evaluate their choice next time they see the opt-in text.

      // We make the Extended Reporting pref mimic the state of the Scout
      // Reporting pref. So we either Clear it or set it to False.
      if (prefs->HasPrefPath(prefs::kSafeBrowsingScoutReportingEnabled)) {
        // Scout Reporting pref was explicitly set to false, so set the SBER
        // pref to false.
        prefs->SetBoolean(prefs::kSafeBrowsingExtendedReportingEnabled, false);
        UMA_HISTOGRAM_ENUMERATION(kScoutTransitionMetricName,
                                  ROLLBACK_NO_SBER2_SET_SBER1_FALSE,
                                  MAX_REASONS);
      } else {
        // Scout Reporting pref is unset, so clear the SBER pref.
        prefs->ClearPref(prefs::kSafeBrowsingExtendedReportingEnabled);
        UMA_HISTOGRAM_ENUMERATION(kScoutTransitionMetricName,
                                  ROLLBACK_NO_SBER2_CLEAR_SBER1, MAX_REASONS);
      }
    }

    // Also clear both the Scout settings to start over from a clean state and
    // avoid the above logic from triggering on next restart.
    prefs->ClearPref(prefs::kSafeBrowsingScoutGroupSelected);
    prefs->ClearPref(prefs::kSafeBrowsingScoutReportingEnabled);
  }
}

bool IsExtendedReportingEnabled(const PrefService& prefs) {
  return prefs.GetBoolean(GetExtendedReportingPrefName(prefs));
}

bool IsScout(const PrefService& prefs) {
  return GetExtendedReportingPrefName(prefs) ==
         prefs::kSafeBrowsingScoutReportingEnabled;
}

void RecordExtendedReportingMetrics(const PrefService& prefs) {
  // This metric tracks the extended browsing opt-in based on whichever setting
  // the user is currently seeing. It tells us whether extended reporting is
  // happening for this user.
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.Pref.Extended",
                        IsExtendedReportingEnabled(prefs));

  // These metrics track the Scout transition.
  if (prefs.GetBoolean(prefs::kSafeBrowsingScoutGroupSelected)) {
    // Users in the Scout group: currently seeing the Scout opt-in.
    UMA_HISTOGRAM_BOOLEAN(
        "SafeBrowsing.Pref.Scout.ScoutGroup.SBER1Pref",
        prefs.GetBoolean(prefs::kSafeBrowsingExtendedReportingEnabled));
    UMA_HISTOGRAM_BOOLEAN(
        "SafeBrowsing.Pref.Scout.ScoutGroup.SBER2Pref",
        prefs.GetBoolean(prefs::kSafeBrowsingScoutReportingEnabled));
  } else {
    // Users not in the Scout group: currently seeing the SBER opt-in.
    UMA_HISTOGRAM_BOOLEAN(
        "SafeBrowsing.Pref.Scout.NoScoutGroup.SBER1Pref",
        prefs.GetBoolean(prefs::kSafeBrowsingExtendedReportingEnabled));
    // The following metric is a corner case. User was previously in the
    // Scout group and was able to opt-in to the Scout pref, but was since
    // removed from the Scout group (eg: by rolling back a Scout experiment).
    UMA_HISTOGRAM_BOOLEAN(
        "SafeBrowsing.Pref.Scout.NoScoutGroup.SBER2Pref",
        prefs.GetBoolean(prefs::kSafeBrowsingScoutReportingEnabled));
  }
}

void SetExtendedReportingPref(PrefService* prefs, bool value) {
  prefs->SetBoolean(GetExtendedReportingPrefName(*prefs), value);
}

void UpdatePrefsBeforeSecurityInterstitial(PrefService* prefs) {
  // Move the user into the Scout Group if the CanShowScoutOptIn experiment is
  // enabled and they're not in the group already.
  if (base::FeatureList::IsEnabled(kCanShowScoutOptIn) &&
      !prefs->GetBoolean(prefs::kSafeBrowsingScoutGroupSelected)) {
    prefs->SetBoolean(prefs::kSafeBrowsingScoutGroupSelected, true);
    UMA_HISTOGRAM_ENUMERATION(kScoutTransitionMetricName,
                              CAN_SHOW_SCOUT_OPT_IN_SAW_FIRST_INTERSTITIAL,
                              MAX_REASONS);
  }
}

}  // namespace safe_browsing
