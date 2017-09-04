// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_permissions.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

TEST(ExtensionWebRequestPermissions, IsSensitiveURL) {
  struct TestCase {
    const char* url;
    bool is_sensitive;
  } cases[] = {
      {"http://www.google.com", false},
      {"http://www.example.com", false},
      {"http://clients.google.com", true},
      {"http://clients4.google.com", true},
      {"http://clients9999.google.com", true},
      {"http://clients9999..google.com", false},
      {"http://clients9999.example.google.com", false},
      {"http://clients.google.com.", true},
      {"http://.clients.google.com.", true},
      {"http://google.example.com", false},
      {"http://www.example.com", false},
      {"http://clients.google.com", true},
      {"http://sb-ssl.google.com", true},
      {"http://sb-ssl.random.google.com", false},
      {"http://chrome.google.com", false},
      {"http://chrome.google.com/webstore", true},
      {"http://chrome.google.com/webstore?query", true},
  };
  for (const TestCase& test : cases) {
    EXPECT_EQ(test.is_sensitive, IsSensitiveURL(GURL(test.url))) << test.url;
  }
}

}  // namespace extensions
