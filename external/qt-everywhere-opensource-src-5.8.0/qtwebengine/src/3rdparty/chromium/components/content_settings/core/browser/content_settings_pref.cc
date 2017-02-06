// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_pref.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/time/clock.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "url/gurl.h"

namespace {

const char kSettingPath[] = "setting";
const char kPerResourceIdentifierPrefName[] = "per_resource";
const char kLastUsed[] = "last_used";

// If the given content type supports resource identifiers in user preferences,
// returns true and sets |pref_key| to the key in the content settings
// dictionary under which per-resource content settings are stored.
// Otherwise, returns false.
bool SupportsResourceIdentifiers(ContentSettingsType content_type) {
  return content_type == CONTENT_SETTINGS_TYPE_PLUGINS;
}

bool IsValueAllowedForType(const base::Value* value, ContentSettingsType type) {
  const content_settings::ContentSettingsInfo* info =
      content_settings::ContentSettingsRegistry::GetInstance()->Get(type);
  if (info) {
    int setting;
    if (!value->GetAsInteger(&setting))
      return false;
    if (setting == CONTENT_SETTING_DEFAULT)
      return false;
    return info->IsSettingValid(IntToContentSetting(setting));
  }

  // TODO(raymes): We should permit different types of base::Value for
  // website settings.
  return value->GetType() == base::Value::TYPE_DICTIONARY;
}

}  // namespace

namespace content_settings {

ContentSettingsPref::ContentSettingsPref(
    ContentSettingsType content_type,
    PrefService* prefs,
    PrefChangeRegistrar* registrar,
    const std::string& pref_name,
    bool incognito,
    NotifyObserversCallback notify_callback)
    : content_type_(content_type),
      prefs_(prefs),
      registrar_(registrar),
      pref_name_(pref_name),
      is_incognito_(incognito),
      updating_preferences_(false),
      notify_callback_(notify_callback) {
  DCHECK(prefs_);

  ReadContentSettingsFromPref();

  registrar_->Add(
      pref_name_,
      base::Bind(&ContentSettingsPref::OnPrefChanged, base::Unretained(this)));
}

ContentSettingsPref::~ContentSettingsPref() {
}

std::unique_ptr<RuleIterator> ContentSettingsPref::GetRuleIterator(
    const ResourceIdentifier& resource_identifier,
    bool incognito) const {
  if (incognito)
    return incognito_value_map_.GetRuleIterator(content_type_,
                                                resource_identifier,
                                                &lock_);
  return value_map_.GetRuleIterator(content_type_, resource_identifier, &lock_);
}

bool ContentSettingsPref::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    const ResourceIdentifier& resource_identifier,
    base::Value* in_value) {
  DCHECK(!in_value || IsValueAllowedForType(in_value, content_type_));
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs_);
  DCHECK(primary_pattern != ContentSettingsPattern::Wildcard() ||
         secondary_pattern != ContentSettingsPattern::Wildcard() ||
         !resource_identifier.empty());

  // At this point take the ownership of the |in_value|.
  std::unique_ptr<base::Value> value(in_value);

  // Update in memory value map.
  OriginIdentifierValueMap* map_to_modify = &incognito_value_map_;
  if (!is_incognito_)
    map_to_modify = &value_map_;

  {
    base::AutoLock auto_lock(lock_);
    if (value.get()) {
      map_to_modify->SetValue(
          primary_pattern,
          secondary_pattern,
          content_type_,
          resource_identifier,
          value->DeepCopy());
    } else {
      map_to_modify->DeleteValue(
          primary_pattern,
          secondary_pattern,
          content_type_,
          resource_identifier);
    }
  }
  // Update the content settings preference.
  if (!is_incognito_) {
    UpdatePref(primary_pattern,
               secondary_pattern,
               resource_identifier,
               value.get());
  }

  notify_callback_.Run(
      primary_pattern, secondary_pattern, content_type_, resource_identifier);

  return true;
}

void ContentSettingsPref::ClearPref() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs_);

  {
    base::AutoLock auto_lock(lock_);
    value_map_.clear();
  }

  {
    base::AutoReset<bool> auto_reset(&updating_preferences_, true);
    DictionaryPrefUpdate update(prefs_, pref_name_);
    base::DictionaryValue* pattern_pairs_settings = update.Get();
    pattern_pairs_settings->Clear();
  }
}

void ContentSettingsPref::ClearAllContentSettingsRules() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs_);

  if (is_incognito_) {
    base::AutoLock auto_lock(lock_);
    incognito_value_map_.clear();
  } else {
    ClearPref();
  }

  notify_callback_.Run(ContentSettingsPattern(),
                       ContentSettingsPattern(),
                       content_type_,
                       ResourceIdentifier());
}

void ContentSettingsPref::UpdateLastUsage(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    base::Clock* clock) {
  // Don't write if in incognito.
  if (is_incognito_) {
    return;
  }

  // Ensure that |lock_| is not held by this thread, since this function will
  // send out notifications (by |~DictionaryPrefUpdate|).
  AssertLockNotHeld();

  base::AutoReset<bool> auto_reset(&updating_preferences_, true);
  {
    DictionaryPrefUpdate update(prefs_, pref_name_);
    base::DictionaryValue* pattern_pairs_settings = update.Get();

    std::string pattern_str(
        CreatePatternString(primary_pattern, secondary_pattern));
    base::DictionaryValue* settings_dictionary = NULL;
    bool found = pattern_pairs_settings->GetDictionaryWithoutPathExpansion(
        pattern_str, &settings_dictionary);

    if (!found) {
      settings_dictionary = new base::DictionaryValue;
      pattern_pairs_settings->SetWithoutPathExpansion(pattern_str,
                                                      settings_dictionary);
    }

    settings_dictionary->SetWithoutPathExpansion(
        kLastUsed, new base::FundamentalValue(clock->Now().ToDoubleT()));
  }
}

base::Time ContentSettingsPref::GetLastUsage(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  const base::DictionaryValue* pattern_pairs_settings =
      prefs_->GetDictionary(pref_name_);
  std::string pattern_str(
      CreatePatternString(primary_pattern, secondary_pattern));

  const base::DictionaryValue* settings_dictionary = NULL;
  bool found = pattern_pairs_settings->GetDictionaryWithoutPathExpansion(
      pattern_str, &settings_dictionary);

  if (!found)
    return base::Time();

  double last_used_time;
  found = settings_dictionary->GetDoubleWithoutPathExpansion(
      kLastUsed, &last_used_time);

  if (!found)
    return base::Time();

  return base::Time::FromDoubleT(last_used_time);
}

size_t ContentSettingsPref::GetNumExceptions() {
  return value_map_.size();
}

bool ContentSettingsPref::TryLockForTesting() const {
  if (!lock_.Try())
    return false;
  lock_.Release();
  return true;
}

void ContentSettingsPref::ReadContentSettingsFromPref() {
  // |DictionaryPrefUpdate| sends out notifications when destructed. This
  // construction order ensures |AutoLock| gets destroyed first and |lock_| is
  // not held when the notifications are sent. Also, |auto_reset| must be still
  // valid when the notifications are sent, so that |Observe| skips the
  // notification.
  base::AutoReset<bool> auto_reset(&updating_preferences_, true);
  DictionaryPrefUpdate update(prefs_, pref_name_);
  base::AutoLock auto_lock(lock_);

  const base::DictionaryValue* all_settings_dictionary =
      prefs_->GetDictionary(pref_name_);

  value_map_.clear();

  // Careful: The returned value could be NULL if the pref has never been set.
  if (!all_settings_dictionary)
    return;

  base::DictionaryValue* mutable_settings;
  std::unique_ptr<base::DictionaryValue> mutable_settings_scope;

  if (!is_incognito_) {
    mutable_settings = update.Get();
  } else {
    // Create copy as we do not want to persist anything in OTR prefs.
    mutable_settings = all_settings_dictionary->DeepCopy();
    mutable_settings_scope.reset(mutable_settings);
  }
  // Convert all Unicode patterns into punycode form, then read.
  CanonicalizeContentSettingsExceptions(mutable_settings);

  size_t cookies_block_exception_count = 0;
  size_t cookies_allow_exception_count = 0;
  size_t cookies_session_only_exception_count = 0;
  for (base::DictionaryValue::Iterator i(*mutable_settings); !i.IsAtEnd();
       i.Advance()) {
    const std::string& pattern_str(i.key());
    std::pair<ContentSettingsPattern, ContentSettingsPattern> pattern_pair =
        ParsePatternString(pattern_str);
    if (!pattern_pair.first.IsValid() ||
        !pattern_pair.second.IsValid()) {
      // TODO: Change this to DFATAL when crbug.com/132659 is fixed.
      LOG(ERROR) << "Invalid pattern strings: " << pattern_str;
      continue;
    }

    // Get settings dictionary for the current pattern string, and read
    // settings from the dictionary.
    const base::DictionaryValue* settings_dictionary = NULL;
    bool is_dictionary = i.value().GetAsDictionary(&settings_dictionary);
    DCHECK(is_dictionary);

    if (SupportsResourceIdentifiers(content_type_)) {
      const base::DictionaryValue* resource_dictionary = NULL;
      if (settings_dictionary->GetDictionary(
              kPerResourceIdentifierPrefName, &resource_dictionary)) {
        for (base::DictionaryValue::Iterator j(*resource_dictionary);
             !j.IsAtEnd();
             j.Advance()) {
          const std::string& resource_identifier(j.key());
          int setting = CONTENT_SETTING_DEFAULT;
          bool is_integer = j.value().GetAsInteger(&setting);
          DCHECK(is_integer);
          DCHECK_NE(CONTENT_SETTING_DEFAULT, setting);
          std::unique_ptr<base::Value> setting_ptr(
              new base::FundamentalValue(setting));
          value_map_.SetValue(pattern_pair.first,
                              pattern_pair.second,
                              content_type_,
                              resource_identifier,
                              setting_ptr->DeepCopy());
        }
      }
    }

    const base::Value* value = nullptr;
    settings_dictionary->GetWithoutPathExpansion(kSettingPath, &value);

    if (value) {
      DCHECK(IsValueAllowedForType(value, content_type_));
      value_map_.SetValue(pattern_pair.first,
                          pattern_pair.second,
                          content_type_,
                          ResourceIdentifier(),
                          value->DeepCopy());
      if (content_type_ == CONTENT_SETTINGS_TYPE_COOKIES) {
        ContentSetting s = ValueToContentSetting(value);
        switch (s) {
          case CONTENT_SETTING_ALLOW :
            ++cookies_allow_exception_count;
            break;
          case CONTENT_SETTING_BLOCK :
            ++cookies_block_exception_count;
            break;
          case CONTENT_SETTING_SESSION_ONLY :
            ++cookies_session_only_exception_count;
            break;
          default:
            NOTREACHED();
            break;
        }
      }
    }

  }

  if (content_type_ == CONTENT_SETTINGS_TYPE_COOKIES) {
    UMA_HISTOGRAM_COUNTS("ContentSettings.NumberOfBlockCookiesExceptions",
                         cookies_block_exception_count);
    UMA_HISTOGRAM_COUNTS("ContentSettings.NumberOfAllowCookiesExceptions",
                         cookies_allow_exception_count);
    UMA_HISTOGRAM_COUNTS("ContentSettings.NumberOfSessionOnlyCookiesExceptions",
                         cookies_session_only_exception_count);
  }
}

void ContentSettingsPref::OnPrefChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (updating_preferences_)
    return;

  ReadContentSettingsFromPref();

  notify_callback_.Run(ContentSettingsPattern(),
                       ContentSettingsPattern(),
                       content_type_,
                       ResourceIdentifier());
}

void ContentSettingsPref::UpdatePref(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    const ResourceIdentifier& resource_identifier,
    const base::Value* value) {
  // Ensure that |lock_| is not held by this thread, since this function will
  // send out notifications (by |~DictionaryPrefUpdate|).
  AssertLockNotHeld();

  base::AutoReset<bool> auto_reset(&updating_preferences_, true);
  {
    DictionaryPrefUpdate update(prefs_, pref_name_);
    base::DictionaryValue* pattern_pairs_settings = update.Get();

    // Get settings dictionary for the given patterns.
    std::string pattern_str(CreatePatternString(primary_pattern,
                                                secondary_pattern));
    base::DictionaryValue* settings_dictionary = NULL;
    bool found = pattern_pairs_settings->GetDictionaryWithoutPathExpansion(
        pattern_str, &settings_dictionary);

    if (!found && value) {
      settings_dictionary = new base::DictionaryValue;
      pattern_pairs_settings->SetWithoutPathExpansion(
          pattern_str, settings_dictionary);
    }

    if (settings_dictionary) {
      if (SupportsResourceIdentifiers(content_type_) &&
          !resource_identifier.empty()) {
        base::DictionaryValue* resource_dictionary = NULL;
        found = settings_dictionary->GetDictionary(
            kPerResourceIdentifierPrefName, &resource_dictionary);
        if (!found) {
          if (value == NULL)
            return;  // Nothing to remove. Exit early.
          resource_dictionary = new base::DictionaryValue;
          settings_dictionary->Set(
              kPerResourceIdentifierPrefName, resource_dictionary);
        }
        // Update resource dictionary.
        if (value == NULL) {
          resource_dictionary->RemoveWithoutPathExpansion(resource_identifier,
                                                          NULL);
          if (resource_dictionary->empty()) {
            settings_dictionary->RemoveWithoutPathExpansion(
                kPerResourceIdentifierPrefName, NULL);
          }
        } else {
          resource_dictionary->SetWithoutPathExpansion(
              resource_identifier, value->DeepCopy());
        }
      } else {
        // Update settings dictionary.
        if (value == NULL) {
          settings_dictionary->RemoveWithoutPathExpansion(kSettingPath, NULL);
          settings_dictionary->RemoveWithoutPathExpansion(kLastUsed, NULL);
        } else {
          settings_dictionary->SetWithoutPathExpansion(
              kSettingPath, value->DeepCopy());
        }
      }
      // Remove the settings dictionary if it is empty.
      if (settings_dictionary->empty()) {
        pattern_pairs_settings->RemoveWithoutPathExpansion(
            pattern_str, NULL);
      }
    }
  }
}

// static
void ContentSettingsPref::CanonicalizeContentSettingsExceptions(
    base::DictionaryValue* all_settings_dictionary) {
  DCHECK(all_settings_dictionary);

  std::vector<std::string> remove_items;
  base::StringPairs move_items;
  for (base::DictionaryValue::Iterator i(*all_settings_dictionary);
       !i.IsAtEnd();
       i.Advance()) {
    const std::string& pattern_str(i.key());
    std::pair<ContentSettingsPattern, ContentSettingsPattern> pattern_pair =
         ParsePatternString(pattern_str);
    if (!pattern_pair.first.IsValid() ||
        !pattern_pair.second.IsValid()) {
      LOG(ERROR) << "Invalid pattern strings: " << pattern_str;
      continue;
    }

    const std::string canonicalized_pattern_str = CreatePatternString(
        pattern_pair.first, pattern_pair.second);

    if (canonicalized_pattern_str.empty() ||
        canonicalized_pattern_str == pattern_str) {
      continue;
    }

    // Clear old pattern if prefs already have canonicalized pattern.
    const base::DictionaryValue* new_pattern_settings_dictionary = NULL;
    if (all_settings_dictionary->GetDictionaryWithoutPathExpansion(
            canonicalized_pattern_str, &new_pattern_settings_dictionary)) {
      remove_items.push_back(pattern_str);
      continue;
    }

    // Move old pattern to canonicalized pattern.
    const base::DictionaryValue* old_pattern_settings_dictionary = NULL;
    if (i.value().GetAsDictionary(&old_pattern_settings_dictionary)) {
      move_items.push_back(
          std::make_pair(pattern_str, canonicalized_pattern_str));
    }
  }

  for (size_t i = 0; i < remove_items.size(); ++i) {
    all_settings_dictionary->RemoveWithoutPathExpansion(remove_items[i], NULL);
  }

  for (size_t i = 0; i < move_items.size(); ++i) {
    std::unique_ptr<base::Value> pattern_settings_dictionary;
    all_settings_dictionary->RemoveWithoutPathExpansion(
        move_items[i].first, &pattern_settings_dictionary);
    all_settings_dictionary->SetWithoutPathExpansion(
        move_items[i].second, pattern_settings_dictionary.release());
  }
}

void ContentSettingsPref::AssertLockNotHeld() const {
#if !defined(NDEBUG)
  // |Lock::Acquire()| will assert if the lock is held by this thread.
  lock_.Acquire();
  lock_.Release();
#endif
}

}  // namespace content_settings
