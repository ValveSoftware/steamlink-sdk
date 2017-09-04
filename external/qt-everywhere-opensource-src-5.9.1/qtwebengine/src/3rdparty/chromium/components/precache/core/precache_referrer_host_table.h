// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRECACHE_CORE_PRECACHE_REFERRER_HOST_TABLE_H_
#define COMPONENTS_PRECACHE_CORE_PRECACHE_REFERRER_HOST_TABLE_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"

namespace sql {
class Connection;
}

namespace precache {

struct PrecacheReferrerHostEntry {
  static const int64_t kInvalidId;

  PrecacheReferrerHostEntry() : id(kInvalidId) {}
  PrecacheReferrerHostEntry(int64_t id,
                            const std::string& referrer_host,
                            int64_t manifest_id,
                            const base::Time& time)
      : id(id),
        referrer_host(referrer_host),
        manifest_id(manifest_id),
        time(time) {}

  // Comparison for testing.
  bool operator==(const PrecacheReferrerHostEntry& entry) const;

  int64_t id;
  std::string referrer_host;
  int64_t manifest_id;
  base::Time time;
};

class PrecacheReferrerHostTable {
 public:
  PrecacheReferrerHostTable();
  ~PrecacheReferrerHostTable();

  // Initialize the precache referrer host table for use with the specified
  // database connection. The caller keeps ownership of |db|, and |db| must not
  // be NULL. Init must be called before any other methods.
  bool Init(sql::Connection* db);

  // Returns the referrer host information about |referrer_host|.
  PrecacheReferrerHostEntry GetReferrerHost(const std::string& referrer_host);

  // Updates the referrer host information about |referrer_host|.
  int64_t UpdateReferrerHost(const std::string& referrer_host,
                             int64_t manifest_id,
                             const base::Time& time);

  // Deletes entries that were created before the time of |delete_end|.
  void DeleteAllEntriesBefore(const base::Time& delete_end);

  // Delete all entries.
  void DeleteAll();

  // Used by tests to get the contents of the table.
  std::map<std::string, PrecacheReferrerHostEntry> GetAllDataForTesting();

 private:
  bool CreateTableIfNonExistent();

  // Not owned by |this|.
  sql::Connection* db_;

  DISALLOW_COPY_AND_ASSIGN(PrecacheReferrerHostTable);
};

}  // namespace precache

#endif  // COMPONENTS_PRECACHE_CORE_PRECACHE_REFERRER_HOST_TABLE_H_
