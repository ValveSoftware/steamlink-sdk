// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRECACHE_CORE_PRECACHE_URL_TABLE_H_
#define COMPONENTS_PRECACHE_CORE_PRECACHE_URL_TABLE_H_

#include <map>

#include "base/macros.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace sql {
class Connection;
}

namespace precache {

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

  // Adds a precached URL to the table, using the current time as the
  // precache timestamp. Replaces the row if one already exists.
  void AddURL(const GURL& url, const base::Time& precache_time);

  // Returns true if this URL exists in the table.
  bool HasURL(const GURL& url);

  // Deletes the row from the table that has the given URL, if it exists.
  void DeleteURL(const GURL& url);

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
