// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/psl_matching_helper.h"

#include <stddef.h>

#include "base/macros.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

TEST(PSLMatchingUtilsTest, IsPublicSuffixDomainMatch) {
  struct TestPair {
    const char* url1;
    const char* url2;
    bool should_match;
  };

  const TestPair pairs[] = {
      {"http://facebook.com", "http://facebook.com", true},
      {"http://facebook.com/path", "http://facebook.com/path", true},
      {"http://facebook.com/path1", "http://facebook.com/path2", true},
      {"http://facebook.com", "http://m.facebook.com", true},
      {"http://www.facebook.com", "http://m.facebook.com", true},
      {"http://facebook.com/path", "http://m.facebook.com/path", true},
      {"http://facebook.com/path1", "http://m.facebook.com/path2", true},
      {"http://example.com/has space", "http://example.com/has space", true},
      {"http://www.example.com", "http://wwwexample.com", false},
      {"http://www.example.com", "https://www.example.com", false},
      {"http://www.example.com:123", "http://www.example.com", false},
      {"http://www.example.org", "http://www.example.com", false},
      // URLs without registry controlled domains should not match.
      {"http://localhost", "http://127.0.0.1", false},
      // Invalid URLs should not match anything.
      {"http://", "http://", false},
      {"", "", false},
      {"bad url", "bad url", false},
      {"http://www.example.com", "http://", false},
      {"", "http://www.example.com", false},
      {"http://www.example.com", "bad url", false},
      {"http://www.example.com/%00", "http://www.example.com/%00", false},
      {"federation://example.com/google.com", "https://example.com/", false},
  };

  for (const TestPair& pair : pairs) {
    autofill::PasswordForm form1;
    form1.signon_realm = pair.url1;
    autofill::PasswordForm form2;
    form2.signon_realm = pair.url2;
    EXPECT_EQ(pair.should_match,
              IsPublicSuffixDomainMatch(form1.signon_realm, form2.signon_realm))
        << "First URL = " << pair.url1 << ", second URL = " << pair.url2;
  }
}

TEST(PSLMatchingUtilsTest, IsFederatedMatch) {
  struct TestPair {
    const char* signon_realm;
    const char* origin;
    bool should_match;
  };

  const TestPair pairs[] = {
      {"https://facebook.com", "https://facebook.com", false},
      {"", "", false},
      {"", "https://facebook.com/", false},
      {"https://facebook.com/", "", false},
      {"federation://example.com/google.com", "https://example.com/", true},
      {"federation://example.com/google.com", "http://example.com/", true},
      {"federation://example.com/google.com", "example.com", false},
      {"federation://example.com/", "http://example.com/", false},
      {"federation://example.com/google.com", "example", false},
  };

  for (const TestPair& pair : pairs) {
    std::string signon_realm = pair.signon_realm;
    GURL origin(pair.origin);
    EXPECT_EQ(pair.should_match, IsFederatedMatch(signon_realm, origin))
        << "signon_realm = " << pair.signon_realm
        << ", origin = " << pair.origin;
  }
}

}  // namespace

}  // namespace password_manager
