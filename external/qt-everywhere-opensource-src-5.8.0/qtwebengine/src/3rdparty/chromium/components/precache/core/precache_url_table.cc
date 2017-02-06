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

PrecacheURLTable::PrecacheURLTable() : db_(NULL) {}

PrecacheURLTable::~PrecacheURLTable() {}

bool PrecacheURLTable::Init(sql::Connection* db) {
  DCHECK(!db_);  // Init must only be called once.
  DCHECK(db);    // The database connection must be non-NULL.
  db_ = db;
  return CreateTableIfNonExistent();
}

void PrecacheURLTable::AddURL(const GURL& url,
                              const base::Time& precache_time) {
  Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO precache_urls (url, time) VALUES(?,?)"));

  statement.BindString(0, GetKey(url));
  statement.BindInt64(1, precache_time.ToInternalValue());
  statement.Run();
}

bool PrecacheURLTable::HasURL(const GURL& url) {
  Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "SELECT time FROM precache_urls WHERE url=?"));

  statement.BindString(0, GetKey(url));
  return statement.Step();
}

void PrecacheURLTable::DeleteURL(const GURL& url) {
  Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM precache_urls WHERE url=?"));

  statement.BindString(0, GetKey(url));
  statement.Run();
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
      SQL_FROM_HERE, "SELECT url, time FROM precache_urls"));

  while (statement.Step()) {
    GURL url = GURL(statement.ColumnString(0));
    (*map)[url] = base::Time::FromInternalValue(statement.ColumnInt64(1));
  }
}

bool PrecacheURLTable::CreateTableIfNonExistent() {
  return db_->Execute(
      "CREATE TABLE IF NOT EXISTS precache_urls (url TEXT PRIMARY KEY, time "
      "INTEGER)");
}

}  // namespace precache
