// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Maps hostnames to custom content settings.  Written on the UI thread and read
// on any thread.  One instance per profile.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_HOST_CONTENT_SETTINGS_MAP_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_HOST_CONTENT_SETTINGS_MAP_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_checker.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class ExtensionService;
class GURL;
class PrefService;

namespace base {
class Clock;
class Value;
}

namespace content_settings {
class ObservableProvider;
class ProviderInterface;
class PrefProvider;
class TestUtils;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class HostContentSettingsMap : public content_settings::Observer,
                               public RefcountedKeyedService {
 public:
  enum ProviderType {
    // EXTENSION names is a layering violation when this class will move to
    // components.
    // TODO(mukai): find the solution.
    INTERNAL_EXTENSION_PROVIDER = 0,
    POLICY_PROVIDER,
    SUPERVISED_PROVIDER,
    CUSTOM_EXTENSION_PROVIDER,
    PREF_PROVIDER,
    DEFAULT_PROVIDER,
    NUM_PROVIDER_TYPES
  };

  // This should be called on the UI thread, otherwise |thread_checker_| handles
  // CalledOnValidThread() wrongly. Only one (or neither) of
  // |is_incognito_profile| and |is_guest_profile| should be true.
  HostContentSettingsMap(PrefService* prefs,
                         bool is_incognito_profile,
                         bool is_guest_profile);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Adds a new provider for |type|.
  void RegisterProvider(
      ProviderType type,
      std::unique_ptr<content_settings::ObservableProvider> provider);

  // Returns the default setting for a particular content type. If |provider_id|
  // is not NULL, the id of the provider which provided the default setting is
  // assigned to it.
  //
  // This may be called on any thread.
  ContentSetting GetDefaultContentSetting(ContentSettingsType content_type,
                                          std::string* provider_id) const;

  // Returns a single |ContentSetting| which applies to the given URLs.  Note
  // that certain internal schemes are whitelisted. For |CONTENT_TYPE_COOKIES|,
  // |CookieSettings| should be used instead. For content types that can't be
  // converted to a |ContentSetting|, |GetContentSettingValue| should be called.
  // If there is no content setting, returns CONTENT_SETTING_DEFAULT.
  //
  // May be called on any thread.
  ContentSetting GetContentSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      const std::string& resource_identifier) const;

  // Returns a single content setting |Value| which applies to the given URLs.
  // If |info| is not NULL, then the |source| field of |info| is set to the
  // source of the returned |Value| (POLICY, EXTENSION, USER, ...) and the
  // |primary_pattern| and the |secondary_pattern| fields of |info| are set to
  // the patterns of the applying rule.  Note that certain internal schemes are
  // whitelisted. For whitelisted schemes the |source| field of |info| is set
  // the |SETTING_SOURCE_WHITELIST| and the |primary_pattern| and
  // |secondary_pattern| are set to a wildcard pattern.  If there is no content
  // setting, NULL is returned and the |source| field of |info| is set to
  // |SETTING_SOURCE_NONE|. The pattern fiels of |info| are set to empty
  // patterns.
  // May be called on any thread.
  std::unique_ptr<base::Value> GetWebsiteSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      const std::string& resource_identifier,
      content_settings::SettingInfo* info) const;

  // For a given content type, returns all patterns with a non-default setting,
  // mapped to their actual settings, in the precedence order of the rules.
  // |settings| must be a non-NULL outparam.
  //
  // This may be called on any thread.
  void GetSettingsForOneType(ContentSettingsType content_type,
                             const std::string& resource_identifier,
                             ContentSettingsForOneType* settings) const;

  // Sets the default setting for a particular content type. This method must
  // not be invoked on an incognito map.
  //
  // This should only be called on the UI thread.
  void SetDefaultContentSetting(ContentSettingsType content_type,
                                ContentSetting setting);

  // Sets the content |setting| for the given patterns, |content_type| and
  // |resource_identifier|. Setting the value to CONTENT_SETTING_DEFAULT causes
  // the default setting for that type to be used when loading pages matching
  // this pattern. Unless adding a custom-scoped setting, most developers will
  // want to use SetContentSettingDefaultScope() instead.
  //
  // NOTICE: This is just a convenience method for content types that use
  // |CONTENT_SETTING| as their data type. For content types that use other
  // data types please use the method SetWebsiteSettingDefaultScope().
  //
  // This should only be called on the UI thread.
  void SetContentSettingCustomScope(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const std::string& resource_identifier,
      ContentSetting setting);

  // Sets the content |setting| for the default scope of the url that is
  // appropriate for the given |content_type| and |resource_identifier|.
  // Setting the value to CONTENT_SETTING_DEFAULT causes the default setting
  // for that type to be used.
  //
  // NOTICE: This is just a convenience method for content types that use
  // |CONTENT_SETTING| as their data type. For content types that use other
  // data types please use the method SetWebsiteSettingDefaultScope().
  //
  // This should only be called on the UI thread.
  //
  // Internally this will call SetContentSettingCustomScope() with the default
  // scope patterns for the given |content_type|. Developers will generally want
  // to use this function instead of SetContentSettingCustomScope() unless they
  // need to specify custom scoping.
  void SetContentSettingDefaultScope(const GURL& primary_url,
                                     const GURL& secondary_url,
                                     ContentSettingsType content_type,
                                     const std::string& resource_identifier,
                                     ContentSetting setting);

  // Sets the |value| for the default scope of the url that is appropriate for
  // the given |content_type| and |resource_identifier|. Setting the value to
  // null removes the default pattern pair for this content type.
  //
  // Internally this will call SetWebsiteSettingCustomScope() with the default
  // scope patterns for the given |content_type|. Developers will generally want
  // to use this function instead of SetWebsiteSettingCustomScope() unless they
  // need to specify custom scoping.
  void SetWebsiteSettingDefaultScope(const GURL& requesting_url,
                                     const GURL& top_level_url,
                                     ContentSettingsType content_type,
                                     const std::string& resource_identifier,
                                     std::unique_ptr<base::Value> value);

  // Sets a rule to apply the |value| for all sites matching |pattern|,
  // |content_type| and |resource_identifier|. Setting the value to null removes
  // the given pattern pair. Unless adding a custom-scoped setting, most
  // developers will want to use SetWebsiteSettingDefaultScope() instead.
  void SetWebsiteSettingCustomScope(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const std::string& resource_identifier,
      std::unique_ptr<base::Value> value);

  // Sets the most specific rule that currently defines the setting for the
  // given content type. TODO(raymes): Remove this once all content settings
  // are scoped to origin scope. There is no scope more narrow than origin
  // scope, so we can just blindly set the value of the origin scope when that
  // happens.
  void SetNarrowestContentSetting(const GURL& primary_url,
                                  const GURL& secondary_url,
                                  ContentSettingsType type,
                                  ContentSetting setting);

  // Clears all host-specific settings for one content type.
  //
  // This should only be called on the UI thread.
  void ClearSettingsForOneType(ContentSettingsType content_type);

  static bool IsDefaultSettingAllowedForType(ContentSetting setting,
                                             ContentSettingsType content_type);

  // RefcountedKeyedService implementation.
  void ShutdownOnUIThread() override;

  // content_settings::Observer implementation.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               std::string resource_identifier) override;

  // Returns the ProviderType associated with the given source string.
  // TODO(estade): I regret adding this. At the moment there are no legitimate
  // uses. We should stick to ProviderType rather than string so we don't have
  // to convert backwards.
  static ProviderType GetProviderTypeFromSource(const std::string& source);

  bool is_off_the_record() const {
    return is_off_the_record_;
  }

  // Returns a single |ContentSetting| which applies to the given URLs, just as
  // |GetContentSetting| does. If the setting is allowed, it also records the
  // last usage to preferences.
  //
  // This should only be called on the UI thread, unlike |GetContentSetting|.
  ContentSetting GetContentSettingAndMaybeUpdateLastUsage(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      const std::string& resource_identifier);

  // Sets the last time that a given content type has been used for the pattern
  // which matches the URLs to the current time.
  void UpdateLastUsage(const GURL& primary_url,
                       const GURL& secondary_url,
                       ContentSettingsType content_type);

  // Sets the last time that a given content type has been used for a pattern
  // pair to the current time.
  void UpdateLastUsageByPattern(const ContentSettingsPattern& primary_pattern,
                                const ContentSettingsPattern& secondary_pattern,
                                ContentSettingsType content_type);

  // Returns the last time the pattern that matches the URL has requested
  // permission for the |content_type| setting.
  base::Time GetLastUsage(const GURL& primary_url,
                          const GURL& secondary_url,
                          ContentSettingsType content_type);

  // Returns the last time the pattern has requested permission for the
  // |content_type| setting.
  base::Time GetLastUsageByPattern(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type);

  // Adds/removes an observer for content settings changes.
  void AddObserver(content_settings::Observer* observer);
  void RemoveObserver(content_settings::Observer* observer);

  // Schedules any pending lossy website settings to be written to disk.
  void FlushLossyWebsiteSettings();

  // Passes ownership of |clock|.
  void SetPrefClockForTesting(std::unique_ptr<base::Clock> clock);

 private:
  friend class base::RefCountedThreadSafe<HostContentSettingsMap>;
  friend class HostContentSettingsMapTest_MigrateKeygenSettings_Test;

  friend class content_settings::TestUtils;

  typedef std::map<ProviderType, content_settings::ProviderInterface*>
      ProviderMap;
  typedef ProviderMap::iterator ProviderIterator;
  typedef ProviderMap::const_iterator ConstProviderIterator;

  ~HostContentSettingsMap() override;

  ContentSetting GetDefaultContentSettingFromProvider(
      ContentSettingsType content_type,
      content_settings::ProviderInterface* provider) const;

  // Retrieves default content setting for |content_type|, and writes the
  // provider's type to |provider_type| (must not be null).
  ContentSetting GetDefaultContentSettingInternal(
      ContentSettingsType content_type,
      ProviderType* provider_type) const;

  // Migrate Keygen settings which only use a primary pattern. Settings which
  // only used a primary pattern were inconsistent in what they did with the
  // secondary pattern. Some stored a ContentSettingsPattern::Wildcard() whereas
  // others stored the same pattern twice. This function migrates all such
  // settings to use ContentSettingsPattern::Wildcard(). This allows us to make
  // the scoping code consistent across different settings.
  // TODO(lshang): Remove this when clients have migrated (~M53). We should
  // leave in some code to remove old-format settings for a long time.
  void MigrateKeygenSettings();

  // Collect UMA data of exceptions.
  void RecordExceptionMetrics();

  // Adds content settings for |content_type| and |resource_identifier|,
  // provided by |provider|, into |settings|. If |incognito| is true, adds only
  // the content settings which are applicable to the incognito mode and differ
  // from the normal mode. Otherwise, adds the content settings for the normal
  // mode.
  void AddSettingsForOneType(
      const content_settings::ProviderInterface* provider,
      ProviderType provider_type,
      ContentSettingsType content_type,
      const std::string& resource_identifier,
      ContentSettingsForOneType* settings,
      bool incognito) const;

  // Call UsedContentSettingsProviders() whenever you access
  // content_settings_providers_ (apart from initialization and
  // teardown), so that we can DCHECK in RegisterExtensionService that
  // it is not being called too late.
  void UsedContentSettingsProviders() const;

  // Returns the single content setting |value| with a toggle for if it
  // takes the global on/off switch into account.
  std::unique_ptr<base::Value> GetWebsiteSettingInternal(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      const std::string& resource_identifier,
      content_settings::SettingInfo* info) const;

  static std::unique_ptr<base::Value> GetContentSettingValueAndPatterns(
      const content_settings::ProviderInterface* provider,
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      const std::string& resource_identifier,
      bool include_incognito,
      ContentSettingsPattern* primary_pattern,
      ContentSettingsPattern* secondary_pattern);

  static std::unique_ptr<base::Value> GetContentSettingValueAndPatterns(
      content_settings::RuleIterator* rule_iterator,
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsPattern* primary_pattern,
      ContentSettingsPattern* secondary_pattern);

#ifndef NDEBUG
  // This starts as the thread ID of the thread that constructs this
  // object, and remains until used by a different thread, at which
  // point it is set to base::kInvalidThreadId. This allows us to
  // DCHECK on unsafe usage of content_settings_providers_ (they
  // should be set up on a single thread, after which they are
  // immutable).
  mutable base::PlatformThreadId used_from_thread_id_;
#endif

  // Weak; owned by the Profile.
  PrefService* prefs_;

  // Whether this settings map is for an OTR session.
  bool is_off_the_record_;

  // Content setting providers. This is only modified at construction
  // time and by RegisterExtensionService, both of which should happen
  // before any other uses of it.
  ProviderMap content_settings_providers_;

  // content_settings_providers_[PREF_PROVIDER] but specialized.
  content_settings::PrefProvider* pref_provider_ = nullptr;

  base::ThreadChecker thread_checker_;

  base::ObserverList<content_settings::Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(HostContentSettingsMap);
};

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_HOST_CONTENT_SETTINGS_MAP_H_
