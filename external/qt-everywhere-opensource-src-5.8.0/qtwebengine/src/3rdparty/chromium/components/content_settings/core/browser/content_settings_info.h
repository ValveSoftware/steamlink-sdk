// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_INFO_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_INFO_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/content_settings/core/common/content_settings.h"

namespace content_settings {

class WebsiteSettingsInfo;

class ContentSettingsInfo {
 public:
  enum IncognitoBehavior {
    // Content setting will be inherited from regular to incognito profiles
    // as usual.
    INHERIT_IN_INCOGNITO,

    // Content setting will only partially inherit from regular to incognito
    // profiles: BLOCK will inherit as usual, but ALLOW will become ASK.
    // This is unusual, so seek privacy review before using this.
    INHERIT_IN_INCOGNITO_EXCEPT_ALLOW
  };

  // This object does not take ownership of |website_settings_info|.
  ContentSettingsInfo(const WebsiteSettingsInfo* website_settings_info,
                      const std::vector<std::string>& whitelisted_schemes,
                      const std::set<ContentSetting>& valid_settings,
                      IncognitoBehavior incognito_behavior);
  ~ContentSettingsInfo();

  const WebsiteSettingsInfo* website_settings_info() const {
    return website_settings_info_;
  }
  const std::vector<std::string>& whitelisted_schemes() const {
    return whitelisted_schemes_;
  }

  bool IsSettingValid(ContentSetting setting) const;

  IncognitoBehavior incognito_behavior() const { return incognito_behavior_; }

 private:
  const WebsiteSettingsInfo* website_settings_info_;
  const std::vector<std::string> whitelisted_schemes_;
  const std::set<ContentSetting> valid_settings_;
  const IncognitoBehavior incognito_behavior_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsInfo);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_INFO_H_
