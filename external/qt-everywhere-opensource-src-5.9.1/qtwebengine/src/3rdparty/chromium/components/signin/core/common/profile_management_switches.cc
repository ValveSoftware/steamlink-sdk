// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/common/profile_management_switches.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "build/build_config.h"
#include "components/signin/core/common/signin_switches.h"

namespace {

const char kNewProfileManagementFieldTrialName[] = "NewProfileManagement";

// Different state of new profile management/identity consistency.  The code
// below assumes the order of the values in this enum.  That is, new profile
// management is included in consistent identity.
enum State {
  STATE_NEW_AVATAR_MENU,
  STATE_NEW_PROFILE_MANAGEMENT,
  STATE_ACCOUNT_CONSISTENCY,
};

State GetProcessState() {
  // Find the state of both command line args.
  bool is_new_profile_management =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNewProfileManagement);
  bool is_consistent_identity =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAccountConsistency);
  bool not_new_profile_management =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableNewProfileManagement);
  bool not_consistent_identity =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAccountConsistency);
  int count_args = (is_new_profile_management ? 1 : 0) +
      (is_consistent_identity ? 1 : 0) +
      (not_new_profile_management ? 1 : 0) +
      (not_consistent_identity ? 1 : 0);
  bool invalid_commandline = count_args > 1;

  // At most only one of the command line args should be specified, otherwise
  // the finch group assignment is undefined.  If this is the case, disable
  // the field trial so that data is not collected in the wrong group.
  std::string trial_type;
  if (invalid_commandline) {
    base::FieldTrial* field_trial =
        base::FieldTrialList::Find(kNewProfileManagementFieldTrialName);
    if (field_trial)
      field_trial->Disable();

    trial_type.clear();
  } else {
    // Since the experiment is not being disabled, get the full name of the
    // field trial which will initialize the underlying mechanism.
    trial_type =
        base::FieldTrialList::FindFullName(kNewProfileManagementFieldTrialName);
  }

  // Enable command line args take precedent over disable command line args.
  // Consistent identity args take precedent over new profile management args.
  if (is_consistent_identity) {
    return STATE_ACCOUNT_CONSISTENCY;
  } else if (is_new_profile_management) {
    return STATE_NEW_PROFILE_MANAGEMENT;
  } else if (not_new_profile_management) {
    return STATE_NEW_AVATAR_MENU;
  } else if (not_consistent_identity) {
    return STATE_NEW_PROFILE_MANAGEMENT;
  }

  // Set the default state
#if defined(OS_ANDROID) || defined(OS_IOS)
  State state = STATE_ACCOUNT_CONSISTENCY;
#else
  State state = STATE_NEW_PROFILE_MANAGEMENT;
#endif

  if (!trial_type.empty()) {
    if (trial_type == "Enabled") {
      state = STATE_NEW_PROFILE_MANAGEMENT;
    } else if (trial_type == "AccountConsistency") {
      state = STATE_ACCOUNT_CONSISTENCY;
    } else if (trial_type == "NewAvatarMenu") {
      state = STATE_NEW_AVATAR_MENU;
    } else {
      state = STATE_NEW_PROFILE_MANAGEMENT;
    }
  }

  return state;
}

bool CheckFlag(const std::string& command_switch, State min_state) {
  // Individiual flag settings take precedence.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(command_switch))
    return true;

  return GetProcessState() >= min_state;
}

}  // namespace

namespace switches {

bool IsEnableAccountConsistency() {
  return GetProcessState() >= STATE_ACCOUNT_CONSISTENCY;
}

bool IsExtensionsMultiAccount() {
  return CheckFlag(switches::kExtensionsMultiAccount,
                   STATE_ACCOUNT_CONSISTENCY);
}

bool IsGoogleProfileInfo() {
  return CheckFlag(switches::kGoogleProfileInfo, STATE_NEW_AVATAR_MENU);
}

bool IsNewProfileManagement() {
  return GetProcessState() >= STATE_NEW_PROFILE_MANAGEMENT;
}

bool IsNewProfileManagementPreviewEnabled() {
  // No promotion to Enable Account Consistency.
  return false;
}

bool UsePasswordSeparatedSigninFlow() {
  return base::FeatureList::IsEnabled(
      switches::kUsePasswordSeparatedSigninFlow);
}

bool IsMaterialDesignUserMenu() {
  return base::FeatureList::IsEnabled(switches::kMaterialDesignUserMenu);
}

void EnableNewProfileManagementForTesting(base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kEnableNewProfileManagement);
  DCHECK(!command_line->HasSwitch(switches::kDisableNewProfileManagement));
}

void EnableAccountConsistencyForTesting(base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kEnableAccountConsistency);
  DCHECK(!command_line->HasSwitch(switches::kDisableAccountConsistency));
}

}  // namespace switches
