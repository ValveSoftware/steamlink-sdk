// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_binary_value_map.h"

#include <utility>

#include "base/synchronization/lock.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings.h"

namespace content_settings {

namespace {

class RuleIteratorBinary : public RuleIterator {
 public:
  explicit RuleIteratorBinary(bool is_enabled,
                              std::unique_ptr<base::AutoLock> auto_lock)
      : is_done_(is_enabled), auto_lock_(std::move(auto_lock)) {}

  bool HasNext() const override { return !is_done_; }

  Rule Next() override {
    DCHECK(!is_done_);
    is_done_ = true;
    return Rule(ContentSettingsPattern::Wildcard(),
                ContentSettingsPattern::Wildcard(),
                new base::FundamentalValue(CONTENT_SETTING_BLOCK));
  }

 private:
  bool is_done_;
  std::unique_ptr<base::AutoLock> auto_lock_;
};

}  // namespace

BinaryValueMap::BinaryValueMap() {}

BinaryValueMap::~BinaryValueMap() {}

std::unique_ptr<RuleIterator> BinaryValueMap::GetRuleIterator(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    std::unique_ptr<base::AutoLock> auto_lock) const {
  if (resource_identifier.empty()) {
    return std::unique_ptr<RuleIterator>(new RuleIteratorBinary(
        IsContentSettingEnabled(content_type), std::move(auto_lock)));
  }
  return std::unique_ptr<RuleIterator>(new EmptyRuleIterator());
}

void BinaryValueMap::SetContentSettingDisabled(ContentSettingsType content_type,
                                               bool is_disabled) {
  is_enabled_[content_type] = !is_disabled;
}

bool BinaryValueMap::IsContentSettingEnabled(
    ContentSettingsType content_type) const {
  auto it = is_enabled_.find(content_type);
  if (it == is_enabled_.end())
    return true;
  return it->second;
}

}  // namespace content_settings
