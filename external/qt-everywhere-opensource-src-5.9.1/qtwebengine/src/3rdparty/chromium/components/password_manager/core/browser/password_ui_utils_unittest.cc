// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_ui_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

TEST(GetShownOriginTest, RemovePrefixes) {
  const struct {
    const std::string input;
    const std::string output;
  } kTestCases[] = {
      {"http://subdomain.example.com:80/login/index.html?h=90&ind=hello#first",
       "subdomain.example.com"},
      {"https://www.example.com", "example.com"},
      {"https://mobile.example.com", "example.com"},
      {"https://m.example.com", "example.com"},
      {"https://m.subdomain.example.net", "subdomain.example.net"},
      {"https://mobile.de", "mobile.de"},
      {"https://www.de", "www.de"},
      {"https://m.de", "m.de"},
      {"https://www.mobile.de", "mobile.de"},
      {"https://m.mobile.de", "mobile.de"},
      {"https://m.www.de", "www.de"},
      {"https://Mobile.example.de", "example.de"},
      {"https://WWW.Example.DE", "example.de"}};

  for (const auto& test_case : kTestCases) {
    autofill::PasswordForm password_form;
    password_form.origin = GURL(test_case.input);
    EXPECT_EQ(test_case.output, GetShownOrigin(password_form.origin))
        << "for input " << test_case.input;
  }
}

TEST(GetShownOriginAndLinkUrlTest, OriginFromAndroidForm_NoAffiliatedRealm) {
  autofill::PasswordForm android_form;
  android_form.signon_realm =
      "android://"
      "m3HSJL1i83hdltRq0-o9czGb-8KJDKra4t_"
      "3JRlnPKcjI8PZm6XBHXx6zG4UuMXaDEZjR1wuXDre9G9zvN7AQw=="
      "@com.example.android";
  android_form.affiliated_web_realm = std::string();

  bool is_android_uri;
  GURL link_url;
  bool origin_is_clickable;
  EXPECT_EQ("android://com.example.android",
            GetShownOriginAndLinkUrl(android_form, &is_android_uri, &link_url,
                                     &origin_is_clickable));
  EXPECT_TRUE(is_android_uri);
  EXPECT_FALSE(origin_is_clickable);
  EXPECT_EQ(GURL(android_form.signon_realm), link_url);
}

TEST(GetShownOriginAndLinkUrlTest, OriginFromAndroidForm_WithAffiliatedRealm) {
  autofill::PasswordForm android_form;
  android_form.signon_realm =
      "android://"
      "m3HSJL1i83hdltRq0-o9czGb-8KJDKra4t_"
      "3JRlnPKcjI8PZm6XBHXx6zG4UuMXaDEZjR1wuXDre9G9zvN7AQw=="
      "@com.example.android";
  android_form.affiliated_web_realm = "https://example.com/";

  bool is_android_uri;
  GURL link_url;
  bool origin_is_clickable;
  EXPECT_EQ("example.com",
            GetShownOriginAndLinkUrl(android_form, &is_android_uri, &link_url,
                                     &origin_is_clickable));
  EXPECT_TRUE(is_android_uri);
  EXPECT_TRUE(origin_is_clickable);
  EXPECT_EQ(GURL(android_form.affiliated_web_realm), link_url);
}

TEST(GetShownOriginAndLinkUrlTest, OriginFromNonAndroidForm) {
  autofill::PasswordForm form;
  form.signon_realm = "https://example.com/";
  form.origin = GURL("https://example.com/login?ref=1");

  bool is_android_uri;
  GURL link_url;
  bool origin_is_clickable;
  EXPECT_EQ("example.com",
            GetShownOriginAndLinkUrl(form, &is_android_uri, &link_url,
                                     &origin_is_clickable));
  EXPECT_FALSE(is_android_uri);
  EXPECT_TRUE(origin_is_clickable);
  EXPECT_EQ(form.origin, link_url);
}

TEST(SplitByDotAndReverseTest, ReversedHostname) {
  const struct {
    const char* input;
    const char* output;
  } kTestCases[] = {{"com.example.android", "android.example.com"},
                    {"com.example.mobile", "mobile.example.com"},
                    {"net.example.subdomain", "subdomain.example.net"}};
  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.output, SplitByDotAndReverse(test_case.input));
  }
}

}  // namespace password_manager
