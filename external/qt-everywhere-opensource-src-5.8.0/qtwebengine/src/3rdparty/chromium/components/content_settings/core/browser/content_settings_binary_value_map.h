// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_BINARY_VALUE_MAP_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_BINARY_VALUE_MAP_H_

#include <map>

#include "components/content_settings/core/browser/content_settings_provider.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace base {
class AutoLock;
}  // namespace base

namespace content_settings {

class RuleIterator;

// A simplified value map that can be used to disable or enable the entire
// Content Setting. The default behaviour is enabling the Content Setting if
// it is not set explicitly.
class BinaryValueMap {
 public:
  BinaryValueMap();
  ~BinaryValueMap();

  std::unique_ptr<RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      std::unique_ptr<base::AutoLock> lock) const;
  void SetContentSettingDisabled(ContentSettingsType content_type,
                                 bool disabled);
  bool IsContentSettingEnabled(ContentSettingsType content_type) const;

 private:
  std::map<ContentSettingsType, bool> is_enabled_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_BINARY_VALUE_MAP_H_
