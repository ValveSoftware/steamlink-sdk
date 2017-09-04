// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRECACHE_CORE_PRECACHE_DATABASE_H_
#define COMPONENTS_PRECACHE_CORE_PRECACHE_DATABASE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/precache/core/precache_fetcher.h"
#include "components/precache/core/precache_referrer_host_table.h"
#include "components/precache/core/precache_session_table.h"
#include "components/precache/core/precache_url_table.h"

class GURL;

namespace base {
class FilePath;
}

namespace net {
class HttpResponseInfo;
}

namespace sql {
class Connection;
}

namespace precache {

class PrecacheUnfinishedWork;

// Class that tracks information related to precaching. This class may be
// constructed on any thread, but all calls to, and destruction of this class
// must be done on the the DB thread.
class PrecacheDatabase {
 public:
  // A PrecacheDatabase can be constructed on any thread.
  PrecacheDatabase();

  ~PrecacheDatabase();

  // Initializes the precache database, using the specified database file path.
  // Init must be called before any other methods.
  bool Init(const base::FilePath& db_path);

  // Deletes precache history from the precache URL table that is more than 60
  // days older than |current_time|.
  void DeleteExpiredPrecacheHistory(const base::Time& current_time);

  // Delete all history entries from the database.
  void ClearHistory();

  // Setter and getter for the last precache timestamp.
  void SetLastPrecacheTimestamp(const base::Time& time);
  base::Time GetLastPrecacheTimestamp();

  // Report precache-related metrics in response to a URL being fetched, where
  // the fetch was motivated by precaching. This is called from the network
  // delegate, via precache_util.
  void RecordURLPrefetchMetrics(const net::HttpResponseInfo& info,
                                const base::TimeDelta& latency);

  // Records the precache of an url |url| for top host |referrer_host|. This is
  // called from PrecacheFetcher.
  void RecordURLPrefetch(const GURL& url,
                         const std::string& referrer_host,
                         const base::Time& fetch_time,
                         bool was_cached,
                         int64_t size);

  // Report precache-related metrics in response to a URL being fetched, where
  // the fetch was not motivated by precaching. |is_connection_cellular|
  // indicates whether the current network connection is a cellular network.
  // This is called from the network delegate, via precache_util.
  void RecordURLNonPrefetch(const GURL& url,
                            const base::TimeDelta& latency,
                            const base::Time& fetch_time,
                            const net::HttpResponseInfo& info,
                            int64_t size,
                            int host_rank,
                            bool is_connection_cellular);

  // Returns the referrer host entry for the |referrer_host|.
  PrecacheReferrerHostEntry GetReferrerHost(const std::string& referrer_host);

  // Populates the list of used and downloaded resources for referrer host with
  // id |referrer_host_id|. It will also clear the reported downloaded_urls.
  void GetURLListForReferrerHost(int64_t referrer_host_id,
                                 std::vector<GURL>* used_urls,
                                 std::vector<GURL>* downloaded_urls);

  // Updates the |manifest_id| and |fetch_time| for the referrer host
  // |hostname|, and deletes the precached subresource URLs for this top host.
  void UpdatePrecacheReferrerHost(const std::string& hostname,
                                  int64_t manifest_id,
                                  const base::Time& fetch_time);

  // Gets the state required to continue a precache session.
  std::unique_ptr<PrecacheUnfinishedWork> GetUnfinishedWork();

  // Stores the state required to continue a precache session so that the
  // session can be resumed later.
  void SaveUnfinishedWork(
      std::unique_ptr<PrecacheUnfinishedWork> unfinished_work);

  // Deletes unfinished work from the database.
  void DeleteUnfinishedWork();

  // Precache quota.
  void SaveQuota(const PrecacheQuota& quota);
  PrecacheQuota GetQuota();

  base::WeakPtr<PrecacheDatabase> GetWeakPtr();

 private:
  friend class PrecacheDatabaseTest;
  friend class PrecacheFetcherTest;
  friend class PrecacheManagerTest;

  bool IsDatabaseAccessible() const;

  // Flushes any buffered write operations. |buffered_writes_| will be empty
  // after calling this function. To maximize performance, all the buffered
  // writes are run in a single database transaction.
  void Flush();

  // Same as Flush(), but also updates the flag |is_flush_posted_| to indicate
  // that a flush is no longer posted.
  void PostedFlush();

  // Post a call to PostedFlush() on the current thread's MessageLoop, if
  // |buffered_writes_| is non-empty and there isn't already a flush call
  // posted.
  void MaybePostFlush();

  // Records the time since the last precache.
  void RecordTimeSinceLastPrecache(const base::Time& fetch_time);

  void RecordURLPrefetchInternal(const GURL& url,
                                 const std::string& referrer_host,
                                 bool is_precached,
                                 const base::Time& fetch_time,
                                 bool is_download_reported);

  void UpdatePrecacheReferrerHostInternal(const std::string& hostname,
                                          int64_t manifest_id,
                                          const base::Time& fetch_time);

  std::unique_ptr<sql::Connection> db_;

  // Table that keeps track of URLs that are in the cache because of precaching,
  // and wouldn't be in the cache otherwise. If |buffered_writes_| is non-empty,
  // then this table will not be up to date until the next call to Flush().
  PrecacheURLTable precache_url_table_;

  // If |buffered_writes_| is non-empty,
  // then this table will not be up to date until the next call to Flush().
  PrecacheReferrerHostTable precache_referrer_host_table_;

  // Table that persists state related to a precache session, including
  // unfinished work to be done.
  PrecacheSessionTable precache_session_table_;

  // A vector of write operations to be run on the database.
  std::vector<base::Closure> buffered_writes_;

  // Set of URLs that have been modified in |buffered_writes_|. It's a hash set
  // of strings, and not GURLs, because there is no hash function on GURL.
  base::hash_set<std::string> buffered_urls_;

  // Flag indicating whether or not a call to Flush() has been posted to run in
  // the future.
  bool is_flush_posted_;

  // ThreadChecker used to ensure that all methods other than the constructor
  // or destructor are called on the same thread.
  base::ThreadChecker thread_checker_;

  // Time of the last precache. This is a cached copy of
  // precache_session_table_.GetLastPrecacheTimestamp.
  base::Time last_precache_timestamp_;

  // This must be the last member of this class.
  base::WeakPtrFactory<PrecacheDatabase> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrecacheDatabase);
};

}  // namespace precache

#endif  // COMPONENTS_PRECACHE_CORE_PRECACHE_DATABASE_H_
