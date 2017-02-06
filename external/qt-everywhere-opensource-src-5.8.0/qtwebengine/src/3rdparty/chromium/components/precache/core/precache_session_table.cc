// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_session_table.h"

#include <string>

#include "base/logging.h"
#include "base/time/time.h"
#include "components/precache/core/proto/unfinished_work.pb.h"
#include "sql/connection.h"
#include "sql/statement.h"

using sql::Statement;

namespace precache {

PrecacheSessionTable::PrecacheSessionTable() : db_(nullptr) {}

PrecacheSessionTable::~PrecacheSessionTable() {}

bool PrecacheSessionTable::Init(sql::Connection* db) {
  DCHECK(!db_);  // Init must only be called once.
  DCHECK(db);    // The database connection must be non-NULL.
  db_ = db;
  return CreateTableIfNonExistent();
}

// Store unfinished work.
void PrecacheSessionTable::SaveUnfinishedWork(
    std::unique_ptr<PrecacheUnfinishedWork> unfinished_work) {
  Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO precache_session (type, value) VALUES(?,?)"));
  statement.BindInt(0, static_cast<int>(UNFINISHED_WORK));
  statement.BindString(1, unfinished_work->SerializeAsString());
  statement.Run();
}

// Retrieve unfinished work.
std::unique_ptr<PrecacheUnfinishedWork>
PrecacheSessionTable::GetUnfinishedWork() {
  Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "SELECT value from precache_session where type=?"));
  statement.BindInt(0, static_cast<int>(UNFINISHED_WORK));
  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  if (statement.Step())
    unfinished_work->ParseFromString(statement.ColumnString(0));
  return unfinished_work;
}



void PrecacheSessionTable::DeleteUnfinishedWork() {
  Statement statement(
      db_->GetCachedStatement(
          SQL_FROM_HERE, "DELETE FROM precache_session where type=?"));
  statement.BindInt(0, static_cast<int>(UNFINISHED_WORK));
  statement.Run();
}

bool PrecacheSessionTable::CreateTableIfNonExistent() {
  return db_->Execute(
      "CREATE TABLE IF NOT EXISTS precache_session (type INTEGER PRIMARY KEY, "
      "value STRING)");
}

}  // namespace precache
