// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_UTILS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_UTILS_H_

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace base {
class Value;
}

class GURL;
class HostContentSettingsMap;

namespace content_settings {

class ProviderInterface;
class RuleIterator;

typedef std::pair<ContentSettingsPattern, ContentSettingsPattern> PatternPair;

// Helper class to iterate over only the values in a map.
template <typename IteratorType, typename ReferenceType>
class MapValueIterator {
 public:
  explicit MapValueIterator(IteratorType iterator) : iterator_(iterator) {}

  bool operator!=(const MapValueIterator& other) const {
    return iterator_ != other.iterator_;
  }

  MapValueIterator& operator++() {
    ++iterator_;
    return *this;
  }

  ReferenceType operator*() { return iterator_->second.get(); }

 private:
  IteratorType iterator_;
};

// These constants are copied from extensions/common/extension_constants.h and
// content/public/common/url_constants.h to avoid complicated dependencies.
// TODO(vabr): Get these constants through the ContentSettingsClient.
const char kChromeDevToolsScheme[] = "chrome-devtools";
const char kChromeUIScheme[] = "chrome";
const char kExtensionScheme[] = "chrome-extension";

std::string ContentSettingToString(ContentSetting setting);

// Converts a content setting string to the corresponding ContentSetting.
// Returns true if |name| specifies a valid content setting, false otherwise.
bool ContentSettingFromString(const std::string& name, ContentSetting* setting);

// Converts |Value| to |ContentSetting|.
ContentSetting ValueToContentSetting(const base::Value* value);

// Converts a |Value| to a |ContentSetting|. Returns true if |value| encodes
// a valid content setting, false otherwise. Note that |CONTENT_SETTING_DEFAULT|
// is encoded as a NULL value, so it is not allowed as an integer value.
bool ParseContentSettingValue(const base::Value* value,
                              ContentSetting* setting);

PatternPair ParsePatternString(const std::string& pattern_str);

std::string CreatePatternString(
    const ContentSettingsPattern& item_pattern,
    const ContentSettingsPattern& top_level_frame_pattern);

// Returns a |base::Value*| representation of |setting| if |setting| is
// a valid content setting. Otherwise, returns a nullptr.
std::unique_ptr<base::Value> ContentSettingToValue(ContentSetting setting);

// Populates |rules| with content setting rules for content types that are
// handled by the renderer.
void GetRendererContentSettingRules(const HostContentSettingsMap* map,
                                    RendererContentSettingRules* rules);

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_UTILS_H_
