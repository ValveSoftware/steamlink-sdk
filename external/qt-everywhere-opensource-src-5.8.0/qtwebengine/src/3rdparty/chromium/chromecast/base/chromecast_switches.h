// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_CHROMECAST_SWITCHES_H_
#define CHROMECAST_BASE_CHROMECAST_SWITCHES_H_

#include <string>

#include "build/build_config.h"

namespace switches {

// Switch values
extern const char kSwitchValueTrue[];
extern const char kSwitchValueFalse[];

// Media switches
extern const char kEnableCmaMediaPipeline[];
extern const char kHdmiSinkSupportedCodecs[];

// Content-implementation switches
extern const char kEnableLocalFileAccesses[];

// Metrics switches
extern const char kOverrideMetricsUploadUrl[];

// Network switches
extern const char kNoWifi[];

// App switches
extern const char kAllowHiddenMediaPlayback[];

// Switches to communicate app state information
extern const char kLastLaunchedApp[];
extern const char kPreviousApp[];

// Cast Receiver switches
extern const char kAcceptResourceProvider[];

// ALSA-based CMA switches. (Only valid for audio products.)
extern const char kAlsaOutputBufferSize[];
extern const char kAlsaOutputPeriodSize[];
extern const char kAlsaOutputStartThreshold[];
extern const char kAlsaOutputAvailMin[];
extern const char kAlsaCheckCloseTimeout[];
extern const char kAlsaEnableUpsampling[];
extern const char kAlsaFixedOutputSampleRate[];

// Memory pressure switches
extern const char kMemPressureSystemReservedKb[];

}  // namespace switches

namespace chromecast {

// Gets boolean value from switch |switch_string|.
// --|switch_string| -> true
// --|switch_string|="true" -> true
// --|switch_string|="false" -> false
// no switch named |switch_string| -> |default_value|
bool GetSwitchValueBoolean(const std::string& switch_string,
                           const bool default_value);

}  // namespace chromecast

#endif  // CHROMECAST_BASE_CHROMECAST_SWITCHES_H_
