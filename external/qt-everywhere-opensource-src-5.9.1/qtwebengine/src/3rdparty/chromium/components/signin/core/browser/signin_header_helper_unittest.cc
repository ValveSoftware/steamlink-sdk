// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/common/signin_switches.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class SigninHeaderHelperTest : public testing::Test {
 protected:
  void SetUp() override {
    content_settings::CookieSettings::RegisterProfilePrefs(prefs_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());

    settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* incognito_profile */, false /* guest_profile */);
    cookie_settings_ =
        new content_settings::CookieSettings(settings_map_.get(), &prefs_, "");
  }

  void TearDown() override { settings_map_->ShutdownOnUIThread(); }

  void CheckMirrorCookieRequest(const GURL& url,
                                const std::string& account_id,
                                const std::string& expected_request) {
    EXPECT_EQ(signin::BuildMirrorRequestCookieIfPossible(
                  url, account_id, cookie_settings_.get(),
                  signin::PROFILE_MODE_DEFAULT),
              expected_request);
  }

  void CheckMirrorHeaderRequest(const GURL& url,
                                const std::string& account_id,
                                const std::string& expected_request) {
    bool expected_result = !expected_request.empty();
    std::unique_ptr<net::URLRequest> url_request =
        url_request_context_.CreateRequest(url, net::DEFAULT_PRIORITY, nullptr);
    EXPECT_EQ(signin::AppendOrRemoveMirrorRequestHeaderIfPossible(
                  url_request.get(), GURL(), account_id, cookie_settings_.get(),
                  signin::PROFILE_MODE_DEFAULT),
              expected_result);
    std::string request;
    EXPECT_EQ(url_request->extra_request_headers().GetHeader(
                  signin::kChromeConnectedHeader, &request),
              expected_result);
    if (expected_result) {
      EXPECT_EQ(expected_request, request);
    }
  }

  base::MessageLoop loop_;

  user_prefs::TestingPrefServiceSyncable prefs_;
  net::TestURLRequestContext url_request_context_;

  scoped_refptr<HostContentSettingsMap> settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
};

// Tests that no Mirror request is returned when the user is not signed in (no
// account id).
TEST_F(SigninHeaderHelperTest, TestNoMirrorRequestNoAccountId) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAccountConsistency);
  CheckMirrorHeaderRequest(GURL("https://docs.google.com"), "", "");
  CheckMirrorCookieRequest(GURL("https://docs.google.com"), "", "");
}

// Tests that no Mirror request is returned when the cookies aren't allowed to
// be set.
TEST_F(SigninHeaderHelperTest, TestNoMirrorRequestCookieSettingBlocked) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAccountConsistency);
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  CheckMirrorHeaderRequest(GURL("https://docs.google.com"), "0123456789", "");
  CheckMirrorCookieRequest(GURL("https://docs.google.com"), "0123456789", "");
}

// Tests that no Mirror request is returned when the target is a non-Google URL.
TEST_F(SigninHeaderHelperTest, TestNoMirrorRequestExternalURL) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAccountConsistency);
  CheckMirrorHeaderRequest(GURL("https://foo.com"), "0123456789", "");
  CheckMirrorCookieRequest(GURL("https://foo.com"), "0123456789", "");
}

// Tests that the Mirror request is returned without the GAIA Id when the target
// is a google TLD domain.
TEST_F(SigninHeaderHelperTest, TestMirrorRequestGoogleTLD) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAccountConsistency);
  CheckMirrorHeaderRequest(GURL("https://google.fr"), "0123456789",
                           "mode=0,enable_account_consistency=true");
  CheckMirrorCookieRequest(GURL("https://google.de"), "0123456789",
                           "mode=0:enable_account_consistency=true");
}

// Tests that the Mirror request is returned when the target is the domain
// google.com, and that the GAIA Id is only attached for the cookie.
TEST_F(SigninHeaderHelperTest, TestMirrorRequestGoogleCom) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAccountConsistency);
  CheckMirrorHeaderRequest(GURL("https://www.google.com"), "0123456789",
                           "mode=0,enable_account_consistency=true");
  CheckMirrorCookieRequest(
      GURL("https://www.google.com"), "0123456789",
      "id=0123456789:mode=0:enable_account_consistency=true");
}

// Tests that the Mirror request is returned with the GAIA Id on Drive origin,
// even if account consistency is disabled.
TEST_F(SigninHeaderHelperTest, TestMirrorRequestDrive) {
  DCHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableAccountConsistency));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableAccountConsistency);
  CheckMirrorHeaderRequest(
      GURL("https://docs.google.com/document"), "0123456789",
      "id=0123456789,mode=0,enable_account_consistency=false");
  CheckMirrorCookieRequest(
      GURL("https://drive.google.com/drive"), "0123456789",
      "id=0123456789:mode=0:enable_account_consistency=false");

  // Enable Account Consistency will override the disable.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAccountConsistency);
  CheckMirrorHeaderRequest(
      GURL("https://docs.google.com/document"), "0123456789",
      "id=0123456789,mode=0,enable_account_consistency=true");
  CheckMirrorCookieRequest(
      GURL("https://drive.google.com/drive"), "0123456789",
      "id=0123456789:mode=0:enable_account_consistency=true");
}

// Tests that the Mirror header request is returned normally when the redirect
// URL is eligible.
TEST_F(SigninHeaderHelperTest, TestMirrorHeaderEligibleRedirectURL) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAccountConsistency);

  const GURL url("https://docs.google.com/document");
  const GURL redirect_url("https://www.google.com");
  const std::string account_id = "0123456789";
  std::unique_ptr<net::URLRequest> url_request =
      url_request_context_.CreateRequest(url, net::DEFAULT_PRIORITY, nullptr);
  EXPECT_TRUE(signin::AppendOrRemoveMirrorRequestHeaderIfPossible(
      url_request.get(), redirect_url, account_id, cookie_settings_.get(),
      signin::PROFILE_MODE_DEFAULT));
  EXPECT_TRUE(url_request->extra_request_headers().HasHeader(
      signin::kChromeConnectedHeader));
}

// Tests that the Mirror header request is stripped when the redirect URL is not
// eligible.
TEST_F(SigninHeaderHelperTest, TestMirrorHeaderNonEligibleRedirectURL) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAccountConsistency);

  const GURL url("https://docs.google.com/document");
  const GURL redirect_url("http://www.foo.com");
  const std::string account_id = "0123456789";
  std::unique_ptr<net::URLRequest> url_request =
      url_request_context_.CreateRequest(url, net::DEFAULT_PRIORITY, nullptr);
  EXPECT_FALSE(signin::AppendOrRemoveMirrorRequestHeaderIfPossible(
      url_request.get(), redirect_url, account_id, cookie_settings_.get(),
      signin::PROFILE_MODE_DEFAULT));
  EXPECT_FALSE(url_request->extra_request_headers().HasHeader(
      signin::kChromeConnectedHeader));
}

// Tests that the Mirror header, whatever its value is, is untouched when both
// the current and the redirect URL are non-eligible.
TEST_F(SigninHeaderHelperTest, TestIgnoreMirrorHeaderNonEligibleURLs) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAccountConsistency);

  const GURL url("https://www.bar.com");
  const GURL redirect_url("http://www.foo.com");
  const std::string account_id = "0123456789";
  const std::string fake_header = "foo,bar";
  std::unique_ptr<net::URLRequest> url_request =
      url_request_context_.CreateRequest(url, net::DEFAULT_PRIORITY, nullptr);
  url_request->SetExtraRequestHeaderByName(signin::kChromeConnectedHeader,
                                           fake_header, false);
  EXPECT_FALSE(signin::AppendOrRemoveMirrorRequestHeaderIfPossible(
      url_request.get(), redirect_url, account_id, cookie_settings_.get(),
      signin::PROFILE_MODE_DEFAULT));
  std::string header;
  EXPECT_TRUE(url_request->extra_request_headers().GetHeader(
      signin::kChromeConnectedHeader, &header));
  EXPECT_EQ(fake_header, header);
}
