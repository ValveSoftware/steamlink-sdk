// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/v4_get_hash_protocol_manager.h"

#include <memory>
#include <vector>

#include "base/base64.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/safe_browsing_db/safebrowsing.pb.h"
#include "components/safe_browsing_db/testing_util.h"
#include "components/safe_browsing_db/util.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace {

const char kClient[] = "unittest";
const char kAppVer[] = "1.0";
const char kKeyParam[] = "test_key_param";

}  // namespace

namespace safe_browsing {

class SafeBrowsingV4GetHashProtocolManagerTest : public testing::Test {
 protected:
  std::unique_ptr<V4GetHashProtocolManager> CreateProtocolManager() {
    V4ProtocolConfig config;
    config.client_name = kClient;
    config.version = kAppVer;
    config.key_param = kKeyParam;
    return std::unique_ptr<V4GetHashProtocolManager>(
        V4GetHashProtocolManager::Create(NULL, config));
  }

  std::string GetStockV4HashResponse() {
    FindFullHashesResponse res;
    res.mutable_negative_cache_duration()->set_seconds(600);
    ThreatMatch* m = res.add_matches();
    m->set_threat_type(API_ABUSE);
    m->set_platform_type(CHROME_PLATFORM);
    m->set_threat_entry_type(URL);
    m->mutable_cache_duration()->set_seconds(300);
    m->mutable_threat()->set_hash(
        SBFullHashToString(SBFullHashForString("Everything's shiny, Cap'n.")));
    ThreatEntryMetadata::MetadataEntry* e =
        m->mutable_threat_entry_metadata()->add_entries();
    e->set_key("permission");
    e->set_value("NOTIFICATIONS");

    // Serialize.
    std::string res_data;
    res.SerializeToString(&res_data);

    return res_data;
  }

  void SetTestClock(base::Time now, V4GetHashProtocolManager* pm) {
    base::SimpleTestClock* clock = new base::SimpleTestClock();
    clock->SetNow(now);
    pm->SetClockForTests(base::WrapUnique(clock));
  }
};

void ValidateGetV4HashResults(
    const std::vector<SBFullHashResult>& expected_full_hashes,
    const base::Time& expected_cache_expire,
    const std::vector<SBFullHashResult>& full_hashes,
    const base::Time& cache_expire) {
  EXPECT_EQ(expected_cache_expire, cache_expire);
  ASSERT_EQ(expected_full_hashes.size(), full_hashes.size());

  for (unsigned int i = 0; i < expected_full_hashes.size(); ++i) {
    const SBFullHashResult& expected = expected_full_hashes[i];
    const SBFullHashResult& actual = full_hashes[i];
    EXPECT_TRUE(SBFullHashEqual(expected.hash, actual.hash));
    EXPECT_EQ(expected.metadata, actual.metadata);
    EXPECT_EQ(expected.cache_expire_after, actual.cache_expire_after);
  }
}

TEST_F(SafeBrowsingV4GetHashProtocolManagerTest,
       TestGetHashErrorHandlingNetwork) {
  net::TestURLFetcherFactory factory;
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

  std::vector<SBPrefix> prefixes;
  std::vector<SBFullHashResult> expected_full_hashes;
  base::Time expected_cache_expire;

  pm->GetFullHashesWithApis(
      prefixes, base::Bind(&ValidateGetV4HashResults, expected_full_hashes,
                           expected_cache_expire));

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  DCHECK(fetcher);
  // Failed request status should result in error.
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                            net::ERR_CONNECTION_RESET));
  fetcher->set_response_code(200);
  fetcher->SetResponseString(GetStockV4HashResponse());
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // Should have recorded one error, but back off multiplier is unchanged.
  EXPECT_EQ(1ul, pm->gethash_error_count_);
  EXPECT_EQ(1ul, pm->gethash_back_off_mult_);
}

TEST_F(SafeBrowsingV4GetHashProtocolManagerTest,
       TestGetHashErrorHandlingResponseCode) {
  net::TestURLFetcherFactory factory;
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

  std::vector<SBPrefix> prefixes;
  std::vector<SBFullHashResult> expected_full_hashes;
  base::Time expected_cache_expire;

  pm->GetFullHashesWithApis(
      prefixes, base::Bind(&ValidateGetV4HashResults, expected_full_hashes,
                           expected_cache_expire));

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  DCHECK(fetcher);
  fetcher->set_status(net::URLRequestStatus());
  // Response code of anything other than 200 should result in error.
  fetcher->set_response_code(204);
  fetcher->SetResponseString(GetStockV4HashResponse());
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // Should have recorded one error, but back off multiplier is unchanged.
  EXPECT_EQ(1ul, pm->gethash_error_count_);
  EXPECT_EQ(1ul, pm->gethash_back_off_mult_);
}

TEST_F(SafeBrowsingV4GetHashProtocolManagerTest, TestGetHashErrorHandlingOK) {
  net::TestURLFetcherFactory factory;
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

  base::Time now = base::Time::UnixEpoch();
  SetTestClock(now, pm.get());

  std::vector<SBPrefix> prefixes;
  std::vector<SBFullHashResult> expected_full_hashes;
  SBFullHashResult hash_result;
  hash_result.hash = SBFullHashForString("Everything's shiny, Cap'n.");
  hash_result.metadata.api_permissions.insert("NOTIFICATIONS");
  hash_result.cache_expire_after = now + base::TimeDelta::FromSeconds(300);
  expected_full_hashes.push_back(hash_result);
  base::Time expected_cache_expire = now + base::TimeDelta::FromSeconds(600);

  pm->GetFullHashesWithApis(
      prefixes, base::Bind(&ValidateGetV4HashResults, expected_full_hashes,
                           expected_cache_expire));

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  DCHECK(fetcher);
  fetcher->set_status(net::URLRequestStatus());
  fetcher->set_response_code(200);
  fetcher->SetResponseString(GetStockV4HashResponse());
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // No error, back off multiplier is unchanged.
  EXPECT_EQ(0ul, pm->gethash_error_count_);
  EXPECT_EQ(1ul, pm->gethash_back_off_mult_);
}

TEST_F(SafeBrowsingV4GetHashProtocolManagerTest, TestGetHashRequest) {
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

  FindFullHashesRequest req;
  ThreatInfo* info = req.mutable_threat_info();
  info->add_threat_types(API_ABUSE);
  info->add_platform_types(CHROME_PLATFORM);
  info->add_threat_entry_types(URL);

  SBPrefix one = 1u;
  SBPrefix two = 2u;
  SBPrefix three = 3u;
  std::string hash(reinterpret_cast<const char*>(&one), sizeof(SBPrefix));
  info->add_threat_entries()->set_hash(hash);
  hash.clear();
  hash.append(reinterpret_cast<const char*>(&two), sizeof(SBPrefix));
  info->add_threat_entries()->set_hash(hash);
  hash.clear();
  hash.append(reinterpret_cast<const char*>(&three), sizeof(SBPrefix));
  info->add_threat_entries()->set_hash(hash);

  // Serialize and Base64 encode.
  std::string req_data, req_base64;
  req.SerializeToString(&req_data);
  base::Base64Encode(req_data, &req_base64);

  std::vector<PlatformType> platform;
  platform.push_back(CHROME_PLATFORM);
  std::vector<SBPrefix> prefixes;
  prefixes.push_back(one);
  prefixes.push_back(two);
  prefixes.push_back(three);
  EXPECT_EQ(req_base64, pm->GetHashRequest(prefixes, platform, API_ABUSE));
}

TEST_F(SafeBrowsingV4GetHashProtocolManagerTest, TestParseHashResponse) {
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

  base::Time now = base::Time::UnixEpoch();
  SetTestClock(now, pm.get());

  FindFullHashesResponse res;
  res.mutable_negative_cache_duration()->set_seconds(600);
  res.mutable_minimum_wait_duration()->set_seconds(400);
  ThreatMatch* m = res.add_matches();
  m->set_threat_type(API_ABUSE);
  m->set_platform_type(CHROME_PLATFORM);
  m->set_threat_entry_type(URL);
  m->mutable_cache_duration()->set_seconds(300);
  m->mutable_threat()->set_hash(
      SBFullHashToString(SBFullHashForString("Everything's shiny, Cap'n.")));
  ThreatEntryMetadata::MetadataEntry* e =
      m->mutable_threat_entry_metadata()->add_entries();
  e->set_key("permission");
  e->set_value("NOTIFICATIONS");

  // Serialize.
  std::string res_data;
  res.SerializeToString(&res_data);

  std::vector<SBFullHashResult> full_hashes;
  base::Time cache_expire;
  EXPECT_TRUE(pm->ParseHashResponse(res_data, &full_hashes, &cache_expire));

  EXPECT_EQ(now + base::TimeDelta::FromSeconds(600), cache_expire);
  EXPECT_EQ(1ul, full_hashes.size());
  EXPECT_TRUE(SBFullHashEqual(SBFullHashForString("Everything's shiny, Cap'n."),
                              full_hashes[0].hash));
  EXPECT_EQ(1ul, full_hashes[0].metadata.api_permissions.size());
  EXPECT_EQ(1ul,
            full_hashes[0].metadata.api_permissions.count("NOTIFICATIONS"));
  EXPECT_EQ(now +
      base::TimeDelta::FromSeconds(300), full_hashes[0].cache_expire_after);
  EXPECT_EQ(now + base::TimeDelta::FromSeconds(400), pm->next_gethash_time_);
}

// Adds an entry with an ignored ThreatEntryType.
TEST_F(SafeBrowsingV4GetHashProtocolManagerTest,
       TestParseHashResponseWrongThreatEntryType) {
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

  base::Time now = base::Time::UnixEpoch();
  SetTestClock(now, pm.get());

  FindFullHashesResponse res;
  res.mutable_negative_cache_duration()->set_seconds(600);
  res.add_matches()->set_threat_entry_type(EXECUTABLE);

  // Serialize.
  std::string res_data;
  res.SerializeToString(&res_data);

  std::vector<SBFullHashResult> full_hashes;
  base::Time cache_expire;
  EXPECT_FALSE(pm->ParseHashResponse(res_data, &full_hashes, &cache_expire));

  EXPECT_EQ(now + base::TimeDelta::FromSeconds(600), cache_expire);
  // There should be no hash results.
  EXPECT_EQ(0ul, full_hashes.size());
}

// Adds entries with a ThreatPatternType metadata.
TEST_F(SafeBrowsingV4GetHashProtocolManagerTest,
       TestParseHashThreatPatternType) {
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

  base::Time now = base::Time::UnixEpoch();
  SetTestClock(now, pm.get());

  // Test social engineering pattern type.
  FindFullHashesResponse se_res;
  se_res.mutable_negative_cache_duration()->set_seconds(600);
  ThreatMatch* se = se_res.add_matches();
  se->set_threat_type(SOCIAL_ENGINEERING_PUBLIC);
  se->set_platform_type(CHROME_PLATFORM);
  se->set_threat_entry_type(URL);
  SBFullHash hash_string = SBFullHashForString("Everything's shiny, Cap'n.");
  se->mutable_threat()->set_hash(SBFullHashToString(hash_string));
  ThreatEntryMetadata::MetadataEntry* se_meta =
      se->mutable_threat_entry_metadata()->add_entries();
  se_meta->set_key("se_pattern_type");
  se_meta->set_value("SOCIAL_ENGINEERING_LANDING");

  std::string se_data;
  se_res.SerializeToString(&se_data);

  std::vector<SBFullHashResult> full_hashes;
  base::Time cache_expire;
  EXPECT_TRUE(pm->ParseHashResponse(se_data, &full_hashes, &cache_expire));

  EXPECT_EQ(now + base::TimeDelta::FromSeconds(600), cache_expire);
  EXPECT_EQ(1ul, full_hashes.size());
  EXPECT_TRUE(SBFullHashEqual(hash_string, full_hashes[0].hash));
  EXPECT_EQ(ThreatPatternType::SOCIAL_ENGINEERING_LANDING,
            full_hashes[0].metadata.threat_pattern_type);

  // Test potentially harmful application pattern type.
  FindFullHashesResponse pha_res;
  pha_res.mutable_negative_cache_duration()->set_seconds(600);
  ThreatMatch* pha = pha_res.add_matches();
  pha->set_threat_type(POTENTIALLY_HARMFUL_APPLICATION);
  pha->set_threat_entry_type(URL);
  pha->set_platform_type(CHROME_PLATFORM);
  hash_string = SBFullHashForString("Not to fret.");
  pha->mutable_threat()->set_hash(SBFullHashToString(hash_string));
  ThreatEntryMetadata::MetadataEntry* pha_meta =
      pha->mutable_threat_entry_metadata()->add_entries();
  pha_meta->set_key("pha_pattern_type");
  pha_meta->set_value("LANDING");

  std::string pha_data;
  pha_res.SerializeToString(&pha_data);
  full_hashes.clear();
  EXPECT_TRUE(pm->ParseHashResponse(pha_data, &full_hashes, &cache_expire));
  EXPECT_EQ(1ul, full_hashes.size());
  EXPECT_TRUE(SBFullHashEqual(hash_string, full_hashes[0].hash));
  EXPECT_EQ(ThreatPatternType::MALWARE_LANDING,
            full_hashes[0].metadata.threat_pattern_type);

  // Test invalid pattern type.
  FindFullHashesResponse invalid_res;
  invalid_res.mutable_negative_cache_duration()->set_seconds(600);
  ThreatMatch* invalid = invalid_res.add_matches();
  invalid->set_threat_type(POTENTIALLY_HARMFUL_APPLICATION);
  invalid->set_threat_entry_type(URL);
  invalid->set_platform_type(CHROME_PLATFORM);
  invalid->mutable_threat()->set_hash(SBFullHashToString(hash_string));
  ThreatEntryMetadata::MetadataEntry* invalid_meta =
      invalid->mutable_threat_entry_metadata()->add_entries();
  invalid_meta->set_key("pha_pattern_type");
  invalid_meta->set_value("INVALIDE_VALUE");

  std::string invalid_data;
  invalid_res.SerializeToString(&invalid_data);
  full_hashes.clear();
  EXPECT_FALSE(
      pm->ParseHashResponse(invalid_data, &full_hashes, &cache_expire));
  EXPECT_EQ(0ul, full_hashes.size());
}

// Adds metadata with a key value that is not "permission".
TEST_F(SafeBrowsingV4GetHashProtocolManagerTest,
       TestParseHashResponseNonPermissionMetadata) {
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

  base::Time now = base::Time::UnixEpoch();
  SetTestClock(now, pm.get());

  FindFullHashesResponse res;
  res.mutable_negative_cache_duration()->set_seconds(600);
  ThreatMatch* m = res.add_matches();
  m->set_threat_type(API_ABUSE);
  m->set_platform_type(CHROME_PLATFORM);
  m->set_threat_entry_type(URL);
  m->mutable_threat()->set_hash(
      SBFullHashToString(SBFullHashForString("Not to fret.")));
  ThreatEntryMetadata::MetadataEntry* e =
      m->mutable_threat_entry_metadata()->add_entries();
  e->set_key("notpermission");
  e->set_value("NOTGEOLOCATION");

  // Serialize.
  std::string res_data;
  res.SerializeToString(&res_data);

  std::vector<SBFullHashResult> full_hashes;
  base::Time cache_expire;
  EXPECT_FALSE(pm->ParseHashResponse(res_data, &full_hashes, &cache_expire));

  EXPECT_EQ(now + base::TimeDelta::FromSeconds(600), cache_expire);
  EXPECT_EQ(0ul, full_hashes.size());
}

TEST_F(SafeBrowsingV4GetHashProtocolManagerTest,
       TestParseHashResponseInconsistentThreatTypes) {
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

  FindFullHashesResponse res;
  res.mutable_negative_cache_duration()->set_seconds(600);
  ThreatMatch* m1 = res.add_matches();
  m1->set_threat_type(API_ABUSE);
  m1->set_platform_type(CHROME_PLATFORM);
  m1->set_threat_entry_type(URL);
  m1->mutable_threat()->set_hash(
      SBFullHashToString(SBFullHashForString("Everything's shiny, Cap'n.")));
  m1->mutable_threat_entry_metadata()->add_entries();
  ThreatMatch* m2 = res.add_matches();
  m2->set_threat_type(MALWARE_THREAT);
  m2->set_threat_entry_type(URL);
  m2->mutable_threat()->set_hash(
      SBFullHashToString(SBFullHashForString("Not to fret.")));

  // Serialize.
  std::string res_data;
  res.SerializeToString(&res_data);

  std::vector<SBFullHashResult> full_hashes;
  base::Time cache_expire;
  EXPECT_FALSE(pm->ParseHashResponse(res_data, &full_hashes, &cache_expire));
}

}  // namespace safe_browsing
