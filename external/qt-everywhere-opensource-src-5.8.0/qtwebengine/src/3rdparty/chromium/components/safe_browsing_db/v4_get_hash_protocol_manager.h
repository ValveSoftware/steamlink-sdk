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

class V4GetHashProtocolManagerFactory;

class V4GetHashProtocolManager : public net::URLFetcherDelegate,
                                 public base::NonThreadSafe {
 public:
  // FullHashCallback is invoked when GetFullHashes completes.
  // Parameters:
  //   - The vector of full hash results. If empty, indicates that there
  //     were no matches, and that the resource is safe.
  //   - The negative cache expire time of the result. This value may be
  //     uninitialized, and the results should not be cached in this case.
  typedef base::Callback<void(const std::vector<SBFullHashResult>&,
                              const base::Time&)>
      FullHashCallback;

  ~V4GetHashProtocolManager() override;

  // Makes the passed |factory| the factory used to instantiate
  // a V4GetHashProtocolManager. Useful for tests.
  static void RegisterFactory(
      std::unique_ptr<V4GetHashProtocolManagerFactory> factory);

  // Create an instance of the safe browsing v4 protocol manager.
  static V4GetHashProtocolManager* Create(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config);

  // net::URLFetcherDelegate interface.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Retrieve the full hash for a set of prefixes, and invoke the callback
  // argument when the results are retrieved. The callback may be invoked
  // synchronously.
  virtual void GetFullHashes(const std::vector<SBPrefix>& prefixes,
                             const std::vector<PlatformType>& platforms,
                             ThreatType threat_type,
                             FullHashCallback callback);

  // Retrieve the full hash and API metadata for a set of prefixes, and invoke
  // the callback argument when the results are retrieved. The callback may be
  // invoked synchronously.
  virtual void GetFullHashesWithApis(const std::vector<SBPrefix>& prefixes,
                                     FullHashCallback callback);

  // Overrides the clock used to check the time.
  void SetClockForTests(std::unique_ptr<base::Clock> clock);

 protected:
  // Constructs a V4GetHashProtocolManager that issues
  // network requests using |request_context_getter|.
  V4GetHashProtocolManager(net::URLRequestContextGetter* request_context_getter,
                           const V4ProtocolConfig& config);

 private:
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4GetHashProtocolManagerTest,
                           TestGetHashRequest);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4GetHashProtocolManagerTest,
                           TestParseHashResponse);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4GetHashProtocolManagerTest,
                           TestParseHashResponseWrongThreatEntryType);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4GetHashProtocolManagerTest,
                           TestParseHashThreatPatternType);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4GetHashProtocolManagerTest,
                           TestParseHashResponseNonPermissionMetadata);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4GetHashProtocolManagerTest,
                           TestParseHashResponseInconsistentThreatTypes);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4GetHashProtocolManagerTest,
                           TestGetHashErrorHandlingOK);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4GetHashProtocolManagerTest,
                           TestGetHashErrorHandlingNetwork);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4GetHashProtocolManagerTest,
                           TestGetHashErrorHandlingResponseCode);
  friend class V4GetHashProtocolManagerFactoryImpl;

  void GetHashUrlAndHeaders(const std::string& request_base64,
                            GURL* gurl,
                            net::HttpRequestHeaders* headers) const;

  // Fills a FindFullHashesRequest protocol buffer for a request.
  // Returns the serialized and base 64 encoded request as a string.
  std::string GetHashRequest(const std::vector<SBPrefix>& prefixes,
                             const std::vector<PlatformType>& platforms,
                             ThreatType threat_type);

  // Parses a FindFullHashesResponse protocol buffer and fills the results in
  // |full_hashes| and |negative_cache_expire|. |data| is a serialized
  // FindFullHashes protocol buffer. |negative_cache_expire| is the cache expiry
  // time of the response for entities that did not match the threat list.
  // Returns true if parsing is successful, false otherwise.
  bool ParseHashResponse(const std::string& data_base64,
                         std::vector<SBFullHashResult>* full_hashes,
                         base::Time* negative_cache_expire);

  // Resets the gethash error counter and multiplier.
  void ResetGetHashErrors();

  // Updates internal state for each GetHash response error, assuming that
  // the current time is |now|.
  void HandleGetHashError(const base::Time& now);

 private:
  // Map of GetHash requests to parameters which created it.
  typedef base::hash_map<const net::URLFetcher*, FullHashCallback> HashRequests;

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

  HashRequests hash_requests_;

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

  DISALLOW_COPY_AND_ASSIGN(V4GetHashProtocolManager);
};

// Interface of a factory to create V4GetHashProtocolManager.  Useful for tests.
class V4GetHashProtocolManagerFactory {
 public:
  V4GetHashProtocolManagerFactory() {}
  virtual ~V4GetHashProtocolManagerFactory() {}
  virtual V4GetHashProtocolManager* CreateProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(V4GetHashProtocolManagerFactory);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_GET_HASH_PROTOCOL_MANAGER_H_
