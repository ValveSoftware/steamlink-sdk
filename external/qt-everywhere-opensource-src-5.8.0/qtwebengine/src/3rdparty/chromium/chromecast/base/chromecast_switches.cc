// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/chromecast_switches.h"

#include "base/command_line.h"

namespace switches {

// Value indicating whether flag from command line switch is true.
const char kSwitchValueTrue[] = "true";

// Value indicating whether flag from command line switch is false.
const char kSwitchValueFalse[] = "false";

// Enable the CMA media pipeline.
const char kEnableCmaMediaPipeline[] = "enable-cma-media-pipeline";

// The bitmask of codecs (media_caps.h) supported by the current HDMI sink.
const char kHdmiSinkSupportedCodecs[] = "hdmi-sink-supported-codecs";

// Enable file accesses. It should not be enabled for most Cast devices.
const char kEnableLocalFileAccesses[] = "enable-local-file-accesses";

// Override the URL to which metrics logs are sent for debugging.
const char kOverrideMetricsUploadUrl[] = "override-metrics-upload-url";

// Disable features that require WiFi management.
const char kNoWifi[] = "no-wifi";

// Allows media playback for hidden WebContents
const char kAllowHiddenMediaPlayback[] = "allow-hidden-media-playback";

// Pass the app id information to the renderer process, to be used for logging.
// last-launched-app should be the app that just launched and is spawning the
// renderer.
const char kLastLaunchedApp[] = "last-launched-app";
// previous-app should be the app that was running when last-launched-app
// started.
const char kPreviousApp[] = "previous-app";

// Flag indicating that a resource provider must be set up to provide cast
// receiver with resources. Apps cannot start until provided resources.
// This flag implies --alsa-check-close-timeout=0.
const char kAcceptResourceProvider[] = "accept-resource-provider";

// Size of the ALSA output buffer in frames. This directly sets the latency of
// the output device. Latency can be calculated by multiplying the sample rate
// by the output buffer size.
const char kAlsaOutputBufferSize[] = "alsa-output-buffer-size";

// Size of the ALSA output period in frames. The period of an ALSA output device
// determines how many frames elapse between hardware interrupts.
const char kAlsaOutputPeriodSize[] = "alsa-output-period-size";

// How many frames need to be in the output buffer before output starts.
const char kAlsaOutputStartThreshold[] = "alsa-output-start-threshold";

// Minimum number of available frames for scheduling a transfer.
const char kAlsaOutputAvailMin[] = "alsa-output-avail-min";

// Time in ms to wait before closing the PCM handle when no more mixer inputs
// remain. Assumed to be 0 if --accept-resource-provider is present.
const char kAlsaCheckCloseTimeout[] = "alsa-check-close-timeout";

// Flag that enables resampling audio with sample rate below 32kHz up to 48kHz.
// Should be set to true for internal audio products.
const char kAlsaEnableUpsampling[] = "alsa-enable-upsampling";

// Optional flag to set a fixed sample rate for the alsa device.
const char kAlsaFixedOutputSampleRate[] = "alsa-fixed-output-sample-rate";

// Some platforms typically have very little 'free' memory, but plenty is
// available in buffers+cached.  For such platforms, configure this amount
// as the portion of buffers+cached memory that should be treated as
// unavailable.  If this switch is not used, a simple pressure heuristic based
// purely on free memory will be used.
const char kMemPressureSystemReservedKb[] = "mem-pressure-system-reserved-kb";

}  // namespace switches

namespace chromecast {

bool GetSwitchValueBoolean(const std::string& switch_string,
                           const bool default_value) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switch_string)) {
    if (command_line->GetSwitchValueASCII(switch_string) !=
            switches::kSwitchValueTrue &&
        command_line->GetSwitchValueASCII(switch_string) !=
            switches::kSwitchValueFalse &&
        command_line->GetSwitchValueASCII(switch_string) != "") {
      LOG(WARNING) << "Invalid switch value " << switch_string << "="
                   << command_line->GetSwitchValueASCII(switch_string)
                   << "; assuming default value of " << default_value;
      return default_value;
    }
    return command_line->GetSwitchValueASCII(switch_string) !=
           switches::kSwitchValueFalse;
  }
  return default_value;
}

}  // namespace chromecast
