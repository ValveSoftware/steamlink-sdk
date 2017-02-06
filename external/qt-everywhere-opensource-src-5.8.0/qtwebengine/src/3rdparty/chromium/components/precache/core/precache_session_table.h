// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRECACHE_CORE_PRECACHE_SESSION_TABLE_H_
#define COMPONENTS_PRECACHE_CORE_PRECACHE_SESSION_TABLE_H_

#include <list>
#include <map>
#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace base {
class TimeTicks;
}

namespace sql {
class Connection;
}

namespace precache {

class PrecacheUnfinishedWork;

// Denotes the type of session information being stored.
enum SessionDataType {
  // Unfinished work to do sometime later.
  UNFINISHED_WORK = 0,
};

class PrecacheSessionTable {
 public:
  PrecacheSessionTable();
  virtual ~PrecacheSessionTable();

  // Initializes the precache task URL table for use with the specified database
  // connection. The caller keeps ownership of |db|, and |db| must not be null.
  // Init must be called before any other methods.
  bool Init(sql::Connection* db);

  // Stores unfinished work.
  void SaveUnfinishedWork(
      std::unique_ptr<PrecacheUnfinishedWork> unfinished_work);

  // Retrieves unfinished work.
  std::unique_ptr<PrecacheUnfinishedWork> GetUnfinishedWork();

  // Removes all unfinished work from the database.
  void DeleteUnfinishedWork();

 private:
  bool CreateTableIfNonExistent();

  // Non-owned pointer.
  sql::Connection* db_;

  DISALLOW_COPY_AND_ASSIGN(PrecacheSessionTable);
};

}  // namespace precache
#endif  // COMPONENTS_PRECACHE_CORE_PRECACHE_SESSION_TABLE_H_
