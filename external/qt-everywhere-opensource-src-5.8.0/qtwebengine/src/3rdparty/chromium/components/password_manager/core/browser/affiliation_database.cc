// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation_database.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "sql/connection.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace password_manager {

namespace {
const int kVersion = 1;
const int kCompatibleVersion = 1;
}  // namespace

AffiliationDatabase::AffiliationDatabase() {
}

AffiliationDatabase::~AffiliationDatabase() {
}

bool AffiliationDatabase::Init(const base::FilePath& path) {
  sql_connection_.reset(new sql::Connection);
  sql_connection_->set_histogram_tag("Affiliation");
  sql_connection_->set_error_callback(base::Bind(
      &AffiliationDatabase::SQLErrorCallback, base::Unretained(this)));

  if (!sql_connection_->Open(path))
    return false;

  if (!sql_connection_->Execute("PRAGMA foreign_keys=1")) {
    sql_connection_->Poison();
    return false;
  }

  sql::MetaTable metatable;
  if (!metatable.Init(sql_connection_.get(), kVersion, kCompatibleVersion)) {
    sql_connection_->Poison();
    return false;
  }

  if (metatable.GetCompatibleVersionNumber() > kVersion) {
    LOG(WARNING) << "AffiliationDatabase is too new.";
    sql_connection_->Poison();
    return false;
  }

  if (!CreateTablesAndIndicesIfNeeded()) {
    sql_connection_->Poison();
    return false;
  }

  return true;
}

bool AffiliationDatabase::GetAffiliationsForFacet(
    const FacetURI& facet_uri,
    AffiliatedFacetsWithUpdateTime* result) const {
  DCHECK(result);
  result->facets.clear();

  sql::Statement statement(sql_connection_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT m2.facet_uri, c.last_update_time "
      "FROM eq_class_members m1, eq_class_members m2, eq_classes c "
      "WHERE m1.facet_uri = ? AND m1.set_id = m2.set_id AND m1.set_id = c.id"));
  statement.BindString(0, facet_uri.canonical_spec());

  while (statement.Step()) {
    result->facets.push_back(
        FacetURI::FromCanonicalSpec(statement.ColumnString(0)));
    result->last_update_time =
        base::Time::FromInternalValue(statement.ColumnInt64(1));
  }

  return !result->facets.empty();
}

void AffiliationDatabase::GetAllAffiliations(
    std::vector<AffiliatedFacetsWithUpdateTime>* results) const {
  DCHECK(results);
  results->clear();

  sql::Statement statement(sql_connection_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT m.facet_uri, c.last_update_time, c.id "
      "FROM eq_class_members m, eq_classes c "
      "WHERE m.set_id = c.id "
      "ORDER BY c.id"));

  int64_t last_eq_class_id = 0;
  while (statement.Step()) {
    int64_t eq_class_id = statement.ColumnInt64(2);
    if (results->empty() || eq_class_id != last_eq_class_id) {
      results->push_back(AffiliatedFacetsWithUpdateTime());
      last_eq_class_id = eq_class_id;
    }
    results->back().facets.push_back(
        FacetURI::FromCanonicalSpec(statement.ColumnString(0)));
    results->back().last_update_time =
        base::Time::FromInternalValue(statement.ColumnInt64(1));
  }
}

void AffiliationDatabase::DeleteAffiliationsForFacet(
    const FacetURI& facet_uri) {
  sql::Transaction transaction(sql_connection_.get());
  if (!transaction.Begin())
    return;

  sql::Statement statement_lookup(sql_connection_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT m.set_id FROM eq_class_members m "
      "WHERE m.facet_uri = ?"));
  statement_lookup.BindString(0, facet_uri.canonical_spec());

  // No such |facet_uri|, nothing to do.
  if (!statement_lookup.Step())
    return;

  int64_t eq_class_id = statement_lookup.ColumnInt64(0);

  // Children will get deleted due to 'ON DELETE CASCADE'.
  sql::Statement statement_parent(sql_connection_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM eq_classes WHERE eq_classes.id = ?"));
  statement_parent.BindInt64(0, eq_class_id);
  if (!statement_parent.Run())
    return;

  transaction.Commit();
}

void AffiliationDatabase::DeleteAffiliationsOlderThan(
    const base::Time& cutoff_threshold) {
  // Children will get deleted due to 'ON DELETE CASCADE'.
  sql::Statement statement_parent(sql_connection_->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM eq_classes "
      "WHERE eq_classes.last_update_time < ?"));
  statement_parent.BindInt64(0, cutoff_threshold.ToInternalValue());
  statement_parent.Run();
}

void AffiliationDatabase::DeleteAllAffiliations() {
  // Children will get deleted due to 'ON DELETE CASCADE'.
  sql::Statement statement_parent(
      sql_connection_->GetUniqueStatement("DELETE FROM eq_classes"));
  statement_parent.Run();
}

bool AffiliationDatabase::Store(
    const AffiliatedFacetsWithUpdateTime& affiliated_facets) {
  DCHECK(!affiliated_facets.facets.empty());

  sql::Statement statement_parent(sql_connection_->GetCachedStatement(
      SQL_FROM_HERE, "INSERT INTO eq_classes(last_update_time) VALUES (?)"));

  sql::Statement statement_child(sql_connection_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO eq_class_members(facet_uri, set_id) VALUES (?, ?)"));

  sql::Transaction transaction(sql_connection_.get());
  if (!transaction.Begin())
    return false;

  statement_parent.BindInt64(
      0, affiliated_facets.last_update_time.ToInternalValue());
  if (!statement_parent.Run())
    return false;

  int64_t eq_class_id = sql_connection_->GetLastInsertRowId();
  for (const FacetURI& uri : affiliated_facets.facets) {
    statement_child.Reset(true);
    statement_child.BindString(0, uri.canonical_spec());
    statement_child.BindInt64(1, eq_class_id);
    if (!statement_child.Run())
      return false;
  }

  return transaction.Commit();
}

void AffiliationDatabase::StoreAndRemoveConflicting(
    const AffiliatedFacetsWithUpdateTime& affiliation,
    std::vector<AffiliatedFacetsWithUpdateTime>* removed_affiliations) {
  DCHECK(!affiliation.facets.empty());
  DCHECK(removed_affiliations);
  removed_affiliations->clear();

  sql::Transaction transaction(sql_connection_.get());
  if (!transaction.Begin())
    return;

  for (const FacetURI& uri : affiliation.facets) {
    AffiliatedFacetsWithUpdateTime old_affiliation;
    if (GetAffiliationsForFacet(uri, &old_affiliation)) {
      if (!AreEquivalenceClassesEqual(old_affiliation.facets,
                                      affiliation.facets)) {
        removed_affiliations->push_back(old_affiliation);
      }
      DeleteAffiliationsForFacet(uri);
    }
  }

  if (!Store(affiliation))
    NOTREACHED();

  transaction.Commit();
}

// static
void AffiliationDatabase::Delete(const base::FilePath& path) {
  bool success = sql::Connection::Delete(path);
  DCHECK(success);
}

bool AffiliationDatabase::CreateTablesAndIndicesIfNeeded() {
  if (!sql_connection_->Execute(
          "CREATE TABLE IF NOT EXISTS eq_classes("
          "id INTEGER PRIMARY KEY,"
          "last_update_time INTEGER)")) {
    return false;
  }

  if (!sql_connection_->Execute(
          "CREATE TABLE IF NOT EXISTS eq_class_members("
          "id INTEGER PRIMARY KEY,"
          "facet_uri LONGVARCHAR UNIQUE NOT NULL,"
          "set_id INTEGER NOT NULL"
          "    REFERENCES eq_classes(id) ON DELETE CASCADE)")) {
    return false;
  }

  // An index on eq_class_members.facet_uri is automatically created due to the
  // UNIQUE constraint, however, we must create one on eq_class_members.set_id
  // manually (to prevent linear scan when joining).
  return sql_connection_->Execute(
      "CREATE INDEX IF NOT EXISTS index_on_eq_class_members_set_id ON "
      "eq_class_members (set_id)");
}

void AffiliationDatabase::SQLErrorCallback(int error,
                                           sql::Statement* statement) {
  if (sql::IsErrorCatastrophic(error)) {
    // Normally this will poison the database, causing any subsequent operations
    // to silently fail without any side effects. However, if RazeAndClose() is
    // called from the error callback in response to an error raised from within
    // sql::Connection::Open, opening the now-razed database will be retried.
    sql_connection_->RazeAndClose();
    return;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Connection::IsExpectedSqliteError(error))
    DLOG(FATAL) << sql_connection_->GetErrorMessage();
}

}  // namespace password_manager
