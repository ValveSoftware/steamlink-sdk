// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/content_settings/content_settings_store.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_api_constants.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_helpers.h"
#include "components/content_settings/core/browser/content_settings_origin_identifier_value_map.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;
using content_settings::ConcatenationIterator;
using content_settings::Rule;
using content_settings::RuleIterator;
using content_settings::OriginIdentifierValueMap;
using content_settings::ResourceIdentifier;
using content_settings::ValueToContentSetting;

namespace extensions {

namespace helpers = content_settings_helpers;
namespace keys = content_settings_api_constants;

struct ContentSettingsStore::ExtensionEntry {
  // Extension id
  std::string id;
  // Whether extension is enabled in the profile.
  bool enabled;
  // Content settings.
  OriginIdentifierValueMap settings;
  // Persistent incognito content settings.
  OriginIdentifierValueMap incognito_persistent_settings;
  // Session-only incognito content settings.
  OriginIdentifierValueMap incognito_session_only_settings;
};

ContentSettingsStore::ContentSettingsStore() {
  DCHECK(OnCorrectThread());
}

ContentSettingsStore::~ContentSettingsStore() {
  STLDeleteValues(&entries_);
}

std::unique_ptr<RuleIterator> ContentSettingsStore::GetRuleIterator(
    ContentSettingsType type,
    const content_settings::ResourceIdentifier& identifier,
    bool incognito) const {
  std::vector<std::unique_ptr<RuleIterator>> iterators;
  // Iterate the extensions based on install time (last installed extensions
  // first).
  ExtensionEntryMap::const_reverse_iterator entry;

  // The individual |RuleIterators| shouldn't lock; pass |lock_| to the
  // |ConcatenationIterator| in a locked state.
  std::unique_ptr<base::AutoLock> auto_lock(new base::AutoLock(lock_));

  for (entry = entries_.rbegin(); entry != entries_.rend(); ++entry) {
    if (!entry->second->enabled)
      continue;

    if (incognito) {
      iterators.push_back(
          entry->second->incognito_session_only_settings.GetRuleIterator(
              type,
              identifier,
              NULL));
      iterators.push_back(
          entry->second->incognito_persistent_settings.GetRuleIterator(
              type,
              identifier,
              NULL));
    } else {
      iterators.push_back(
          entry->second->settings.GetRuleIterator(type, identifier, NULL));
    }
  }
  return std::unique_ptr<RuleIterator>(
      new ConcatenationIterator(std::move(iterators), auto_lock.release()));
}

void ContentSettingsStore::SetExtensionContentSetting(
    const std::string& ext_id,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType type,
    const content_settings::ResourceIdentifier& identifier,
    ContentSetting setting,
    ExtensionPrefsScope scope) {
  {
    base::AutoLock lock(lock_);
    OriginIdentifierValueMap* map = GetValueMap(ext_id, scope);
    if (setting == CONTENT_SETTING_DEFAULT) {
      map->DeleteValue(primary_pattern, secondary_pattern, type, identifier);
    } else {
      map->SetValue(primary_pattern, secondary_pattern, type, identifier,
                    new base::FundamentalValue(setting));
    }
  }

  // Send notification that content settings changed.
  // TODO(markusheintz): Notifications should only be sent if the set content
  // setting is effective and not hidden by another setting of another
  // extension installed more recently.
  NotifyOfContentSettingChanged(ext_id,
                                scope != kExtensionPrefsScopeRegular);
}

void ContentSettingsStore::RegisterExtension(
    const std::string& ext_id,
    const base::Time& install_time,
    bool is_enabled) {
  base::AutoLock lock(lock_);
  ExtensionEntryMap::iterator i = FindEntry(ext_id);
  ExtensionEntry* entry;
  if (i != entries_.end()) {
    entry = i->second;
  } else {
    entry = new ExtensionEntry;
    entries_.insert(std::make_pair(install_time, entry));
  }

  entry->id = ext_id;
  entry->enabled = is_enabled;
}

void ContentSettingsStore::UnregisterExtension(
    const std::string& ext_id) {
  bool notify = false;
  bool notify_incognito = false;
  {
    base::AutoLock lock(lock_);
    ExtensionEntryMap::iterator i = FindEntry(ext_id);
    if (i == entries_.end())
      return;
    notify = !i->second->settings.empty();
    notify_incognito = !i->second->incognito_persistent_settings.empty() ||
                       !i->second->incognito_session_only_settings.empty();

    delete i->second;
    entries_.erase(i);
  }
  if (notify)
    NotifyOfContentSettingChanged(ext_id, false);
  if (notify_incognito)
    NotifyOfContentSettingChanged(ext_id, true);
}

void ContentSettingsStore::SetExtensionState(
    const std::string& ext_id, bool is_enabled) {
  bool notify = false;
  bool notify_incognito = false;
  {
    base::AutoLock lock(lock_);
    ExtensionEntryMap::const_iterator i = FindEntry(ext_id);
    if (i == entries_.end())
      return;
    notify = !i->second->settings.empty();
    notify_incognito = !i->second->incognito_persistent_settings.empty() ||
                       !i->second->incognito_session_only_settings.empty();

    i->second->enabled = is_enabled;
  }
  if (notify)
    NotifyOfContentSettingChanged(ext_id, false);
  if (notify_incognito)
    NotifyOfContentSettingChanged(ext_id, true);
}

OriginIdentifierValueMap* ContentSettingsStore::GetValueMap(
    const std::string& ext_id,
    ExtensionPrefsScope scope) {
  ExtensionEntryMap::const_iterator i = FindEntry(ext_id);
  if (i != entries_.end()) {
    switch (scope) {
      case kExtensionPrefsScopeRegular:
        return &(i->second->settings);
      case kExtensionPrefsScopeRegularOnly:
        // TODO(bauerb): Implement regular-only content settings.
        NOTREACHED();
        return NULL;
      case kExtensionPrefsScopeIncognitoPersistent:
        return &(i->second->incognito_persistent_settings);
      case kExtensionPrefsScopeIncognitoSessionOnly:
        return &(i->second->incognito_session_only_settings);
    }
  }
  return NULL;
}

const OriginIdentifierValueMap* ContentSettingsStore::GetValueMap(
    const std::string& ext_id,
    ExtensionPrefsScope scope) const {
  ExtensionEntryMap::const_iterator i = FindEntry(ext_id);
  if (i == entries_.end())
    return NULL;

  switch (scope) {
    case kExtensionPrefsScopeRegular:
      return &(i->second->settings);
    case kExtensionPrefsScopeRegularOnly:
      // TODO(bauerb): Implement regular-only content settings.
      NOTREACHED();
      return NULL;
    case kExtensionPrefsScopeIncognitoPersistent:
      return &(i->second->incognito_persistent_settings);
    case kExtensionPrefsScopeIncognitoSessionOnly:
      return &(i->second->incognito_session_only_settings);
  }

  NOTREACHED();
  return NULL;
}

void ContentSettingsStore::ClearContentSettingsForExtension(
    const std::string& ext_id,
    ExtensionPrefsScope scope) {
  bool notify = false;
  {
    base::AutoLock lock(lock_);
    OriginIdentifierValueMap* map = GetValueMap(ext_id, scope);
    if (!map) {
      // Fail gracefully in Release builds.
      NOTREACHED();
      return;
    }
    notify = !map->empty();
    map->clear();
  }
  if (notify) {
    NotifyOfContentSettingChanged(ext_id, scope != kExtensionPrefsScopeRegular);
  }
}

base::ListValue* ContentSettingsStore::GetSettingsForExtension(
    const std::string& extension_id,
    ExtensionPrefsScope scope) const {
  base::AutoLock lock(lock_);
  const OriginIdentifierValueMap* map = GetValueMap(extension_id, scope);
  if (!map)
    return NULL;
  base::ListValue* settings = new base::ListValue();
  OriginIdentifierValueMap::EntryMap::const_iterator it;
  for (it = map->begin(); it != map->end(); ++it) {
    std::unique_ptr<RuleIterator> rule_iterator(map->GetRuleIterator(
        it->first.content_type, it->first.resource_identifier,
        NULL));  // We already hold the lock.
    while (rule_iterator->HasNext()) {
      const Rule& rule = rule_iterator->Next();
      std::unique_ptr<base::DictionaryValue> setting_dict(
          new base::DictionaryValue());
      setting_dict->SetString(keys::kPrimaryPatternKey,
                              rule.primary_pattern.ToString());
      setting_dict->SetString(keys::kSecondaryPatternKey,
                              rule.secondary_pattern.ToString());
      setting_dict->SetString(
          keys::kContentSettingsTypeKey,
          helpers::ContentSettingsTypeToString(it->first.content_type));
      setting_dict->SetString(keys::kResourceIdentifierKey,
                              it->first.resource_identifier);
      ContentSetting content_setting = ValueToContentSetting(rule.value.get());
      DCHECK_NE(CONTENT_SETTING_DEFAULT, content_setting);

      std::string setting_string =
          content_settings::ContentSettingToString(content_setting);
      DCHECK(!setting_string.empty());

      setting_dict->SetString(keys::kContentSettingKey, setting_string);
      settings->Append(std::move(setting_dict));
    }
  }
  return settings;
}

void ContentSettingsStore::SetExtensionContentSettingFromList(
    const std::string& extension_id,
    const base::ListValue* list,
    ExtensionPrefsScope scope) {
  for (const auto& value : *list) {
    base::DictionaryValue* dict;
    if (!value->GetAsDictionary(&dict)) {
      NOTREACHED();
      continue;
    }
    std::string primary_pattern_str;
    dict->GetString(keys::kPrimaryPatternKey, &primary_pattern_str);
    ContentSettingsPattern primary_pattern =
        ContentSettingsPattern::FromString(primary_pattern_str);
    DCHECK(primary_pattern.IsValid());

    std::string secondary_pattern_str;
    dict->GetString(keys::kSecondaryPatternKey, &secondary_pattern_str);
    ContentSettingsPattern secondary_pattern =
        ContentSettingsPattern::FromString(secondary_pattern_str);
    DCHECK(secondary_pattern.IsValid());

    std::string content_settings_type_str;
    dict->GetString(keys::kContentSettingsTypeKey, &content_settings_type_str);
    ContentSettingsType content_settings_type =
        helpers::StringToContentSettingsType(content_settings_type_str);
    DCHECK_NE(CONTENT_SETTINGS_TYPE_DEFAULT, content_settings_type);

    std::string resource_identifier;
    dict->GetString(keys::kResourceIdentifierKey, &resource_identifier);

    std::string content_setting_string;
    dict->GetString(keys::kContentSettingKey, &content_setting_string);
    ContentSetting setting;
    bool result = content_settings::ContentSettingFromString(
        content_setting_string, &setting);
    DCHECK(result);

    SetExtensionContentSetting(extension_id,
                               primary_pattern,
                               secondary_pattern,
                               content_settings_type,
                               resource_identifier,
                               setting,
                               scope);
  }
}

void ContentSettingsStore::AddObserver(Observer* observer) {
  DCHECK(OnCorrectThread());
  observers_.AddObserver(observer);
}

void ContentSettingsStore::RemoveObserver(Observer* observer) {
  DCHECK(OnCorrectThread());
  observers_.RemoveObserver(observer);
}

void ContentSettingsStore::NotifyOfContentSettingChanged(
    const std::string& extension_id,
    bool incognito) {
  FOR_EACH_OBSERVER(
      ContentSettingsStore::Observer,
      observers_,
      OnContentSettingChanged(extension_id, incognito));
}

bool ContentSettingsStore::OnCorrectThread() {
  // If there is no UI thread, we're most likely in a unit test.
  return !BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI);
}

ContentSettingsStore::ExtensionEntryMap::iterator
ContentSettingsStore::FindEntry(const std::string& ext_id) {
  ExtensionEntryMap::iterator i;
  for (i = entries_.begin(); i != entries_.end(); ++i) {
    if (i->second->id == ext_id)
      return i;
  }
  return entries_.end();
}

ContentSettingsStore::ExtensionEntryMap::const_iterator
ContentSettingsStore::FindEntry(const std::string& ext_id) const {
  ExtensionEntryMap::const_iterator i;
  for (i = entries_.begin(); i != entries_.end(); ++i) {
    if (i->second->id == ext_id)
      return i;
  }
  return entries_.end();
}

}  // namespace extensions
