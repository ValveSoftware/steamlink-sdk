// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SQL_TABLE_BUILDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SQL_TABLE_BUILDER_H_

#include <limits>
#include <list>
#include <string>

#include "base/macros.h"

namespace sql {
class Connection;
}

namespace password_manager {

// Use this class to represent the versioned evolution of a SQLite table
// structure and generate creating and migrating statements for it.
//
// Two values are currently hard-coded, because they are the same in the
// current use-cases:
// * The table name is hard-coded as "logins".
// * The index is built for column "signon_realm" which is expected to exist.
//
// Usage example:
//
// SQLTableBuilder builder;
//
// // First describe a couple of versions:
// builder.AddColumn("name", "VARCHAR");
// builder.AddColumn("icon", "VARCHAR");
// builder.AddColumn("password", "VARCHAR NOT NULL");
// unsigned version = builder.SealVersion();  // Version 0 is sealed.
// DCHECK_EQ(0u, version);
// builder.RenameColumn("icon", "avatar");
// version = builder.SealVersion();  // Version 1 is sealed.
// DCHECK_EQ(1u, version);
//
// // Now, assuming that |db| has a table "logins" in a state corresponding
// // version 0, this will migrate it to version 1:
// sql::Connection* db = ...;
// builder.MigrateToNextFrom(0, db);
//
// // And assuming |db| has no table called "logins", this will create one
// // in a state corresponding the latest sealed version:
// builder.CreateTable(db);
class SQLTableBuilder {
 public:
  // Create the builder for "logins".
  SQLTableBuilder();

  ~SQLTableBuilder();

  // Adds a column in the table description, with |name| and |type|. |name|
  // must not have been added to the table in this version before.
  void AddColumn(const std::string& name, const std::string& type);

  // As AddColumn but also adds column |name| to the unique key of the table.
  // SealVersion must not have been called yet (the builder does not currently
  // support migration code for changing the unique key between versions).
  void AddColumnToUniqueKey(const std::string& name, const std::string& type);

  // Renames column |old_name| to |new_name|. |old_name| must have been added in
  // the past. The column "signon_realm" must not be renamed.
  void RenameColumn(const std::string& old_name, const std::string& new_name);

  // Removes column |name|. |name| must have been added in the past. The column
  // "signon_realm" must not be renamed.
  void DropColumn(const std::string& name);

  // Increments the internal version counter and marks the current state of the
  // table as that version. Returns the sealed version. Calling any of the
  // *Column* methods above will result in starting a new version which is not
  // considered sealed. The first version is 0. The column "signon_realm" must
  // be present in |columns_| every time this is called, and at least one call
  // to AddColumnToUniqueKey must have been done before this is called the first
  // time.
  unsigned SealVersion();

  // Assuming that the database connected through |db| contains a table called
  // "logins" in a state described by version |old_version|, migrates it to
  // the current version, which must be sealed. Returns true on success.
  bool MigrateFrom(unsigned old_version, sql::Connection* db);

  // If |db| connects to a database where table "logins" already exists,
  // this is a no-op and returns true. Otherwise, "logins" is created in a
  // state described by the current version known to the builder. The current
  // version must be sealed. Returns true on success.
  bool CreateTable(sql::Connection* db);

  // Returns the comma-separated list of all column names present in the last
  // version. The last version must be sealed.
  std::string ListAllColumnNames() const;

  // Same as ListAllColumnNames, but for non-unique key names only, and with
  // names followed by " = ?".
  std::string ListAllNonuniqueKeyNames() const;

  // Same as ListAllNonuniqueKeyNames, but for unique key names and separated by
  // " AND ".
  std::string ListAllUniqueKeyNames() const;

  // Returns the number of all columns present in the last version. The last
  // version must be sealed.
  size_t NumberOfColumns() const;

 private:
  // Stores the information about one column (name, type, etc.).
  struct Column;

  static unsigned constexpr kInvalidVersion =
      std::numeric_limits<unsigned>::max();

  // Assuming that the database connected through |db| contains a table called
  // "logins" in a state described by version |old_version|, migrates it to
  // version |old_version + 1|. The current version known to the builder must be
  // at least |old_version + 1| and sealed. Returns true on success.
  bool MigrateToNextFrom(unsigned old_version, sql::Connection* db);

  // Looks up column named |name| in |columns_|. If present, returns the last
  // one.
  std::list<Column>::reverse_iterator FindLastByName(const std::string& name);

  // Returns whether the last version is |version| and whether it was sealed
  // (by calling SealVersion with no table modifications afterwards).
  bool IsVersionLastAndSealed(unsigned version) const;

  // Whether |column| is present in the last version. The last version must be
  // sealed.
  bool IsInLastVersion(const Column& column) const;

  // Last sealed version, kInvalidVersion means "none".
  unsigned sealed_version_ = kInvalidVersion;

  std::list<Column> columns_;  // Columns of the table, across all versions.

  // The "UNIQUE" part of an SQL CREATE TABLE constraint. This value is
  // computed dring sealing the first version (0).
  std::string unique_constraint_;

  DISALLOW_COPY_AND_ASSIGN(SQLTableBuilder);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SQL_TABLE_BUILDER_H_
