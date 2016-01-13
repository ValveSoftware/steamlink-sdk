// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_auth_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace gaia {

TEST(GaiaAuthUtilTest, EmailAddressNoOp) {
  const char lower_case[] = "user@what.com";
  EXPECT_EQ(lower_case, CanonicalizeEmail(lower_case));
}

TEST(GaiaAuthUtilTest, EmailAddressIgnoreCaps) {
  EXPECT_EQ(CanonicalizeEmail("user@what.com"),
            CanonicalizeEmail("UsEr@what.com"));
}

TEST(GaiaAuthUtilTest, EmailAddressIgnoreDomainCaps) {
  EXPECT_EQ(CanonicalizeEmail("user@what.com"),
            CanonicalizeEmail("UsEr@what.COM"));
}

TEST(GaiaAuthUtilTest, EmailAddressRejectOneUsernameDot) {
  EXPECT_NE(CanonicalizeEmail("u.ser@what.com"),
            CanonicalizeEmail("UsEr@what.com"));
}

TEST(GaiaAuthUtilTest, EmailAddressMatchWithOneUsernameDot) {
  EXPECT_EQ(CanonicalizeEmail("u.ser@what.com"),
            CanonicalizeEmail("U.sEr@what.com"));
}

TEST(GaiaAuthUtilTest, EmailAddressIgnoreOneUsernameDot) {
  EXPECT_EQ(CanonicalizeEmail("us.er@gmail.com"),
            CanonicalizeEmail("UsEr@gmail.com"));
}

TEST(GaiaAuthUtilTest, EmailAddressIgnoreManyUsernameDots) {
  EXPECT_EQ(CanonicalizeEmail("u.ser@gmail.com"),
            CanonicalizeEmail("Us.E.r@gmail.com"));
}

TEST(GaiaAuthUtilTest, EmailAddressIgnoreConsecutiveUsernameDots) {
  EXPECT_EQ(CanonicalizeEmail("use.r@gmail.com"),
            CanonicalizeEmail("Us....E.r@gmail.com"));
}

TEST(GaiaAuthUtilTest, EmailAddressDifferentOnesRejected) {
  EXPECT_NE(CanonicalizeEmail("who@what.com"),
            CanonicalizeEmail("Us....E.r@what.com"));
}

TEST(GaiaAuthUtilTest, GooglemailNotCanonicalizedToGmail) {
  const char googlemail[] = "user@googlemail.com";
  EXPECT_EQ(googlemail, CanonicalizeEmail(googlemail));
}

TEST(GaiaAuthUtilTest, CanonicalizeDomain) {
  const char domain[] = "example.com";
  EXPECT_EQ(domain, CanonicalizeDomain("example.com"));
  EXPECT_EQ(domain, CanonicalizeDomain("EXAMPLE.cOm"));
}

TEST(GaiaAuthUtilTest, ExtractDomainName) {
  const char domain[] = "example.com";
  EXPECT_EQ(domain, ExtractDomainName("who@example.com"));
  EXPECT_EQ(domain, ExtractDomainName("who@EXAMPLE.cOm"));
}

TEST(GaiaAuthUtilTest, SanitizeMissingDomain) {
  EXPECT_EQ("nodomain@gmail.com", SanitizeEmail("nodomain"));
}

TEST(GaiaAuthUtilTest, SanitizeExistingDomain) {
  const char existing[] = "test@example.com";
  EXPECT_EQ(existing, SanitizeEmail(existing));
}

TEST(GaiaAuthUtilTest, AreEmailsSame) {
  EXPECT_TRUE(AreEmailsSame("foo", "foo"));
  EXPECT_TRUE(AreEmailsSame("foo", "foo@gmail.com"));
  EXPECT_TRUE(AreEmailsSame("foo@gmail.com", "Foo@Gmail.com"));
  EXPECT_FALSE(AreEmailsSame("foo@gmail.com", "foo@othermail.com"));
  EXPECT_FALSE(AreEmailsSame("user@gmail.com", "foo@gmail.com"));
}

TEST(GaiaAuthUtilTest, GmailAndGooglemailAreSame) {
  EXPECT_TRUE(AreEmailsSame("foo@gmail.com", "foo@googlemail.com"));
  EXPECT_FALSE(AreEmailsSame("bar@gmail.com", "foo@googlemail.com"));
}

TEST(GaiaAuthUtilTest, IsGaiaSignonRealm) {
  // Only https versions of Gaia URLs should be considered valid.
  EXPECT_TRUE(IsGaiaSignonRealm(GURL("https://accounts.google.com/")));
  EXPECT_FALSE(IsGaiaSignonRealm(GURL("http://accounts.google.com/")));

  // Other Google URLs are not valid.
  EXPECT_FALSE(IsGaiaSignonRealm(GURL("https://www.google.com/")));
  EXPECT_FALSE(IsGaiaSignonRealm(GURL("http://www.google.com/")));
  EXPECT_FALSE(IsGaiaSignonRealm(GURL("https://google.com/")));
  EXPECT_FALSE(IsGaiaSignonRealm(GURL("https://mail.google.com/")));

  // Other https URLs are not valid.
  EXPECT_FALSE(IsGaiaSignonRealm(GURL("https://www.example.com/")));
}

TEST(GaiaAuthUtilTest, ParseListAccountsData) {
  std::vector<std::pair<std::string, bool> > accounts;
  ASSERT_FALSE(ParseListAccountsData("", &accounts));
  ASSERT_EQ(0u, accounts.size());

  ASSERT_FALSE(ParseListAccountsData("1", &accounts));
  ASSERT_EQ(0u, accounts.size());

  ASSERT_FALSE(ParseListAccountsData("[]", &accounts));
  ASSERT_EQ(0u, accounts.size());

  ASSERT_FALSE(ParseListAccountsData("[\"foo\", \"bar\"]", &accounts));
  ASSERT_EQ(0u, accounts.size());

  ASSERT_TRUE(ParseListAccountsData("[\"foo\", []]", &accounts));
  ASSERT_EQ(0u, accounts.size());

  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", [[\"bar\", 0, \"name\", 0, \"photo\", 0, 0, 0]]]", &accounts));
  ASSERT_EQ(0u, accounts.size());

  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", [[\"bar\", 0, \"name\", \"u@g.c\", \"photo\", 0, 0, 0]]]",
      &accounts));
  ASSERT_EQ(1u, accounts.size());
  ASSERT_EQ("u@g.c", accounts[0].first);
  ASSERT_TRUE(accounts[0].second);

  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", [[\"bar1\", 0, \"name1\", \"u1@g.c\", \"photo1\", 0, 0, 0], "
                 "[\"bar2\", 0, \"name2\", \"u2@g.c\", \"photo2\", 0, 0, 0]]]",
      &accounts));
  ASSERT_EQ(2u, accounts.size());
  ASSERT_EQ("u1@g.c", accounts[0].first);
  ASSERT_TRUE(accounts[0].second);
  ASSERT_EQ("u2@g.c", accounts[1].first);
  ASSERT_TRUE(accounts[1].second);

  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", [[\"b1\", 0, \"name1\", \"U1@g.c\", \"photo1\", 0, 0, 0], "
                 "[\"b2\", 0, \"name2\", \"u.2@g.c\", \"photo2\", 0, 0, 0]]]",
      &accounts));
  ASSERT_EQ(2u, accounts.size());
  ASSERT_EQ(CanonicalizeEmail("U1@g.c"), accounts[0].first);
  ASSERT_TRUE(accounts[0].second);
  ASSERT_EQ(CanonicalizeEmail("u.2@g.c"), accounts[1].first);
  ASSERT_TRUE(accounts[1].second);
}

TEST(GaiaAuthUtilTest, ParseListAccountsDataValidSession) {
  std::vector<std::pair<std::string, bool> > accounts;

  // Missing valid session means: return account.
  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", [[\"b\", 0, \"n\", \"u@g.c\", \"p\", 0, 0, 0]]]",
      &accounts));
  ASSERT_EQ(1u, accounts.size());
  ASSERT_EQ("u@g.c", accounts[0].first);
  ASSERT_TRUE(accounts[0].second);

  // Valid session is true means: return account.
  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", [[\"b\", 0, \"n\", \"u@g.c\", \"p\", 0, 0, 0, 0, 1]]]",
      &accounts));
  ASSERT_EQ(1u, accounts.size());
  ASSERT_EQ("u@g.c", accounts[0].first);
  ASSERT_TRUE(accounts[0].second);

  // Valid session is false means: return account with valid bit false.
  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", [[\"b\", 0, \"n\", \"u@g.c\", \"p\", 0, 0, 0, 0, 0]]]",
      &accounts));
  ASSERT_EQ(1u, accounts.size());
  ASSERT_FALSE(accounts[0].second);
}

}  // namespace gaia
