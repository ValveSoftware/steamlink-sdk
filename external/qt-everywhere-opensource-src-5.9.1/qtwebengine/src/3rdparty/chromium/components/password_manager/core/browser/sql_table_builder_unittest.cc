// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sql_table_builder.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

class SQLTableBuilderTest : public testing::Test {
 public:
  SQLTableBuilderTest() { Init(); }

  ~SQLTableBuilderTest() override = default;

 protected:
  // Checks whether a column with a given |name| is listed with the given
  // |type| in the database.
  bool IsColumnOfType(const std::string& name, const std::string& type);

  sql::Connection* db() { return &db_; }

  SQLTableBuilder* builder() { return &builder_; }

 private:
  // Part of constructor, needs to be a void-returning function to use ASSERTs.
  void Init();

  // Error handler for the SQL connection, prints the error code and the
  // statement details.
  void PrintDBError(int code, sql::Statement* statement);

  sql::Connection db_;
  SQLTableBuilder builder_;

  DISALLOW_COPY_AND_ASSIGN(SQLTableBuilderTest);
};

bool SQLTableBuilderTest::IsColumnOfType(const std::string& name,
                                         const std::string& type) {
  return db()->GetSchema().find(name + " " + type) != std::string::npos;
}

void SQLTableBuilderTest::Init() {
  db_.set_error_callback(
      base::Bind(&SQLTableBuilderTest::PrintDBError, base::Unretained(this)));
  ASSERT_TRUE(db_.OpenInMemory());
  // The following column must always be present, so let's add it here.
  builder_.AddColumnToUniqueKey("signon_realm", "VARCHAR NOT NULL");
}

void SQLTableBuilderTest::PrintDBError(int code, sql::Statement* statement) {
  VLOG(0) << "DB error encountered, code = " << code;
  if (statement) {
    VLOG(0) << "statement string = " << statement->GetSQLStatement();
    VLOG(0) << "statement is " << (statement->is_valid() ? "valid" : "invalid");
  }
}

TEST_F(SQLTableBuilderTest, SealVersion_0) {
  EXPECT_EQ(0u, builder()->SealVersion());
  EXPECT_TRUE(builder()->MigrateFrom(0, db()));
  EXPECT_TRUE(builder()->CreateTable(db()));
  EXPECT_TRUE(db()->DoesTableExist("logins"));
  EXPECT_TRUE(db()->DoesIndexExist("logins_signon"));
  EXPECT_TRUE(db()->DoesColumnExist("logins", "signon_realm"));
  EXPECT_TRUE(IsColumnOfType("signon_realm", "VARCHAR NOT NULL"));
}

TEST_F(SQLTableBuilderTest, AddColumn) {
  builder()->AddColumn("password_value", "BLOB");
  EXPECT_EQ(0u, builder()->SealVersion());
  EXPECT_TRUE(builder()->MigrateFrom(0, db()));
  EXPECT_TRUE(builder()->CreateTable(db()));
  EXPECT_TRUE(db()->DoesTableExist("logins"));
  EXPECT_TRUE(db()->DoesIndexExist("logins_signon"));
  EXPECT_TRUE(db()->DoesColumnExist("logins", "signon_realm"));
  EXPECT_TRUE(db()->DoesColumnExist("logins", "password_value"));
  EXPECT_TRUE(IsColumnOfType("password_value", "BLOB"));
}

TEST_F(SQLTableBuilderTest, RenameColumn_InSameVersion) {
  builder()->AddColumn("old_name", "BLOB");
  builder()->RenameColumn("old_name", "password_value");
  EXPECT_EQ(0u, builder()->SealVersion());
  EXPECT_TRUE(builder()->CreateTable(db()));
  EXPECT_TRUE(db()->DoesTableExist("logins"));
  EXPECT_FALSE(db()->DoesColumnExist("logins", "old_name"));
  EXPECT_TRUE(db()->DoesColumnExist("logins", "password_value"));
  EXPECT_TRUE(IsColumnOfType("password_value", "BLOB"));
}

TEST_F(SQLTableBuilderTest, RenameColumn_InNextVersion) {
  builder()->AddColumn("old_name", "BLOB");
  EXPECT_EQ(0u, builder()->SealVersion());
  builder()->RenameColumn("old_name", "password_value");
  EXPECT_EQ(1u, builder()->SealVersion());
  EXPECT_TRUE(builder()->CreateTable(db()));
  EXPECT_TRUE(db()->DoesTableExist("logins"));
  EXPECT_FALSE(db()->DoesColumnExist("logins", "old_name"));
  EXPECT_TRUE(db()->DoesColumnExist("logins", "password_value"));
  EXPECT_TRUE(IsColumnOfType("password_value", "BLOB"));
}

TEST_F(SQLTableBuilderTest, RenameColumn_SameNameInSameVersion) {
  builder()->AddColumn("name", "BLOB");
  builder()->RenameColumn("name", "name");
  EXPECT_EQ(0u, builder()->SealVersion());
  EXPECT_TRUE(builder()->CreateTable(db()));
  EXPECT_TRUE(db()->DoesTableExist("logins"));
  EXPECT_TRUE(db()->DoesColumnExist("logins", "name"));
  EXPECT_TRUE(IsColumnOfType("name", "BLOB"));
}

TEST_F(SQLTableBuilderTest, RenameColumn_SameNameInNextVersion) {
  builder()->AddColumn("name", "BLOB");
  EXPECT_EQ(0u, builder()->SealVersion());
  builder()->RenameColumn("name", "name");
  EXPECT_EQ(1u, builder()->SealVersion());
  EXPECT_TRUE(builder()->CreateTable(db()));
  EXPECT_TRUE(db()->DoesTableExist("logins"));
  EXPECT_TRUE(db()->DoesColumnExist("logins", "name"));
  EXPECT_TRUE(IsColumnOfType("name", "BLOB"));
}

TEST_F(SQLTableBuilderTest, DropColumn_InSameVersion) {
  builder()->AddColumn("password_value", "BLOB");
  builder()->DropColumn("password_value");
  EXPECT_EQ(0u, builder()->SealVersion());
  EXPECT_TRUE(builder()->CreateTable(db()));
  EXPECT_TRUE(db()->DoesTableExist("logins"));
  EXPECT_FALSE(db()->DoesColumnExist("logins", "password_value"));
}

TEST_F(SQLTableBuilderTest, DropColumn_InNextVersion) {
  builder()->AddColumn("password_value", "BLOB");
  EXPECT_EQ(0u, builder()->SealVersion());
  builder()->DropColumn("password_value");
  EXPECT_EQ(1u, builder()->SealVersion());
  EXPECT_TRUE(builder()->CreateTable(db()));
  EXPECT_TRUE(db()->DoesTableExist("logins"));
  EXPECT_FALSE(db()->DoesColumnExist("logins", "password_value"));
}

TEST_F(SQLTableBuilderTest, MigrateFrom) {
  // First, create a table at version 0, with some columns.
  builder()->AddColumn("for_renaming", "INTEGER DEFAULT 100");
  builder()->AddColumn("for_deletion", "INTEGER");
  EXPECT_EQ(0u, builder()->SealVersion());
  EXPECT_TRUE(builder()->CreateTable(db()));
  EXPECT_TRUE(db()->DoesTableExist("logins"));
  EXPECT_TRUE(db()->DoesColumnExist("logins", "for_renaming"));
  EXPECT_TRUE(db()->DoesColumnExist("logins", "for_deletion"));
  EXPECT_TRUE(
      db()->Execute("INSERT INTO logins (signon_realm, for_renaming, "
                    "for_deletion) VALUES ('abc', 123, 456)"));
  const char retrieval[] = "SELECT * FROM logins";
  sql::Statement first_check(
      db()->GetCachedStatement(SQL_FROM_HERE, retrieval));
  EXPECT_TRUE(first_check.Step());
  EXPECT_EQ(3, first_check.ColumnCount());
  EXPECT_EQ("abc", first_check.ColumnString(0));
  EXPECT_EQ(123, first_check.ColumnInt(1));
  EXPECT_EQ(456, first_check.ColumnInt(2));
  EXPECT_FALSE(first_check.Step());
  EXPECT_TRUE(first_check.Succeeded());

  // Now, specify some modifications for version 1.
  builder()->RenameColumn("for_renaming", "renamed");
  builder()->DropColumn("for_deletion");
  builder()->AddColumn("new_column", "INTEGER DEFAULT 789");
  EXPECT_EQ(1u, builder()->SealVersion());

  // The migration should have the following effect:
  // * The renamed column should keep its non-default value.
  // * The succession of column removal and addition should not result in the
  //   values from the deleted column to be copied to the added one.
  EXPECT_TRUE(builder()->MigrateFrom(0, db()));
  EXPECT_FALSE(db()->DoesColumnExist("logins", "for_renaming"));
  EXPECT_FALSE(db()->DoesColumnExist("logins", "for_deletion"));
  EXPECT_TRUE(db()->DoesColumnExist("logins", "renamed"));
  EXPECT_TRUE(IsColumnOfType("renamed", "INTEGER"));
  EXPECT_TRUE(db()->DoesColumnExist("logins", "new_column"));
  sql::Statement second_check(
      db()->GetCachedStatement(SQL_FROM_HERE, retrieval));
  EXPECT_TRUE(second_check.Step());
  EXPECT_EQ(3, second_check.ColumnCount());
  EXPECT_EQ("abc", second_check.ColumnString(0));
  EXPECT_EQ(123, second_check.ColumnInt(1));
  EXPECT_EQ(789, second_check.ColumnInt(2));
  EXPECT_FALSE(second_check.Step());
  EXPECT_TRUE(second_check.Succeeded());
}

TEST_F(SQLTableBuilderTest, MigrateFrom_RenameAndAdd) {
  builder()->AddColumn("old_name", "INTEGER");
  EXPECT_EQ(0u, builder()->SealVersion());

  EXPECT_TRUE(builder()->CreateTable(db()));

  builder()->RenameColumn("old_name", "new_name");
  EXPECT_EQ(1u, builder()->SealVersion());

  builder()->AddColumn("added", "VARCHAR");
  EXPECT_EQ(2u, builder()->SealVersion());

  EXPECT_TRUE(builder()->MigrateFrom(0, db()));
  EXPECT_FALSE(db()->DoesColumnExist("logins", "old_name"));
  EXPECT_TRUE(db()->DoesColumnExist("logins", "added"));
  EXPECT_TRUE(db()->DoesColumnExist("logins", "new_name"));
  EXPECT_TRUE(IsColumnOfType("added", "VARCHAR"));
  EXPECT_TRUE(IsColumnOfType("new_name", "INTEGER"));
  EXPECT_EQ(3u, builder()->NumberOfColumns());
  EXPECT_EQ("signon_realm, new_name, added", builder()->ListAllColumnNames());
  EXPECT_EQ("new_name=?, added=?", builder()->ListAllNonuniqueKeyNames());
  EXPECT_EQ("signon_realm=?", builder()->ListAllUniqueKeyNames());
}

TEST_F(SQLTableBuilderTest, MigrateFrom_RenameAndAddAndDrop) {
  builder()->AddColumnToUniqueKey("uni", "VARCHAR NOT NULL");
  builder()->AddColumn("old_name", "INTEGER");
  EXPECT_EQ(0u, builder()->SealVersion());

  EXPECT_TRUE(builder()->CreateTable(db()));

  builder()->RenameColumn("old_name", "new_name");
  EXPECT_EQ(1u, builder()->SealVersion());

  builder()->AddColumn("added", "VARCHAR");
  EXPECT_EQ(2u, builder()->SealVersion());

  builder()->DropColumn("added");
  EXPECT_EQ(3u, builder()->SealVersion());

  EXPECT_TRUE(builder()->MigrateFrom(0, db()));
  EXPECT_FALSE(db()->DoesColumnExist("logins", "old_name"));
  EXPECT_FALSE(db()->DoesColumnExist("logins", "added"));
  EXPECT_TRUE(db()->DoesColumnExist("logins", "uni"));
  EXPECT_TRUE(db()->DoesColumnExist("logins", "new_name"));
  EXPECT_TRUE(IsColumnOfType("new_name", "INTEGER"));
  EXPECT_EQ(3u, builder()->NumberOfColumns());
  EXPECT_EQ("signon_realm, uni, new_name", builder()->ListAllColumnNames());
  EXPECT_EQ("new_name=?", builder()->ListAllNonuniqueKeyNames());
  EXPECT_EQ("signon_realm=? AND uni=?", builder()->ListAllUniqueKeyNames());
}

}  // namespace password_manager
