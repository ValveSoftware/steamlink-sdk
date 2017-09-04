// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRECACHE_CORE_PRECACHE_SESSION_TABLE_H_
#define COMPONENTS_PRECACHE_CORE_PRECACHE_SESSION_TABLE_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/precache/core/proto/quota.pb.h"

namespace sql {
class Connection;
}

namespace precache {

class PrecacheUnfinishedWork;

// Denotes the type of session information being stored.
enum class SessionDataType {
  // Unfinished work to do sometime later.
  UNFINISHED_WORK = 0,

  // Timestamp of the last precache.
  LAST_PRECACHE_TIMESTAMP = 1,

  // Remaining quota limits.
  QUOTA = 2,
};

class PrecacheSessionTable {
 public:
  PrecacheSessionTable();
  virtual ~PrecacheSessionTable();

  // Initializes the precache task URL table for use with the specified database
  // connection. The caller keeps ownership of |db|, and |db| must not be null.
  // Init must be called before any other methods.
  bool Init(sql::Connection* db);

  // -- Time since last precache --

  void SetLastPrecacheTimestamp(const base::Time& time);

  // If none present, it will return base::Time(), so it can be checked via
  // is_null().
  base::Time GetLastPrecacheTimestamp();

  void DeleteLastPrecacheTimestamp();

  // Precache quota.
  void SaveQuota(const PrecacheQuota& quota);
  PrecacheQuota GetQuota();

  // -- Unfinished work --

  // Stores unfinished work.
  void SaveUnfinishedWork(
      std::unique_ptr<PrecacheUnfinishedWork> unfinished_work);

  // Retrieves unfinished work.
  std::unique_ptr<PrecacheUnfinishedWork> GetUnfinishedWork();

  // Removes all unfinished work from the database.
  void DeleteUnfinishedWork();

 private:
  bool CreateTableIfNonExistent();

  void SetSessionDataType(SessionDataType id, const std::string& data);
  std::string GetSessionDataType(SessionDataType id);

  // Non-owned pointer.
  sql::Connection* db_;

  DISALLOW_COPY_AND_ASSIGN(PrecacheSessionTable);
};

}  // namespace precache
#endif  // COMPONENTS_PRECACHE_CORE_PRECACHE_SESSION_TABLE_H_
