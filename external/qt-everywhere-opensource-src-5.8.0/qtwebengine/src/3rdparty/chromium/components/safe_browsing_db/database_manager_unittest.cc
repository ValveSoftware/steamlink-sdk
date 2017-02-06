// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/database_manager.h"

#include <stddef.h>

#include <set>
#include <string>
#include <vector>

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/safe_browsing_db/test_database_manager.h"
#include "components/safe_browsing_db/v4_get_hash_protocol_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace safe_browsing {

namespace {

void InvokeFullHashCallback(
    V4GetHashProtocolManager::FullHashCallback callback,
    const std::vector<SBFullHashResult>& full_hashes,
    base::Time negative_cache_expire) {
  callback.Run(full_hashes, negative_cache_expire);
}

// A TestV4GetHashProtocolManager that returns fixed responses from the
// Safe Browsing server for testing purpose.
class TestV4GetHashProtocolManager : public V4GetHashProtocolManager {
 public:
  TestV4GetHashProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config)
      : V4GetHashProtocolManager(request_context_getter, config),
        negative_cache_expire_(base::Time()), delay_seconds_(0) {}

  ~TestV4GetHashProtocolManager() override {}

  void GetFullHashesWithApis(const std::vector<SBPrefix>& prefixes,
                             FullHashCallback callback) override {
    prefixes_ = prefixes;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(InvokeFullHashCallback, callback, full_hashes_,
                              negative_cache_expire_),
        base::TimeDelta::FromSeconds(delay_seconds_));
  }

  void SetDelaySeconds(int delay) {
    delay_seconds_ = delay;
  }

  void SetNegativeCacheDurationMins(base::Time now,
      int negative_cache_duration_mins) {
    // Don't add a TimeDelta to the maximum time to avoid undefined behavior.
    negative_cache_expire_ = now.is_max() ? now :
        now + base::TimeDelta::FromMinutes(negative_cache_duration_mins);
  }

  // Prepare the GetFullHash results for the next request.
  void AddGetFullHashResponse(const SBFullHashResult& full_hash_result) {
    full_hashes_.push_back(full_hash_result);
  }

  // Clear the GetFullHash results for the next request.
  void ClearFullHashResponse() {
    full_hashes_.clear();
  }

  // Returns the prefixes that were sent in the last request.
  const std::vector<SBPrefix>& GetRequestPrefixes() { return prefixes_; }

 private:
  std::vector<SBPrefix> prefixes_;
  std::vector<SBFullHashResult> full_hashes_;
  base::Time negative_cache_expire_;
  int delay_seconds_;
};

// Factory that creates test protocol manager instances.
class TestV4GetHashProtocolManagerFactory :
    public V4GetHashProtocolManagerFactory {
 public:
  TestV4GetHashProtocolManagerFactory() {}
  ~TestV4GetHashProtocolManagerFactory() override {}

  V4GetHashProtocolManager* CreateProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config) override {
    return new TestV4GetHashProtocolManager(request_context_getter, config);
  }
};

class TestClient : public SafeBrowsingDatabaseManager::Client {
 public:
  TestClient() : callback_invoked_(false) {}
  ~TestClient() override {}

  void OnCheckApiBlacklistUrlResult(const GURL& url,
                                    const ThreatMetadata& metadata) override {
    blocked_permissions_ = metadata.api_permissions;
    callback_invoked_ = true;
  }

  const std::set<std::string>& GetBlockedPermissions() {
    return blocked_permissions_;
  }

  bool callback_invoked() {return callback_invoked_;}

 private:
  std::set<std::string> blocked_permissions_;
  bool callback_invoked_;
  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

}  // namespace

class SafeBrowsingDatabaseManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    V4GetHashProtocolManager::RegisterFactory(
        base::WrapUnique(new TestV4GetHashProtocolManagerFactory()));

    db_manager_ = new TestSafeBrowsingDatabaseManager();
    db_manager_->StartOnIOThread(NULL, V4ProtocolConfig());
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    db_manager_->StopOnIOThread(false);
    V4GetHashProtocolManager::RegisterFactory(nullptr);
  }

  scoped_refptr<SafeBrowsingDatabaseManager> db_manager_;

 private:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
};

TEST_F(SafeBrowsingDatabaseManagerTest, CheckApiBlacklistUrlWrongScheme) {
  TestClient client;
  const GURL url("file://example.txt");
  EXPECT_TRUE(db_manager_->CheckApiBlacklistUrl(url, &client));
}

TEST_F(SafeBrowsingDatabaseManagerTest, CheckApiBlacklistUrlPrefixes) {
  TestClient client;
  const GURL url("https://www.example.com/more");
  // Generated from the sorted output of UrlToFullHashes in util.h.
  std::vector<SBPrefix> expected_prefixes =
      {1237562338, 2871045197, 3553205461, 3766933875};

  TestV4GetHashProtocolManager* pm = static_cast<TestV4GetHashProtocolManager*>(
      db_manager_->v4_get_hash_protocol_manager_);
  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  base::RunLoop().RunUntilIdle();
  std::vector<SBPrefix> prefixes = pm->GetRequestPrefixes();
  EXPECT_EQ(expected_prefixes.size(), prefixes.size());
  for (unsigned int i = 0; i < prefixes.size(); ++i) {
    EXPECT_EQ(expected_prefixes[i], prefixes[i]);
  }
}

TEST_F(SafeBrowsingDatabaseManagerTest, HandleGetHashesWithApisResults) {
  TestClient client;
  const GURL url("https://www.example.com/more");
  TestV4GetHashProtocolManager* pm = static_cast<TestV4GetHashProtocolManager*>(
      db_manager_->v4_get_hash_protocol_manager_);
  SBFullHashResult full_hash_result;
  full_hash_result.hash = SBFullHashForString("example.com/");
  full_hash_result.metadata.api_permissions.insert("GEOLOCATION");
  pm->AddGetFullHashResponse(full_hash_result);

  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(client.callback_invoked());
  const std::set<std::string>& permissions = client.GetBlockedPermissions();
  EXPECT_EQ(1ul, permissions.size());
  EXPECT_EQ(1ul, permissions.count("GEOLOCATION"));
}

TEST_F(SafeBrowsingDatabaseManagerTest, HandleGetHashesWithApisResultsNoMatch) {
  TestClient client;
  const GURL url("https://www.example.com/more");
  TestV4GetHashProtocolManager* pm = static_cast<TestV4GetHashProtocolManager*>(
      db_manager_->v4_get_hash_protocol_manager_);
  SBFullHashResult full_hash_result;
  full_hash_result.hash = SBFullHashForString("wrongexample.com/");
  full_hash_result.metadata.api_permissions.insert("GEOLOCATION");
  pm->AddGetFullHashResponse(full_hash_result);

  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(client.callback_invoked());
  const std::set<std::string>& permissions = client.GetBlockedPermissions();
  EXPECT_EQ(0ul, permissions.size());
}

TEST_F(SafeBrowsingDatabaseManagerTest, HandleGetHashesWithApisResultsMatches) {
  TestClient client;
  const GURL url("https://www.example.com/more");
  TestV4GetHashProtocolManager* pm = static_cast<TestV4GetHashProtocolManager*>(
      db_manager_->v4_get_hash_protocol_manager_);
  SBFullHashResult full_hash_result;
  full_hash_result.hash = SBFullHashForString("example.com/");
  full_hash_result.metadata.api_permissions.insert("GEOLOCATION");
  pm->AddGetFullHashResponse(full_hash_result);
  SBFullHashResult full_hash_result2;
  full_hash_result2.hash = SBFullHashForString("example.com/more");
  full_hash_result2.metadata.api_permissions.insert("NOTIFICATIONS");
  pm->AddGetFullHashResponse(full_hash_result2);
  SBFullHashResult full_hash_result3;
  full_hash_result3.hash = SBFullHashForString("wrongexample.com/");
  full_hash_result3.metadata.api_permissions.insert("AUDIO_CAPTURE");
  pm->AddGetFullHashResponse(full_hash_result3);

  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(client.callback_invoked());
  const std::set<std::string>& permissions = client.GetBlockedPermissions();
  EXPECT_EQ(2ul, permissions.size());
  EXPECT_EQ(1ul, permissions.count("GEOLOCATION"));
  EXPECT_EQ(1ul, permissions.count("NOTIFICATIONS"));
}

TEST_F(SafeBrowsingDatabaseManagerTest, CancelApiCheck) {
  TestClient client;
  const GURL url("https://www.example.com/more");
  TestV4GetHashProtocolManager* pm = static_cast<TestV4GetHashProtocolManager*>(
      db_manager_->v4_get_hash_protocol_manager_);
  SBFullHashResult full_hash_result;
  full_hash_result.hash = SBFullHashForString("example.com/");
  full_hash_result.metadata.api_permissions.insert("GEOLOCATION");
  pm->AddGetFullHashResponse(full_hash_result);
  pm->SetDelaySeconds(100);

  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  EXPECT_TRUE(db_manager_->CancelApiCheck(&client));
  base::RunLoop().RunUntilIdle();

  const std::set<std::string>& permissions = client.GetBlockedPermissions();
  EXPECT_EQ(0ul, permissions.size());
  EXPECT_FALSE(client.callback_invoked());
}

TEST_F(SafeBrowsingDatabaseManagerTest, ResultsAreCached) {
  TestClient client;
  const GURL url("https://www.example.com/more");
  TestV4GetHashProtocolManager* pm = static_cast<TestV4GetHashProtocolManager*>(
      db_manager_->v4_get_hash_protocol_manager_);
  base::Time now = base::Time::UnixEpoch();
  SBFullHashResult full_hash_result;
  full_hash_result.hash = SBFullHashForString("example.com/");
  full_hash_result.metadata.api_permissions.insert("GEOLOCATION");
  full_hash_result.cache_expire_after = now + base::TimeDelta::FromMinutes(3);
  pm->AddGetFullHashResponse(full_hash_result);
  pm->SetNegativeCacheDurationMins(now, 5);

  EXPECT_TRUE(db_manager_->v4_full_hash_cache_.empty());
  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(client.callback_invoked());
  const std::set<std::string>& permissions = client.GetBlockedPermissions();
  EXPECT_EQ(1ul, permissions.size());
  EXPECT_EQ(1ul, permissions.count("GEOLOCATION"));

  // Check the cache.
  const SafeBrowsingDatabaseManager::PrefixToFullHashResultsMap& cache =
      db_manager_->v4_full_hash_cache_[SB_THREAT_TYPE_API_ABUSE];
  // Generated from the sorted output of UrlToFullHashes in util.h.
  std::vector<SBPrefix> expected_prefixes =
      {1237562338, 2871045197, 3553205461, 3766933875};
  EXPECT_EQ(expected_prefixes.size(),
            db_manager_->v4_full_hash_cache_[SB_THREAT_TYPE_API_ABUSE].size());

  auto entry = cache.find(expected_prefixes[0]);
  EXPECT_NE(cache.end(), entry);
  EXPECT_EQ(now + base::TimeDelta::FromMinutes(5), entry->second.expire_after);
  EXPECT_EQ(0ul, entry->second.full_hashes.size());

  entry = cache.find(expected_prefixes[1]);
  EXPECT_NE(cache.end(), entry);
  EXPECT_EQ(now + base::TimeDelta::FromMinutes(5), entry->second.expire_after);
  EXPECT_EQ(0ul, entry->second.full_hashes.size());

  entry = cache.find(expected_prefixes[2]);
  EXPECT_NE(cache.end(), entry);
  EXPECT_EQ(now + base::TimeDelta::FromMinutes(5), entry->second.expire_after);
  EXPECT_EQ(0ul, entry->second.full_hashes.size());

  entry = cache.find(expected_prefixes[3]);
  EXPECT_NE(cache.end(), entry);
  EXPECT_EQ(now + base::TimeDelta::FromMinutes(5), entry->second.expire_after);
  EXPECT_EQ(1ul, entry->second.full_hashes.size());
  EXPECT_TRUE(SBFullHashEqual(full_hash_result.hash,
                              entry->second.full_hashes[0].hash));
  EXPECT_EQ(1ul, entry->second.full_hashes[0].metadata.api_permissions.size());
  EXPECT_EQ(1ul, entry->second.full_hashes[0].metadata.api_permissions.
      count("GEOLOCATION"));
  EXPECT_EQ(full_hash_result.cache_expire_after,
            entry->second.full_hashes[0].cache_expire_after);
}

// An uninitialized value for negative cache expire does not cache results.
TEST_F(SafeBrowsingDatabaseManagerTest, ResultsAreNotCachedOnNull) {
  TestClient client;
  const GURL url("https://www.example.com/more");
  TestV4GetHashProtocolManager* pm = static_cast<TestV4GetHashProtocolManager*>(
      db_manager_->v4_get_hash_protocol_manager_);
  base::Time now = base::Time::UnixEpoch();
  SBFullHashResult full_hash_result;
  full_hash_result.hash = SBFullHashForString("example.com/");
  full_hash_result.cache_expire_after = now + base::TimeDelta::FromMinutes(3);
  pm->AddGetFullHashResponse(full_hash_result);

  EXPECT_TRUE(db_manager_->v4_full_hash_cache_.empty());
  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(client.callback_invoked());
  EXPECT_TRUE(
      db_manager_->v4_full_hash_cache_[SB_THREAT_TYPE_API_ABUSE].empty());
}

// Checks that results are looked up correctly in the cache.
TEST_F(SafeBrowsingDatabaseManagerTest, GetCachedResults) {
  base::Time now = base::Time::UnixEpoch();
  std::vector<SBFullHash> full_hashes;
  SBFullHash full_hash = SBFullHashForString("example.com/");
  full_hashes.push_back(full_hash);
  std::vector<SBFullHashResult> cached_results;
  std::vector<SBPrefix> prefixes;
  db_manager_->GetFullHashCachedResults(SB_THREAT_TYPE_API_ABUSE,
      full_hashes, now, &prefixes, &cached_results);

  // The cache is empty.
  EXPECT_TRUE(cached_results.empty());
  EXPECT_EQ(1ul, prefixes.size());
  EXPECT_EQ(full_hash.prefix, prefixes[0]);

  // Prefix has a cache entry but full hash is not there.
  SBCachedFullHashResult& entry = db_manager_->
      v4_full_hash_cache_[SB_THREAT_TYPE_API_ABUSE][full_hash.prefix] =
      SBCachedFullHashResult(now + base::TimeDelta::FromMinutes(5));
  db_manager_->GetFullHashCachedResults(SB_THREAT_TYPE_API_ABUSE,
      full_hashes, now, &prefixes, &cached_results);

  EXPECT_TRUE(prefixes.empty());
  EXPECT_TRUE(cached_results.empty());

  // Expired negative cache entry.
  entry.expire_after = now - base::TimeDelta::FromMinutes(5);
  db_manager_->GetFullHashCachedResults(SB_THREAT_TYPE_API_ABUSE,
      full_hashes, now, &prefixes, &cached_results);

  EXPECT_TRUE(cached_results.empty());
  EXPECT_EQ(1ul, prefixes.size());
  EXPECT_EQ(full_hash.prefix, prefixes[0]);

  // Now put the full hash in the cache.
  SBFullHashResult full_hash_result;
  full_hash_result.hash = full_hash;
  full_hash_result.cache_expire_after = now + base::TimeDelta::FromMinutes(3);
  entry.full_hashes.push_back(full_hash_result);
  db_manager_->GetFullHashCachedResults(SB_THREAT_TYPE_API_ABUSE,
      full_hashes, now, &prefixes, &cached_results);

  EXPECT_TRUE(prefixes.empty());
  EXPECT_EQ(1ul, cached_results.size());
  EXPECT_TRUE(SBFullHashEqual(full_hash, cached_results[0].hash));

  // Expired full hash in cache.
  entry.full_hashes.clear();
  full_hash_result.cache_expire_after = now - base::TimeDelta::FromMinutes(3);
  entry.full_hashes.push_back(full_hash_result);
  db_manager_->GetFullHashCachedResults(SB_THREAT_TYPE_API_ABUSE,
      full_hashes, now, &prefixes, &cached_results);

  EXPECT_TRUE(cached_results.empty());
  EXPECT_EQ(1ul, prefixes.size());
  EXPECT_EQ(full_hash.prefix, prefixes[0]);
}

// Checks that the cached results and request results are merged.
TEST_F(SafeBrowsingDatabaseManagerTest, CachedResultsMerged) {
  TestClient client;
  const GURL url("https://www.example.com/more");
  TestV4GetHashProtocolManager* pm = static_cast<TestV4GetHashProtocolManager*>(
      db_manager_->v4_get_hash_protocol_manager_);
  // Set now to max time so the cache expire times are in the future.
  SBFullHashResult full_hash_result;
  full_hash_result.hash = SBFullHashForString("example.com/");
  full_hash_result.metadata.api_permissions.insert("GEOLOCATION");
  full_hash_result.cache_expire_after = base::Time::Max();
  pm->AddGetFullHashResponse(full_hash_result);
  pm->SetNegativeCacheDurationMins(base::Time::Max(), 0);

  EXPECT_TRUE(db_manager_->v4_full_hash_cache_.empty());
  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(client.callback_invoked());
  const std::set<std::string>& permissions = client.GetBlockedPermissions();
  EXPECT_EQ(1ul, permissions.size());
  EXPECT_EQ(1ul, permissions.count("GEOLOCATION"));

  // The results should be cached, so remove them from the protocol manager
  // response.
  TestClient client2;
  pm->ClearFullHashResponse();
  pm->SetNegativeCacheDurationMins(base::Time(), 0);
  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client2));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(client2.callback_invoked());
  const std::set<std::string>& permissions2 =
      client2.GetBlockedPermissions();
  EXPECT_EQ(1ul, permissions2.size());
  EXPECT_EQ(1ul, permissions2.count("GEOLOCATION"));

  // Add a different result to the protocol manager response and ensure it is
  // merged with the cached result in the metadata.
  TestClient client3;
  const GURL url2("https://m.example.com/more");
  full_hash_result.hash = SBFullHashForString("m.example.com/");
  full_hash_result.metadata.api_permissions.insert("NOTIFICATIONS");
  pm->AddGetFullHashResponse(full_hash_result);
  pm->SetNegativeCacheDurationMins(base::Time::Max(), 0);
  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url2, &client3));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(client3.callback_invoked());
  const std::set<std::string>& permissions3 =
      client3.GetBlockedPermissions();
  EXPECT_EQ(2ul, permissions3.size());
  EXPECT_EQ(1ul, permissions3.count("GEOLOCATION"));
  EXPECT_EQ(1ul, permissions3.count("NOTIFICATIONS"));
}

TEST_F(SafeBrowsingDatabaseManagerTest, CachedResultsAreEvicted) {
  base::Time epoch = base::Time::UnixEpoch();
  SBFullHashResult full_hash_result;
  full_hash_result.hash = SBFullHashForString("example.com/");
  full_hash_result.cache_expire_after = epoch;

  SafeBrowsingDatabaseManager::PrefixToFullHashResultsMap& cache =
      db_manager_->v4_full_hash_cache_[SB_THREAT_TYPE_API_ABUSE];

  // Fill the cache with some expired entries.
  // Both negative cache and full hash expired.
  cache[full_hash_result.hash.prefix] = SBCachedFullHashResult(epoch);
  cache[full_hash_result.hash.prefix].full_hashes.push_back(full_hash_result);

  TestClient client;
  const GURL url("https://www.example.com/more");

  EXPECT_EQ(1ul, cache.size());
  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  base::RunLoop().RunUntilIdle();

  // Cache should be empty.
  EXPECT_TRUE(client.callback_invoked());
  EXPECT_TRUE(cache.empty());

  // Negative cache still valid and full hash expired.
  cache[full_hash_result.hash.prefix] =
      SBCachedFullHashResult(base::Time::Max());
  cache[full_hash_result.hash.prefix].full_hashes.push_back(full_hash_result);

  EXPECT_EQ(1ul, cache.size());
  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  base::RunLoop().RunUntilIdle();

  // Cache entry should still be there.
  EXPECT_EQ(1ul, cache.size());
  auto entry = cache.find(full_hash_result.hash.prefix);
  EXPECT_NE(cache.end(), entry);
  EXPECT_EQ(base::Time::Max(), entry->second.expire_after);
  EXPECT_EQ(1ul, entry->second.full_hashes.size());
  EXPECT_TRUE(SBFullHashEqual(full_hash_result.hash,
                              entry->second.full_hashes[0].hash));
  EXPECT_EQ(full_hash_result.cache_expire_after,
            entry->second.full_hashes[0].cache_expire_after);

  // Negative cache still valid and full hash still valid.
  cache[full_hash_result.hash.prefix].full_hashes[0].
      cache_expire_after = base::Time::Max();

  EXPECT_EQ(1ul, cache.size());
  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  base::RunLoop().RunUntilIdle();

  // Cache entry should still be there.
  EXPECT_EQ(1ul, cache.size());
  entry = cache.find(full_hash_result.hash.prefix);
  EXPECT_NE(cache.end(), entry);
  EXPECT_EQ(base::Time::Max(), entry->second.expire_after);
  EXPECT_EQ(1ul, entry->second.full_hashes.size());
  EXPECT_TRUE(SBFullHashEqual(full_hash_result.hash,
                              entry->second.full_hashes[0].hash));
  EXPECT_EQ(base::Time::Max(),
            entry->second.full_hashes[0].cache_expire_after);

  // Negative cache expired and full hash still valid.
  cache[full_hash_result.hash.prefix].expire_after = epoch;

  EXPECT_EQ(1ul, cache.size());
  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  base::RunLoop().RunUntilIdle();

  // Cache entry should still be there.
  EXPECT_EQ(1ul, cache.size());
  entry = cache.find(full_hash_result.hash.prefix);
  EXPECT_NE(cache.end(), entry);
  EXPECT_EQ(epoch, entry->second.expire_after);
  EXPECT_EQ(1ul, entry->second.full_hashes.size());
  EXPECT_TRUE(SBFullHashEqual(full_hash_result.hash,
                              entry->second.full_hashes[0].hash));
  EXPECT_EQ(base::Time::Max(),
            entry->second.full_hashes[0].cache_expire_after);
}

}  // namespace safe_browsing
