// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_database.h"

#include <stdint.h>

#include <map>
#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_base.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_constants.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using ::testing::ContainerEq;
using ::testing::ElementsAre;
using base::Bucket;
using net::HttpResponseInfo;

const GURL kURL("http://url.com");
const int kReferrerID = 1;
const base::TimeDelta kLatency = base::TimeDelta::FromMilliseconds(5);
const base::Time kFetchTime = base::Time() + base::TimeDelta::FromHours(1000);
const base::Time kOldFetchTime = kFetchTime - base::TimeDelta::FromDays(1);
const base::Time kNewFetchTime =
    base::Time() + base::TimeDelta::FromHours(2000);
const base::Time kPrecacheTime =
    base::Time() + base::TimeDelta::FromHours(3000);
const int64_t kSize = 5000;
const int64_t kFreshnessBucket10K = 9089;
// One of the possible CacheEntryStatus for when the fetch was served from the
// network.
const HttpResponseInfo::CacheEntryStatus kFromNetwork =
    HttpResponseInfo::CacheEntryStatus::ENTRY_UPDATED;

std::map<GURL, base::Time> BuildURLTableMap(const GURL& url,
                                            const base::Time& precache_time) {
  std::map<GURL, base::Time> url_table_map;
  url_table_map[url] = precache_time;
  return url_table_map;
}

HttpResponseInfo CreateHttpResponseInfo(bool was_cached,
                                        bool network_accessed) {
  HttpResponseInfo result;
  result.was_cached = was_cached;
  result.network_accessed = network_accessed;
  if (was_cached) {
    if (network_accessed) {
      result.cache_entry_status =
          HttpResponseInfo::CacheEntryStatus::ENTRY_VALIDATED;
    } else {
      result.cache_entry_status =
          HttpResponseInfo::CacheEntryStatus::ENTRY_USED;
    }
  } else {  // !was_cached.
    result.cache_entry_status = kFromNetwork;
  }
  std::string header(
      "HTTP/1.1 200 OK\n"
      "cache-control: max-age=10000\n\n");
  result.headers = new net::HttpResponseHeaders(
      net::HttpUtil::AssembleRawHeaders(header.c_str(), header.size()));
  DCHECK_EQ(
      10000,
      result.headers->GetFreshnessLifetimes(base::Time()).freshness.InSeconds())
      << "Error parsing the test headers: " << header;
  return result;
}

}  // namespace

namespace precache {

class PrecacheDatabaseTest : public testing::Test {
 public:
  PrecacheDatabaseTest() {}
  ~PrecacheDatabaseTest() override {}

 protected:
  void SetUp() override {
    precache_database_.reset(new PrecacheDatabase());

    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    base::FilePath db_path = scoped_temp_dir_.GetPath().Append(
        base::FilePath(FILE_PATH_LITERAL("precache_database")));
    ASSERT_TRUE(precache_database_->Init(db_path));
  }

  void TearDown() override { precache_url_table()->DeleteAll(); }

  std::map<GURL, base::Time> GetActualURLTableMap() {
    // Flush any buffered writes so that the URL table will be up to date.
    precache_database_->Flush();

    std::map<GURL, base::Time> url_table_map;
    precache_url_table()->GetAllDataForTesting(&url_table_map);
    return url_table_map;
  }

  PrecacheURLTable* precache_url_table() {
    return &precache_database_->precache_url_table_;
  }

  void Flush() { precache_database_->Flush(); }

  // Convenience methods for recording different types of URL fetches. These
  // exist to improve the readability of the tests.
  void RecordPrecacheFromNetwork(const GURL& url,
                                 base::TimeDelta latency,
                                 const base::Time& fetch_time,
                                 int64_t size);
  void RecordPrecacheFromCache(const GURL& url,
                               const base::Time& fetch_time,
                               int64_t size);
  void RecordFetchFromNetwork(const GURL& url,
                              base::TimeDelta latency,
                              const base::Time& fetch_time,
                              int64_t size);
  void RecordFetchFromNetwork(const GURL& url,
                              base::TimeDelta latency,
                              const base::Time& fetch_time,
                              int64_t size,
                              int host_rank);
  void RecordFetchFromNetworkCellular(const GURL& url,
                                      base::TimeDelta latency,
                                      const base::Time& fetch_time,
                                      int64_t size);
  void RecordFetchFromCache(const GURL& url,
                            const base::Time& fetch_time,
                            int64_t size);
  void RecordFetchFromCacheCellular(const GURL& url,
                                    const base::Time& fetch_time,
                                    int64_t size);

  // Must be declared first so that it is destroyed last.
  base::ScopedTempDir scoped_temp_dir_;

  // Having this MessageLoop member variable causes base::MessageLoop::current()
  // to be set properly.
  base::MessageLoopForUI loop_;

  std::unique_ptr<PrecacheDatabase> precache_database_;
  base::HistogramTester histograms_;
  base::HistogramTester::CountsMap expected_histogram_counts_;

  void ExpectNewSample(const std::string& histogram_name,
                       base::HistogramBase::Sample sample) {
    histograms_.ExpectUniqueSample(histogram_name, sample, 1);
    expected_histogram_counts_[histogram_name]++;
  }

  void ExpectNoOtherSamples() {
    EXPECT_THAT(histograms_.GetTotalCountsForPrefix("Precache."),
                ContainerEq(expected_histogram_counts_));
  }
};

void PrecacheDatabaseTest::RecordPrecacheFromNetwork(
    const GURL& url,
    base::TimeDelta latency,
    const base::Time& fetch_time,
    int64_t size) {
  const HttpResponseInfo info = CreateHttpResponseInfo(
      false /* was_cached */, false /* network_accessed */);
  precache_database_->RecordURLPrefetchMetrics(info, latency);
  precache_database_->RecordURLPrefetch(url, std::string(), fetch_time,
                                        info.was_cached, size);
}

void PrecacheDatabaseTest::RecordPrecacheFromCache(const GURL& url,
                                                   const base::Time& fetch_time,
                                                   int64_t size) {
  const HttpResponseInfo info = CreateHttpResponseInfo(
      true /* was_cached */, false /* network_accessed */);
  precache_database_->RecordURLPrefetchMetrics(info,
                                               base::TimeDelta() /* latency */);
  precache_database_->RecordURLPrefetch(url, std::string(), fetch_time,
                                        info.was_cached, size);
}

void PrecacheDatabaseTest::RecordFetchFromNetwork(const GURL& url,
                                                  base::TimeDelta latency,
                                                  const base::Time& fetch_time,
                                                  int64_t size) {
  const HttpResponseInfo info = CreateHttpResponseInfo(
      false /* was_cached */, false /* network_accessed */);
  precache_database_->RecordURLNonPrefetch(url, latency, fetch_time, info, size,
                                           history::kMaxTopHosts,
                                           false /* is_connection_cellular */);
}

void PrecacheDatabaseTest::RecordFetchFromNetwork(const GURL& url,
                                                  base::TimeDelta latency,
                                                  const base::Time& fetch_time,
                                                  int64_t size,
                                                  int host_rank) {
  const HttpResponseInfo info = CreateHttpResponseInfo(
      false /* was_cached */, false /* network_accessed */);
  precache_database_->RecordURLNonPrefetch(url, latency, fetch_time, info, size,
                                           host_rank,
                                           false /* is_connection_cellular */);
}

void PrecacheDatabaseTest::RecordFetchFromNetworkCellular(
    const GURL& url,
    base::TimeDelta latency,
    const base::Time& fetch_time,
    int64_t size) {
  const HttpResponseInfo info = CreateHttpResponseInfo(
      false /* was_cached */, false /* network_accessed */);
  precache_database_->RecordURLNonPrefetch(url, latency, fetch_time, info, size,
                                           history::kMaxTopHosts,
                                           true /* is_connection_cellular */);
}

void PrecacheDatabaseTest::RecordFetchFromCache(const GURL& url,
                                                const base::Time& fetch_time,
                                                int64_t size) {
  const HttpResponseInfo info = CreateHttpResponseInfo(
      true /* was_cached */, false /* network_accessed */);
  precache_database_->RecordURLNonPrefetch(
      url, base::TimeDelta() /* latency */, fetch_time, info, size,
      history::kMaxTopHosts, false /* is_connection_cellular */);
}

void PrecacheDatabaseTest::RecordFetchFromCacheCellular(
    const GURL& url,
    const base::Time& fetch_time,
    int64_t size) {
  const HttpResponseInfo info = CreateHttpResponseInfo(
      true /* was_cached */, false /* network_accessed */);
  precache_database_->RecordURLNonPrefetch(
      url, base::TimeDelta() /* latency */, fetch_time, info, size,
      history::kMaxTopHosts, true /* is_connection_cellular */);
}

namespace {

TEST_F(PrecacheDatabaseTest, PrecacheOverNetwork) {
  RecordPrecacheFromNetwork(kURL, kLatency, kFetchTime, kSize);

  EXPECT_EQ(BuildURLTableMap(kURL, kFetchTime), GetActualURLTableMap());

  ExpectNewSample("Precache.DownloadedPrecacheMotivated", kSize);
  ExpectNewSample("Precache.Latency.Prefetch", kLatency.InMilliseconds());
  ExpectNewSample("Precache.CacheStatus.Prefetch", kFromNetwork);
  ExpectNewSample("Precache.Freshness.Prefetch", kFreshnessBucket10K);
  ExpectNoOtherSamples();
}

TEST_F(PrecacheDatabaseTest, PrecacheFromCacheWithURLTableEntry) {
  precache_url_table()->AddURL(kURL, kReferrerID, true, kOldFetchTime, false);
  RecordPrecacheFromCache(kURL, kFetchTime, kSize);

  // The URL table entry should have been updated to have |kFetchTime| as the
  // timestamp.
  EXPECT_EQ(BuildURLTableMap(kURL, kFetchTime), GetActualURLTableMap());

  ExpectNewSample("Precache.Latency.Prefetch", 0);
  ExpectNewSample("Precache.CacheStatus.Prefetch",
                  net::HttpResponseInfo::ENTRY_USED);
  ExpectNewSample("Precache.Freshness.Prefetch", kFreshnessBucket10K);
  ExpectNoOtherSamples();
}

TEST_F(PrecacheDatabaseTest, PrecacheFromCacheWithoutURLTableEntry) {
  RecordPrecacheFromCache(kURL, kFetchTime, kSize);

  EXPECT_TRUE(GetActualURLTableMap().empty());

  ExpectNewSample("Precache.Latency.Prefetch", 0);
  ExpectNewSample("Precache.CacheStatus.Prefetch",
                  net::HttpResponseInfo::ENTRY_USED);
  ExpectNewSample("Precache.Freshness.Prefetch", kFreshnessBucket10K);
  ExpectNoOtherSamples();
}

TEST_F(PrecacheDatabaseTest, FetchOverNetwork_NonCellular) {
  RecordFetchFromNetwork(kURL, kLatency, kFetchTime, kSize);

  EXPECT_TRUE(GetActualURLTableMap().empty());

  ExpectNewSample("Precache.DownloadedNonPrecache", kSize);
  ExpectNewSample("Precache.CacheStatus.NonPrefetch", kFromNetwork);
  ExpectNewSample("Precache.Latency.NonPrefetch", kLatency.InMilliseconds());
  ExpectNewSample("Precache.Latency.NonPrefetch.NonTopHosts",
                  kLatency.InMilliseconds());
  ExpectNoOtherSamples();
}

TEST_F(PrecacheDatabaseTest, FetchOverNetwork_NonCellular_TopHosts) {
  RecordFetchFromNetwork(kURL, kLatency, kFetchTime, kSize, 0 /* host_rank */);

  EXPECT_TRUE(GetActualURLTableMap().empty());

  ExpectNewSample("Precache.DownloadedNonPrecache", kSize);
  ExpectNewSample("Precache.CacheStatus.NonPrefetch", kFromNetwork);
  ExpectNewSample("Precache.Latency.NonPrefetch", kLatency.InMilliseconds());
  ExpectNewSample("Precache.Latency.NonPrefetch.TopHosts",
                  kLatency.InMilliseconds());
  ExpectNoOtherSamples();
}

TEST_F(PrecacheDatabaseTest, FetchOverNetwork_Cellular) {
  RecordFetchFromNetworkCellular(kURL, kLatency, kFetchTime, kSize);

  EXPECT_TRUE(GetActualURLTableMap().empty());

  ExpectNewSample("Precache.DownloadedNonPrecache", kSize);
  ExpectNewSample("Precache.DownloadedNonPrecache.Cellular", kSize);
  ExpectNewSample("Precache.CacheStatus.NonPrefetch", kFromNetwork);
  ExpectNewSample("Precache.Latency.NonPrefetch", kLatency.InMilliseconds());
  ExpectNewSample("Precache.Latency.NonPrefetch.NonTopHosts",
                  kLatency.InMilliseconds());
  ExpectNoOtherSamples();
}

TEST_F(PrecacheDatabaseTest, FetchOverNetworkWithURLTableEntry) {
  precache_url_table()->AddURL(kURL, kReferrerID, true, kOldFetchTime, false);
  RecordFetchFromNetwork(kURL, kLatency, kFetchTime, kSize);

  // The URL table entry should have been deleted.
  EXPECT_TRUE(GetActualURLTableMap().empty());

  ExpectNewSample("Precache.DownloadedNonPrecache", kSize);
  ExpectNewSample("Precache.Latency.NonPrefetch", kLatency.InMilliseconds());
  ExpectNewSample("Precache.Latency.NonPrefetch.NonTopHosts",
                  kLatency.InMilliseconds());
  ExpectNewSample("Precache.CacheStatus.NonPrefetch", kFromNetwork);
  ExpectNewSample("Precache.CacheStatus.NonPrefetch.FromPrecache",
                  kFromNetwork);
  ExpectNoOtherSamples();
}

TEST_F(PrecacheDatabaseTest, FetchFromCacheWithURLTableEntry_NonCellular) {
  precache_url_table()->AddURL(kURL, kReferrerID, true, kOldFetchTime, false);
  RecordFetchFromCache(kURL, kFetchTime, kSize);

  // The URL table entry should have been deleted.
  EXPECT_TRUE(GetActualURLTableMap().empty());

  ExpectNewSample("Precache.Latency.NonPrefetch", 0);
  ExpectNewSample("Precache.Latency.NonPrefetch.NonTopHosts", 0);
  ExpectNewSample("Precache.CacheStatus.NonPrefetch",
                  HttpResponseInfo::CacheEntryStatus::ENTRY_USED);
  ExpectNewSample("Precache.CacheStatus.NonPrefetch.FromPrecache",
                  HttpResponseInfo::CacheEntryStatus::ENTRY_USED);
  ExpectNewSample("Precache.Saved", kSize);
  ExpectNewSample("Precache.Saved.Freshness", kFreshnessBucket10K);
  ExpectNoOtherSamples();
}

TEST_F(PrecacheDatabaseTest, FetchFromCacheWithURLTableEntry_Cellular) {
  precache_url_table()->AddURL(kURL, kReferrerID, true, kOldFetchTime, false);
  RecordFetchFromCacheCellular(kURL, kFetchTime, kSize);

  // The URL table entry should have been deleted.
  EXPECT_TRUE(GetActualURLTableMap().empty());

  ExpectNewSample("Precache.Latency.NonPrefetch", 0);
  ExpectNewSample("Precache.Latency.NonPrefetch.NonTopHosts", 0);
  ExpectNewSample("Precache.CacheStatus.NonPrefetch",
                  HttpResponseInfo::CacheEntryStatus::ENTRY_USED);
  ExpectNewSample("Precache.CacheStatus.NonPrefetch.FromPrecache",
                  HttpResponseInfo::CacheEntryStatus::ENTRY_USED);
  ExpectNewSample("Precache.Saved", kSize);
  ExpectNewSample("Precache.Saved.Cellular", kSize);
  ExpectNewSample("Precache.Saved.Freshness", kFreshnessBucket10K);
  ExpectNoOtherSamples();
}

TEST_F(PrecacheDatabaseTest, FetchFromCacheWithoutURLTableEntry) {
  RecordFetchFromCache(kURL, kFetchTime, kSize);

  EXPECT_TRUE(GetActualURLTableMap().empty());

  ExpectNewSample("Precache.Latency.NonPrefetch", 0);
  ExpectNewSample("Precache.Latency.NonPrefetch.NonTopHosts", 0);
  ExpectNewSample("Precache.CacheStatus.NonPrefetch",
                  HttpResponseInfo::CacheEntryStatus::ENTRY_USED);
  ExpectNoOtherSamples();
}

TEST_F(PrecacheDatabaseTest, DeleteExpiredPrecacheHistory) {
  const base::Time kToday = base::Time() + base::TimeDelta::FromDays(1000);
  const base::Time k59DaysAgo = kToday - base::TimeDelta::FromDays(59);
  const base::Time k61DaysAgo = kToday - base::TimeDelta::FromDays(61);

  precache_url_table()->AddURL(GURL("http://expired-precache.com"), kReferrerID,
                               true, k61DaysAgo, false);
  precache_url_table()->AddURL(GURL("http://old-precache.com"), kReferrerID,
                               true, k59DaysAgo, false);

  precache_database_->DeleteExpiredPrecacheHistory(kToday);

  EXPECT_EQ(BuildURLTableMap(GURL("http://old-precache.com"), k59DaysAgo),
            GetActualURLTableMap());
}

TEST_F(PrecacheDatabaseTest, SampleInteraction) {
  const GURL kURL1("http://url1.com");
  const int64_t kSize1 = 1;
  const GURL kURL2("http://url2.com");
  const int64_t kSize2 = 2;
  const GURL kURL3("http://url3.com");
  const int64_t kSize3 = 3;
  const GURL kURL4("http://url4.com");
  const int64_t kSize4 = 4;
  const GURL kURL5("http://url5.com");
  const int64_t kSize5 = 5;

  RecordPrecacheFromNetwork(kURL1, kLatency, kFetchTime, kSize1);
  RecordPrecacheFromNetwork(kURL2, kLatency, kFetchTime, kSize2);
  RecordPrecacheFromNetwork(kURL3, kLatency, kFetchTime, kSize3);
  RecordPrecacheFromNetwork(kURL4, kLatency, kFetchTime, kSize4);

  RecordFetchFromCacheCellular(kURL1, kFetchTime, kSize1);
  RecordFetchFromCacheCellular(kURL1, kFetchTime, kSize1);
  RecordFetchFromNetworkCellular(kURL2, kLatency, kFetchTime, kSize2);
  RecordFetchFromNetworkCellular(kURL5, kLatency, kFetchTime, kSize5);
  RecordFetchFromCacheCellular(kURL5, kFetchTime, kSize5);

  RecordPrecacheFromCache(kURL1, kFetchTime, kSize1);
  RecordPrecacheFromNetwork(kURL2, kLatency, kFetchTime, kSize2);
  RecordPrecacheFromCache(kURL3, kFetchTime, kSize3);
  RecordPrecacheFromCache(kURL4, kFetchTime, kSize4);

  RecordFetchFromCache(kURL1, kFetchTime, kSize1);
  RecordFetchFromNetwork(kURL2, kLatency, kFetchTime, kSize2);
  RecordFetchFromCache(kURL3, kFetchTime, kSize3);
  RecordFetchFromCache(kURL5, kFetchTime, kSize5);

  EXPECT_THAT(histograms_.GetAllSamples("Precache.DownloadedPrecacheMotivated"),
              ElementsAre(Bucket(kSize1, 1), Bucket(kSize2, 2),
                          Bucket(kSize3, 1), Bucket(kSize4, 1)));

  EXPECT_THAT(histograms_.GetAllSamples("Precache.DownloadedNonPrecache"),
              ElementsAre(Bucket(kSize2, 2), Bucket(kSize5, 1)));

  EXPECT_THAT(
      histograms_.GetAllSamples("Precache.DownloadedNonPrecache.Cellular"),
      ElementsAre(Bucket(kSize2, 1), Bucket(kSize5, 1)));

  EXPECT_THAT(histograms_.GetAllSamples("Precache.Latency.Prefetch"),
              ElementsAre(Bucket(0, 3), Bucket(kLatency.InMilliseconds(), 5)));

  EXPECT_THAT(histograms_.GetAllSamples("Precache.Latency.NonPrefetch"),
              ElementsAre(Bucket(0, 6), Bucket(kLatency.InMilliseconds(), 3)));

  EXPECT_THAT(histograms_.GetAllSamples("Precache.Saved"),
              ElementsAre(Bucket(kSize1, 1), Bucket(kSize3, 1)));

  EXPECT_THAT(histograms_.GetAllSamples("Precache.Saved.Cellular"),
              ElementsAre(Bucket(kSize1, 1)));
}

TEST_F(PrecacheDatabaseTest, LastPrecacheTimestamp) {
  // So that it starts recording TimeSinceLastPrecache.
  const base::Time kStartTime =
      base::Time() + base::TimeDelta::FromSeconds(100);
  precache_database_->SetLastPrecacheTimestamp(kStartTime);

  RecordPrecacheFromNetwork(kURL, kLatency, kStartTime, kSize);
  RecordPrecacheFromNetwork(kURL, kLatency, kStartTime, kSize);
  RecordPrecacheFromNetwork(kURL, kLatency, kStartTime, kSize);
  RecordPrecacheFromNetwork(kURL, kLatency, kStartTime, kSize);

  EXPECT_THAT(histograms_.GetAllSamples("Precache.TimeSinceLastPrecache"),
              ElementsAre());

  const base::Time kTimeA = kStartTime + base::TimeDelta::FromSeconds(7);
  const base::Time kTimeB = kStartTime + base::TimeDelta::FromMinutes(42);
  const base::Time kTimeC = kStartTime + base::TimeDelta::FromHours(20);

  RecordFetchFromCacheCellular(kURL, kTimeA, kSize);
  RecordFetchFromCacheCellular(kURL, kTimeA, kSize);
  RecordFetchFromNetworkCellular(kURL, kLatency, kTimeB, kSize);
  RecordFetchFromNetworkCellular(kURL, kLatency, kTimeB, kSize);
  RecordFetchFromCacheCellular(kURL, kTimeB, kSize);
  RecordFetchFromCacheCellular(kURL, kTimeC, kSize);

  EXPECT_THAT(histograms_.GetAllSamples("Precache.TimeSinceLastPrecache"),
              ElementsAre(Bucket(0, 2), Bucket(2406, 3), Bucket(69347, 1)));
}

TEST_F(PrecacheDatabaseTest, PrecacheFreshnessPrefetch) {
  auto info = CreateHttpResponseInfo(false /* was_cached */,
                                     false /* network_accessed */);
  RecordPrecacheFromNetwork(kURL, kLatency, kFetchTime, kSize);

  EXPECT_THAT(histograms_.GetAllSamples("Precache.Freshness.Prefetch"),
              ElementsAre(Bucket(kFreshnessBucket10K, 1)));
}

TEST_F(PrecacheDatabaseTest, UpdateAndGetReferrerHost) {
  // Invalid ID should be returned for referrer host that does not exist.
  EXPECT_EQ(PrecacheReferrerHostEntry::kInvalidId,
            precache_database_->GetReferrerHost(std::string()).id);
  EXPECT_EQ(PrecacheReferrerHostEntry::kInvalidId,
            precache_database_->GetReferrerHost("not_created_host.com").id);

  // Create a new entry.
  precache_database_->UpdatePrecacheReferrerHost("foo.com", 1, kFetchTime);
  Flush();
  auto actual_entry = precache_database_->GetReferrerHost("foo.com");
  EXPECT_EQ("foo.com", actual_entry.referrer_host);
  EXPECT_EQ(1, actual_entry.manifest_id);
  EXPECT_EQ(kFetchTime, actual_entry.time);

  // Update the existing entry.
  precache_database_->UpdatePrecacheReferrerHost("foo.com", 2, kNewFetchTime);
  Flush();
  actual_entry = precache_database_->GetReferrerHost("foo.com");
  EXPECT_EQ("foo.com", actual_entry.referrer_host);
  EXPECT_EQ(2, actual_entry.manifest_id);
  EXPECT_EQ(kNewFetchTime, actual_entry.time);
}

TEST_F(PrecacheDatabaseTest, GetURLListForReferrerHost) {
  precache_database_->UpdatePrecacheReferrerHost("foo.com", 1, kFetchTime);
  precache_database_->UpdatePrecacheReferrerHost("bar.com", 2, kNewFetchTime);
  precache_database_->UpdatePrecacheReferrerHost("foobar.com", 3,
                                                 kNewFetchTime);
  precache_database_->UpdatePrecacheReferrerHost("empty.com", 3, kNewFetchTime);

  struct test_resource_info {
    std::string url;
    bool is_user_browsed;
    bool is_network_fetched;
    bool is_cellular_fetched;
    bool expected_in_used;
  };

  const struct {
    std::string hostname;
    std::vector<test_resource_info> resource_info;
  } tests[] = {
      {
          "foo.com",
          {
              {"http://foo.com/from-cache.js", true, false, false, true},
              {"http://some-cdn.com/from-network.js", true, true, false, false},
              {"http://foo.com/from-cache-cellular.js", true, false, true,
               true},
              {"http://foo.com/from-network-cellular.js", true, true, true,
               false},
              {"http://foo.com/not-browsed.js", false, false, false, false},
          },
      },
      {
          "bar.com",
          {
              {"http://bar.com/a.js", true, false, false, true},
              {"http://some-cdn.com/b.js", true, false, true, true},
              {"http://bar.com/not-browsed.js", false, false, false, false},
          },
      },
      {
          "foobar.com",
          {
              {"http://some-cdn.com/not-used.js", true, true, true, false},
          },
      },
      {
          "empty.com", std::vector<test_resource_info>{},
      },
  };
  // Add precached resources.
  for (const auto& test : tests) {
    for (const auto& resource : test.resource_info) {
      precache_database_->RecordURLPrefetch(GURL(resource.url), test.hostname,
                                            kPrecacheTime, false, kSize);
    }
  }
  // Update some resources as used due to user browsing.
  for (const auto& test : tests) {
    for (const auto& resource : test.resource_info) {
      if (!resource.is_user_browsed)
        continue;
      if (!resource.is_network_fetched && !resource.is_cellular_fetched) {
        RecordFetchFromCache(GURL(resource.url), kFetchTime, kSize);
      } else if (!resource.is_network_fetched && resource.is_cellular_fetched) {
        RecordFetchFromCacheCellular(GURL(resource.url), kFetchTime, kSize);
      } else if (resource.is_network_fetched && !resource.is_cellular_fetched) {
        RecordFetchFromNetwork(GURL(resource.url), kLatency, kFetchTime, kSize);
      } else if (resource.is_network_fetched && resource.is_cellular_fetched) {
        RecordFetchFromNetworkCellular(GURL(resource.url), kLatency, kFetchTime,
                                       kSize);
      }
    }
  }
  Flush();
  // Verify the used and downloaded resources.
  for (const auto& test : tests) {
    std::vector<GURL> expected_used_urls, expected_downloaded_urls;
    for (const auto& resource : test.resource_info) {
      if (resource.expected_in_used) {
        expected_used_urls.push_back(GURL(resource.url));
      }
      expected_downloaded_urls.push_back(GURL(resource.url));
    }
    std::vector<GURL> actual_used_urls, actual_downloaded_urls;
    auto referrer_id = precache_database_->GetReferrerHost(test.hostname).id;
    EXPECT_NE(PrecacheReferrerHostEntry::kInvalidId, referrer_id);
    precache_database_->GetURLListForReferrerHost(
        referrer_id, &actual_used_urls, &actual_downloaded_urls);
    EXPECT_THAT(actual_used_urls, ::testing::ContainerEq(expected_used_urls));
    EXPECT_THAT(actual_downloaded_urls,
                ::testing::ContainerEq(expected_downloaded_urls))
        << "Host: " << test.hostname;
  }
  // Subsequent manifest updates should clear the used and downloaded resources.
  for (const auto& test : tests) {
    precache_database_->UpdatePrecacheReferrerHost(test.hostname, 100,
                                                   kNewFetchTime);
    Flush();
    std::vector<GURL> actual_used_urls, actual_downloaded_urls;
    auto referrer_id = precache_database_->GetReferrerHost(test.hostname).id;
    EXPECT_NE(PrecacheReferrerHostEntry::kInvalidId, referrer_id);
    precache_database_->GetURLListForReferrerHost(
        referrer_id, &actual_used_urls, &actual_downloaded_urls);
    EXPECT_THAT(actual_used_urls, ::testing::IsEmpty());
    EXPECT_THAT(actual_downloaded_urls, ::testing::IsEmpty());
  }
  // Resources that were precached previously and not seen in user browsing
  // should be still marked as precached.
  std::map<GURL, base::Time> expected_url_table_map;
  for (const auto& test : tests) {
    for (const auto& resource : test.resource_info) {
      if (!resource.is_user_browsed)
        expected_url_table_map[GURL(resource.url)] = kPrecacheTime;
    }
  }
  EXPECT_EQ(expected_url_table_map, GetActualURLTableMap());
}

TEST_F(PrecacheDatabaseTest, GetURLListForReferrerHostReportsDownloadedOnce) {
  precache_database_->UpdatePrecacheReferrerHost("foo.com", 1, kFetchTime);
  // Add two resources that shouldn't appear in downloaded.
  Flush();  // We need to write the referrer_host_id.
  const std::string already_reported_and_not_refetch =
      "http://foo.com/already-reported-and-not-refetch.js";
  precache_database_->RecordURLPrefetch(GURL(already_reported_and_not_refetch),
                                        "foo.com", kPrecacheTime, false, kSize);
  const std::string already_reported_and_in_cache =
      "http://foo.com/already-reported-and-in-cache.js";
  precache_database_->RecordURLPrefetch(GURL(already_reported_and_in_cache),
                                        "foo.com", kPrecacheTime, false, kSize);
  const std::string already_reported_but_refetch =
      "http://foo.com/already-reported-but-refetch.js";
  precache_database_->RecordURLPrefetch(GURL(already_reported_but_refetch),
                                        "foo.com", kPrecacheTime, false, kSize);
  {
    // Let's mark existing resources as is_download_reported = 1 by calling
    // GetURLListForReferrerHost.
    std::vector<GURL> unused_a, unused_b;
    auto id = precache_database_->GetReferrerHost("foo.com").id;
    ASSERT_NE(PrecacheReferrerHostEntry::kInvalidId, id);
    precache_database_->GetURLListForReferrerHost(id, &unused_a, &unused_b);
  }

  precache_database_->RecordURLPrefetch(GURL(already_reported_and_in_cache),
                                        "foo.com", kPrecacheTime,
                                        true /* was_cached */, kSize);
  precache_database_->RecordURLPrefetch(GURL(already_reported_but_refetch),
                                        "foo.com", kPrecacheTime,
                                        false /* was_cached */, kSize);
  Flush();

  // Only the refetch resource should be reported as downloaded this time
  // around.
  std::vector<GURL> _, actual_downloaded_urls;
  auto referrer_id = precache_database_->GetReferrerHost("foo.com").id;
  EXPECT_NE(PrecacheReferrerHostEntry::kInvalidId, referrer_id);
  precache_database_->GetURLListForReferrerHost(referrer_id, &_,
                                                &actual_downloaded_urls);
  EXPECT_THAT(actual_downloaded_urls,
              ElementsAre(GURL(already_reported_but_refetch)));
}

}  // namespace

}  // namespace precache
