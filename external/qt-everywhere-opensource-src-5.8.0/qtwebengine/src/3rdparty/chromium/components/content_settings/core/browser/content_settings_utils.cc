// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_utils.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"

namespace {

const char kPatternSeparator[] = ",";

struct ContentSettingsStringMapping {
  ContentSetting content_setting;
  const char* content_setting_str;
};
const ContentSettingsStringMapping kContentSettingsStringMapping[] = {
    {CONTENT_SETTING_DEFAULT, "default"},
    {CONTENT_SETTING_ALLOW, "allow"},
    {CONTENT_SETTING_BLOCK, "block"},
    {CONTENT_SETTING_ASK, "ask"},
    {CONTENT_SETTING_SESSION_ONLY, "session_only"},
    {CONTENT_SETTING_DETECT_IMPORTANT_CONTENT, "detect_important_content"},
};
static_assert(arraysize(kContentSettingsStringMapping) ==
                  CONTENT_SETTING_NUM_SETTINGS,
              "kContentSettingsToFromString should have "
              "CONTENT_SETTING_NUM_SETTINGS elements");

}  // namespace

namespace content_settings {

std::string ContentSettingToString(ContentSetting setting) {
  if (setting >= CONTENT_SETTING_DEFAULT &&
      setting < CONTENT_SETTING_NUM_SETTINGS) {
    return kContentSettingsStringMapping[setting].content_setting_str;
  }
  return std::string();
}

bool ContentSettingFromString(const std::string& name,
                              ContentSetting* setting) {
  // We are starting the index from 1, as |CONTENT_SETTING_DEFAULT| is not
  // a recognized content setting.
  for (size_t i = 1; i < arraysize(kContentSettingsStringMapping); ++i) {
    if (name == kContentSettingsStringMapping[i].content_setting_str) {
      *setting = kContentSettingsStringMapping[i].content_setting;
      return true;
    }
  }
  *setting = CONTENT_SETTING_DEFAULT;
  return false;
}

std::string CreatePatternString(
    const ContentSettingsPattern& item_pattern,
    const ContentSettingsPattern& top_level_frame_pattern) {
  return item_pattern.ToString()
         + std::string(kPatternSeparator)
         + top_level_frame_pattern.ToString();
}

PatternPair ParsePatternString(const std::string& pattern_str) {
  std::vector<std::string> pattern_str_list = base::SplitString(
      pattern_str, std::string(1, kPatternSeparator[0]),
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // If the |pattern_str| is an empty string then the |pattern_string_list|
  // contains a single empty string. In this case the empty string will be
  // removed to signal an invalid |pattern_str|. Invalid pattern strings are
  // handle by the "if"-statment below. So the order of the if statements here
  // must be preserved.
  if (pattern_str_list.size() == 1) {
    if (pattern_str_list[0].empty()) {
      pattern_str_list.pop_back();
    } else {
      pattern_str_list.push_back("*");
    }
  }

  if (pattern_str_list.size() > 2 ||
      pattern_str_list.size() == 0) {
    return PatternPair(ContentSettingsPattern(),
                       ContentSettingsPattern());
  }

  PatternPair pattern_pair;
  pattern_pair.first =
      ContentSettingsPattern::FromString(pattern_str_list[0]);
  pattern_pair.second =
      ContentSettingsPattern::FromString(pattern_str_list[1]);
  return pattern_pair;
}

ContentSetting ValueToContentSetting(const base::Value* value) {
  ContentSetting setting = CONTENT_SETTING_DEFAULT;
  bool valid = ParseContentSettingValue(value, &setting);
  DCHECK(valid);
  return setting;
}

bool ParseContentSettingValue(const base::Value* value,
                              ContentSetting* setting) {
  if (!value) {
    *setting = CONTENT_SETTING_DEFAULT;
    return true;
  }
  int int_value = -1;
  if (!value->GetAsInteger(&int_value))
    return false;
  *setting = IntToContentSetting(int_value);
  return *setting != CONTENT_SETTING_DEFAULT;
}

std::unique_ptr<base::Value> ContentSettingToValue(ContentSetting setting) {
  if (setting <= CONTENT_SETTING_DEFAULT ||
      setting >= CONTENT_SETTING_NUM_SETTINGS) {
    return nullptr;
  }
  return base::WrapUnique(new base::FundamentalValue(setting));
}

void GetRendererContentSettingRules(const HostContentSettingsMap* map,
                                    RendererContentSettingRules* rules) {
#if !defined(OS_ANDROID)
  map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_IMAGES,
      ResourceIdentifier(),
      &(rules->image_rules));
#else
  // Android doesn't use image content settings, so ALLOW rule is added for
  // all origins.
  rules->image_rules.push_back(
      ContentSettingPatternSource(ContentSettingsPattern::Wildcard(),
                                  ContentSettingsPattern::Wildcard(),
                                  CONTENT_SETTING_ALLOW,
                                  std::string(),
                                  map->is_off_the_record()));
#endif
  map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT,
      ResourceIdentifier(),
      &(rules->script_rules));
  map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_AUTOPLAY,
      ResourceIdentifier(),
      &(rules->autoplay_rules));
}

}  // namespace content_settings
