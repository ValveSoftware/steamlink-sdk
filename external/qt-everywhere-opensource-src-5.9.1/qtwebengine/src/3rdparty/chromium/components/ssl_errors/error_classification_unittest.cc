// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ssl_errors/error_classification.h"

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_split.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "components/network_time/network_time_test_utils.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/testing_pref_service.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
const char kNetworkTimeHistogram[] = "interstitial.ssl.clockstate.network3";

static std::unique_ptr<net::test_server::HttpResponse>
NetworkErrorResponseHandler(const net::test_server::HttpRequest& request) {
  return std::unique_ptr<net::test_server::HttpResponse>(
      new net::test_server::RawHttpResponse("", ""));
}

}  // namespace

class SSLErrorClassificationTest : public ::testing::Test {
 public:
  SSLErrorClassificationTest()
      : field_trial_test_(network_time::FieldTrialTest::CreateForUnitTest()) {}
  network_time::FieldTrialTest* field_trial_test() {
    return field_trial_test_.get();
  }

 private:
  std::unique_ptr<network_time::FieldTrialTest> field_trial_test_;
};

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
  EXPECT_TRUE(ssl_errors::HostNameHasKnownTLD("www.google.com"));
  EXPECT_TRUE(ssl_errors::HostNameHasKnownTLD("b.appspot.com"));
  EXPECT_FALSE(ssl_errors::HostNameHasKnownTLD("a.private"));
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
  const char kBuildTimeHistogram[] = "interstitial.ssl.clockstate.build_time";
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kBuildTimeHistogram, 0);
  histograms.ExpectTotalCount(kNetworkTimeHistogram, 0);
  TestingPrefServiceSimple pref_service;
  network_time::NetworkTimeTracker::RegisterPrefs(pref_service.registry());
  base::MessageLoop loop;
  network_time::NetworkTimeTracker network_time_tracker(
      base::MakeUnique<base::DefaultClock>(),
      base::MakeUnique<base::DefaultTickClock>(), &pref_service,
      new net::TestURLRequestContextGetter(
          base::ThreadTaskRunnerHandle::Get()));

  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_UNKNOWN,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));
  histograms.ExpectTotalCount(kBuildTimeHistogram, 1);
  histograms.ExpectBucketCount(kBuildTimeHistogram,
                               ssl_errors::ClockState::CLOCK_STATE_UNKNOWN, 1);
  histograms.ExpectTotalCount(kNetworkTimeHistogram, 1);
  histograms.ExpectBucketCount(
      kNetworkTimeHistogram,
      ssl_errors::NETWORK_CLOCK_STATE_UNKNOWN_NO_SYNC_ATTEMPT, 1);

  ssl_errors::SetBuildTimeForTesting(base::Time::Now() -
                                     base::TimeDelta::FromDays(367));
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_FUTURE,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));
  histograms.ExpectTotalCount(kBuildTimeHistogram, 2);
  histograms.ExpectBucketCount(kBuildTimeHistogram,
                               ssl_errors::ClockState::CLOCK_STATE_FUTURE, 1);
  histograms.ExpectTotalCount(kNetworkTimeHistogram, 2);
  histograms.ExpectBucketCount(
      kNetworkTimeHistogram,
      ssl_errors::NETWORK_CLOCK_STATE_UNKNOWN_NO_SYNC_ATTEMPT, 2);

  ssl_errors::SetBuildTimeForTesting(base::Time::Now() +
                                     base::TimeDelta::FromDays(3));
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_PAST,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));
  histograms.ExpectTotalCount(kBuildTimeHistogram, 3);
  histograms.ExpectBucketCount(kBuildTimeHistogram,
                               ssl_errors::ClockState::CLOCK_STATE_FUTURE, 1);
  histograms.ExpectTotalCount(kNetworkTimeHistogram, 3);
  histograms.ExpectBucketCount(
      kNetworkTimeHistogram,
      ssl_errors::NETWORK_CLOCK_STATE_UNKNOWN_NO_SYNC_ATTEMPT, 3);

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
  histograms.ExpectTotalCount(kBuildTimeHistogram, 4);
  histograms.ExpectTotalCount(kNetworkTimeHistogram, 4);
  histograms.ExpectBucketCount(
      kNetworkTimeHistogram, ssl_errors::NETWORK_CLOCK_STATE_CLOCK_IN_PAST, 1);

  network_time_tracker.UpdateNetworkTime(
      base::Time::Now() - base::TimeDelta::FromHours(1),
      base::TimeDelta::FromSeconds(1),         // resolution
      base::TimeDelta::FromMilliseconds(250),  // latency
      base::TimeTicks::Now());                 // posting time
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_FUTURE,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));
  histograms.ExpectTotalCount(kBuildTimeHistogram, 5);
  histograms.ExpectTotalCount(kNetworkTimeHistogram, 5);
  histograms.ExpectBucketCount(kNetworkTimeHistogram,
                               ssl_errors::NETWORK_CLOCK_STATE_CLOCK_IN_FUTURE,
                               1);

  network_time_tracker.UpdateNetworkTime(
      base::Time::Now(),
      base::TimeDelta::FromSeconds(1),         // resolution
      base::TimeDelta::FromMilliseconds(250),  // latency
      base::TimeTicks::Now());                 // posting time
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_OK,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));
  histograms.ExpectTotalCount(kBuildTimeHistogram, 6);
  histograms.ExpectTotalCount(kNetworkTimeHistogram, 6);
  histograms.ExpectBucketCount(kNetworkTimeHistogram,
                               ssl_errors::NETWORK_CLOCK_STATE_OK, 1);

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

// Tests that all possible NetworkClockState histogram values are recorded
// appropriately.
TEST_F(SSLErrorClassificationTest, NetworkClockStateHistogram) {
  base::Thread io_thread("IO thread");
  base::Thread::Options thread_options;
  thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
  EXPECT_TRUE(io_thread.StartWithOptions(thread_options));

  net::EmbeddedTestServer test_server;
  ASSERT_TRUE(test_server.InitializeAndListen());

  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kNetworkTimeHistogram, 0);
  TestingPrefServiceSimple pref_service;
  network_time::NetworkTimeTracker::RegisterPrefs(pref_service.registry());
  base::SimpleTestTickClock* tick_clock = new base::SimpleTestTickClock;
  base::SimpleTestClock* clock = new base::SimpleTestClock;
  // Do this to be sure that |is_null| returns false.
  clock->Advance(base::TimeDelta::FromDays(111));
  tick_clock->Advance(base::TimeDelta::FromDays(222));

  base::MessageLoop loop;
  network_time::NetworkTimeTracker network_time_tracker(
      std::unique_ptr<base::Clock>(clock),
      std::unique_ptr<base::TickClock>(tick_clock), &pref_service,
      new net::TestURLRequestContextGetter(io_thread.task_runner()));
  network_time_tracker.SetTimeServerURLForTesting(test_server.GetURL("/"));
  field_trial_test()->SetNetworkQueriesWithVariationsService(
      true, 0.0,
      network_time::NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND);

  // No sync attempt.
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_UNKNOWN,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));
  histograms.ExpectTotalCount(kNetworkTimeHistogram, 1);
  histograms.ExpectBucketCount(
      kNetworkTimeHistogram,
      ssl_errors::NETWORK_CLOCK_STATE_UNKNOWN_NO_SYNC_ATTEMPT, 1);

  // First sync attempt is pending.
  test_server.RegisterRequestHandler(base::Bind(&NetworkErrorResponseHandler));
  test_server.StartAcceptingConnections();
  EXPECT_TRUE(network_time_tracker.QueryTimeServiceForTesting());
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_UNKNOWN,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));
  histograms.ExpectTotalCount(kNetworkTimeHistogram, 2);
  histograms.ExpectBucketCount(
      kNetworkTimeHistogram,
      ssl_errors::NETWORK_CLOCK_STATE_UNKNOWN_FIRST_SYNC_PENDING, 1);
  network_time_tracker.WaitForFetchForTesting(123123123);

  // No successful sync.
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_UNKNOWN,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));
  histograms.ExpectTotalCount(kNetworkTimeHistogram, 3);
  histograms.ExpectBucketCount(
      kNetworkTimeHistogram,
      ssl_errors::NETWORK_CLOCK_STATE_UNKNOWN_NO_SUCCESSFUL_SYNC, 1);

  // Subsequent sync attempt is pending.
  EXPECT_TRUE(network_time_tracker.QueryTimeServiceForTesting());
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_UNKNOWN,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));
  histograms.ExpectTotalCount(kNetworkTimeHistogram, 4);
  histograms.ExpectBucketCount(
      kNetworkTimeHistogram,
      ssl_errors::NETWORK_CLOCK_STATE_UNKNOWN_SUBSEQUENT_SYNC_PENDING, 1);
  network_time_tracker.WaitForFetchForTesting(123123123);

  // System clock is correct.
  network_time_tracker.UpdateNetworkTime(
      clock->Now(),
      base::TimeDelta::FromSeconds(1),         // resolution
      base::TimeDelta::FromMilliseconds(250),  // latency
      tick_clock->NowTicks());                 // posting time
  EXPECT_EQ(ssl_errors::ClockState::CLOCK_STATE_OK,
            ssl_errors::GetClockState(clock->Now(), &network_time_tracker));
  histograms.ExpectTotalCount(kNetworkTimeHistogram, 5);
  histograms.ExpectBucketCount(kNetworkTimeHistogram,
                               ssl_errors::NETWORK_CLOCK_STATE_OK, 1);

  // System clock is in the past.
  network_time_tracker.UpdateNetworkTime(
      clock->Now() + base::TimeDelta::FromHours(1),
      base::TimeDelta::FromSeconds(1),         // resolution
      base::TimeDelta::FromMilliseconds(250),  // latency
      tick_clock->NowTicks());                 // posting time
  EXPECT_EQ(ssl_errors::ClockState::CLOCK_STATE_PAST,
            ssl_errors::GetClockState(clock->Now(), &network_time_tracker));
  histograms.ExpectTotalCount(kNetworkTimeHistogram, 6);
  histograms.ExpectBucketCount(
      kNetworkTimeHistogram, ssl_errors::NETWORK_CLOCK_STATE_CLOCK_IN_PAST, 1);

  // System clock is in the future.
  network_time_tracker.UpdateNetworkTime(
      clock->Now() - base::TimeDelta::FromHours(1),
      base::TimeDelta::FromSeconds(1),         // resolution
      base::TimeDelta::FromMilliseconds(250),  // latency
      tick_clock->NowTicks());                 // posting time
  EXPECT_EQ(ssl_errors::ClockState::CLOCK_STATE_FUTURE,
            ssl_errors::GetClockState(clock->Now(), &network_time_tracker));
  histograms.ExpectTotalCount(kNetworkTimeHistogram, 7);
  histograms.ExpectBucketCount(kNetworkTimeHistogram,
                               ssl_errors::NETWORK_CLOCK_STATE_CLOCK_IN_FUTURE,
                               1);

  // Sync has been lost.
  tick_clock->Advance(base::TimeDelta::FromSeconds(1));
  clock->Advance(base::TimeDelta::FromDays(1));
  // GetClockState() will fall back to the build time heuristic.
  ssl_errors::GetClockState(clock->Now(), &network_time_tracker);
  histograms.ExpectTotalCount(kNetworkTimeHistogram, 8);
  histograms.ExpectBucketCount(
      kNetworkTimeHistogram, ssl_errors::NETWORK_CLOCK_STATE_UNKNOWN_SYNC_LOST,
      1);

  io_thread.Stop();
}
