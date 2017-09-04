// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_session_table.h"

#include <stdint.h>
#include <string>

#include "base/logging.h"
#include "base/time/time.h"
#include "components/precache/core/proto/timestamp.pb.h"
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

void PrecacheSessionTable::SetSessionDataType(SessionDataType id,
                                              const std::string& data) {
  Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO precache_session (type, value) VALUES(?,?)"));
  statement.BindInt(0, static_cast<int>(id));
  statement.BindString(1, data);
  statement.Run();
}

std::string PrecacheSessionTable::GetSessionDataType(SessionDataType id) {
  Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "SELECT value from precache_session where type=?"));
  statement.BindInt(0, static_cast<int>(id));
  return statement.Step() ? statement.ColumnString(0) : std::string();
}

void PrecacheSessionTable::SetLastPrecacheTimestamp(const base::Time& time) {
  DCHECK(!time.is_null());
  Timestamp timestamp;
  timestamp.set_seconds((time - base::Time::UnixEpoch()).InSeconds());
  SetSessionDataType(SessionDataType::LAST_PRECACHE_TIMESTAMP,
                     timestamp.SerializeAsString());
}

base::Time PrecacheSessionTable::GetLastPrecacheTimestamp() {
  Timestamp timestamp;
  const std::string data =
      GetSessionDataType(SessionDataType::LAST_PRECACHE_TIMESTAMP);
  if (!data.empty())
    timestamp.ParseFromString(data);
  return timestamp.has_seconds()
             ? base::Time::UnixEpoch() +
                   base::TimeDelta::FromSeconds(timestamp.seconds())
             : base::Time();
}

void PrecacheSessionTable::DeleteLastPrecacheTimestamp() {
  Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM precache_session where type=?"));
  statement.BindInt(0,
                    static_cast<int>(SessionDataType::LAST_PRECACHE_TIMESTAMP));
  statement.Run();
}

// Store unfinished work.
void PrecacheSessionTable::SaveUnfinishedWork(
    std::unique_ptr<PrecacheUnfinishedWork> unfinished_work) {
  SetSessionDataType(SessionDataType::UNFINISHED_WORK,
                     unfinished_work->SerializeAsString());
}

// Retrieve unfinished work.
std::unique_ptr<PrecacheUnfinishedWork>
PrecacheSessionTable::GetUnfinishedWork() {
  const std::string data = GetSessionDataType(SessionDataType::UNFINISHED_WORK);
  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  if (!data.empty())
    unfinished_work->ParseFromString(data);
  return unfinished_work;
}

void PrecacheSessionTable::DeleteUnfinishedWork() {
  Statement statement(
      db_->GetCachedStatement(
          SQL_FROM_HERE, "DELETE FROM precache_session where type=?"));
  statement.BindInt(0, static_cast<int>(SessionDataType::UNFINISHED_WORK));
  statement.Run();
}

void PrecacheSessionTable::SaveQuota(const PrecacheQuota& quota) {
  SetSessionDataType(SessionDataType::QUOTA, quota.SerializeAsString());
}

PrecacheQuota PrecacheSessionTable::GetQuota() {
  PrecacheQuota quota;
  const std::string data = GetSessionDataType(SessionDataType::QUOTA);
  if (!data.empty())
    quota.ParseFromString(data);
  return quota;
}

bool PrecacheSessionTable::CreateTableIfNonExistent() {
  return db_->Execute(
      "CREATE TABLE IF NOT EXISTS precache_session (type INTEGER PRIMARY KEY, "
      "value STRING)");
}

}  // namespace precache
