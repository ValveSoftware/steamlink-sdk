// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_GET_HASH_PROTOCOL_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_GET_HASH_PROTOCOL_MANAGER_H_

// A class that implements Chrome's interface with the SafeBrowsing V4 protocol.
//
// The V4GetHashProtocolManager handles formatting and making requests of, and
// handling responses from, Google's SafeBrowsing servers. The purpose of this
// class is to get full hash matches from the SB server for the given set of
// hash prefixes.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/safe_browsing_db/safebrowsing.pb.h"
#include "components/safe_browsing_db/util.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"
#include "net/url_request/url_fetcher_delegate.h"

class GURL;

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}  // namespace net

namespace safe_browsing {

// The matching hash prefixes and corresponding stores, for each full hash
// generated for a given URL.
typedef base::hash_map<FullHash, StoreAndHashPrefixes>
    FullHashToStoreAndHashPrefixesMap;

// ----------------------------------------------------------------

// All information about a particular full hash i.e. negative TTL, store for
// which it is valid, and metadata associated with that store.
struct FullHashInfo {
 public:
  FullHash full_hash;

  // The list for which this full hash is applicable.
  ListIdentifier list_id;

  // The expiration time of the full hash for a particular store.
  base::Time positive_expiry;

  // Any metadata for this full hash for a particular store.
  ThreatMetadata metadata;

  FullHashInfo(const FullHash& full_hash,
               const ListIdentifier& list_id,
               const base::Time& positive_expiry);
  FullHashInfo(const FullHashInfo& other);
  ~FullHashInfo();

  bool operator==(const FullHashInfo& other) const;
  bool operator!=(const FullHashInfo& other) const;

 private:
  FullHashInfo();
};

// Caches individual response from GETHASH response.
struct CachedHashPrefixInfo {
  // The negative TTL for the hash prefix that leads to this
  // CachedHashPrefixInfo. The client should not send any more requests for that
  // hash prefix until this time.
  base::Time negative_expiry;

  // The list of all full hashes (and related info) that start with a
  // particular hash prefix and are known to be unsafe.
  std::vector<FullHashInfo> full_hash_infos;

  CachedHashPrefixInfo();
  CachedHashPrefixInfo(const CachedHashPrefixInfo& other);
  ~CachedHashPrefixInfo();
};

// Cached full hashes received from the server for the corresponding hash
// prefixes.
typedef base::hash_map<HashPrefix, CachedHashPrefixInfo> FullHashCache;

// FullHashCallback is invoked when GetFullHashes completes. The parameter is
// the vector of full hash results. If empty, indicates that there were no
// matches, and that the resource is safe.
typedef base::Callback<void(const std::vector<FullHashInfo>&)> FullHashCallback;

// Information needed to update the cache and call the callback to post the
// results.
struct FullHashCallbackInfo {
  FullHashCallbackInfo();
  FullHashCallbackInfo(const std::vector<FullHashInfo>& cached_full_hash_infos,
                       const std::vector<HashPrefix>& prefixes_requested,
                       std::unique_ptr<net::URLFetcher> fetcher,
                       const FullHashToStoreAndHashPrefixesMap&
                           full_hash_to_store_and_hash_prefixes,
                       const FullHashCallback& callback,
                       const base::Time& network_start_time);
  ~FullHashCallbackInfo();

  // The FullHashInfo objects retrieved from cache. These are merged with the
  // results received from the server before invoking the callback.
  std::vector<FullHashInfo> cached_full_hash_infos;

  // The callback method to call after collecting the full hashes for given
  // hash prefixes.
  FullHashCallback callback;

  // The fetcher that will return the response from the server. This is stored
  // here as a unique pointer to be able to reason about its lifetime easily.
  std::unique_ptr<net::URLFetcher> fetcher;

  // The generated full hashes and the corresponding prefixes and the stores in
  // which to look for a full hash match.
  FullHashToStoreAndHashPrefixesMap full_hash_to_store_and_hash_prefixes;

  // Used to measure how long did it take to fetch the full hash response from
  // the server.
  base::Time network_start_time;

  // The prefixes that were requested from the server.
  std::vector<HashPrefix> prefixes_requested;
};

// ----------------------------------------------------------------

class V4GetHashProtocolManagerFactory;

class V4GetHashProtocolManager : public net::URLFetcherDelegate,
                                 public base::NonThreadSafe {
 public:
  // Invoked when GetFullHashesWithApis completes.
  // Parameters:
  //   - The API threat metadata for the given URL.
  typedef base::Callback<void(const ThreatMetadata& md)>
      ThreatMetadataForApiCallback;

  ~V4GetHashProtocolManager() override;

  // Create an instance of the safe browsing v4 protocol manager.
  static std::unique_ptr<V4GetHashProtocolManager> Create(
      net::URLRequestContextGetter* request_context_getter,
      const StoresToCheck& stores_to_check,
      const V4ProtocolConfig& config);

  // Makes the passed |factory| the factory used to instantiate
  // a V4GetHashProtocolManager. Useful for tests.
  static void RegisterFactory(
      std::unique_ptr<V4GetHashProtocolManagerFactory> factory);

  // Empties the cache.
  void ClearCache();

  // Retrieve the full hash for a set of prefixes, and invoke the callback
  // argument when the results are retrieved. The callback may be invoked
  // synchronously.
  virtual void GetFullHashes(const FullHashToStoreAndHashPrefixesMap&
                                 full_hash_to_matching_hash_prefixes,
                             FullHashCallback callback);

  // Retrieve the full hash and API metadata for the origin of |url|, and invoke
  // the callback argument when the results are retrieved. The callback may be
  // invoked synchronously.
  // GetFullHashesWithApis is a special case of GetFullHashes. It is here
  // primarily for legacy reasons: so that DatabaseManager, which speaks PVer3,
  // and V4LocalDatabaseManager, which speaks PVer4, can both use this class to
  // perform API lookups. Once PVer4 migration is complete, DatabaseManager
  // should be deleted and then this method can be moved to the
  // V4LocalDatabaseManager class.
  // TODO(vakh): Move this method to V4LocalDatabaseManager after launching
  // PVer4 in Chromium.
  virtual void GetFullHashesWithApis(const GURL& url,
                                     ThreatMetadataForApiCallback api_callback);

  // net::URLFetcherDelegate interface.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

 protected:
  // Constructs a V4GetHashProtocolManager that issues network requests using
  // |request_context_getter|.
  V4GetHashProtocolManager(net::URLRequestContextGetter* request_context_getter,
                           const StoresToCheck& stores_to_check,
                           const V4ProtocolConfig& config);

 private:
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest, TestGetHashRequest);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest, TestParseHashResponse);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest,
                           TestParseHashResponseWrongThreatEntryType);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest,
                           TestParseHashThreatPatternType);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest,
                           TestParseHashResponseNonPermissionMetadata);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest,
                           TestParseHashResponseInconsistentThreatTypes);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest,
                           TestGetHashErrorHandlingOK);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest,
                           TestResultsNotCachedForNegativeCacheDuration);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest,
                           TestGetHashErrorHandlingNetwork);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest,
                           TestGetHashErrorHandlingResponseCode);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest, GetCachedResults);
  FRIEND_TEST_ALL_PREFIXES(V4GetHashProtocolManagerTest, TestUpdatesAreMerged);
  friend class V4GetHashProtocolManagerTest;
  friend class V4GetHashProtocolManagerFactoryImpl;

  FullHashCache* full_hash_cache_for_tests() { return &full_hash_cache_; }

  // Looks up the cached results for full hashes in
  // |full_hash_to_store_and_hash_prefixes|. Fills |prefixes_to_request| with
  // the prefixes that need to be requested. Fills |cached_full_hash_infos|
  // with the cached results.
  // Note: It is valid for both |prefixes_to_request| and
  // |cached_full_hash_infos| to be empty after this function finishes.
  void GetFullHashCachedResults(
      const FullHashToStoreAndHashPrefixesMap&
          full_hash_to_store_and_hash_prefixes,
      const base::Time& now,
      std::vector<HashPrefix>* prefixes_to_request,
      std::vector<FullHashInfo>* cached_full_hash_infos) const;

  // Fills a FindFullHashesRequest protocol buffer for a request.
  // Returns the serialized and base 64 encoded request as a string.
  std::string GetHashRequest(
      const std::vector<HashPrefix>& prefixes_to_request);

  void GetHashUrlAndHeaders(const std::string& request_base64,
                            GURL* gurl,
                            net::HttpRequestHeaders* headers) const;

  // Updates internal state for each GetHash response error, assuming that
  // the current time is |now|.
  void HandleGetHashError(const base::Time& now);

  // Merges the results from the cache and the results from the server. The
  // response from the server may include information for full hashes from
  // stores other than those required by this client so it filters out those
  // results that the client did not ask for.
  void MergeResults(const FullHashToStoreAndHashPrefixesMap&
                        full_hash_to_store_and_hash_prefixes,
                    const std::vector<FullHashInfo>& full_hash_infos,
                    std::vector<FullHashInfo>* merged_full_hash_infos);

  // Calls |api_callback| with an object of ThreatMetadata that contains
  // permission API metadata for full hashes in those |full_hash_infos| that
  // have a full hash in |full_hashes|.
  void OnFullHashForApi(const ThreatMetadataForApiCallback& api_callback,
                        const std::vector<FullHash>& full_hashes,
                        const std::vector<FullHashInfo>& full_hash_infos);

  // Parses a FindFullHashesResponse protocol buffer and fills the results in
  // |full_hash_infos| and |negative_cache_expire|. |response_data| is a
  // serialized FindFullHashes protocol buffer. |negative_cache_expire| is the
  // cache expiry time of the hash prefixes that were requested. Returns true if
  // parsing is successful; false otherwise.
  bool ParseHashResponse(const std::string& response_data,
                         std::vector<FullHashInfo>* full_hash_infos,
                         base::Time* negative_cache_expire);

  // Parses the store specific |metadata| information from |match|. Logs errors
  // to UMA if the metadata information was not parsed correctly or was
  // inconsistent with what's expected from that corresponding store.
  static void ParseMetadata(const ThreatMatch& match, ThreatMetadata* metadata);

  // Resets the gethash error counter and multiplier.
  void ResetGetHashErrors();

  // Overrides the clock used to check the time.
  void SetClockForTests(std::unique_ptr<base::Clock> clock);

  // Updates the state of the full hash cache upon receiving a valid response
  // from the server.
  void UpdateCache(const std::vector<HashPrefix>& prefixes_requested,
                   const std::vector<FullHashInfo>& full_hash_infos,
                   const base::Time& negative_cache_expire);

 private:
  // Map of GetHash requests to parameters which created it.
  using PendingHashRequests =
      base::hash_map<const net::URLFetcher*,
                     std::unique_ptr<FullHashCallbackInfo>>;

  // The factory that controls the creation of V4GetHashProtocolManager.
  // This is used by tests.
  static V4GetHashProtocolManagerFactory* factory_;

  // Current active request (in case we need to cancel) for updates or chunks
  // from the SafeBrowsing service. We can only have one of these outstanding
  // at any given time unlike GetHash requests, which are tracked separately.
  std::unique_ptr<net::URLFetcher> request_;

  // The number of HTTP response errors since the the last successful HTTP
  // response, used for request backoff timing.
  size_t gethash_error_count_;

  // Multiplier for the backoff error after the second.
  size_t gethash_back_off_mult_;

  PendingHashRequests pending_hash_requests_;

  // For v4, the next gethash time is set to the backoff time is the last
  // response was an error, or the minimum wait time if the last response was
  // successful.
  base::Time next_gethash_time_;

  // The config of the client making Pver4 requests.
  const V4ProtocolConfig config_;

  // The context we use to issue network requests.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // ID for URLFetchers for testing.
  int url_fetcher_id_;

  // The clock used to vend times.
  std::unique_ptr<base::Clock> clock_;

  // A cache of full hash results.
  FullHashCache full_hash_cache_;

  // The following sets represent the combination of lists that we would always
  // request from the server, irrespective of which list we found the hash
  // prefix match in.
  std::vector<PlatformType> platform_types_;
  std::vector<ThreatEntryType> threat_entry_types_;
  std::vector<ThreatType> threat_types_;

  DISALLOW_COPY_AND_ASSIGN(V4GetHashProtocolManager);
};

// Interface of a factory to create V4GetHashProtocolManager.  Useful for tests.
class V4GetHashProtocolManagerFactory {
 public:
  V4GetHashProtocolManagerFactory() {}
  virtual ~V4GetHashProtocolManagerFactory() {}
  virtual std::unique_ptr<V4GetHashProtocolManager> CreateProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const StoresToCheck& stores_to_check,
      const V4ProtocolConfig& config) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(V4GetHashProtocolManagerFactory);
};

#ifndef NDEBUG
std::ostream& operator<<(std::ostream& os, const FullHashInfo& id);
#endif

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_GET_HASH_PROTOCOL_MANAGER_H_
