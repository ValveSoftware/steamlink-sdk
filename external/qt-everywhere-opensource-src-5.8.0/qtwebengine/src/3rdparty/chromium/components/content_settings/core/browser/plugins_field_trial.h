// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_H_

#include "base/macros.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace content_settings {

// This class manages the Plugins field trials.
class PluginsFieldTrial {
 public:
  // Returns the effective content setting for plugins. Passes non-plugin
  // content settings through without modification.
  static ContentSetting EffectiveContentSetting(ContentSettingsType type,
                                                ContentSetting setting);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PluginsFieldTrial);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_H_
