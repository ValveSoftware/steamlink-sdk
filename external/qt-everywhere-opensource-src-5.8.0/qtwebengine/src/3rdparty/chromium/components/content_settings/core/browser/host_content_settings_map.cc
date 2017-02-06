// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/host_content_settings_map.h"

#include <stddef.h>

#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/clock.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_default_provider.h"
#include "components/content_settings/core/browser/content_settings_details.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/browser/content_settings_policy_provider.h"
#include "components/content_settings/core/browser/content_settings_pref_provider.h"
#include "components/content_settings/core/browser/content_settings_provider.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"
#include "url/gurl.h"

using content_settings::WebsiteSettingsInfo;

namespace {

typedef std::vector<content_settings::Rule> Rules;

typedef std::pair<std::string, std::string> StringPair;

struct ProviderNamesSourceMapEntry {
  const char* provider_name;
  content_settings::SettingSource provider_source;
};

const ProviderNamesSourceMapEntry kProviderNamesSourceMap[] = {
    {"platform_app", content_settings::SETTING_SOURCE_EXTENSION},
    {"policy", content_settings::SETTING_SOURCE_POLICY},
    {"supervised_user", content_settings::SETTING_SOURCE_SUPERVISED},
    {"extension", content_settings::SETTING_SOURCE_EXTENSION},
    {"preference", content_settings::SETTING_SOURCE_USER},
    {"default", content_settings::SETTING_SOURCE_USER},
};

static_assert(
    arraysize(kProviderNamesSourceMap) ==
        HostContentSettingsMap::NUM_PROVIDER_TYPES,
    "kProviderNamesSourceMap should have NUM_PROVIDER_TYPES elements");

// Returns true if the |content_type| supports a resource identifier.
// Resource identifiers are supported (but not required) for plugins.
bool SupportsResourceIdentifier(ContentSettingsType content_type) {
  return content_type == CONTENT_SETTINGS_TYPE_PLUGINS;
}

bool SchemeCanBeWhitelisted(const std::string& scheme) {
  return scheme == content_settings::kChromeDevToolsScheme ||
         scheme == content_settings::kExtensionScheme ||
         scheme == content_settings::kChromeUIScheme;
}

// Handles inheritance of settings from the regular profile into the incognito
// profile.
std::unique_ptr<base::Value> ProcessIncognitoInheritanceBehavior(
    ContentSettingsType content_type,
    std::unique_ptr<base::Value> value) {
  // Website setting inheritance can be completely disallowed.
  const content_settings::WebsiteSettingsInfo* website_settings_info =
      content_settings::WebsiteSettingsRegistry::GetInstance()->Get(
          content_type);
  if (website_settings_info &&
      website_settings_info->incognito_behavior() ==
          content_settings::WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO) {
    return nullptr;
  }

  // Content setting inheritance can be disabled for CONTENT_SETTING_ALLOW.
  const content_settings::ContentSettingsInfo* content_settings_info =
      content_settings::ContentSettingsRegistry::GetInstance()->Get(
          content_type);
  if (content_settings_info) {
    if (content_settings_info->incognito_behavior() !=
        content_settings::ContentSettingsInfo::
            INHERIT_IN_INCOGNITO_EXCEPT_ALLOW)
      return value;
    ContentSetting setting =
        content_settings::ValueToContentSetting(value.get());
    if (setting != CONTENT_SETTING_ALLOW)
      return value;
    DCHECK(content_settings_info->IsSettingValid(CONTENT_SETTING_ASK));
    return content_settings::ContentSettingToValue(CONTENT_SETTING_ASK);
  }

  return value;
}

content_settings::PatternPair GetPatternsFromScopingType(
    WebsiteSettingsInfo::ScopingType scoping_type,
    const GURL& primary_url,
    const GURL& secondary_url) {
  DCHECK(!primary_url.is_empty());
  content_settings::PatternPair patterns;

  switch (scoping_type) {
    case WebsiteSettingsInfo::TOP_LEVEL_DOMAIN_ONLY_SCOPE:
    case WebsiteSettingsInfo::REQUESTING_DOMAIN_ONLY_SCOPE:
      patterns.first = ContentSettingsPattern::FromURL(primary_url);
      patterns.second = ContentSettingsPattern::Wildcard();
      break;
    case WebsiteSettingsInfo::REQUESTING_ORIGIN_ONLY_SCOPE:
      patterns.first = ContentSettingsPattern::FromURLNoWildcard(primary_url);
      patterns.second = ContentSettingsPattern::Wildcard();
      break;
    case WebsiteSettingsInfo::REQUESTING_ORIGIN_AND_TOP_LEVEL_ORIGIN_SCOPE:
      DCHECK(!secondary_url.is_empty());
      patterns.first = ContentSettingsPattern::FromURLNoWildcard(primary_url);
      patterns.second =
          ContentSettingsPattern::FromURLNoWildcard(secondary_url);
      break;
  }
  return patterns;
}

}  // namespace

HostContentSettingsMap::HostContentSettingsMap(PrefService* prefs,
                                               bool is_incognito_profile,
                                               bool is_guest_profile)
    :
#ifndef NDEBUG
      used_from_thread_id_(base::PlatformThread::CurrentId()),
#endif
      prefs_(prefs),
      is_off_the_record_(is_incognito_profile || is_guest_profile) {
  DCHECK(!(is_incognito_profile && is_guest_profile));

  content_settings::PolicyProvider* policy_provider =
      new content_settings::PolicyProvider(prefs_);
  content_settings_providers_[POLICY_PROVIDER] = policy_provider;
  policy_provider->AddObserver(this);

  pref_provider_ =
      new content_settings::PrefProvider(prefs_, is_off_the_record_);
  content_settings_providers_[PREF_PROVIDER] = pref_provider_;
  pref_provider_->AddObserver(this);

  // This ensures that content settings are cleared for the guest profile. This
  // wouldn't be needed except that we used to allow settings to be stored for
  // the guest profile and so we need to ensure those get cleared.
  if (is_guest_profile)
    pref_provider_->ClearPrefs();

  content_settings::ObservableProvider* default_provider =
      new content_settings::DefaultProvider(prefs_, is_off_the_record_);
  default_provider->AddObserver(this);
  content_settings_providers_[DEFAULT_PROVIDER] = default_provider;

  MigrateKeygenSettings();

  RecordExceptionMetrics();
}

// static
void HostContentSettingsMap::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Ensure the content settings are all registered.
  content_settings::ContentSettingsRegistry::GetInstance();

  registry->RegisterIntegerPref(prefs::kContentSettingsWindowLastTabIndex, 0);

  // Register the prefs for the content settings providers.
  content_settings::DefaultProvider::RegisterProfilePrefs(registry);
  content_settings::PrefProvider::RegisterProfilePrefs(registry);
  content_settings::PolicyProvider::RegisterProfilePrefs(registry);
}

void HostContentSettingsMap::RegisterProvider(
    ProviderType type,
    std::unique_ptr<content_settings::ObservableProvider> provider) {
  DCHECK(!content_settings_providers_[type]);
  provider->AddObserver(this);
  content_settings_providers_[type] = provider.release();

#ifndef NDEBUG
  DCHECK_NE(used_from_thread_id_, base::kInvalidThreadId)
      << "Used from multiple threads before initialization complete.";
#endif

  OnContentSettingChanged(ContentSettingsPattern(),
                          ContentSettingsPattern(),
                          CONTENT_SETTINGS_TYPE_DEFAULT,
                          std::string());
}

ContentSetting HostContentSettingsMap::GetDefaultContentSettingFromProvider(
    ContentSettingsType content_type,
    content_settings::ProviderInterface* provider) const {
  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      provider->GetRuleIterator(content_type, std::string(), false));

  ContentSettingsPattern wildcard = ContentSettingsPattern::Wildcard();
  while (rule_iterator->HasNext()) {
    content_settings::Rule rule = rule_iterator->Next();
    if (rule.primary_pattern == wildcard &&
        rule.secondary_pattern == wildcard) {
      return content_settings::ValueToContentSetting(rule.value.get());
    }
  }
  return CONTENT_SETTING_DEFAULT;
}

ContentSetting HostContentSettingsMap::GetDefaultContentSettingInternal(
    ContentSettingsType content_type,
    ProviderType* provider_type) const {
  DCHECK(provider_type);
  UsedContentSettingsProviders();

  // Iterate through the list of providers and return the first non-NULL value
  // that matches |primary_url| and |secondary_url|.
  for (ConstProviderIterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end();
       ++provider) {
    if (provider->first == PREF_PROVIDER)
      continue;
    ContentSetting default_setting =
        GetDefaultContentSettingFromProvider(content_type, provider->second);
    if (is_off_the_record_) {
      default_setting = content_settings::ValueToContentSetting(
          ProcessIncognitoInheritanceBehavior(
              content_type,
              content_settings::ContentSettingToValue(default_setting))
              .get());
    }
    if (default_setting != CONTENT_SETTING_DEFAULT) {
      *provider_type = provider->first;
      return default_setting;
    }
  }

  return CONTENT_SETTING_DEFAULT;
}

ContentSetting HostContentSettingsMap::GetDefaultContentSetting(
    ContentSettingsType content_type,
    std::string* provider_id) const {
  ProviderType provider_type = NUM_PROVIDER_TYPES;
  ContentSetting content_setting =
      GetDefaultContentSettingInternal(content_type, &provider_type);
  if (content_setting != CONTENT_SETTING_DEFAULT && provider_id)
    *provider_id = kProviderNamesSourceMap[provider_type].provider_name;
  return content_setting;
}

ContentSetting HostContentSettingsMap::GetContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier) const {
  DCHECK(content_settings::ContentSettingsRegistry::GetInstance()->Get(
      content_type));
  std::unique_ptr<base::Value> value = GetWebsiteSetting(
      primary_url, secondary_url, content_type, resource_identifier, NULL);
  return content_settings::ValueToContentSetting(value.get());
}

void HostContentSettingsMap::GetSettingsForOneType(
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ContentSettingsForOneType* settings) const {
  DCHECK(SupportsResourceIdentifier(content_type) ||
         resource_identifier.empty());
  DCHECK(settings);
  UsedContentSettingsProviders();

  settings->clear();
  for (ConstProviderIterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end();
       ++provider) {
    // For each provider, iterate first the incognito-specific rules, then the
    // normal rules.
    if (is_off_the_record_) {
      AddSettingsForOneType(provider->second,
                            provider->first,
                            content_type,
                            resource_identifier,
                            settings,
                            true);
    }
    AddSettingsForOneType(provider->second,
                          provider->first,
                          content_type,
                          resource_identifier,
                          settings,
                          false);
  }
}

void HostContentSettingsMap::SetDefaultContentSetting(
    ContentSettingsType content_type,
    ContentSetting setting) {
  std::unique_ptr<base::Value> value;
  // A value of CONTENT_SETTING_DEFAULT implies deleting the content setting.
  if (setting != CONTENT_SETTING_DEFAULT) {
    DCHECK(IsDefaultSettingAllowedForType(setting, content_type));
    value.reset(new base::FundamentalValue(setting));
  }
  SetWebsiteSettingCustomScope(ContentSettingsPattern::Wildcard(),
                               ContentSettingsPattern::Wildcard(), content_type,
                               std::string(), std::move(value));
}

void HostContentSettingsMap::SetWebsiteSettingDefaultScope(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    std::unique_ptr<base::Value> value) {
  const WebsiteSettingsInfo* info =
      content_settings::WebsiteSettingsRegistry::GetInstance()->Get(
          content_type);
  content_settings::PatternPair patterns = GetPatternsFromScopingType(
      info->scoping_type(), primary_url, secondary_url);
  ContentSettingsPattern primary_pattern = patterns.first;
  ContentSettingsPattern secondary_pattern = patterns.second;
  if (!primary_pattern.IsValid() || !secondary_pattern.IsValid())
    return;

  SetWebsiteSettingCustomScope(primary_pattern, secondary_pattern, content_type,
                               resource_identifier, std::move(value));
}

void HostContentSettingsMap::SetWebsiteSettingCustomScope(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    std::unique_ptr<base::Value> value) {
  DCHECK(SupportsResourceIdentifier(content_type) ||
         resource_identifier.empty());
  UsedContentSettingsProviders();

  for (auto& provider_pair : content_settings_providers_) {
    if (provider_pair.second->SetWebsiteSetting(
            primary_pattern, secondary_pattern, content_type,
            resource_identifier, value.get())) {
      // If succesful then ownership is passed to the provider.
      ignore_result(value.release());
      return;
    }
  }
  NOTREACHED();
}

void HostContentSettingsMap::SetNarrowestContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType type,
    ContentSetting setting) {
  // Permission settings are specified via rules. There exists always at least
  // one rule for the default setting. Get the rule that currently defines
  // the permission for the given permission |type|. Then test whether the
  // existing rule is more specific than the rule we are about to create. If
  // the existing rule is more specific, than change the existing rule instead
  // of creating a new rule that would be hidden behind the existing rule.
  content_settings::SettingInfo info;
  std::unique_ptr<base::Value> v = GetWebsiteSettingInternal(
      primary_url, secondary_url, type, std::string(), &info);
  DCHECK_EQ(content_settings::SETTING_SOURCE_USER, info.source);

  const WebsiteSettingsInfo* website_settings_info =
      content_settings::WebsiteSettingsRegistry::GetInstance()->Get(type);
  content_settings::PatternPair patterns = GetPatternsFromScopingType(
      website_settings_info->scoping_type(), primary_url, secondary_url);

  ContentSettingsPattern narrow_primary = patterns.first;
  ContentSettingsPattern narrow_secondary = patterns.second;

  ContentSettingsPattern::Relation r1 =
      info.primary_pattern.Compare(patterns.first);
  if (r1 == ContentSettingsPattern::PREDECESSOR) {
    narrow_primary = info.primary_pattern;
  } else if (r1 == ContentSettingsPattern::IDENTITY) {
    ContentSettingsPattern::Relation r2 =
        info.secondary_pattern.Compare(patterns.second);
    DCHECK(r2 != ContentSettingsPattern::DISJOINT_ORDER_POST &&
           r2 != ContentSettingsPattern::DISJOINT_ORDER_PRE);
    if (r2 == ContentSettingsPattern::PREDECESSOR)
      narrow_secondary = info.secondary_pattern;
  }

  SetContentSettingCustomScope(narrow_primary, narrow_secondary, type,
                               std::string(), setting);
}

void HostContentSettingsMap::SetContentSettingCustomScope(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ContentSetting setting) {
  DCHECK(content_settings::ContentSettingsRegistry::GetInstance()->Get(
      content_type));
  if (setting == CONTENT_SETTING_ALLOW &&
      (content_type == CONTENT_SETTINGS_TYPE_GEOLOCATION ||
       content_type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS)) {
    UpdateLastUsageByPattern(primary_pattern, secondary_pattern, content_type);
  }

  std::unique_ptr<base::Value> value;
  // A value of CONTENT_SETTING_DEFAULT implies deleting the content setting.
  if (setting != CONTENT_SETTING_DEFAULT) {
    DCHECK(content_settings::ContentSettingsRegistry::GetInstance()
               ->Get(content_type)
               ->IsSettingValid(setting));
    value.reset(new base::FundamentalValue(setting));
  }
  SetWebsiteSettingCustomScope(primary_pattern, secondary_pattern, content_type,
                               resource_identifier, std::move(value));
}

void HostContentSettingsMap::SetContentSettingDefaultScope(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ContentSetting setting) {
  using content_settings::ContentSettingsInfo;
  const ContentSettingsInfo* info =
      content_settings::ContentSettingsRegistry::GetInstance()->Get(
          content_type);
  DCHECK(info);

  content_settings::PatternPair patterns =
      GetPatternsFromScopingType(info->website_settings_info()->scoping_type(),
                                 primary_url, secondary_url);
  ContentSettingsPattern primary_pattern = patterns.first;
  ContentSettingsPattern secondary_pattern = patterns.second;
  if (!primary_pattern.IsValid() || !secondary_pattern.IsValid())
    return;

  SetContentSettingCustomScope(primary_pattern, secondary_pattern, content_type,
                               resource_identifier, setting);
}

void HostContentSettingsMap::MigrateKeygenSettings() {
  const content_settings::ContentSettingsInfo* info =
      content_settings::ContentSettingsRegistry::GetInstance()->Get(
          CONTENT_SETTINGS_TYPE_KEYGEN);
  if (info) {
    ContentSettingsForOneType settings;
    GetSettingsForOneType(CONTENT_SETTINGS_TYPE_KEYGEN, std::string(),
                          &settings);

    for (const ContentSettingPatternSource& setting_entry : settings) {
      // Migrate user preference settings only.
      if (setting_entry.source != "preference")
        continue;
      // Migrate old-format settings only.
      if (setting_entry.secondary_pattern !=
          ContentSettingsPattern::Wildcard()) {
        GURL url(setting_entry.primary_pattern.ToString());
        // Pull out the value of the old-format setting. Only do this if the
        // patterns are as we expect them to be, otherwise the setting will just
        // be removed for safety.
        ContentSetting content_setting = CONTENT_SETTING_DEFAULT;
        if (setting_entry.primary_pattern == setting_entry.secondary_pattern &&
            url.is_valid()) {
          content_setting = GetContentSetting(
              url, url, CONTENT_SETTINGS_TYPE_KEYGEN, std::string());
        }
        // Remove the old pattern.
        SetContentSettingCustomScope(setting_entry.primary_pattern,
                                     setting_entry.secondary_pattern,
                                     CONTENT_SETTINGS_TYPE_KEYGEN,
                                     std::string(), CONTENT_SETTING_DEFAULT);
        // Set the new pattern.
        SetContentSettingDefaultScope(url, GURL(), CONTENT_SETTINGS_TYPE_KEYGEN,
                                      std::string(), content_setting);
      }
    }
  }
}

void HostContentSettingsMap::RecordExceptionMetrics() {
  for (const content_settings::WebsiteSettingsInfo* info :
       *content_settings::WebsiteSettingsRegistry::GetInstance()) {
    ContentSettingsType content_type = info->type();
    const std::string type_name = info->name();

    ContentSettingsForOneType settings;
    GetSettingsForOneType(content_type, std::string(), &settings);
    size_t num_exceptions = 0;
    for (const ContentSettingPatternSource& setting_entry : settings) {
      // Skip default settings.
      if (setting_entry.primary_pattern == ContentSettingsPattern::Wildcard() &&
          setting_entry.secondary_pattern ==
              ContentSettingsPattern::Wildcard()) {
        continue;
      }

      UMA_HISTOGRAM_ENUMERATION("ContentSettings.ExceptionScheme",
                                setting_entry.primary_pattern.GetScheme(),
                                ContentSettingsPattern::SCHEME_MAX);

      if (setting_entry.source == "preference")
        ++num_exceptions;
    }

    std::string histogram_name =
        "ContentSettings.Exceptions." + type_name;

    base::HistogramBase* histogram_pointer = base::Histogram::FactoryGet(
        histogram_name, 1, 1000, 30,
        base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram_pointer->Add(num_exceptions);
  }
}

ContentSetting HostContentSettingsMap::GetContentSettingAndMaybeUpdateLastUsage(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ContentSetting setting = GetContentSetting(
      primary_url, secondary_url, content_type, resource_identifier);
  if (setting == CONTENT_SETTING_ALLOW) {
    UpdateLastUsageByPattern(
        ContentSettingsPattern::FromURLNoWildcard(primary_url),
        ContentSettingsPattern::FromURLNoWildcard(secondary_url),
        content_type);
  }
  return setting;
}

void HostContentSettingsMap::UpdateLastUsage(const GURL& primary_url,
                                             const GURL& secondary_url,
                                             ContentSettingsType content_type) {
  UpdateLastUsageByPattern(
      ContentSettingsPattern::FromURLNoWildcard(primary_url),
      ContentSettingsPattern::FromURLNoWildcard(secondary_url),
      content_type);
}

void HostContentSettingsMap::UpdateLastUsageByPattern(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  UsedContentSettingsProviders();

  pref_provider_->UpdateLastUsage(
      primary_pattern, secondary_pattern, content_type);
}

base::Time HostContentSettingsMap::GetLastUsage(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type) {
  return GetLastUsageByPattern(
      ContentSettingsPattern::FromURLNoWildcard(primary_url),
      ContentSettingsPattern::FromURLNoWildcard(secondary_url),
      content_type);
}

base::Time HostContentSettingsMap::GetLastUsageByPattern(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  UsedContentSettingsProviders();

  return pref_provider_->GetLastUsage(
      primary_pattern, secondary_pattern, content_type);
}

void HostContentSettingsMap::AddObserver(content_settings::Observer* observer) {
  observers_.AddObserver(observer);
}

void HostContentSettingsMap::RemoveObserver(
    content_settings::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void HostContentSettingsMap::FlushLossyWebsiteSettings() {
  prefs_->SchedulePendingLossyWrites();
}

void HostContentSettingsMap::SetPrefClockForTesting(
    std::unique_ptr<base::Clock> clock) {
  UsedContentSettingsProviders();

  pref_provider_->SetClockForTesting(std::move(clock));
}

void HostContentSettingsMap::ClearSettingsForOneType(
    ContentSettingsType content_type) {
  UsedContentSettingsProviders();
  for (ProviderIterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end();
       ++provider) {
    provider->second->ClearAllContentSettingsRules(content_type);
  }
  FlushLossyWebsiteSettings();
}

// TODO(raymes): Remove this function. Consider making it a property of
// ContentSettingsInfo or removing it altogether (it's unclear whether we should
// be restricting allowed default values at this layer).
// static
bool HostContentSettingsMap::IsDefaultSettingAllowedForType(
    ContentSetting setting,
    ContentSettingsType content_type) {
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  // Don't support ALLOW for protected media default setting until migration.
  if (content_type == CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER &&
      setting == CONTENT_SETTING_ALLOW) {
    return false;
  }
#endif

  // Don't support ALLOW for the default media settings.
  if ((content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA ||
       content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) &&
      setting == CONTENT_SETTING_ALLOW) {
    return false;
  }

  const content_settings::ContentSettingsInfo* info =
      content_settings::ContentSettingsRegistry::GetInstance()->Get(
          content_type);
  DCHECK(info);
  return info->IsSettingValid(setting);
}

void HostContentSettingsMap::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    std::string resource_identifier) {
  FOR_EACH_OBSERVER(content_settings::Observer,
                    observers_,
                    OnContentSettingChanged(primary_pattern,
                                            secondary_pattern,
                                            content_type,
                                            resource_identifier));
}

HostContentSettingsMap::~HostContentSettingsMap() {
  DCHECK(!prefs_);
  STLDeleteValues(&content_settings_providers_);
}

void HostContentSettingsMap::ShutdownOnUIThread() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs_);
  prefs_ = NULL;
  for (ProviderIterator it = content_settings_providers_.begin();
       it != content_settings_providers_.end();
       ++it) {
    it->second->ShutdownOnUIThread();
  }
}

void HostContentSettingsMap::AddSettingsForOneType(
    const content_settings::ProviderInterface* provider,
    ProviderType provider_type,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ContentSettingsForOneType* settings,
    bool incognito) const {
  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      provider->GetRuleIterator(content_type, resource_identifier, incognito));
  while (rule_iterator->HasNext()) {
    const content_settings::Rule& rule = rule_iterator->Next();
    ContentSetting setting_value = CONTENT_SETTING_DEFAULT;
    // TODO(bauerb): Return rules as a list of values, not content settings.
    // Handle the case using base::Values for its exceptions and default
    // setting. Here we assume all the exceptions are granted as
    // |CONTENT_SETTING_ALLOW|.
    if (!content_settings::ContentSettingsRegistry::GetInstance()->Get(
            content_type) &&
        rule.value.get() &&
        rule.primary_pattern != ContentSettingsPattern::Wildcard()) {
      setting_value = CONTENT_SETTING_ALLOW;
    } else {
      setting_value = content_settings::ValueToContentSetting(rule.value.get());
    }
    settings->push_back(ContentSettingPatternSource(
        rule.primary_pattern, rule.secondary_pattern, setting_value,
        kProviderNamesSourceMap[provider_type].provider_name, incognito));
  }
}

void HostContentSettingsMap::UsedContentSettingsProviders() const {
#ifndef NDEBUG
  if (used_from_thread_id_ == base::kInvalidThreadId)
    return;

  if (base::PlatformThread::CurrentId() != used_from_thread_id_)
    used_from_thread_id_ = base::kInvalidThreadId;
#endif
}

std::unique_ptr<base::Value> HostContentSettingsMap::GetWebsiteSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    content_settings::SettingInfo* info) const {
  DCHECK(SupportsResourceIdentifier(content_type) ||
         resource_identifier.empty());

  // Check if the requested setting is whitelisted.
  // TODO(raymes): Move this into GetContentSetting. This has nothing to do with
  // website settings
  const content_settings::ContentSettingsInfo* content_settings_info =
      content_settings::ContentSettingsRegistry::GetInstance()->Get(
          content_type);
  if (content_settings_info) {
    for (const std::string& scheme :
         content_settings_info->whitelisted_schemes()) {
      DCHECK(SchemeCanBeWhitelisted(scheme));

      if (primary_url.SchemeIs(scheme.c_str())) {
        if (info) {
          info->source = content_settings::SETTING_SOURCE_WHITELIST;
          info->primary_pattern = ContentSettingsPattern::Wildcard();
          info->secondary_pattern = ContentSettingsPattern::Wildcard();
        }
        return std::unique_ptr<base::Value>(
            new base::FundamentalValue(CONTENT_SETTING_ALLOW));
      }
    }
  }

  return GetWebsiteSettingInternal(primary_url,
                                   secondary_url,
                                   content_type,
                                   resource_identifier,
                                   info);
}

// static
HostContentSettingsMap::ProviderType
HostContentSettingsMap::GetProviderTypeFromSource(const std::string& source) {
  for (size_t i = 0; i < arraysize(kProviderNamesSourceMap); ++i) {
    if (source == kProviderNamesSourceMap[i].provider_name)
      return static_cast<ProviderType>(i);
  }

  NOTREACHED();
  return DEFAULT_PROVIDER;
}

std::unique_ptr<base::Value> HostContentSettingsMap::GetWebsiteSettingInternal(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    content_settings::SettingInfo* info) const {
  UsedContentSettingsProviders();
  ContentSettingsPattern* primary_pattern = NULL;
  ContentSettingsPattern* secondary_pattern = NULL;
  if (info) {
    primary_pattern = &info->primary_pattern;
    secondary_pattern = &info->secondary_pattern;
  }

  // The list of |content_settings_providers_| is ordered according to their
  // precedence.
  for (ConstProviderIterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end();
       ++provider) {
    std::unique_ptr<base::Value> value = GetContentSettingValueAndPatterns(
        provider->second, primary_url, secondary_url, content_type,
        resource_identifier, is_off_the_record_, primary_pattern,
        secondary_pattern);
    if (value) {
      if (info)
        info->source = kProviderNamesSourceMap[provider->first].provider_source;
      return value;
    }
  }

  if (info) {
    info->source = content_settings::SETTING_SOURCE_NONE;
    info->primary_pattern = ContentSettingsPattern();
    info->secondary_pattern = ContentSettingsPattern();
  }
  return std::unique_ptr<base::Value>();
}

// static
std::unique_ptr<base::Value>
HostContentSettingsMap::GetContentSettingValueAndPatterns(
    const content_settings::ProviderInterface* provider,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    bool include_incognito,
    ContentSettingsPattern* primary_pattern,
    ContentSettingsPattern* secondary_pattern) {
  if (include_incognito) {
    // Check incognito-only specific settings. It's essential that the
    // |RuleIterator| gets out of scope before we get a rule iterator for the
    // normal mode.
    std::unique_ptr<content_settings::RuleIterator> incognito_rule_iterator(
        provider->GetRuleIterator(content_type, resource_identifier,
                                  true /* incognito */));
    std::unique_ptr<base::Value> value = GetContentSettingValueAndPatterns(
        incognito_rule_iterator.get(), primary_url, secondary_url,
        primary_pattern, secondary_pattern);
    if (value)
      return value;
  }
  // No settings from the incognito; use the normal mode.
  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      provider->GetRuleIterator(content_type, resource_identifier,
                                false /* incognito */));
  std::unique_ptr<base::Value> value = GetContentSettingValueAndPatterns(
      rule_iterator.get(), primary_url, secondary_url, primary_pattern,
      secondary_pattern);
  if (value && include_incognito)
    value = ProcessIncognitoInheritanceBehavior(content_type, std::move(value));
  return value;
}

// static
std::unique_ptr<base::Value>
HostContentSettingsMap::GetContentSettingValueAndPatterns(
    content_settings::RuleIterator* rule_iterator,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsPattern* primary_pattern,
    ContentSettingsPattern* secondary_pattern) {
  while (rule_iterator->HasNext()) {
    const content_settings::Rule& rule = rule_iterator->Next();
    if (rule.primary_pattern.Matches(primary_url) &&
        rule.secondary_pattern.Matches(secondary_url)) {
      if (primary_pattern)
        *primary_pattern = rule.primary_pattern;
      if (secondary_pattern)
        *secondary_pattern = rule.secondary_pattern;
      return base::WrapUnique(rule.value.get()->DeepCopy());
    }
  }
  return std::unique_ptr<base::Value>();
}
