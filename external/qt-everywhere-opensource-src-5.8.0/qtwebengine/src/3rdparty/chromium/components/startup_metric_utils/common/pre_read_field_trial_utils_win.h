// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STARTUP_METRIC_UTILS_COMMON_PRE_READ_FIELD_TRIAL_UTILS_WIN_H_
#define COMPONENTS_STARTUP_METRIC_UTILS_COMMON_PRE_READ_FIELD_TRIAL_UTILS_WIN_H_

#include <string>

#include "base/callback_forward.h"
#include "base/strings/string16.h"

// Utility functions to support the PreRead field trial. The PreRead field trial
// changes the way DLLs are pre-read during startup.

namespace startup_metric_utils {

// Callback to register a synthetic field trial.
using RegisterPreReadSyntheticFieldTrialCallback =
    const base::Callback<bool(const std::string&, const std::string&)>;

// The options controlled by the PreRead field trial.
struct PreReadOptions {
  // Pre-read DLLs explicitly.
  bool pre_read : 1;

  // Pre-read DLLs with a high thread priority.
  bool high_priority : 1;

  // Pre-read DLLs using the ::PrefetchVirtualMemory function, if available.
  bool prefetch_virtual_memory : 1;

  // Pre-read chrome_child.dll in the browser process and not in child
  // processes.
  bool pre_read_chrome_child_in_browser : 1;
};

// Initializes DLL pre-reading options from the registry.
// |product_registry_path| is the registry path under which the registry key for
// this field trial resides.
void InitializePreReadOptions(const base::string16& product_registry_path);

// Returns the bitfield of the DLL pre-reading options to use for the current
// process. InitializePreReadOptions() must have been called before this.
PreReadOptions GetPreReadOptions();

// Updates DLL pre-reading options in the registry with the latest info for the
// next startup. |product_registry_path| is the registry path under which the
// registry key for this field trial resides.
void UpdatePreReadOptions(const base::string16& product_registry_path);

// Registers a synthetic field trial with the PreRead group currently stored in
// the registry. This must be done before the first metric log
// (metrics::MetricsLog) is created. Otherwise, UMA metrics generated during
// startup won't be correctly annotated. |product_registry_path| is the registry
// path under which the key for this field trial resides.
void RegisterPreReadSyntheticFieldTrial(
    const base::string16& product_registry_path,
    const RegisterPreReadSyntheticFieldTrialCallback&
        register_synthetic_field_trial);

}  // namespace startup_metric_utils

#endif  // COMPONENTS_STARTUP_METRIC_UTILS_COMMON_PRE_READ_FIELD_TRIAL_UTILS_WIN_H_
