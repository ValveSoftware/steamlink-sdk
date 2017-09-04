// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_url_table.h"

#include <string>

#include "base/logging.h"
#include "sql/connection.h"
#include "sql/statement.h"

using sql::Statement;

namespace {

// Returns the spec of the given URL.
std::string GetKey(const GURL& url) {
  return url.spec();
}

}  // namespace

namespace precache {

bool PrecacheURLInfo::operator==(const PrecacheURLInfo& other) const {
  return was_precached == other.was_precached &&
         is_precached == other.is_precached && was_used == other.was_used &&
         is_download_reported == other.is_download_reported;
}

PrecacheURLTable::PrecacheURLTable() : db_(NULL) {}

PrecacheURLTable::~PrecacheURLTable() {}

bool PrecacheURLTable::Init(sql::Connection* db) {
  DCHECK(!db_);  // Init must only be called once.
  DCHECK(db);    // The database connection must be non-NULL.
  db_ = db;
  return CreateTableIfNonExistent();
}

void PrecacheURLTable::AddURL(const GURL& url,
                              int64_t referrer_host_id,
                              bool is_precached,
                              const base::Time& precache_time,
                              bool is_download_reported) {
  Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO precache_urls "
      "(url, referrer_host_id, was_used, is_precached, time, "
      " is_download_reported)"
      "VALUES(?, ?, 0, ?, ?, ?)"));
  statement.BindString(0, GetKey(url));
  statement.BindInt64(1, referrer_host_id);
  statement.BindInt64(2, is_precached ? 1 : 0);
  statement.BindInt64(3, precache_time.ToInternalValue());
  statement.BindInt64(4, is_download_reported);
  statement.Run();
}

PrecacheURLInfo PrecacheURLTable::GetURLInfo(const GURL& url) {
  Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT is_precached, was_used, is_download_reported "
      "FROM precache_urls WHERE url=?"));
  statement.BindString(0, GetKey(url));

  if (statement.Step()) {
    return {/*present=*/true, /*is_precached=*/statement.ColumnBool(0),
            /*was_used==*/statement.ColumnBool(1),
            /*is_download_reported=*/statement.ColumnBool(2)};
  } else {
    return {/*present=*/false, /*is_precached=*/false, /*was_used=*/false,
            /*is_download_reported=*/false};
  }
}

void PrecacheURLTable::SetPrecachedURLAsUsed(const GURL& url) {
  Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "UPDATE precache_urls SET was_used=1, "
                              "is_precached=0 "
                              "WHERE url=? and was_used=0 and is_precached=1"));

  statement.BindString(0, GetKey(url));
  statement.Run();
}

void PrecacheURLTable::SetURLAsNotPrecached(const GURL& url) {
  Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "UPDATE precache_urls SET is_precached=0 "
                              "WHERE url=? and is_precached=1"));
  statement.BindString(0, GetKey(url));
  statement.Run();
}

void PrecacheURLTable::GetURLListForReferrerHost(
    int64_t referrer_host_id,
    std::vector<GURL>* used_urls,
    std::vector<GURL>* downloaded_urls) {
  Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "SELECT url, was_used, is_download_reported "
                              "from precache_urls where referrer_host_id=?"));
  statement.BindInt64(0, referrer_host_id);
  while (statement.Step()) {
    GURL url(statement.ColumnString(0));
    if (statement.ColumnInt(1))
      used_urls->push_back(url);
    if (!statement.ColumnInt(2))
      downloaded_urls->push_back(url);
  }
}

void PrecacheURLTable::SetDownloadReported(int64_t referrer_host_id) {
  Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "UPDATE precache_urls SET is_download_reported=1 "
                              "WHERE referrer_host_id=?"));
  statement.BindInt64(0, referrer_host_id);
  statement.Run();
}

void PrecacheURLTable::ClearAllForReferrerHost(int64_t referrer_host_id) {
  // Delete the URLs that are not precached.
  Statement delete_statement(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "DELETE FROM precache_urls WHERE "
                              "referrer_host_id=? AND is_precached=0"));
  delete_statement.BindInt64(0, referrer_host_id);
  delete_statement.Run();

  // Clear was_used for precached URLs.
  Statement update_statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE precache_urls SET was_used=0 WHERE referrer_host_id=?"));
  update_statement.BindInt64(0, referrer_host_id);
  update_statement.Run();
}

void PrecacheURLTable::DeleteAllPrecachedBefore(const base::Time& delete_end) {
  Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM precache_urls WHERE time < ?"));

  statement.BindInt64(0, delete_end.ToInternalValue());
  statement.Run();
}

void PrecacheURLTable::DeleteAll() {
  Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, "DELETE FROM precache_urls"));

  statement.Run();
}

void PrecacheURLTable::GetAllDataForTesting(std::map<GURL, base::Time>* map) {
  map->clear();

  Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT url, time FROM precache_urls where is_precached=1"));

  while (statement.Step()) {
    GURL url = GURL(statement.ColumnString(0));
    (*map)[url] = base::Time::FromInternalValue(statement.ColumnInt64(1));
  }
}

bool PrecacheURLTable::CreateTableIfNonExistent() {
  // TODO(jamartin): The PRIMARY KEY should be (url, referrer_host_id).
  if (!db_->DoesTableExist("precache_urls")) {
    return db_->Execute(
        "CREATE TABLE precache_urls "
        "(url TEXT PRIMARY KEY, referrer_host_id INTEGER, was_used INTEGER, "
        "is_precached INTEGER, "
        "time INTEGER, is_download_reported INTEGER)");
  } else {
    // Migrate the table by creating the missing columns.
    if (!db_->DoesColumnExist("precache_urls", "was_used") &&
        !db_->Execute("ALTER TABLE precache_urls "
                      "ADD COLUMN was_used INTEGER")) {
      return false;
    }
    if (!db_->DoesColumnExist("precache_urls", "is_precached") &&
        !db_->Execute("ALTER TABLE precache_urls ADD COLUMN is_precached "
                      "INTEGER default 1")) {
      return false;
    }
    if (!db_->DoesColumnExist("precache_urls", "referrer_host_id") &&
        !db_->Execute(
            "ALTER TABLE precache_urls ADD COLUMN referrer_host_id INTEGER")) {
      return false;
    }
    if (!db_->DoesColumnExist("precache_urls", "is_download_reported") &&
        !db_->Execute("ALTER TABLE precache_urls "
                      "ADD COLUMN is_download_reported INTEGER")) {
      return false;
    }
  }
  return true;
}

}  // namespace precache
