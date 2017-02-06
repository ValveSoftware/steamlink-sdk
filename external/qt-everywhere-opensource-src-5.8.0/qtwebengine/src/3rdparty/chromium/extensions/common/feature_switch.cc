// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/feature_switch.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "extensions/common/switches.h"

namespace extensions {

namespace {

// The switch media-router is defined in chrome/common/chrome_switches.cc, but
// we can't depend on chrome here.
const char kMediaRouterFlag[] = "media-router";

const char kEnableMediaRouterExperiment[] = "EnableMediaRouter";
const char kExtensionActionRedesignExperiment[] = "ExtensionActionRedesign";

class CommonSwitches {
 public:
  CommonSwitches()
      : easy_off_store_install(nullptr, FeatureSwitch::DEFAULT_DISABLED),
        force_dev_mode_highlighting(switches::kForceDevModeHighlighting,
                                    FeatureSwitch::DEFAULT_DISABLED),
        prompt_for_external_extensions(
#if defined(CHROMIUM_BUILD)
            switches::kPromptForExternalExtensions,
#else
            nullptr,
#endif
#if defined(OS_WIN)
            FeatureSwitch::DEFAULT_ENABLED),
#else
            FeatureSwitch::DEFAULT_DISABLED),
#endif
        error_console(switches::kErrorConsole, FeatureSwitch::DEFAULT_DISABLED),
        enable_override_bookmarks_ui(switches::kEnableOverrideBookmarksUI,
                                     FeatureSwitch::DEFAULT_DISABLED),
        extension_action_redesign(switches::kExtensionActionRedesign,
                                  kExtensionActionRedesignExperiment,
                                  FeatureSwitch::DEFAULT_ENABLED),
        extension_action_redesign_override(switches::kExtensionActionRedesign,
                                           FeatureSwitch::DEFAULT_ENABLED),
        scripts_require_action(switches::kScriptsRequireAction,
                               FeatureSwitch::DEFAULT_DISABLED),
        embedded_extension_options(switches::kEmbeddedExtensionOptions,
                                   FeatureSwitch::DEFAULT_DISABLED),
        trace_app_source(switches::kTraceAppSource,
                         FeatureSwitch::DEFAULT_ENABLED),
        media_router(kMediaRouterFlag,
                     kEnableMediaRouterExperiment,
                     FeatureSwitch::DEFAULT_DISABLED) {
  }

  // Enables extensions to be easily installed from sites other than the web
  // store.
  FeatureSwitch easy_off_store_install;

  FeatureSwitch force_dev_mode_highlighting;

  // Should we prompt the user before allowing external extensions to install?
  // Default is yes.
  FeatureSwitch prompt_for_external_extensions;

  FeatureSwitch error_console;
  FeatureSwitch enable_override_bookmarks_ui;
  FeatureSwitch extension_action_redesign;
  FeatureSwitch extension_action_redesign_override;
  FeatureSwitch scripts_require_action;
  FeatureSwitch embedded_extension_options;
  FeatureSwitch trace_app_source;
  FeatureSwitch media_router;
};

base::LazyInstance<CommonSwitches> g_common_switches =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

FeatureSwitch* FeatureSwitch::force_dev_mode_highlighting() {
  return &g_common_switches.Get().force_dev_mode_highlighting;
}
FeatureSwitch* FeatureSwitch::easy_off_store_install() {
  return &g_common_switches.Get().easy_off_store_install;
}
FeatureSwitch* FeatureSwitch::prompt_for_external_extensions() {
  return &g_common_switches.Get().prompt_for_external_extensions;
}
FeatureSwitch* FeatureSwitch::error_console() {
  return &g_common_switches.Get().error_console;
}
FeatureSwitch* FeatureSwitch::enable_override_bookmarks_ui() {
  return &g_common_switches.Get().enable_override_bookmarks_ui;
}
FeatureSwitch* FeatureSwitch::extension_action_redesign() {
  // Force-enable the redesigned extension action toolbar when the Media Router
  // is enabled. Should be removed when the toolbar redesign is used by default.
  // See crbug.com/514694
  // Note that if Media Router is enabled by experiment, it implies that the
  // extension action redesign is also enabled by experiment. Thus it is fine
  // to return the override switch.
  // TODO(kmarshall): Remove this override.
  if (media_router()->IsEnabled())
    return &g_common_switches.Get().extension_action_redesign_override;

  return &g_common_switches.Get().extension_action_redesign;
}
FeatureSwitch* FeatureSwitch::scripts_require_action() {
  return &g_common_switches.Get().scripts_require_action;
}
FeatureSwitch* FeatureSwitch::embedded_extension_options() {
  return &g_common_switches.Get().embedded_extension_options;
}
FeatureSwitch* FeatureSwitch::trace_app_source() {
  return &g_common_switches.Get().trace_app_source;
}
FeatureSwitch* FeatureSwitch::media_router() {
  return &g_common_switches.Get().media_router;
}

FeatureSwitch::ScopedOverride::ScopedOverride(FeatureSwitch* feature,
                                              bool override_value)
    : feature_(feature),
      previous_value_(feature->GetOverrideValue()) {
  feature_->SetOverrideValue(
      override_value ? OVERRIDE_ENABLED : OVERRIDE_DISABLED);
}

FeatureSwitch::ScopedOverride::~ScopedOverride() {
  feature_->SetOverrideValue(previous_value_);
}

FeatureSwitch::FeatureSwitch(const char* switch_name,
                             DefaultValue default_value)
    : FeatureSwitch(base::CommandLine::ForCurrentProcess(),
                    switch_name,
                    default_value) {}

FeatureSwitch::FeatureSwitch(const char* switch_name,
                             const char* field_trial_name,
                             DefaultValue default_value)
    : FeatureSwitch(base::CommandLine::ForCurrentProcess(),
                    switch_name,
                    field_trial_name,
                    default_value) {}

FeatureSwitch::FeatureSwitch(const base::CommandLine* command_line,
                             const char* switch_name,
                             DefaultValue default_value)
    : FeatureSwitch(command_line, switch_name, nullptr, default_value) {}

FeatureSwitch::FeatureSwitch(const base::CommandLine* command_line,
                             const char* switch_name,
                             const char* field_trial_name,
                             DefaultValue default_value)
    : command_line_(command_line),
      switch_name_(switch_name),
      field_trial_name_(field_trial_name),
      default_value_(default_value == DEFAULT_ENABLED),
      override_value_(OVERRIDE_NONE) {}

bool FeatureSwitch::IsEnabled() const {
  if (override_value_ != OVERRIDE_NONE)
    return override_value_ == OVERRIDE_ENABLED;

  if (!switch_name_)
    return default_value_;

  std::string temp = command_line_->GetSwitchValueASCII(switch_name_);
  std::string switch_value;
  base::TrimWhitespaceASCII(temp, base::TRIM_ALL, &switch_value);

  if (switch_value == "1")
    return true;

  if (switch_value == "0")
    return false;

  // TODO(imcheng): Don't check |default_value_|. Otherwise, we could improperly
  // ignore an enable/disable switch if there is a field trial active.
  // crbug.com/585569
  if (!default_value_ && command_line_->HasSwitch(GetLegacyEnableFlag()))
    return true;

  if (default_value_ && command_line_->HasSwitch(GetLegacyDisableFlag()))
    return false;

  if (field_trial_name_) {
    std::string group_name =
        base::FieldTrialList::FindFullName(field_trial_name_);
    if (base::StartsWith(group_name, "Enabled", base::CompareCase::SENSITIVE))
      return true;
    if (base::StartsWith(group_name, "Disabled", base::CompareCase::SENSITIVE))
      return false;
  }

  return default_value_;
}

std::string FeatureSwitch::GetLegacyEnableFlag() const {
  DCHECK(switch_name_);
  return std::string("enable-") + switch_name_;
}

std::string FeatureSwitch::GetLegacyDisableFlag() const {
  DCHECK(switch_name_);
  return std::string("disable-") + switch_name_;
}

void FeatureSwitch::SetOverrideValue(OverrideValue override_value) {
  override_value_ = override_value;
}

FeatureSwitch::OverrideValue FeatureSwitch::GetOverrideValue() const {
  return override_value_;
}

}  // namespace extensions
