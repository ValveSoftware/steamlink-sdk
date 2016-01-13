// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppapi_preferences.h"

namespace ppapi {

Preferences::Preferences()
    : default_font_size(0),
      default_fixed_font_size(0),
      number_of_cpu_cores(0),
      is_3d_supported(true),
      is_stage3d_supported(false),
      is_stage3d_baseline_supported(false),
      is_accelerated_video_decode_enabled(false) {}

Preferences::Preferences(const WebPreferences& prefs)
    : standard_font_family_map(prefs.standard_font_family_map),
      fixed_font_family_map(prefs.fixed_font_family_map),
      serif_font_family_map(prefs.serif_font_family_map),
      sans_serif_font_family_map(prefs.sans_serif_font_family_map),
      default_font_size(prefs.default_font_size),
      default_fixed_font_size(prefs.default_fixed_font_size),
      number_of_cpu_cores(prefs.number_of_cpu_cores),
      is_3d_supported(prefs.flash_3d_enabled),
      is_stage3d_supported(prefs.flash_stage3d_enabled),
      is_stage3d_baseline_supported(prefs.flash_stage3d_baseline_enabled),
      is_webgl_supported(prefs.experimental_webgl_enabled),
      is_accelerated_video_decode_enabled(
          prefs.pepper_accelerated_video_decode_enabled) {}

Preferences::~Preferences() {}

}  // namespace ppapi
