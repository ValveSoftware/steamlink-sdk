// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRECACHE_CORE_PRECACHE_URL_TABLE_H_
#define COMPONENTS_PRECACHE_CORE_PRECACHE_URL_TABLE_H_

#include <stdint.h>

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace sql {
class Connection;
}

namespace precache {

// Information about a given URL with respect to the PrecacheURLTable.
struct PrecacheURLInfo {
  // The url has been prefetched in the past 60 days. (This number comes from
  // kPrecacheHistoryExpiryPeriodDays in precache_database.cc.)
  bool was_precached;

  // True if the cache entry is the one fetched by PrecacheFetcher. False if a
  // new network fetch overwrote the cache entry since the prefetch.
  bool is_precached;

  // The prefetched copy of the URL was used in browsing (i.e. while
  // is_precached was true).
  bool was_used;

  // It was already reported that this resources was downloaded.
  bool is_download_reported;

  bool operator==(const PrecacheURLInfo& other) const;
};

// Interface for database table that keeps track of the URLs that have been
// precached but not used. This table is used to count how many bytes were saved
// by precached resources.
// Each row in this table represents a URL that was precached over the network,
// and has not been fetched through user browsing since then.
// Manages one table { URL (primary key), precache timestamp }.
class PrecacheURLTable {
 public:
  PrecacheURLTable();
  ~PrecacheURLTable();

  // Initialize the precache URL table for use with the specified database
  // connection. The caller keeps ownership of |db|, and |db| must not be NULL.
  // Init must be called before any other methods.
  bool Init(sql::Connection* db);

  // Adds an URL to the table, |referrer_host_id| is the id of the referrer host
  // in PrecacheReferrerHostTable, |is_precached| indicates if the URL is
  // precached, |time| is the timestamp, |is_download_reported| indicates if
  // this the download of this URL was already reported. Replaces the row if one
  // already exists.
  void AddURL(const GURL& url,
              int64_t referrer_host_id,
              bool is_precached,
              const base::Time& precache_time,
              bool is_download_reported);

  // Returns information about the URL's status with respect to prefetching.
  PrecacheURLInfo GetURLInfo(const GURL& url);

  // Sets the precached URL as used.
  void SetPrecachedURLAsUsed(const GURL& url);

  // Set the previously precached URL as not precached, during user browsing.
  void SetURLAsNotPrecached(const GURL& url);

  // Populates the used and downloaded resource URLs for the referrer host with
  // id |referrer_host_id|.
  void GetURLListForReferrerHost(int64_t referrer_host_id,
                                 std::vector<GURL>* used_urls,
                                 std::vector<GURL>* downloaded_urls);

  // Sets all the URLs of the given referrer_host_id as is_download_reported.
  void SetDownloadReported(int64_t referrer_host_id);

  // Clears all URL entries for the referrer host |referrer_host_id|.
  void ClearAllForReferrerHost(int64_t referrer_host_id);

  // Deletes entries that were precached before the time of |delete_end|.
  void DeleteAllPrecachedBefore(const base::Time& delete_end);

  // Delete all entries.
  void DeleteAll();

  // Used by tests to get the contents of the table.
  void GetAllDataForTesting(std::map<GURL, base::Time>* map);

 private:
  bool CreateTableIfNonExistent();

  // Non-owned pointer.
  sql::Connection* db_;

  DISALLOW_COPY_AND_ASSIGN(PrecacheURLTable);
};

}  // namespace precache

#endif  // COMPONENTS_PRECACHE_CORE_PRECACHE_URL_TABLE_H_
