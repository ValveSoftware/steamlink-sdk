// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial.h"

#if defined(OS_ANDROID)
#include <jni.h>
#endif

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/field_trial.h"
#include "components/ntp_tiles/switches.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "jni/MostVisitedSites_jni.h"
#endif

namespace {
const char kPopularSiteDefaultGroup[] = "Default";
const char kPopularSiteControlGroup[] = "Control";
const char kPopularSiteEnabledGroup[] = "Enabled";
const char kPopularSiteEnabledCommandLineSwitchGroup[] =
    "EnabledWithCommandLineSwitch";
const char kPopularSiteDisabledCommandLineSwitchGroup[] =
    "DisabledWithCommandLineSwitch";
}  // namespace

namespace ntp_tiles {

// On iOS it is not technically possible to prep the field trials on first
// launch, the configuration file is downloaded too late. In order to run some
// experiments that need to be active on first launch to be meaningful these
// are hardcoded, but can be superceeded by a server side config on subsequent
// launches.
void SetUpFirstLaunchFieldTrial(bool is_stable_channel) {
  // Check first if a server side config superceeded this experiment.
  if (base::FieldTrialList::TrialExists(kPopularSitesFieldTrialName))
    return;

  // Stable channels will run with 10% probability.
  // Non-stable channels will run with 50% probability.
  const base::FieldTrial::Probability kTotalProbability = 100;
  const base::FieldTrial::Probability kEnabledAndControlProbability =
      is_stable_channel ? 10 : 50;

  // Experiment enabled until March 15, 2017. By default, disabled.
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kPopularSitesFieldTrialName, kTotalProbability,
          kPopularSiteDefaultGroup, 2017, 3, 15,  // Mar 15, 2017
          base::FieldTrial::ONE_TIME_RANDOMIZED, nullptr));

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ntp_tiles::switches::kEnableNTPPopularSites)) {
    trial->AppendGroup(kPopularSiteEnabledCommandLineSwitchGroup,
                       kTotalProbability);
  } else if (command_line->HasSwitch(
                 ntp_tiles::switches::kDisableNTPPopularSites)) {
    trial->AppendGroup(kPopularSiteDisabledCommandLineSwitchGroup,
                       kTotalProbability);
  } else {
    trial->AppendGroup(kPopularSiteControlGroup, kEnabledAndControlProbability);
    trial->AppendGroup(kPopularSiteEnabledGroup, kEnabledAndControlProbability);
  }
}

bool ShouldShowPopularSites() {
  // Note: It's important to query the field trial state first, to ensure that
  // UMA reports the correct group.
  const std::string group_name =
      base::FieldTrialList::FindFullName(kPopularSitesFieldTrialName);

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kDisableNTPPopularSites))
    return false;

  if (cmd_line->HasSwitch(switches::kEnableNTPPopularSites))
    return true;

#if defined(OS_ANDROID)
  if (Java_MostVisitedSites_isPopularSitesForceEnabled(
          base::android::AttachCurrentThread())) {
    return true;
  }
#endif

  return base::StartsWith(group_name, "Enabled",
                          base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace ntp_tiles
