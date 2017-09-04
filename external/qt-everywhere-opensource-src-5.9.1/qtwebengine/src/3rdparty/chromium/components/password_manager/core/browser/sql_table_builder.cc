// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sql_table_builder.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/numerics/safe_conversions.h"
#include "sql/connection.h"
#include "sql/transaction.h"

namespace password_manager {

namespace {

// Appends |name| to |list_of_names|, separating items with ", ".
void Append(const std::string& name, std::string* list_of_names) {
  if (list_of_names->empty())
    *list_of_names = name;
  else
    *list_of_names += ", " + name;
}

}  // namespace

struct SQLTableBuilder::Column {
  std::string name;
  std::string type;
  bool part_of_unique_key;
  // The first version this column is part of.
  unsigned min_version;
  // The last version this column is part of. The value of kInvalidVersion
  // means that it is part of all versions since |min_version|.
  unsigned max_version;
  // Renaming of a column is stored as a sequence of one removed and one added
  // column in |columns_|. To distinguish it from an unrelated removal and
  // addition, the following bit is set to true for the added columns which
  // are part of renaming. Those columns will get the data of their
  // predecessors. If the bit is false, the column will be filled with the
  // default value on creation.
  bool gets_previous_data;
};

SQLTableBuilder::SQLTableBuilder() = default;

SQLTableBuilder::~SQLTableBuilder() = default;

void SQLTableBuilder::AddColumn(const std::string& name,
                                const std::string& type) {
  DCHECK(FindLastByName(name) == columns_.rend());
  Column column = {name, type, false, sealed_version_ + 1, kInvalidVersion,
                   false};
  columns_.push_back(std::move(column));
}

void SQLTableBuilder::AddColumnToUniqueKey(const std::string& name,
                                           const std::string& type) {
  AddColumn(name, type);
  columns_.back().part_of_unique_key = true;
}

void SQLTableBuilder::RenameColumn(const std::string& old_name,
                                   const std::string& new_name) {
  auto old_column = FindLastByName(old_name);
  DCHECK(old_column != columns_.rend());

  if (old_name == new_name)  // The easy case.
    return;

  DCHECK_NE("signon_realm", old_name);
  if (sealed_version_ != kInvalidVersion &&
      old_column->min_version <= sealed_version_) {
    // This column exists in the last sealed version. Therefore it cannot be
    // just replaced, it needs to be kept for generating the migration code.
    Column new_column = {new_name,
                         old_column->type,
                         old_column->part_of_unique_key,
                         sealed_version_ + 1,
                         kInvalidVersion,
                         true};
    old_column->max_version = sealed_version_;
    auto past_old =
        old_column.base();  // Points one element after |old_column|.
    columns_.insert(past_old, std::move(new_column));
  } else {
    // This column was just introduced in the currently unsealed version. To
    // rename it, it is enough just to modify the entry in columns_.
    old_column->name = new_name;
  }
}

// Removes column |name|. |name| must have been added in the past.
void SQLTableBuilder::DropColumn(const std::string& name) {
  auto column = FindLastByName(name);
  DCHECK(column != columns_.rend());
  DCHECK_NE("signon_realm", name);
  if (sealed_version_ != kInvalidVersion &&
      column->min_version <= sealed_version_) {
    // This column exists in the last sealed version. Therefore it cannot be
    // just deleted, it needs to be kept for generating the migration code.
    column->max_version = sealed_version_;
  } else {
    // This column was just introduced in the currently unsealed version. It
    // can be just erased from |columns_|.
    columns_.erase(
        --(column.base()));  // base() points one element after |column|.
  }
}

unsigned SQLTableBuilder::SealVersion() {
  DCHECK(FindLastByName("signon_realm") != columns_.rend());
  if (sealed_version_ == kInvalidVersion) {
    DCHECK_EQ(std::string(), unique_constraint_);
    // First sealed version, time to compute the UNIQUE string.
    std::string unique_key;
    for (const Column& column : columns_) {
      if (column.part_of_unique_key)
        Append(column.name, &unique_key);
    }
    DCHECK(!unique_key.empty());
    unique_constraint_ = "UNIQUE (" + unique_key + ")";
  }
  DCHECK(!unique_constraint_.empty());
  return ++sealed_version_;
}

bool SQLTableBuilder::MigrateFrom(unsigned old_version, sql::Connection* db) {
  for (; old_version < sealed_version_; ++old_version) {
    if (!MigrateToNextFrom(old_version, db))
      return false;
  }

  return true;
}

bool SQLTableBuilder::CreateTable(sql::Connection* db) {
  DCHECK(IsVersionLastAndSealed(sealed_version_));
  DCHECK(!unique_constraint_.empty());

  if (db->DoesTableExist("logins"))
    return true;

  std::string names;  // Names and types of the current columns.
  for (const Column& column : columns_) {
    if (IsInLastVersion(column))
      Append(column.name + " " + column.type, &names);
  }

  sql::Transaction transaction(db);
  return transaction.Begin() &&
         db->Execute(
             ("CREATE TABLE logins (" + names + ", " + unique_constraint_ + ")")
                 .c_str()) &&
         db->Execute("CREATE INDEX logins_signon ON logins (signon_realm)") &&
         transaction.Commit();
}

std::string SQLTableBuilder::ListAllColumnNames() const {
  DCHECK(IsVersionLastAndSealed(sealed_version_));
  std::string result;
  for (const Column& column : columns_) {
    if (IsInLastVersion(column))
      Append(column.name, &result);
  }
  return result;
}

std::string SQLTableBuilder::ListAllNonuniqueKeyNames() const {
  DCHECK(IsVersionLastAndSealed(sealed_version_));
  std::string result;
  for (const Column& column : columns_) {
    if (IsInLastVersion(column) && !column.part_of_unique_key)
      Append(column.name + "=?", &result);
  }
  return result;
}

std::string SQLTableBuilder::ListAllUniqueKeyNames() const {
  DCHECK(IsVersionLastAndSealed(sealed_version_));
  std::string result;
  for (const Column& column : columns_) {
    if (IsInLastVersion(column) && column.part_of_unique_key) {
      if (!result.empty())
        result += " AND ";
      result += column.name + "=?";
    }
  }
  return result;
}

size_t SQLTableBuilder::NumberOfColumns() const {
  DCHECK(IsVersionLastAndSealed(sealed_version_));
  return base::checked_cast<size_t>(std::count_if(
      columns_.begin(), columns_.end(),
      [this](const Column& column) { return (IsInLastVersion(column)); }));
}

bool SQLTableBuilder::MigrateToNextFrom(unsigned old_version,
                                        sql::Connection* db) {
  DCHECK_LT(old_version, sealed_version_);
  DCHECK_GE(old_version, 0u);
  DCHECK(IsVersionLastAndSealed(sealed_version_));
  DCHECK(!unique_constraint_.empty());

  // Names of columns from old version, values of which are copied.
  std::string old_names;
  // Names of columns in new version, except for added ones.
  std::string new_names;
  std::vector<std::string> added_names;  // Names of added columns.

  // A temporary table will be needed if some columns are dropped or renamed,
  // because that is not supported by a single SQLite command.
  bool needs_temp_table = false;

  for (auto column = columns_.begin(); column != columns_.end(); ++column) {
    if (column->max_version == old_version) {
      DCHECK(!column->part_of_unique_key);
      // This column was deleted after |old_version|. It can have two reasons:
      needs_temp_table = true;
      auto next_column = column;
      ++next_column;
      if (next_column != columns_.end() && next_column->gets_previous_data) {
        // (1) The column is being renamed.
        DCHECK_EQ(column->type, next_column->type);
        DCHECK_NE(column->name, next_column->name);
        Append(column->name, &old_names);
        Append(next_column->name + " " + next_column->type, &new_names);
        ++column;  // Avoid processing next_column in the next loop.
      } else {
        // (2) The column is being dropped.
      }
    } else if (column->min_version == old_version + 1) {
      // This column was added after old_version.
      DCHECK(!column->part_of_unique_key);
      added_names.push_back(column->name + " " + column->type);
    } else if (column->min_version <= old_version &&
               (column->max_version == kInvalidVersion ||
                column->max_version > old_version)) {
      // This column stays.
      Append(column->name, &old_names);
      Append(column->name + " " + column->type, &new_names);
    }
  }

  if (needs_temp_table) {
    // Following the instructions from
    // https://www.sqlite.org/lang_altertable.html#otheralter, this code works
    // around the fact that SQLite does not allow dropping or renaming
    // columns. Instead, a new table is constructed, with the new column
    // names, and data from all but dropped columns from the current table are
    // copied into it. After that, the new table is renamed to the current
    // one.

    // Foreign key constraints are not enabled for the login database, so no
    // PRAGMA foreign_keys=off needed.
    sql::Transaction transaction(db);
    if (!transaction.Begin() ||
        !db->Execute(("CREATE TABLE temp_logins (" + new_names + ", " +
                      unique_constraint_ + ")")
                         .c_str()) ||
        !db->Execute(("INSERT OR REPLACE INTO temp_logins SELECT " + old_names +
                      " FROM logins")
                         .c_str()) ||
        !db->Execute("DROP TABLE logins") ||
        !db->Execute("ALTER TABLE temp_logins RENAME TO logins") ||
        !db->Execute("CREATE INDEX logins_signon ON logins (signon_realm)") ||
        !transaction.Commit()) {
      return false;
    }
  }

  if (!added_names.empty()) {
    sql::Transaction transaction(db);
    if (!transaction.Begin())
      return false;
    for (const std::string& name : added_names) {
      if (!db->Execute(("ALTER TABLE logins ADD COLUMN " + name).c_str()))
        return false;
    }
    if (!transaction.Commit())
      return false;
  }

  return true;
}

std::list<SQLTableBuilder::Column>::reverse_iterator
SQLTableBuilder::FindLastByName(const std::string& name) {
  return std::find_if(
      columns_.rbegin(), columns_.rend(),
      [&name](const Column& column) { return name == column.name; });
}

bool SQLTableBuilder::IsVersionLastAndSealed(unsigned version) const {
  // Is |version| the last sealed one?
  if (sealed_version_ != version)
    return false;
  // Is the current version the last sealed one? In other words, is there
  // either a column added past the sealed version (min_version > sealed) or
  // deleted one version after the sealed (max_version == sealed)?
  return columns_.end() ==
         std::find_if(columns_.begin(), columns_.end(),
                      [this](const Column& column) {
                        return column.min_version > sealed_version_ ||
                               column.max_version == sealed_version_;
                      });
}

bool SQLTableBuilder::IsInLastVersion(const Column& column) const {
  DCHECK(IsVersionLastAndSealed(sealed_version_));
  return (column.min_version <= sealed_version_ &&
          (column.max_version == kInvalidVersion ||
           column.max_version >= sealed_version_));
}

}  // namespace password_manager
