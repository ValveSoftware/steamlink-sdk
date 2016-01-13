// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPAPI_PREFERENCES_H_
#define PPAPI_SHARED_IMPL_PPAPI_PREFERENCES_H_

#include "ppapi/shared_impl/ppapi_shared_export.h"
#include "webkit/common/webpreferences.h"

struct WebPreferences;

namespace ppapi {

struct PPAPI_SHARED_EXPORT Preferences {
 public:
  Preferences();
  explicit Preferences(const WebPreferences& prefs);
  ~Preferences();

  webkit_glue::ScriptFontFamilyMap standard_font_family_map;
  webkit_glue::ScriptFontFamilyMap fixed_font_family_map;
  webkit_glue::ScriptFontFamilyMap serif_font_family_map;
  webkit_glue::ScriptFontFamilyMap sans_serif_font_family_map;
  int default_font_size;
  int default_fixed_font_size;
  int number_of_cpu_cores;
  bool is_3d_supported;
  bool is_stage3d_supported;
  bool is_stage3d_baseline_supported;
  bool is_webgl_supported;
  bool is_accelerated_video_decode_enabled;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPAPI_PREFERENCES_H_
