// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_ENGINE_SETTINGS_H_
#define BLIMP_ENGINE_APP_ENGINE_SETTINGS_H_

#include "content/public/common/web_preferences.h"

namespace blimp {
namespace engine {

// The EngineSettings includes all parameters which are configured for the
// BlimpEngine and can be set by the client. This includes the values for
// content::WebPreferences which override the default values.
struct EngineSettings {
  EngineSettings() {}

  // -------WebPreferences-------
  // These members mirror the parameters in content::WebPreferences. See
  // content/public/common/web_preferences.h for details.

  bool record_whole_document = false;

  // Disable animation in images by default.
  content::ImageAnimationPolicy animation_policy =
      content::ImageAnimationPolicy::IMAGE_ANIMATION_POLICY_NO_ANIMATION;

  bool viewport_meta_enabled = true;
  bool shrinks_viewport_contents_to_fit = true;
  float default_minimum_page_scale_factor = 0.25f;
  float default_maximum_page_scale_factor = 5.f;
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_ENGINE_SETTINGS_H_
