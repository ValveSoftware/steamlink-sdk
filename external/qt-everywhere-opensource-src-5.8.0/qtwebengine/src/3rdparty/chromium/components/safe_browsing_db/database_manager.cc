// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/database_manager.h"

#include "components/safe_browsing_db/v4_get_hash_protocol_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace safe_browsing {

SafeBrowsingDatabaseManager::SafeBrowsingDatabaseManager()
    : v4_get_hash_protocol_manager_(NULL) {
}

SafeBrowsingDatabaseManager::~SafeBrowsingDatabaseManager() {
  DCHECK(v4_get_hash_protocol_manager_ == NULL);
}

void SafeBrowsingDatabaseManager::StartOnIOThread(
    net::URLRequestContextGetter* request_context_getter,
    const V4ProtocolConfig& config) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  v4_get_hash_protocol_manager_ = V4GetHashProtocolManager::Create(
      request_context_getter, config);
}

// |shutdown| not used. Destroys the v4 protocol managers. This may be called
// multiple times during the life of the DatabaseManager.
// Must be called on IO thread.
void SafeBrowsingDatabaseManager::StopOnIOThread(bool shutdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // This cancels all in-flight GetHash requests.
  if (v4_get_hash_protocol_manager_) {
    delete v4_get_hash_protocol_manager_;
    v4_get_hash_protocol_manager_ = NULL;
  }

  // Delete pending checks, calling back any clients with empty metadata.
  for (auto check : api_checks_) {
    if (check->client()) {
      check->client()->
          OnCheckApiBlacklistUrlResult(check->url(), ThreatMetadata());
    }
  }
  STLDeleteElements(&api_checks_);
}

SafeBrowsingDatabaseManager::ApiCheckSet::iterator
SafeBrowsingDatabaseManager::FindClientApiCheck(Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (ApiCheckSet::iterator it = api_checks_.begin();
      it != api_checks_.end(); ++it) {
    if ((*it)->client() == client) {
      return it;
    }
  }
  return api_checks_.end();
}

bool SafeBrowsingDatabaseManager::CancelApiCheck(Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ApiCheckSet::iterator it = FindClientApiCheck(client);
  if (it != api_checks_.end()) {
    delete *it;
    api_checks_.erase(it);
    return true;
  }
  NOTREACHED();
  return false;
}

bool SafeBrowsingDatabaseManager::CheckApiBlacklistUrl(const GURL& url,
                                                       Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(v4_get_hash_protocol_manager_);

  // Make sure we can check this url.
  if (!(url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme))) {
    return true;
  }

  // There can only be one in-progress check for the same client at a time.
  DCHECK(FindClientApiCheck(client) == api_checks_.end());

  // Compute a list of hashes for this url.
  std::vector<SBFullHash> full_hashes;
  UrlToFullHashes(url, false, &full_hashes);
  if (full_hashes.empty())
    return true;

  // First check the cache.

  // Used to determine cache expiration.
  base::Time now = base::Time::Now();

  std::vector<SBFullHashResult> cached_results;
  std::vector<SBPrefix> prefixes_needing_reqs;
  GetFullHashCachedResults(SB_THREAT_TYPE_API_ABUSE,
      full_hashes, now, &prefixes_needing_reqs, &cached_results);

  if (prefixes_needing_reqs.empty() && cached_results.empty())
    return true;

  SafeBrowsingApiCheck* check =
      new SafeBrowsingApiCheck(url, prefixes_needing_reqs, full_hashes,
                               cached_results, client);
  api_checks_.insert(check);

  if (prefixes_needing_reqs.empty()) {
    // We can call the callback immediately if no prefixes require a request.
    // The |full_hash_results| representing the results fromt eh SB server will
    // be empty.
    std::vector<SBFullHashResult> full_hash_results;
    HandleGetHashesWithApisResults(check, full_hash_results, base::Time());
    return false;
  }

  v4_get_hash_protocol_manager_->GetFullHashesWithApis(prefixes_needing_reqs,
      base::Bind(&SafeBrowsingDatabaseManager::HandleGetHashesWithApisResults,
                 base::Unretained(this), check));

  return false;
}

void SafeBrowsingDatabaseManager::GetFullHashCachedResults(
    const SBThreatType& threat_type,
    const std::vector<SBFullHash>& full_hashes,
    base::Time now,
    std::vector<SBPrefix>* prefixes_needing_reqs,
    std::vector<SBFullHashResult>* cached_results) {
  DCHECK(prefixes_needing_reqs);
  prefixes_needing_reqs->clear();
  DCHECK(cached_results);
  cached_results->clear();

  // Caching behavior is documented here:
  // https://developers.google.com/safe-browsing/v4/caching#about-caching
  //
  // The cache operates as follows:
  // Lookup:
  //     Case 1: The prefix is in the cache.
  //         Case a: The full hash is in the cache.
  //             Case i : The positive full hash result has not expired.
  //                      The result is unsafe and we do not need to send a new
  //                      request.
  //             Case ii: The positive full hash result has expired.
  //                      We need to send a request for full hashes.
  //         Case b: The full hash is not in the cache.
  //             Case i : The negative cache entry has not expired.
  //                      The result is still safe and we do not need to send a
  //                      new request.
  //             Case ii: The negative cache entry has expired.
  //                      We need to send a request for full hashes.
  //     Case 2: The prefix is not in the cache.
  //             We need to send a request for full hashes.
  //
  // Eviction:
  //   SBCachedFullHashResult entries can be removed from the cache only when
  //   the negative cache expire time and the cache expire time of all full
  //   hash results for that prefix have expired.
  //   Individual full hash results can be removed from the prefix's
  //   cache entry if they expire AND their expire time is after the negative
  //   cache expire time.
  for (const SBFullHash& full_hash : full_hashes) {
    auto entry = v4_full_hash_cache_[threat_type].find(full_hash.prefix);
    if (entry != v4_full_hash_cache_[threat_type].end()) {
      // Case 1.
      SBCachedFullHashResult& cache_result = entry->second;

      const SBFullHashResult* found_full_hash = nullptr;
      size_t matched_idx = 0;
      for (const SBFullHashResult& hash_result : cache_result.full_hashes) {
        if (SBFullHashEqual(full_hash, hash_result.hash)) {
          found_full_hash = &hash_result;
          break;
        }
        ++matched_idx;
      }

      if (found_full_hash) {
        // Case a.
        if (found_full_hash->cache_expire_after > now) {
          // Case i.
          cached_results->push_back(*found_full_hash);
        } else {
          // Case ii.
          prefixes_needing_reqs->push_back(full_hash.prefix);
          // If the negative cache expire time has passed, evict this full hash
          // result from the cache.
          if (cache_result.expire_after <= now) {
            cache_result.full_hashes.erase(
                cache_result.full_hashes.begin() + matched_idx);
            // If there are no more full hashes, we can evict the entire entry.
            if (cache_result.full_hashes.empty()) {
              v4_full_hash_cache_[threat_type].erase(entry);
            }
          }
        }
      } else {
        // Case b.
        if (cache_result.expire_after > now) {
          // Case i.
        } else {
          // Case ii.
          prefixes_needing_reqs->push_back(full_hash.prefix);
        }
      }
    } else {
      // Case 2.
      prefixes_needing_reqs->push_back(full_hash.prefix);
    }
  }

  // Multiple full hashes could share a prefix, remove duplicates.
  // TODO(kcarattini): Make |prefixes_needing_reqs| a set.
  std::sort(prefixes_needing_reqs->begin(), prefixes_needing_reqs->end());
  prefixes_needing_reqs->erase(std::unique(prefixes_needing_reqs->begin(),
      prefixes_needing_reqs->end()), prefixes_needing_reqs->end());
}

void SafeBrowsingDatabaseManager::HandleGetHashesWithApisResults(
    SafeBrowsingApiCheck* check,
    const std::vector<SBFullHashResult>& full_hash_results,
    const base::Time& negative_cache_expire) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(check);

  // If the time is uninitialized, don't cache the results.
  if (!negative_cache_expire.is_null()) {
    // Cache the results.
    // Create or reset all cached results for this prefix.
    for (const SBPrefix& prefix : check->prefixes()) {
      v4_full_hash_cache_[SB_THREAT_TYPE_API_ABUSE][prefix] =
          SBCachedFullHashResult(negative_cache_expire);
    }
    // Insert any full hash hits. Note that there may be one, multiple, or no
    // full hashes for any given prefix.
    for (const SBFullHashResult& result : full_hash_results) {
      v4_full_hash_cache_[SB_THREAT_TYPE_API_ABUSE][result.hash.prefix].
          full_hashes.push_back(result);
    }
  }

  // If the check is not in |api_checks_| then the request was cancelled by the
  // client.
  ApiCheckSet::iterator it = api_checks_.find(check);
  if (it == api_checks_.end())
    return;

  ThreatMetadata md;
  // Merge the metadata from all matching results.
  // Note: A full hash may have a result in both the cached results (from
  // its own cache lookup) and in the server results (if another full hash
  // with the same prefix needed to request results from the server). In this
  // unlikely case, the two results' metadata will be merged.
  PopulateApiMetadataResult(full_hash_results, check->full_hashes(), &md);
  PopulateApiMetadataResult(check->cached_results(), check->full_hashes(), &md);

  check->client()->OnCheckApiBlacklistUrlResult(check->url(), md);
  api_checks_.erase(it);
  delete check;
}

// TODO(kcarattini): This is O(N^2). Look at improving performance by
// using a map, sorting or doing binary search etc..
void SafeBrowsingDatabaseManager::PopulateApiMetadataResult(
    const std::vector<SBFullHashResult>& results,
    const std::vector<SBFullHash>& full_hashes,
    ThreatMetadata* md) {
  DCHECK(md);
  for (const SBFullHashResult& result : results) {
    for (const SBFullHash& full_hash : full_hashes) {
      if (SBFullHashEqual(full_hash, result.hash)) {
        md->api_permissions.insert(result.metadata.api_permissions.begin(),
                                   result.metadata.api_permissions.end());
        break;
      }
    }
  }
}

SafeBrowsingDatabaseManager::SafeBrowsingApiCheck::SafeBrowsingApiCheck(
    const GURL& url,
    const std::vector<SBPrefix>& prefixes,
    const std::vector<SBFullHash>& full_hashes,
    const std::vector<SBFullHashResult>& cached_results,
    Client* client)
    : url_(url), prefixes_(prefixes), full_hashes_(full_hashes),
      cached_results_(cached_results), client_(client) {
}

SafeBrowsingDatabaseManager::SafeBrowsingApiCheck::~SafeBrowsingApiCheck() {
}

}  // namespace safe_browsing
