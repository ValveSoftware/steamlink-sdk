// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/cookie_settings.h"

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content_settings {

namespace {

class CookieSettingsTest : public testing::Test {
 public:
  CookieSettingsTest()
      : kBlockedSite("http://ads.thirdparty.com"),
        kAllowedSite("http://good.allays.com"),
        kFirstPartySite("http://cool.things.com"),
        kChromeURL("chrome://foo"),
        kExtensionURL("chrome-extension://deadbeef"),
        kHttpSite("http://example.com"),
        kHttpsSite("https://example.com"),
        kAllHttpsSitesPattern(ContentSettingsPattern::FromString("https://*")) {
    CookieSettings::RegisterProfilePrefs(prefs_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* incognito_profile */, false /* guest_profile */);
    cookie_settings_ =
        new CookieSettings(settings_map_.get(), &prefs_, "chrome-extension");
  }

  ~CookieSettingsTest() override { settings_map_->ShutdownOnUIThread(); }

 protected:
  user_prefs::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
  scoped_refptr<CookieSettings> cookie_settings_;
  const GURL kBlockedSite;
  const GURL kAllowedSite;
  const GURL kFirstPartySite;
  const GURL kChromeURL;
  const GURL kExtensionURL;
  const GURL kHttpSite;
  const GURL kHttpsSite;
  ContentSettingsPattern kAllHttpsSitesPattern;
};

TEST_F(CookieSettingsTest, TestWhitelistedScheme) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(cookie_settings_->IsReadingCookieAllowed(kHttpSite, kChromeURL));
  EXPECT_TRUE(cookie_settings_->IsReadingCookieAllowed(kHttpsSite, kChromeURL));
  EXPECT_TRUE(cookie_settings_->IsReadingCookieAllowed(kChromeURL, kHttpSite));
#if defined(ENABLE_EXTENSIONS)
  EXPECT_TRUE(
      cookie_settings_->IsReadingCookieAllowed(kExtensionURL, kExtensionURL));
#else
  EXPECT_FALSE(
      cookie_settings_->IsReadingCookieAllowed(kExtensionURL, kExtensionURL));
#endif
  EXPECT_FALSE(
      cookie_settings_->IsReadingCookieAllowed(kExtensionURL, kHttpSite));
}

TEST_F(CookieSettingsTest, CookiesBlockSingle) {
  cookie_settings_->SetCookieSetting(kBlockedSite, CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(
      cookie_settings_->IsReadingCookieAllowed(kBlockedSite, kBlockedSite));
}

TEST_F(CookieSettingsTest, CookiesBlockThirdParty) {
  prefs_.SetBoolean(prefs::kBlockThirdPartyCookies, true);
  EXPECT_FALSE(
      cookie_settings_->IsReadingCookieAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
  EXPECT_FALSE(
      cookie_settings_->IsSettingCookieAllowed(kBlockedSite, kFirstPartySite));
}

TEST_F(CookieSettingsTest, CookiesAllowThirdParty) {
  EXPECT_TRUE(
      cookie_settings_->IsReadingCookieAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(
      cookie_settings_->IsSettingCookieAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
}

TEST_F(CookieSettingsTest, CookiesExplicitBlockSingleThirdParty) {
  cookie_settings_->SetCookieSetting(kBlockedSite, CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(
      cookie_settings_->IsReadingCookieAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_FALSE(
      cookie_settings_->IsSettingCookieAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(
      cookie_settings_->IsSettingCookieAllowed(kAllowedSite, kFirstPartySite));
}

TEST_F(CookieSettingsTest, CookiesExplicitSessionOnly) {
  cookie_settings_->SetCookieSetting(kBlockedSite,
                                     CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(
      cookie_settings_->IsReadingCookieAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(
      cookie_settings_->IsSettingCookieAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));

  prefs_.SetBoolean(prefs::kBlockThirdPartyCookies, true);
  EXPECT_TRUE(
      cookie_settings_->IsReadingCookieAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(
      cookie_settings_->IsSettingCookieAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
}

TEST_F(CookieSettingsTest, CookiesThirdPartyBlockedExplicitAllow) {
  cookie_settings_->SetCookieSetting(kAllowedSite, CONTENT_SETTING_ALLOW);
  prefs_.SetBoolean(prefs::kBlockThirdPartyCookies, true);
  EXPECT_TRUE(
      cookie_settings_->IsReadingCookieAllowed(kAllowedSite, kFirstPartySite));
  EXPECT_TRUE(
      cookie_settings_->IsSettingCookieAllowed(kAllowedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));

  // Extensions should always be allowed to use cookies.
  EXPECT_TRUE(
      cookie_settings_->IsReadingCookieAllowed(kAllowedSite, kExtensionURL));
  EXPECT_TRUE(
      cookie_settings_->IsSettingCookieAllowed(kAllowedSite, kExtensionURL));
}

TEST_F(CookieSettingsTest, CookiesThirdPartyBlockedAllSitesAllowed) {
  cookie_settings_->SetCookieSetting(kAllowedSite, CONTENT_SETTING_ALLOW);
  prefs_.SetBoolean(prefs::kBlockThirdPartyCookies, true);
  // As an example for a url that matches all hosts but not all origins,
  // match all HTTPS sites.
  settings_map_->SetContentSettingCustomScope(
      kAllHttpsSitesPattern, ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), CONTENT_SETTING_ALLOW);
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);

  // |kAllowedSite| should be allowed.
  EXPECT_TRUE(
      cookie_settings_->IsReadingCookieAllowed(kAllowedSite, kBlockedSite));
  EXPECT_TRUE(
      cookie_settings_->IsSettingCookieAllowed(kAllowedSite, kBlockedSite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));

  // HTTPS sites should be allowed in a first-party context.
  EXPECT_TRUE(cookie_settings_->IsReadingCookieAllowed(kHttpsSite, kHttpsSite));
  EXPECT_TRUE(cookie_settings_->IsSettingCookieAllowed(kHttpsSite, kHttpsSite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));

  // HTTP sites should be allowed, but session-only.
  EXPECT_TRUE(cookie_settings_->IsReadingCookieAllowed(kFirstPartySite,
                                                       kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsSettingCookieAllowed(kFirstPartySite,
                                                       kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kFirstPartySite));

  // Third-party cookies should be blocked.
  EXPECT_FALSE(
      cookie_settings_->IsReadingCookieAllowed(kFirstPartySite, kBlockedSite));
  EXPECT_FALSE(
      cookie_settings_->IsSettingCookieAllowed(kFirstPartySite, kBlockedSite));
  EXPECT_FALSE(
      cookie_settings_->IsReadingCookieAllowed(kHttpsSite, kBlockedSite));
  EXPECT_FALSE(
      cookie_settings_->IsSettingCookieAllowed(kHttpsSite, kBlockedSite));
}

TEST_F(CookieSettingsTest, CookiesBlockEverything) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  EXPECT_FALSE(cookie_settings_->IsReadingCookieAllowed(kFirstPartySite,
                                                        kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsSettingCookieAllowed(kFirstPartySite,
                                                        kFirstPartySite));
  EXPECT_FALSE(
      cookie_settings_->IsSettingCookieAllowed(kAllowedSite, kFirstPartySite));
}

TEST_F(CookieSettingsTest, CookiesBlockEverythingExceptAllowed) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  cookie_settings_->SetCookieSetting(kAllowedSite, CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(cookie_settings_->IsReadingCookieAllowed(kFirstPartySite,
                                                        kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsSettingCookieAllowed(kFirstPartySite,
                                                        kFirstPartySite));
  EXPECT_TRUE(
      cookie_settings_->IsReadingCookieAllowed(kAllowedSite, kFirstPartySite));
  EXPECT_TRUE(
      cookie_settings_->IsSettingCookieAllowed(kAllowedSite, kFirstPartySite));
  EXPECT_TRUE(
      cookie_settings_->IsReadingCookieAllowed(kAllowedSite, kAllowedSite));
  EXPECT_TRUE(
      cookie_settings_->IsSettingCookieAllowed(kAllowedSite, kAllowedSite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));
}

TEST_F(CookieSettingsTest, ExtensionsRegularSettings) {
  cookie_settings_->SetCookieSetting(kBlockedSite, CONTENT_SETTING_BLOCK);

  // Regular cookie settings also apply to extensions.
  EXPECT_FALSE(
      cookie_settings_->IsReadingCookieAllowed(kBlockedSite, kExtensionURL));
}

TEST_F(CookieSettingsTest, ExtensionsOwnCookies) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

#if defined(ENABLE_EXTENSIONS)
  // Extensions can always use cookies (and site data) in their own origin.
  EXPECT_TRUE(
      cookie_settings_->IsReadingCookieAllowed(kExtensionURL, kExtensionURL));
#else
  // Except if extensions are disabled. Then the extension-specific checks do
  // not exist and the default setting is to block.
  EXPECT_FALSE(
      cookie_settings_->IsReadingCookieAllowed(kExtensionURL, kExtensionURL));
#endif
}

TEST_F(CookieSettingsTest, ExtensionsThirdParty) {
  prefs_.SetBoolean(prefs::kBlockThirdPartyCookies, true);

  // XHRs stemming from extensions are exempt from third-party cookie blocking
  // rules (as the first party is always the extension's security origin).
  EXPECT_TRUE(
      cookie_settings_->IsSettingCookieAllowed(kBlockedSite, kExtensionURL));
}

}  // namespace

}  // namespace content_settings
