// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_referrer_host_table.h"

#include "sql/connection.h"
#include "sql/statement.h"

using sql::Statement;

namespace precache {

const int64_t PrecacheReferrerHostEntry::kInvalidId = -1;

bool PrecacheReferrerHostEntry::operator==(
    const PrecacheReferrerHostEntry& entry) const {
  return id == entry.id && referrer_host == entry.referrer_host &&
         manifest_id == entry.manifest_id && time == entry.time;
}

PrecacheReferrerHostTable::PrecacheReferrerHostTable() : db_(NULL) {}

PrecacheReferrerHostTable::~PrecacheReferrerHostTable() {}

bool PrecacheReferrerHostTable::Init(sql::Connection* db) {
  DCHECK(!db_);  // Init must only be called once.
  DCHECK(db);    // The database connection must be non-NULL.
  db_ = db;
  return CreateTableIfNonExistent();
}

PrecacheReferrerHostEntry PrecacheReferrerHostTable::GetReferrerHost(
    const std::string& referrer_host) {
  Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT id, referrer_host, manifest_id, time "
      "FROM precache_referrer_hosts WHERE referrer_host=?"));

  statement.BindString(0, referrer_host);
  if (statement.Step()) {
    return PrecacheReferrerHostEntry(
        statement.ColumnInt64(0), statement.ColumnString(1),
        statement.ColumnInt64(2),
        base::Time::FromInternalValue(statement.ColumnInt64(3)));
  }
  return PrecacheReferrerHostEntry(PrecacheReferrerHostEntry::kInvalidId,
                                   std::string(), 0, base::Time());
}

int64_t PrecacheReferrerHostTable::UpdateReferrerHost(
    const std::string& referrer_host,
    int64_t manifest_id,
    const base::Time& time) {
  int64_t referrer_host_id = GetReferrerHost(referrer_host).id;
  if (referrer_host_id == PrecacheReferrerHostEntry::kInvalidId) {
    Statement statement(
        db_->GetCachedStatement(SQL_FROM_HERE,
                                "INSERT INTO precache_referrer_hosts "
                                "(id, referrer_host, manifest_id, time) "
                                "VALUES(NULL, ?, ?, ?)"));

    statement.BindString(0, referrer_host);
    statement.BindInt64(1, manifest_id);
    statement.BindInt64(2, time.ToInternalValue());
    if (statement.Run())
      return db_->GetLastInsertRowId();
  } else {
    Statement statement(
        db_->GetCachedStatement(SQL_FROM_HERE,
                                "UPDATE precache_referrer_hosts "
                                "SET manifest_id=?, time=? "
                                "WHERE id=?"));

    statement.BindInt64(0, manifest_id);
    statement.BindInt64(1, time.ToInternalValue());
    ;
    statement.BindInt64(2, referrer_host_id);
    if (statement.Run())
      return referrer_host_id;
  }
  return -1;
}

void PrecacheReferrerHostTable::DeleteAllEntriesBefore(
    const base::Time& delete_end) {
  Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM precache_referrer_hosts WHERE time < ?"));
  statement.BindInt64(0, delete_end.ToInternalValue());
  statement.Run();
}

void PrecacheReferrerHostTable::DeleteAll() {
  Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM precache_referrer_hosts"));

  statement.Run();
}

bool PrecacheReferrerHostTable::CreateTableIfNonExistent() {
  return db_->Execute(
      "CREATE TABLE IF NOT EXISTS precache_referrer_hosts "
      "(id INTEGER PRIMARY KEY, referrer_host TEXT KEY, manifest_id INTEGER, "
      "time INTEGER)");
}

std::map<std::string, PrecacheReferrerHostEntry>
PrecacheReferrerHostTable::GetAllDataForTesting() {
  std::map<std::string, PrecacheReferrerHostEntry> all_data;
  Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "SELECT id, referrer_host, manifest_id, time "
                              "FROM precache_referrer_hosts"));
  while (statement.Step()) {
    all_data[statement.ColumnString(1)] = PrecacheReferrerHostEntry(
        statement.ColumnInt64(0), statement.ColumnString(1),
        statement.ColumnInt64(2),
        base::Time::FromInternalValue(statement.ColumnInt64(3)));
  }
  return all_data;
}

}  // namespace precache
