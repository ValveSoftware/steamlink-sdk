// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/plugins_field_trial.h"

namespace content_settings {

// static
ContentSetting PluginsFieldTrial::EffectiveContentSetting(
    ContentSettingsType type,
    ContentSetting setting) {
  if (type != CONTENT_SETTINGS_TYPE_PLUGINS)
    return setting;

  // For Plugins, ASK is obsolete. Show as BLOCK to reflect actual behavior.
  if (setting == ContentSetting::CONTENT_SETTING_ASK)
    return ContentSetting::CONTENT_SETTING_BLOCK;

  return setting;
}

}  // namespace content_settings
