// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_info.h"

#include "base/stl_util.h"

namespace content_settings {

ContentSettingsInfo::ContentSettingsInfo(
    const WebsiteSettingsInfo* website_settings_info,
    const std::vector<std::string>& whitelisted_schemes,
    const std::set<ContentSetting>& valid_settings,
    IncognitoBehavior incognito_behavior)
    : website_settings_info_(website_settings_info),
      whitelisted_schemes_(whitelisted_schemes),
      valid_settings_(valid_settings),
      incognito_behavior_(incognito_behavior) {}

ContentSettingsInfo::~ContentSettingsInfo() {}

bool ContentSettingsInfo::IsSettingValid(ContentSetting setting) const {
  return ContainsKey(valid_settings_, setting);
}

}  // namespace content_settings
