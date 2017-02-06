// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/cookie_settings.h"

#include "base/bind.h"
#include "base/logging.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"
#include "url/gurl.h"

namespace {

bool IsValidSetting(ContentSetting setting) {
  return (setting == CONTENT_SETTING_ALLOW ||
          setting == CONTENT_SETTING_SESSION_ONLY ||
          setting == CONTENT_SETTING_BLOCK);
}

bool IsAllowed(ContentSetting setting) {
  DCHECK(IsValidSetting(setting));
  return (setting == CONTENT_SETTING_ALLOW ||
          setting == CONTENT_SETTING_SESSION_ONLY);
}

}  // namespace

namespace content_settings {

CookieSettings::CookieSettings(
    HostContentSettingsMap* host_content_settings_map,
    PrefService* prefs,
    const char* extension_scheme)
    : host_content_settings_map_(host_content_settings_map),
      extension_scheme_(extension_scheme),
      block_third_party_cookies_(
          prefs->GetBoolean(prefs::kBlockThirdPartyCookies)) {
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kBlockThirdPartyCookies,
      base::Bind(&CookieSettings::OnBlockThirdPartyCookiesChanged,
                 base::Unretained(this)));
}

ContentSetting CookieSettings::GetDefaultCookieSetting(
    std::string* provider_id) const {
  return host_content_settings_map_->GetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, provider_id);
}

bool CookieSettings::IsReadingCookieAllowed(const GURL& url,
                                            const GURL& first_party_url) const {
  ContentSetting setting = GetCookieSetting(url, first_party_url, false, NULL);
  return IsAllowed(setting);
}

bool CookieSettings::IsSettingCookieAllowed(const GURL& url,
                                            const GURL& first_party_url) const {
  ContentSetting setting = GetCookieSetting(url, first_party_url, true, NULL);
  return IsAllowed(setting);
}

bool CookieSettings::IsCookieSessionOnly(const GURL& origin) const {
  ContentSetting setting = GetCookieSetting(origin, origin, true, NULL);
  DCHECK(IsValidSetting(setting));
  return (setting == CONTENT_SETTING_SESSION_ONLY);
}

void CookieSettings::GetCookieSettings(
    ContentSettingsForOneType* settings) const {
  host_content_settings_map_->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), settings);
}

void CookieSettings::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kBlockThirdPartyCookies, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void CookieSettings::SetDefaultCookieSetting(ContentSetting setting) {
  DCHECK(IsValidSetting(setting));
  host_content_settings_map_->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, setting);
}

void CookieSettings::SetCookieSetting(const GURL& primary_url,
                                      ContentSetting setting) {
  DCHECK(IsValidSetting(setting));
  host_content_settings_map_->SetContentSettingDefaultScope(
      primary_url, GURL(), CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
      setting);
}

void CookieSettings::ResetCookieSetting(const GURL& primary_url) {
  host_content_settings_map_->SetNarrowestContentSetting(
      primary_url, GURL(), CONTENT_SETTINGS_TYPE_COOKIES,
      CONTENT_SETTING_DEFAULT);
}

bool CookieSettings::IsStorageDurable(const GURL& origin) const {
  // TODO(dgrogan): Don't use host_content_settings_map_ directly.
  // https://crbug.com/539538
  ContentSetting setting = host_content_settings_map_->GetContentSetting(
      origin /*primary*/, origin /*secondary*/,
      CONTENT_SETTINGS_TYPE_DURABLE_STORAGE,
      std::string() /*resource_identifier*/);
  return setting == CONTENT_SETTING_ALLOW;
}

void CookieSettings::ShutdownOnUIThread() {
  DCHECK(thread_checker_.CalledOnValidThread());
  pref_change_registrar_.RemoveAll();
}

ContentSetting CookieSettings::GetCookieSetting(const GURL& url,
                                                const GURL& first_party_url,
                                                bool setting_cookie,
                                                SettingSource* source) const {
  // Auto-allow in extensions or for WebUI embedded in a secure origin.
  if (url.SchemeIsCryptographic() && first_party_url.SchemeIs(kChromeUIScheme))
    return CONTENT_SETTING_ALLOW;

#if defined(ENABLE_EXTENSIONS)
  if (url.SchemeIs(kExtensionScheme) &&
      first_party_url.SchemeIs(kExtensionScheme)) {
    return CONTENT_SETTING_ALLOW;
  }
#endif

  // First get any host-specific settings.
  SettingInfo info;
  std::unique_ptr<base::Value> value =
      host_content_settings_map_->GetWebsiteSetting(
          url, first_party_url, CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
          &info);
  if (source)
    *source = info.source;

  // If no explicit exception has been made and third-party cookies are blocked
  // by default, apply that rule.
  if (info.primary_pattern.MatchesAllHosts() &&
      info.secondary_pattern.MatchesAllHosts() &&
      ShouldBlockThirdPartyCookies() &&
      !first_party_url.SchemeIs(extension_scheme_)) {
    net::StaticCookiePolicy policy(
        net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES);
    int rv;
    if (setting_cookie)
      rv = policy.CanSetCookie(url, first_party_url);
    else
      rv = policy.CanGetCookies(url, first_party_url);
    DCHECK_NE(net::ERR_IO_PENDING, rv);
    if (rv != net::OK)
      return CONTENT_SETTING_BLOCK;
  }

  // We should always have a value, at least from the default provider.
  DCHECK(value.get());
  return ValueToContentSetting(value.get());
}

CookieSettings::~CookieSettings() {
}

void CookieSettings::OnBlockThirdPartyCookiesChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::AutoLock auto_lock(lock_);
  block_third_party_cookies_ = pref_change_registrar_.prefs()->GetBoolean(
      prefs::kBlockThirdPartyCookies);
}

bool CookieSettings::ShouldBlockThirdPartyCookies() const {
  base::AutoLock auto_lock(lock_);
  return block_third_party_cookies_;
}

}  // namespace content_settings
