// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_switches_internal.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace content {

namespace {

#if defined(OS_WIN)
const base::Feature kUseZoomForDsfEnabledByDefault {
  "use-zoom-for-dsf enabled by default", base::FEATURE_ENABLED_BY_DEFAULT
};
#endif

bool IsUseZoomForDSFEnabledByDefault() {
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
  return true;
#elif defined(OS_WIN)
  return base::FeatureList::IsEnabled(kUseZoomForDsfEnabledByDefault);
#else
  return false;
#endif
}

#if defined(ANDROID)
const base::Feature kProgressBarCompletionResourcesBeforeDOMContentLoaded {
    "progress-bar-completion-resources-before-domContentLoaded",
    base::FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace

bool IsPinchToZoomEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Enable pinch everywhere unless it's been explicitly disabled.
  return !command_line.HasSwitch(switches::kDisablePinch);
}

#if defined(OS_WIN)

bool IsWin32kLockdownEnabled() {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return false;
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kDisableWin32kLockDown))
    return false;
  return true;
}

#endif

V8CacheOptions GetV8CacheOptions() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string v8_cache_options =
      command_line.GetSwitchValueASCII(switches::kV8CacheOptions);
  if (v8_cache_options.empty())
    v8_cache_options = base::FieldTrialList::FindFullName("V8CacheOptions");
  if (v8_cache_options == "none") {
    return V8_CACHE_OPTIONS_NONE;
  } else if (v8_cache_options == "parse") {
    return V8_CACHE_OPTIONS_PARSE;
  } else if (v8_cache_options == "code") {
    return V8_CACHE_OPTIONS_CODE;
  } else {
    return V8_CACHE_OPTIONS_DEFAULT;
  }
}

bool IsUseZoomForDSFEnabled() {
  static bool use_zoom_for_dsf_enabled_by_default =
      IsUseZoomForDSFEnabledByDefault();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool enabled =
      (command_line->HasSwitch(switches::kEnableUseZoomForDSF) ||
       use_zoom_for_dsf_enabled_by_default) &&
      command_line->GetSwitchValueASCII(
          switches::kEnableUseZoomForDSF) != "false";

  return enabled;
}

ProgressBarCompletion GetProgressBarCompletionPolicy() {
#if defined(OS_ANDROID)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string progress_bar_completion =
      command_line.GetSwitchValueASCII(switches::kProgressBarCompletion);
  if (progress_bar_completion == "loadEvent")
    return ProgressBarCompletion::LOAD_EVENT;
  if (progress_bar_completion == "resourcesBeforeDOMContentLoaded")
    return ProgressBarCompletion::RESOURCES_BEFORE_DCL;
  if (progress_bar_completion == "domContentLoaded")
    return ProgressBarCompletion::DOM_CONTENT_LOADED;
  if (progress_bar_completion ==
      "resourcesBeforeDOMContentLoadedAndSameOriginIframes") {
    return ProgressBarCompletion::RESOURCES_BEFORE_DCL_AND_SAME_ORIGIN_IFRAMES;
  }
  // The command line, which is set by the user, takes priority. Otherwise,
  // fall back to the feature flag.
  if (base::FeatureList::IsEnabled(
          kProgressBarCompletionResourcesBeforeDOMContentLoaded)) {
    return ProgressBarCompletion::RESOURCES_BEFORE_DCL;
  }
#endif
  return ProgressBarCompletion::LOAD_EVENT;
}

} // namespace content
