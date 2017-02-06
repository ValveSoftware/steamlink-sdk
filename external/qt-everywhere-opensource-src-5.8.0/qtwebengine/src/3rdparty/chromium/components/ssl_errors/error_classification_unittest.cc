// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ssl_errors/error_classification.h"

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/testing_pref_service.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class SSLErrorClassificationTest : public testing::Test {};

TEST_F(SSLErrorClassificationTest, TestNameMismatch) {
  scoped_refptr<net::X509Certificate> google_cert(
      net::X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(google_der), sizeof(google_der)));
  ASSERT_TRUE(google_cert.get());
  std::vector<std::string> dns_names_google;
  google_cert->GetDNSNames(&dns_names_google);
  ASSERT_EQ(1u, dns_names_google.size());  // ["www.google.com"]
  std::vector<std::string> hostname_tokens_google =
      ssl_errors::Tokenize(dns_names_google[0]);
  ASSERT_EQ(3u, hostname_tokens_google.size());  // ["www","google","com"]
  std::vector<std::vector<std::string>> dns_name_tokens_google;
  dns_name_tokens_google.push_back(hostname_tokens_google);
  ASSERT_EQ(1u, dns_name_tokens_google.size());  // [["www","google","com"]]

  {
    GURL origin("https://google.com");
    std::string www_host;
    std::vector<std::string> host_name_tokens = base::SplitString(
        origin.host(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_TRUE(
        ssl_errors::GetWWWSubDomainMatch(origin, dns_names_google, &www_host));
    EXPECT_EQ("www.google.com", www_host);
    EXPECT_FALSE(ssl_errors::NameUnderAnyNames(host_name_tokens,
                                               dns_name_tokens_google));
    EXPECT_FALSE(ssl_errors::AnyNamesUnderName(dns_name_tokens_google,
                                               host_name_tokens));
    EXPECT_FALSE(ssl_errors::IsSubDomainOutsideWildcard(origin, *google_cert));
    EXPECT_FALSE(
        ssl_errors::IsCertLikelyFromMultiTenantHosting(origin, *google_cert));
    EXPECT_TRUE(ssl_errors::IsCertLikelyFromSameDomain(origin, *google_cert));
  }

  {
    GURL origin("https://foo.blah.google.com");
    std::string www_host;
    std::vector<std::string> host_name_tokens = base::SplitString(
        origin.host(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_FALSE(
        ssl_errors::GetWWWSubDomainMatch(origin, dns_names_google, &www_host));
    EXPECT_FALSE(ssl_errors::NameUnderAnyNames(host_name_tokens,
                                               dns_name_tokens_google));
    EXPECT_FALSE(ssl_errors::AnyNamesUnderName(dns_name_tokens_google,
                                               host_name_tokens));
    EXPECT_TRUE(ssl_errors::IsCertLikelyFromSameDomain(origin, *google_cert));
  }

  {
    GURL origin("https://foo.www.google.com");
    std::string www_host;
    std::vector<std::string> host_name_tokens = base::SplitString(
        origin.host(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_FALSE(
        ssl_errors::GetWWWSubDomainMatch(origin, dns_names_google, &www_host));
    EXPECT_TRUE(ssl_errors::NameUnderAnyNames(host_name_tokens,
                                              dns_name_tokens_google));
    EXPECT_FALSE(ssl_errors::AnyNamesUnderName(dns_name_tokens_google,
                                               host_name_tokens));
    EXPECT_TRUE(ssl_errors::IsCertLikelyFromSameDomain(origin, *google_cert));
  }

  {
    GURL origin("https://www.google.com.foo");
    std::string www_host;
    std::vector<std::string> host_name_tokens = base::SplitString(
        origin.host(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_FALSE(
        ssl_errors::GetWWWSubDomainMatch(origin, dns_names_google, &www_host));
    EXPECT_FALSE(ssl_errors::NameUnderAnyNames(host_name_tokens,
                                               dns_name_tokens_google));
    EXPECT_FALSE(ssl_errors::AnyNamesUnderName(dns_name_tokens_google,
                                               host_name_tokens));
    EXPECT_FALSE(ssl_errors::IsCertLikelyFromSameDomain(origin, *google_cert));
  }

  {
    GURL origin("https://www.foogoogle.com.");
    std::string www_host;
    std::vector<std::string> host_name_tokens = base::SplitString(
        origin.host(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_FALSE(
        ssl_errors::GetWWWSubDomainMatch(origin, dns_names_google, &www_host));
    EXPECT_FALSE(ssl_errors::NameUnderAnyNames(host_name_tokens,
                                               dns_name_tokens_google));
    EXPECT_FALSE(ssl_errors::AnyNamesUnderName(dns_name_tokens_google,
                                               host_name_tokens));
    EXPECT_FALSE(ssl_errors::IsCertLikelyFromSameDomain(origin, *google_cert));
  }

  scoped_refptr<net::X509Certificate> webkit_cert(
      net::X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(webkit_der), sizeof(webkit_der)));
  ASSERT_TRUE(webkit_cert.get());
  std::vector<std::string> dns_names_webkit;
  webkit_cert->GetDNSNames(&dns_names_webkit);
  ASSERT_EQ(2u, dns_names_webkit.size());  // ["*.webkit.org", "webkit.org"]
  std::vector<std::string> hostname_tokens_webkit_0 =
      ssl_errors::Tokenize(dns_names_webkit[0]);
  ASSERT_EQ(3u, hostname_tokens_webkit_0.size());  // ["*", "webkit","org"]
  std::vector<std::string> hostname_tokens_webkit_1 =
      ssl_errors::Tokenize(dns_names_webkit[1]);
  ASSERT_EQ(2u, hostname_tokens_webkit_1.size());  // ["webkit","org"]
  std::vector<std::vector<std::string>> dns_name_tokens_webkit;
  dns_name_tokens_webkit.push_back(hostname_tokens_webkit_0);
  dns_name_tokens_webkit.push_back(hostname_tokens_webkit_1);
  ASSERT_EQ(2u, dns_name_tokens_webkit.size());
  {
    GURL origin("https://a.b.webkit.org");
    std::string www_host;
    std::vector<std::string> host_name_tokens = base::SplitString(
        origin.host(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_FALSE(
        ssl_errors::GetWWWSubDomainMatch(origin, dns_names_webkit, &www_host));
    EXPECT_FALSE(ssl_errors::NameUnderAnyNames(host_name_tokens,
                                               dns_name_tokens_webkit));
    EXPECT_FALSE(ssl_errors::AnyNamesUnderName(dns_name_tokens_webkit,
                                               host_name_tokens));
    EXPECT_TRUE(ssl_errors::IsSubDomainOutsideWildcard(origin, *webkit_cert));
    EXPECT_FALSE(
        ssl_errors::IsCertLikelyFromMultiTenantHosting(origin, *webkit_cert));
    EXPECT_TRUE(ssl_errors::IsCertLikelyFromSameDomain(origin, *webkit_cert));
  }
}

TEST_F(SSLErrorClassificationTest, TestHostNameHasKnownTLD) {
  EXPECT_TRUE(ssl_errors::IsHostNameKnownTLD("www.google.com"));
  EXPECT_TRUE(ssl_errors::IsHostNameKnownTLD("b.appspot.com"));
  EXPECT_FALSE(ssl_errors::IsHostNameKnownTLD("a.private"));
}

TEST_F(SSLErrorClassificationTest, TestPrivateURL) {
  EXPECT_FALSE(ssl_errors::IsHostnameNonUniqueOrDotless("www.foogoogle.com."));
  EXPECT_TRUE(ssl_errors::IsHostnameNonUniqueOrDotless("go"));
  EXPECT_TRUE(ssl_errors::IsHostnameNonUniqueOrDotless("172.17.108.108"));
  EXPECT_TRUE(ssl_errors::IsHostnameNonUniqueOrDotless("foo.blah"));
}

TEST(ErrorClassification, LevenshteinDistance) {
  EXPECT_EQ(0u, ssl_errors::GetLevenshteinDistance("banana", "banana"));

  EXPECT_EQ(2u, ssl_errors::GetLevenshteinDistance("ab", "ba"));
  EXPECT_EQ(2u, ssl_errors::GetLevenshteinDistance("ba", "ab"));

  EXPECT_EQ(2u, ssl_errors::GetLevenshteinDistance("ananas", "banana"));
  EXPECT_EQ(2u, ssl_errors::GetLevenshteinDistance("banana", "ananas"));

  EXPECT_EQ(2u, ssl_errors::GetLevenshteinDistance("unclear", "nuclear"));
  EXPECT_EQ(2u, ssl_errors::GetLevenshteinDistance("nuclear", "unclear"));

  EXPECT_EQ(3u, ssl_errors::GetLevenshteinDistance("chrome", "chromium"));
  EXPECT_EQ(3u, ssl_errors::GetLevenshteinDistance("chromium", "chrome"));

  EXPECT_EQ(4u, ssl_errors::GetLevenshteinDistance("", "abcd"));
  EXPECT_EQ(4u, ssl_errors::GetLevenshteinDistance("abcd", ""));

  EXPECT_EQ(4u, ssl_errors::GetLevenshteinDistance("xxx", "xxxxxxx"));
  EXPECT_EQ(4u, ssl_errors::GetLevenshteinDistance("xxxxxxx", "xxx"));

  EXPECT_EQ(7u, ssl_errors::GetLevenshteinDistance("yyy", "xxxxxxx"));
  EXPECT_EQ(7u, ssl_errors::GetLevenshteinDistance("xxxxxxx", "yyy"));
}

TEST_F(SSLErrorClassificationTest, GetClockState) {
  // This test aims to obtain all possible return values of
  // |GetClockState|.
  TestingPrefServiceSimple pref_service;
  network_time::NetworkTimeTracker::RegisterPrefs(pref_service.registry());
  base::MessageLoop loop;
  network_time::NetworkTimeTracker network_time_tracker(
      base::WrapUnique(new base::DefaultClock()),
      base::WrapUnique(new base::DefaultTickClock()), &pref_service,
      new net::TestURLRequestContextGetter(
          base::ThreadTaskRunnerHandle::Get()));

  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_UNKNOWN,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));

  ssl_errors::SetBuildTimeForTesting(base::Time::Now() -
                                     base::TimeDelta::FromDays(367));
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_FUTURE,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));

  ssl_errors::SetBuildTimeForTesting(base::Time::Now() +
                                     base::TimeDelta::FromDays(3));
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_PAST,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));

  // Intentionally leave the build time alone.  It should be ignored
  // in favor of network time.
  network_time_tracker.UpdateNetworkTime(
      base::Time::Now() + base::TimeDelta::FromHours(1),
      base::TimeDelta::FromSeconds(1),         // resolution
      base::TimeDelta::FromMilliseconds(250),  // latency
      base::TimeTicks::Now());                 // posting time
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_PAST,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));

  network_time_tracker.UpdateNetworkTime(
      base::Time::Now() - base::TimeDelta::FromHours(1),
      base::TimeDelta::FromSeconds(1),         // resolution
      base::TimeDelta::FromMilliseconds(250),  // latency
      base::TimeTicks::Now());                 // posting time
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_FUTURE,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));

  network_time_tracker.UpdateNetworkTime(
      base::Time::Now(),
      base::TimeDelta::FromSeconds(1),         // resolution
      base::TimeDelta::FromMilliseconds(250),  // latency
      base::TimeTicks::Now());                 // posting time
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_OK,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));

  // Now clear the network time.  The build time should reassert
  // itself.
  network_time_tracker.UpdateNetworkTime(
      base::Time(),
      base::TimeDelta::FromSeconds(1),         // resolution
      base::TimeDelta::FromMilliseconds(250),  // latency
      base::TimeTicks::Now());                 // posting time
  ssl_errors::SetBuildTimeForTesting(base::Time::Now() +
                                     base::TimeDelta::FromDays(3));
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_PAST,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));

  // Now set the build time to something reasonable.  We should be
  // back to the know-nothing state.
  ssl_errors::SetBuildTimeForTesting(base::Time::Now());
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_UNKNOWN,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));
}
