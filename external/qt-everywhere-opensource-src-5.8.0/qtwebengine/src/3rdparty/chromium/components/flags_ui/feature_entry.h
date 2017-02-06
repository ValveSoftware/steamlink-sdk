// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FLAGS_UI_FEATURE_ENTRY_H_
#define COMPONENTS_FLAGS_UI_FEATURE_ENTRY_H_

#include <string>

#include "base/strings/string16.h"

namespace base {
struct Feature;
}

namespace flags_ui {

// FeatureEntry is used to describe an experimental feature.
//
// Note that features should eventually be either turned on by default with no
// about_flags entries or deleted. Most feature entries should only be around
// for a few milestones, until their full launch.
struct FeatureEntry {
  enum Type {
    // A feature with a single flag value. This is typically what you want.
    SINGLE_VALUE,

    // A default enabled feature with a single flag value to disable it. Please
    // consider whether you really need a flag to disable the feature, and even
    // if so remove the disable flag as soon as it is no longer needed.
    SINGLE_DISABLE_VALUE,

    // The feature has multiple values only one of which is ever enabled.
    // The first of the values should correspond to a deactivated state for this
    // feature (i.e. no command line option). For MULTI_VALUE entries, the
    // command_line of the FeatureEntry is not used. If the experiment is
    // enabled the command line of the selected Choice is enabled.
    MULTI_VALUE,

    // The feature has three possible values: Default, Enabled and Disabled.
    // This should be used for features that may have their own logic to decide
    // if the feature should be on when not explicitly specified via about
    // flags - for example via FieldTrials.
    ENABLE_DISABLE_VALUE,

    // Corresponds to a base::Feature, per base/feature_list.h. The entry will
    // have three states: Default, Enabled, Disabled. When not specified or set
    // to Default, the normal default value of the feature is used.
    FEATURE_VALUE,

    // Corresponds to a base::Feature and additional options [O_1, ..., O_n]
    // that specify variation parameters. Each of the options can specify a set
    // of variation parameters. The entry will have n+2 states: Default,
    // Enabled: V_1, ..., Enabled: V_n, Disabled. When not specified or set to
    // Default, the normal default values of the feature and of the parameters
    // are used.
    FEATURE_WITH_VARIATIONS_VALUE,
  };

  // Describes state of a feature.
  enum FeatureState {
    // The state of the feature is not overridden by the user.
    DEFAULT,
    // The feature is enabled by the user.
    ENABLED,
    // The feature is disabled by the user.
    DISABLED,
  };

  // Used for MULTI_VALUE types to describe one of the possible values the user
  // can select.
  struct Choice {
    // ID of the message containing the choice name.
    int description_id;

    // Command line switch and value to enabled for this choice.
    const char* command_line_switch;
    // Simple switches that have no value should use "" for command_line_value.
    const char* command_line_value;
  };

  // Configures one parameter for FEATURE_WITH_VARIATIONS_VALUE.
  struct FeatureParam {
    const char* param_name;
    const char* param_value;
  };

  // Specified one variation (list of parameter values) for
  // FEATURE_WITH_VARIATIONS_VALUE.
  struct FeatureVariation {
    // Text that denotes the variation in chrome://flags. For each variation,
    // the user is shown an option labeled "Enabled <description_text>" (with
    // the exception of the first option labeled "Enabled" to make clear it is
    // the default one). No need for description_id, chrome://flags should not
    // get translated. The other parts here use ids for historical reasons and
    // can realistically also be moved to direct description_texts.
    const char* description_text;
    const FeatureParam* params;
    int num_params;
  };

  // The internal name of the feature entry. This is never shown to the user.
  // It _is_ however stored in the prefs file, so you shouldn't change the
  // name of existing flags.
  const char* internal_name;

  // String id of the message containing the feature's name.
  int visible_name_id;

  // String id of the message containing the feature's description.
  int visible_description_id;

  // The platforms the feature is available on.
  // Needs to be more than a compile-time #ifdef because of profile sync.
  unsigned supported_platforms;  // bitmask

  // Type of entry.
  Type type;

  // The commandline switch and value that are added when this flag is active.
  // This is different from |internal_name| so that the commandline flag can be
  // renamed without breaking the prefs file.
  // This is used if type is SINGLE_VALUE or ENABLE_DISABLE_VALUE.
  const char* command_line_switch;

  // Simple switches that have no value should use "" for command_line_value.
  const char* command_line_value;

  // For ENABLE_DISABLE_VALUE, the command line switch and value to explicitly
  // disable the feature.
  const char* disable_command_line_switch;
  const char* disable_command_line_value;

  // For FEATURE_VALUE, the base::Feature this entry corresponds to.
  const base::Feature* feature;

  // Number of options to choose from. This is used if type is MULTI_VALUE,
  // ENABLE_DISABLE_VALUE, FEATURE_VALUE, or FEATURE_WITH_VARIATIONS_VALUE.
  int num_options;

  // This describes the options if type is MULTI_VALUE.
  const Choice* choices;

  // This describes the options if type is FEATURE_WITH_VARIATIONS_VALUE.
  // The first variation is the default "Enabled" variation, its description_id
  // is disregarded.
  const FeatureVariation* feature_variations;

  // The name of the FieldTrial in which the selected variation parameters
  // should be registered. This is used if type is
  // FEATURE_WITH_VARIATIONS_VALUE.
  const char* feature_trial_name;

  // Returns the name used in prefs for the option at the specified |index|.
  // Only used for types that use |num_options|.
  std::string NameForOption(int index) const;

  // Returns the human readable description for the option at |index|.
  // Only used for types that use |num_options|.
  base::string16 DescriptionForOption(int index) const;

  // Returns the choice for the option at |index|. Only applicable for type
  // FEATURE_MULTI.
  const FeatureEntry::Choice& ChoiceForOption(int index) const;

  // Returns the state of the feature at |index|. Only applicable for types
  // FEATURE_VALUE and FEATURE_WITH_VARIATIONS_VALUE.
  FeatureEntry::FeatureState StateForOption(int index) const;

  // Returns the variation for the option at |index| or nullptr if there is no
  // variation associated at |index|. Only applicable for types FEATURE_VALUE
  // and FEATURE_WITH_VARIATIONS_VALUE.
  const FeatureEntry::FeatureVariation* VariationForOption(int index) const;
};

namespace testing {

// Separator used for multi values. Multi values are represented in prefs as
// name-of-experiment + kMultiSeparator + selected_index.
extern const char kMultiSeparator[];

}  // namespace

}  // namespace flag_ui

#endif  // COMPONENTS_FLAGS_UI_FEATURE_ENTRY_H_
