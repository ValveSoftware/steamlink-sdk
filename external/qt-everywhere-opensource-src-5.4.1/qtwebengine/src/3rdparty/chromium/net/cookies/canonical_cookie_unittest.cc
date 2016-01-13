// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/canonical_cookie.h"

#include "base/memory/scoped_ptr.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

TEST(CanonicalCookieTest, GetCookieSourceFromURL) {
  EXPECT_EQ("http://example.com/",
            CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com")));
  EXPECT_EQ("http://example.com/",
            CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com/")));
  EXPECT_EQ("http://example.com/",
            CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com/test")));
  EXPECT_EQ("file:///tmp/test.html",
            CanonicalCookie::GetCookieSourceFromURL(
                GURL("file:///tmp/test.html")));
  EXPECT_EQ("http://example.com/",
            CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com:1234/")));
  EXPECT_EQ("http://example.com/",
            CanonicalCookie::GetCookieSourceFromURL(
                GURL("https://example.com/")));
  EXPECT_EQ("http://example.com/",
            CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://user:pwd@example.com/")));
  EXPECT_EQ("http://example.com/",
            CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com/test?foo")));
  EXPECT_EQ("http://example.com/",
            CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com/test#foo")));
}

TEST(CanonicalCookieTest, Constructor) {
  GURL url("http://www.example.com/test");
  base::Time current_time = base::Time::Now();

  CanonicalCookie cookie(url, "A", "2", "www.example.com", "/test",
                         current_time, base::Time(), current_time, false, false,
                         COOKIE_PRIORITY_DEFAULT);
  EXPECT_EQ(url.GetOrigin().spec(), cookie.Source());
  EXPECT_EQ("A", cookie.Name());
  EXPECT_EQ("2", cookie.Value());
  EXPECT_EQ("www.example.com", cookie.Domain());
  EXPECT_EQ("/test", cookie.Path());
  EXPECT_FALSE(cookie.IsSecure());

  CanonicalCookie cookie2(url,
                          "A",
                          "2",
                          std::string(),
                          std::string(),
                          current_time,
                          base::Time(),
                          current_time,
                          false,
                          false,
                          COOKIE_PRIORITY_DEFAULT);
  EXPECT_EQ(url.GetOrigin().spec(), cookie.Source());
  EXPECT_EQ("A", cookie2.Name());
  EXPECT_EQ("2", cookie2.Value());
  EXPECT_EQ("", cookie2.Domain());
  EXPECT_EQ("", cookie2.Path());
  EXPECT_FALSE(cookie2.IsSecure());

}

TEST(CanonicalCookieTest, Create) {
  // Test creating cookies from a cookie string.
  GURL url("http://www.example.com/test/foo.html");
  base::Time creation_time = base::Time::Now();
  CookieOptions options;

  scoped_ptr<CanonicalCookie> cookie(
        CanonicalCookie::Create(url, "A=2", creation_time, options));
  EXPECT_EQ(url.GetOrigin().spec(), cookie->Source());
  EXPECT_EQ("A", cookie->Name());
  EXPECT_EQ("2", cookie->Value());
  EXPECT_EQ("www.example.com", cookie->Domain());
  EXPECT_EQ("/test", cookie->Path());
  EXPECT_FALSE(cookie->IsSecure());

  GURL url2("http://www.foo.com");
  cookie.reset(CanonicalCookie::Create(url2, "B=1", creation_time, options));
  EXPECT_EQ(url2.GetOrigin().spec(), cookie->Source());
  EXPECT_EQ("B", cookie->Name());
  EXPECT_EQ("1", cookie->Value());
  EXPECT_EQ("www.foo.com", cookie->Domain());
  EXPECT_EQ("/", cookie->Path());
  EXPECT_FALSE(cookie->IsSecure());

  // Test creating secure cookies. RFC 6265 allows insecure urls to set secure
  // cookies.
  cookie.reset(
      CanonicalCookie::Create(url, "A=2; Secure", creation_time, options));
  EXPECT_TRUE(cookie.get());
  EXPECT_TRUE(cookie->IsSecure());

  // Test creating http only cookies.
  cookie.reset(
      CanonicalCookie::Create(url, "A=2; HttpOnly", creation_time, options));
  EXPECT_FALSE(cookie.get());
  CookieOptions httponly_options;
  httponly_options.set_include_httponly();
  cookie.reset(
      CanonicalCookie::Create(url, "A=2; HttpOnly", creation_time,
                              httponly_options));
  EXPECT_TRUE(cookie->IsHttpOnly());

  // Test the creating cookies using specific parameter instead of a cookie
  // string.
  cookie.reset(CanonicalCookie::Create(
      url, "A", "2", "www.example.com", "/test", creation_time, base::Time(),
      false, false, COOKIE_PRIORITY_DEFAULT));
  EXPECT_EQ(url.GetOrigin().spec(), cookie->Source());
  EXPECT_EQ("A", cookie->Name());
  EXPECT_EQ("2", cookie->Value());
  EXPECT_EQ(".www.example.com", cookie->Domain());
  EXPECT_EQ("/test", cookie->Path());
  EXPECT_FALSE(cookie->IsSecure());

  cookie.reset(CanonicalCookie::Create(
      url, "A", "2", ".www.example.com", "/test", creation_time, base::Time(),
      false, false, COOKIE_PRIORITY_DEFAULT));
  EXPECT_EQ(url.GetOrigin().spec(), cookie->Source());
  EXPECT_EQ("A", cookie->Name());
  EXPECT_EQ("2", cookie->Value());
  EXPECT_EQ(".www.example.com", cookie->Domain());
  EXPECT_EQ("/test", cookie->Path());
  EXPECT_FALSE(cookie->IsSecure());
}

TEST(CanonicalCookieTest, EmptyExpiry) {
  GURL url("http://www7.ipdl.inpit.go.jp/Tokujitu/tjkta.ipdl?N0000=108");
  base::Time creation_time = base::Time::Now();
  CookieOptions options;

  std::string cookie_line =
      "ACSTM=20130308043820420042; path=/; domain=ipdl.inpit.go.jp; Expires=";
  scoped_ptr<CanonicalCookie> cookie(CanonicalCookie::Create(
      url, cookie_line, creation_time, options));
  EXPECT_TRUE(cookie.get());
  EXPECT_FALSE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Time(), cookie->ExpiryDate());

  // With a stale server time
  options.set_server_time(creation_time - base::TimeDelta::FromHours(1));
  cookie.reset(CanonicalCookie::Create(
      url, cookie_line, creation_time, options));
  EXPECT_TRUE(cookie.get());
  EXPECT_FALSE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Time(), cookie->ExpiryDate());

  // With a future server time
  options.set_server_time(creation_time + base::TimeDelta::FromHours(1));
  cookie.reset(CanonicalCookie::Create(
      url, cookie_line, creation_time, options));
  EXPECT_TRUE(cookie.get());
  EXPECT_FALSE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Time(), cookie->ExpiryDate());
}

TEST(CanonicalCookieTest, IsEquivalent) {
  GURL url("http://www.example.com/");
  std::string cookie_name = "A";
  std::string cookie_value = "2EDA-EF";
  std::string cookie_domain = ".www.example.com";
  std::string cookie_path = "/";
  base::Time creation_time = base::Time::Now();
  base::Time last_access_time = creation_time;
  base::Time expiration_time = creation_time + base::TimeDelta::FromDays(2);
  bool secure(false);
  bool httponly(false);

  // Test that a cookie is equivalent to itself.
  scoped_ptr<CanonicalCookie> cookie(
      new CanonicalCookie(url, cookie_name, cookie_value, cookie_domain,
                          cookie_path, creation_time, expiration_time,
                          last_access_time, secure, httponly,
                          COOKIE_PRIORITY_MEDIUM));
  EXPECT_TRUE(cookie->IsEquivalent(*cookie));

  // Test that two identical cookies are equivalent.
  scoped_ptr<CanonicalCookie> other_cookie(
      new CanonicalCookie(url, cookie_name, cookie_value, cookie_domain,
                          cookie_path, creation_time, expiration_time,
                          last_access_time, secure, httponly,
                          COOKIE_PRIORITY_MEDIUM));
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));

  // Tests that use different variations of attribute values that
  // DON'T affect cookie equivalence.
  other_cookie.reset(new CanonicalCookie(url, cookie_name, "2", cookie_domain,
                                         cookie_path, creation_time,
                                         expiration_time, last_access_time,
                                         secure, httponly,
                                         COOKIE_PRIORITY_HIGH));
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));

  base::Time other_creation_time =
      creation_time + base::TimeDelta::FromMinutes(2);
  other_cookie.reset(new CanonicalCookie(url, cookie_name, "2", cookie_domain,
                                         cookie_path, other_creation_time,
                                         expiration_time, last_access_time,
                                         secure, httponly,
                                         COOKIE_PRIORITY_MEDIUM));
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));

  other_cookie.reset(new CanonicalCookie(url, cookie_name, cookie_name,
                                         cookie_domain, cookie_path,
                                         creation_time, expiration_time,
                                         last_access_time, true, httponly,
                                         COOKIE_PRIORITY_LOW));
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));

  // Tests that use different variations of attribute values that
  // DO affect cookie equivalence.
  other_cookie.reset(new CanonicalCookie(url, "B", cookie_value, cookie_domain,
                                         cookie_path, creation_time,
                                         expiration_time, last_access_time,
                                         secure, httponly,
                                         COOKIE_PRIORITY_MEDIUM));
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));

  other_cookie.reset(new CanonicalCookie(url, cookie_name, cookie_value,
                                         "www.example.com", cookie_path,
                                         creation_time, expiration_time,
                                         last_access_time, secure, httponly,
                                         COOKIE_PRIORITY_MEDIUM));
  EXPECT_TRUE(cookie->IsDomainCookie());
  EXPECT_FALSE(other_cookie->IsDomainCookie());
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));

  other_cookie.reset(new CanonicalCookie(url, cookie_name, cookie_value,
                                         ".example.com", cookie_path,
                                         creation_time, expiration_time,
                                         last_access_time, secure, httponly,
                                         COOKIE_PRIORITY_MEDIUM));
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));

  other_cookie.reset(new CanonicalCookie(url, cookie_name, cookie_value,
                                         cookie_domain, "/test/0",
                                         creation_time, expiration_time,
                                         last_access_time, secure, httponly,
                                         COOKIE_PRIORITY_MEDIUM));
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
}

TEST(CanonicalCookieTest, IsDomainMatch) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time creation_time = base::Time::Now();
  CookieOptions options;

  scoped_ptr<CanonicalCookie> cookie(
      CanonicalCookie::Create(url, "A=2", creation_time, options));
  EXPECT_TRUE(cookie->IsHostCookie());
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("foo.www.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("www0.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("example.com"));

  cookie.reset(
      CanonicalCookie::Create(url, "A=2; Domain=www.example.com", creation_time,
                              options));
  EXPECT_TRUE(cookie->IsDomainCookie());
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("foo.www.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("www0.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("example.com"));

  cookie.reset(
      CanonicalCookie::Create(url, "A=2; Domain=.www.example.com",
                              creation_time, options));
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("foo.www.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("www0.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("example.com"));
}

TEST(CanonicalCookieTest, IsOnPath) {
  base::Time creation_time = base::Time::Now();
  CookieOptions options;

  scoped_ptr<CanonicalCookie> cookie(
      CanonicalCookie::Create(GURL("http://www.example.com"),
                              "A=2", creation_time, options));
  EXPECT_TRUE(cookie->IsOnPath("/"));
  EXPECT_TRUE(cookie->IsOnPath("/test"));
  EXPECT_TRUE(cookie->IsOnPath("/test/bar.html"));

  // Test the empty string edge case.
  EXPECT_FALSE(cookie->IsOnPath(std::string()));

  cookie.reset(
      CanonicalCookie::Create(GURL("http://www.example.com/test/foo.html"),
                              "A=2", creation_time, options));
  EXPECT_FALSE(cookie->IsOnPath("/"));
  EXPECT_TRUE(cookie->IsOnPath("/test"));
  EXPECT_TRUE(cookie->IsOnPath("/test/bar.html"));
  EXPECT_TRUE(cookie->IsOnPath("/test/sample/bar.html"));
}

TEST(CanonicalCookieTest, IncludeForRequestURL) {
  GURL url("http://www.example.com");
  base::Time creation_time = base::Time::Now();
  CookieOptions options;

  scoped_ptr<CanonicalCookie> cookie(
      CanonicalCookie::Create(url, "A=2", creation_time, options));
  EXPECT_TRUE(cookie->IncludeForRequestURL(url, options));
  EXPECT_TRUE(cookie->IncludeForRequestURL(
      GURL("http://www.example.com/foo/bar"), options));
  EXPECT_TRUE(cookie->IncludeForRequestURL(
      GURL("https://www.example.com/foo/bar"), options));
  EXPECT_FALSE(cookie->IncludeForRequestURL(GURL("https://sub.example.com"),
                                            options));
  EXPECT_FALSE(cookie->IncludeForRequestURL(GURL("https://sub.www.example.com"),
                                            options));

  // Test that cookie with a cookie path that does not match the url path are
  // not included.
  cookie.reset(CanonicalCookie::Create(url, "A=2; Path=/foo/bar", creation_time,
                                       options));
  EXPECT_FALSE(cookie->IncludeForRequestURL(url, options));
  EXPECT_TRUE(cookie->IncludeForRequestURL(
      GURL("http://www.example.com/foo/bar/index.html"), options));

  // Test that a secure cookie is not included for a non secure URL.
  GURL secure_url("https://www.example.com");
  cookie.reset(CanonicalCookie::Create(secure_url, "A=2; Secure", creation_time,
                                       options));
  EXPECT_TRUE(cookie->IsSecure());
  EXPECT_TRUE(cookie->IncludeForRequestURL(secure_url, options));
  EXPECT_FALSE(cookie->IncludeForRequestURL(url, options));

  // Test that http only cookies are only included if the include httponly flag
  // is set on the cookie options.
  options.set_include_httponly();
  cookie.reset(
      CanonicalCookie::Create(url, "A=2; HttpOnly", creation_time, options));
  EXPECT_TRUE(cookie->IsHttpOnly());
  EXPECT_TRUE(cookie->IncludeForRequestURL(url, options));
  options.set_exclude_httponly();
  EXPECT_FALSE(cookie->IncludeForRequestURL(url, options));
}

}  // namespace net
