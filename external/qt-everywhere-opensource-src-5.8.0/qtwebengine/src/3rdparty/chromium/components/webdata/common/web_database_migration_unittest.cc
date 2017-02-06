// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/password_manager/core/browser/webdata/logins_table.h"
#include "components/search_engines/keyword_table.h"
#include "components/signin/core/browser/webdata/token_service_table.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::AutofillProfile;
using autofill::AutofillTable;
using autofill::CreditCard;
using base::ASCIIToUTF16;
using base::Time;

namespace {

std::string RemoveQuotes(const std::string& has_quotes) {
  std::string no_quotes;
  // SQLite quotes: http://www.sqlite.org/lang_keywords.html
  base::RemoveChars(has_quotes, "\"[]`", &no_quotes);
  return no_quotes;
}

}  // anonymous namespace

// The WebDatabaseMigrationTest encapsulates testing of database migrations.
// Specifically, these tests are intended to exercise any schema changes in
// the WebDatabase and data migrations that occur in
// |WebDatabase::MigrateOldVersionsAsNeeded()|.
class WebDatabaseMigrationTest : public testing::Test {
 public:
  WebDatabaseMigrationTest() {}
  ~WebDatabaseMigrationTest() override {}

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  void DoMigration() {
    AutofillTable autofill_table;
    KeywordTable keyword_table;
    LoginsTable logins_table;
    TokenServiceTable token_service_table;

    WebDatabase db;
    db.AddTable(&autofill_table);
    db.AddTable(&keyword_table);
    db.AddTable(&logins_table);
    db.AddTable(&token_service_table);

    // This causes the migration to occur.
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

 protected:
  // Current tested version number.  When adding a migration in
  // |WebDatabase::MigrateOldVersionsAsNeeded()| and changing the version number
  // |kCurrentVersionNumber| this value should change to reflect the new version
  // number and a new migration test added below.
  static const int kCurrentTestedVersionNumber;

  base::FilePath GetDatabasePath() {
    const base::FilePath::CharType kWebDatabaseFilename[] =
        FILE_PATH_LITERAL("TestWebDatabase.sqlite3");
    return temp_dir_.path().Append(base::FilePath(kWebDatabaseFilename));
  }

  // The textual contents of |file| are read from
  // "components/test/data/web_database" and returned in the string |contents|.
  // Returns true if the file exists and is read successfully, false otherwise.
  bool GetWebDatabaseData(const base::FilePath& file, std::string* contents) {
    base::FilePath source_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &source_path);
    source_path = source_path.AppendASCII("components");
    source_path = source_path.AppendASCII("test");
    source_path = source_path.AppendASCII("data");
    source_path = source_path.AppendASCII("web_database");
    source_path = source_path.Append(file);
    return base::PathExists(source_path) &&
        base::ReadFileToString(source_path, contents);
  }

  static int VersionFromConnection(sql::Connection* connection) {
    // Get version.
    sql::Statement s(connection->GetUniqueStatement(
        "SELECT value FROM meta WHERE key='version'"));
    if (!s.Step())
      return 0;
    return s.ColumnInt(0);
  }

  // The sql files located in "components/test/data/web_database" were generated
  // by launching the Chromium application prior to schema change, then using
  // the sqlite3 command-line application to dump the contents of the "Web Data"
  // database.
  // Like this:
  //   > .output version_nn.sql
  //   > .dump
  void LoadDatabase(const base::FilePath::StringType& file);

 private:
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabaseMigrationTest);
};

const int WebDatabaseMigrationTest::kCurrentTestedVersionNumber = 67;

void WebDatabaseMigrationTest::LoadDatabase(
    const base::FilePath::StringType& file) {
  std::string contents;
  ASSERT_TRUE(GetWebDatabaseData(base::FilePath(file), &contents));

  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(connection.Execute(contents.data()));
}

// Tests that migrating from the golden files version_XX.sql results in the same
// schema as migrating from an empty database.
TEST_F(WebDatabaseMigrationTest, VersionXxSqlFilesAreGolden) {
  DoMigration();
  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  const std::string& expected_schema = RemoveQuotes(connection.GetSchema());
  for (int i = WebDatabase::kDeprecatedVersionNumber + 1;
       i < kCurrentTestedVersionNumber; ++i) {
    // We don't test version 52 because there's a slight discrepancy in the
    // initialization code and the migration code (relating to schema
    // formatting). Fixing the bug is possible, but would require updating every
    // version_nn.sql file.
    if (i == 52)
      continue;

    connection.Raze();
    const base::FilePath& file_name = base::FilePath::FromUTF8Unsafe(
        "version_" + base::IntToString(i) + ".sql");
    ASSERT_NO_FATAL_FAILURE(LoadDatabase(file_name.value()))
        << "Failed to load " << file_name.MaybeAsASCII();
    DoMigration();
    EXPECT_EQ(expected_schema, RemoveQuotes(connection.GetSchema()))
        << "For version " << i;
  }
}

// Tests that the all migrations from an empty database succeed.
TEST_F(WebDatabaseMigrationTest, MigrateEmptyToCurrent) {
  DoMigration();

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // Check that expected tables are present.
    EXPECT_TRUE(connection.DoesTableExist("autofill"));
    // The autofill_dates table is obsolete. (It's been merged into the autofill
    // table.)
    EXPECT_FALSE(connection.DoesTableExist("autofill_dates"));
    EXPECT_TRUE(connection.DoesTableExist("autofill_profiles"));
    EXPECT_TRUE(connection.DoesTableExist("credit_cards"));
    EXPECT_TRUE(connection.DoesTableExist("keywords"));
    // The logins table is obsolete. (We used to store saved passwords here.)
    EXPECT_FALSE(connection.DoesTableExist("logins"));
    EXPECT_TRUE(connection.DoesTableExist("meta"));
    EXPECT_TRUE(connection.DoesTableExist("token_service"));
    // The web_apps and web_apps_icons tables are obsolete as of version 58.
    EXPECT_FALSE(connection.DoesTableExist("web_apps"));
    EXPECT_FALSE(connection.DoesTableExist("web_app_icons"));
    // The web_intents and web_intents_defaults tables are obsolete as of
    // version 58.
    EXPECT_FALSE(connection.DoesTableExist("web_intents"));
    EXPECT_FALSE(connection.DoesTableExist("web_intents_defaults"));
  }
}

// Versions below 52 are deprecated. This verifies that old databases are razed.
TEST_F(WebDatabaseMigrationTest, RazeDeprecatedVersionAndReinit) {
  ASSERT_NO_FATAL_FAILURE(
      LoadDatabase(FILE_PATH_LITERAL("version_50.sql")));

  // Verify pre-conditions.  These are expectations for version 50 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 50, 50));

    ASSERT_FALSE(connection.DoesColumnExist("keywords", "image_url"));
    ASSERT_FALSE(connection.DoesColumnExist("keywords",
                                            "search_url_post_params"));
    ASSERT_FALSE(connection.DoesColumnExist("keywords",
                                            "suggest_url_post_params"));
    ASSERT_FALSE(connection.DoesColumnExist("keywords",
                                            "instant_url_post_params"));
    ASSERT_FALSE(connection.DoesColumnExist("keywords",
                                            "image_url_post_params"));
  }

  DoMigration();

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // New columns should have been created.
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "image_url"));
    EXPECT_TRUE(connection.DoesColumnExist("keywords",
                                           "search_url_post_params"));
    EXPECT_TRUE(connection.DoesColumnExist("keywords",
                                           "suggest_url_post_params"));
    EXPECT_TRUE(connection.DoesColumnExist("keywords",
                                           "instant_url_post_params"));
    EXPECT_TRUE(connection.DoesColumnExist("keywords",
                                           "image_url_post_params"));
  }
}

// Tests that the column |new_tab_url| is added to the keyword table schema for
// a version 52 database.
TEST_F(WebDatabaseMigrationTest, MigrateVersion52ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(
      LoadDatabase(FILE_PATH_LITERAL("version_52.sql")));

  // Verify pre-conditions.  These are expectations for version 52 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 52, 52));

    ASSERT_FALSE(connection.DoesColumnExist("keywords", "new_tab_url"));
  }

  DoMigration();

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // New columns should have been created.
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "new_tab_url"));
  }
}

// Tests that for a version 54 database,
//   (a) The street_address, dependent_locality, and sorting_code columns are
//       added to the autofill_profiles table schema.
//   (b) The address_line1, address_line2, and country columns are dropped from
//       the autofill_profiles table schema.
//   (c) The type column is dropped from the autofill_profile_phones schema.
TEST_F(WebDatabaseMigrationTest, MigrateVersion53ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_53.sql")));

  // Verify pre-conditions.  These are expectations for version 53 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    EXPECT_TRUE(
        connection.DoesColumnExist("autofill_profiles", "address_line_1"));
    EXPECT_TRUE(
        connection.DoesColumnExist("autofill_profiles", "address_line_2"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "country"));
    EXPECT_FALSE(
        connection.DoesColumnExist("autofill_profiles", "street_address"));
    EXPECT_FALSE(
        connection.DoesColumnExist("autofill_profiles", "dependent_locality"));
    EXPECT_FALSE(
        connection.DoesColumnExist("autofill_profiles", "sorting_code"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profile_phones", "type"));
  }

  DoMigration();

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // Columns should have been added and removed appropriately.
    EXPECT_FALSE(
        connection.DoesColumnExist("autofill_profiles", "address_line1"));
    EXPECT_FALSE(
        connection.DoesColumnExist("autofill_profiles", "address_line2"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "country"));
    EXPECT_TRUE(
        connection.DoesColumnExist("autofill_profiles", "street_address"));
    EXPECT_TRUE(
        connection.DoesColumnExist("autofill_profiles", "dependent_locality"));
    EXPECT_TRUE(
        connection.DoesColumnExist("autofill_profiles", "sorting_code"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profile_phones", "type"));

    // Data should have been preserved.
    sql::Statement s_profiles(
        connection.GetUniqueStatement(
            "SELECT guid, company_name, street_address, dependent_locality,"
            " city, state, zipcode, sorting_code, country_code, date_modified,"
            " origin "
            "FROM autofill_profiles"));

    // Address lines 1 and 2.
    ASSERT_TRUE(s_profiles.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000001",
              s_profiles.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Google, Inc."), s_profiles.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("1950 Charleston Rd.\n"
                           "(2nd floor)"),
              s_profiles.ColumnString16(2));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(3));
    EXPECT_EQ(ASCIIToUTF16("Mountain View"), s_profiles.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16("CA"), s_profiles.ColumnString16(5));
    EXPECT_EQ(ASCIIToUTF16("94043"), s_profiles.ColumnString16(6));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(7));
    EXPECT_EQ(ASCIIToUTF16("US"), s_profiles.ColumnString16(8));
    EXPECT_EQ(1386046731, s_profiles.ColumnInt(9));
    EXPECT_EQ(ASCIIToUTF16(autofill::kSettingsOrigin),
              s_profiles.ColumnString16(10));

    // Only address line 1.
    ASSERT_TRUE(s_profiles.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000002",
              s_profiles.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Google!"), s_profiles.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("1600 Amphitheatre Pkwy."),
              s_profiles.ColumnString16(2));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(3));
    EXPECT_EQ(ASCIIToUTF16("Mtn. View"), s_profiles.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16("California"), s_profiles.ColumnString16(5));
    EXPECT_EQ(ASCIIToUTF16("94043-1234"), s_profiles.ColumnString16(6));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(7));
    EXPECT_EQ(ASCIIToUTF16("US"), s_profiles.ColumnString16(8));
    EXPECT_EQ(1386046800, s_profiles.ColumnInt(9));
    EXPECT_EQ(ASCIIToUTF16(autofill::kSettingsOrigin),
              s_profiles.ColumnString16(10));

    // Only address line 2.
    ASSERT_TRUE(s_profiles.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000003",
              s_profiles.ColumnString(0));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("\nOnly line 2???"), s_profiles.ColumnString16(2));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(3));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(4));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(5));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(6));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(7));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(8));
    EXPECT_EQ(1386046834, s_profiles.ColumnInt(9));
    EXPECT_EQ(ASCIIToUTF16(autofill::kSettingsOrigin),
              s_profiles.ColumnString16(10));

    // No address lines.
    ASSERT_TRUE(s_profiles.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000004",
              s_profiles.ColumnString(0));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(1));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(2));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(3));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16("Texas"), s_profiles.ColumnString16(5));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(6));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(7));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(8));
    EXPECT_EQ(1386046847, s_profiles.ColumnInt(9));
    EXPECT_EQ(ASCIIToUTF16(autofill::kSettingsOrigin),
              s_profiles.ColumnString16(10));

    // That should be it.
    EXPECT_FALSE(s_profiles.Step());

    // Verify the phone number data as well.
    sql::Statement s_phones(
        connection.GetUniqueStatement(
            "SELECT guid, number FROM autofill_profile_phones"));

    ASSERT_TRUE(s_phones.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000001", s_phones.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("1.800.555.1234"), s_phones.ColumnString16(1));

    ASSERT_TRUE(s_phones.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000001", s_phones.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("+1 (800) 555-4321"), s_phones.ColumnString16(1));

    ASSERT_TRUE(s_phones.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000002", s_phones.ColumnString(0));
    EXPECT_EQ(base::string16(), s_phones.ColumnString16(1));

    ASSERT_TRUE(s_phones.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000003", s_phones.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("6505557890"), s_phones.ColumnString16(1));

    ASSERT_TRUE(s_phones.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000004", s_phones.ColumnString(0));
    EXPECT_EQ(base::string16(), s_phones.ColumnString16(1));

    EXPECT_FALSE(s_phones.Step());
  }
}

// Tests that migrating from version 54 to version 55 drops the autofill_dates
// table, and merges the appropriate dates into the autofill table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion54ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_54.sql")));

  // Verify pre-conditions.  These are expectations for version 54 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    EXPECT_TRUE(connection.DoesTableExist("autofill_dates"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill", "date_created"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill", "date_last_used"));

    // Verify the incoming data.
    sql::Statement s_autofill(connection.GetUniqueStatement(
        "SELECT name, value, value_lower, pair_id, count FROM autofill"));
    sql::Statement s_dates(connection.GetUniqueStatement(
        "SELECT pair_id, date_created FROM autofill_dates"));

    // An entry with one timestamp.
    ASSERT_TRUE(s_autofill.Step());
    EXPECT_EQ(ASCIIToUTF16("Name"), s_autofill.ColumnString16(0));
    EXPECT_EQ(ASCIIToUTF16("John Doe"), s_autofill.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("john doe"), s_autofill.ColumnString16(2));
    EXPECT_EQ(10, s_autofill.ColumnInt(3));
    EXPECT_EQ(1, s_autofill.ColumnInt(4));
    ASSERT_TRUE(s_dates.Step());
    EXPECT_EQ(10, s_dates.ColumnInt(0));
    EXPECT_EQ(1384299100, s_dates.ColumnInt64(1));

    // Another entry with one timestamp, differing from the previous one in case
    // only.
    ASSERT_TRUE(s_autofill.Step());
    EXPECT_EQ(ASCIIToUTF16("Name"), s_autofill.ColumnString16(0));
    EXPECT_EQ(ASCIIToUTF16("john doe"), s_autofill.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("john doe"), s_autofill.ColumnString16(2));
    EXPECT_EQ(11, s_autofill.ColumnInt(3));
    EXPECT_EQ(1, s_autofill.ColumnInt(4));
    ASSERT_TRUE(s_dates.Step());
    EXPECT_EQ(11, s_dates.ColumnInt(0));
    EXPECT_EQ(1384299200, s_dates.ColumnInt64(1));

    // An entry with two timestamps (with count > 2; this is realistic).
    ASSERT_TRUE(s_autofill.Step());
    EXPECT_EQ(ASCIIToUTF16("Email"), s_autofill.ColumnString16(0));
    EXPECT_EQ(ASCIIToUTF16("jane@example.com"), s_autofill.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("jane@example.com"), s_autofill.ColumnString16(2));
    EXPECT_EQ(20, s_autofill.ColumnInt(3));
    EXPECT_EQ(3, s_autofill.ColumnInt(4));
    ASSERT_TRUE(s_dates.Step());
    EXPECT_EQ(20, s_dates.ColumnInt(0));
    EXPECT_EQ(1384299300, s_dates.ColumnInt64(1));
    ASSERT_TRUE(s_dates.Step());
    EXPECT_EQ(20, s_dates.ColumnInt(0));
    EXPECT_EQ(1384299301, s_dates.ColumnInt64(1));

    // An entry with more than two timestamps, which are stored out of order.
    ASSERT_TRUE(s_autofill.Step());
    EXPECT_EQ(ASCIIToUTF16("Email"), s_autofill.ColumnString16(0));
    EXPECT_EQ(ASCIIToUTF16("jane.doe@example.org"),
              s_autofill.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("jane.doe@example.org"),
              s_autofill.ColumnString16(2));
    EXPECT_EQ(21, s_autofill.ColumnInt(3));
    EXPECT_EQ(4, s_autofill.ColumnInt(4));
    ASSERT_TRUE(s_dates.Step());
    EXPECT_EQ(21, s_dates.ColumnInt(0));
    EXPECT_EQ(1384299401, s_dates.ColumnInt64(1));
    ASSERT_TRUE(s_dates.Step());
    EXPECT_EQ(21, s_dates.ColumnInt(0));
    EXPECT_EQ(1384299400, s_dates.ColumnInt64(1));
    ASSERT_TRUE(s_dates.Step());
    EXPECT_EQ(21, s_dates.ColumnInt(0));
    EXPECT_EQ(1384299403, s_dates.ColumnInt64(1));
    ASSERT_TRUE(s_dates.Step());
    EXPECT_EQ(21, s_dates.ColumnInt(0));
    EXPECT_EQ(1384299402, s_dates.ColumnInt64(1));

    // No more entries expected.
    ASSERT_FALSE(s_autofill.Step());
    ASSERT_FALSE(s_dates.Step());
  }

  DoMigration();

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The autofill_dates table should have been dropped, and its columns should
    // have been migrated to the autofill table.
    EXPECT_FALSE(connection.DoesTableExist("autofill_dates"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill", "date_created"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill", "date_last_used"));

    // Data should have been preserved.  Note that it appears out of order
    // relative to the previous table, as it's been alphabetized.  That's ok.
    sql::Statement s(
        connection.GetUniqueStatement(
            "SELECT name, value, value_lower, date_created, date_last_used,"
            " count "
            "FROM autofill "
            "ORDER BY name, value ASC"));

    // "jane.doe@example.org": Timestamps should be parsed correctly, and only
    // the first and last should be kept.
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(ASCIIToUTF16("Email"), s.ColumnString16(0));
    EXPECT_EQ(ASCIIToUTF16("jane.doe@example.org"), s.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("jane.doe@example.org"), s.ColumnString16(2));
    EXPECT_EQ(1384299400, s.ColumnInt64(3));
    EXPECT_EQ(1384299403, s.ColumnInt64(4));
    EXPECT_EQ(4, s.ColumnInt(5));

    // "jane@example.com": Timestamps should be parsed correctly.
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(ASCIIToUTF16("Email"), s.ColumnString16(0));
    EXPECT_EQ(ASCIIToUTF16("jane@example.com"), s.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("jane@example.com"), s.ColumnString16(2));
    EXPECT_EQ(1384299300, s.ColumnInt64(3));
    EXPECT_EQ(1384299301, s.ColumnInt64(4));
    EXPECT_EQ(3, s.ColumnInt(5));

    // "John Doe": The single timestamp should be assigned as both the creation
    // and the last use timestamp.
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(ASCIIToUTF16("Name"), s.ColumnString16(0));
    EXPECT_EQ(ASCIIToUTF16("John Doe"), s.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("john doe"), s.ColumnString16(2));
    EXPECT_EQ(1384299100, s.ColumnInt64(3));
    EXPECT_EQ(1384299100, s.ColumnInt64(4));
    EXPECT_EQ(1, s.ColumnInt(5));

    // "john doe": Should not be merged with "John Doe" (case-sensitivity).
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(ASCIIToUTF16("Name"), s.ColumnString16(0));
    EXPECT_EQ(ASCIIToUTF16("john doe"), s.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("john doe"), s.ColumnString16(2));
    EXPECT_EQ(1384299200, s.ColumnInt64(3));
    EXPECT_EQ(1384299200, s.ColumnInt64(4));
    EXPECT_EQ(1, s.ColumnInt(5));

    // No more entries expected.
    ASSERT_FALSE(s.Step());
  }
}

// Tests that migrating from version 55 to version 56 adds the language_code
// column to autofill_profiles table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion55ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_55.sql")));

  // Verify pre-conditions. These are expectations for version 55 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_FALSE(
        connection.DoesColumnExist("autofill_profiles", "language_code"));
  }

  DoMigration();

  // Verify post-conditions. These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The language_code column should have been added to autofill_profiles
    // table.
    EXPECT_TRUE(
        connection.DoesColumnExist("autofill_profiles", "language_code"));

    // Data should have been preserved. Language code should have been set to
    // empty string.
    sql::Statement s_profiles(
        connection.GetUniqueStatement(
            "SELECT guid, company_name, street_address, dependent_locality,"
            " city, state, zipcode, sorting_code, country_code, date_modified,"
            " origin, language_code "
            "FROM autofill_profiles"));

    ASSERT_TRUE(s_profiles.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000001",
              s_profiles.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Google Inc"), s_profiles.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("340 Main St"),
              s_profiles.ColumnString16(2));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(3));
    EXPECT_EQ(ASCIIToUTF16("Los Angeles"), s_profiles.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16("CA"), s_profiles.ColumnString16(5));
    EXPECT_EQ(ASCIIToUTF16("90291"), s_profiles.ColumnString16(6));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(7));
    EXPECT_EQ(ASCIIToUTF16("US"), s_profiles.ColumnString16(8));
    EXPECT_EQ(1395948829, s_profiles.ColumnInt(9));
    EXPECT_EQ(ASCIIToUTF16(autofill::kSettingsOrigin),
              s_profiles.ColumnString16(10));
    EXPECT_EQ(std::string(), s_profiles.ColumnString(11));

    // No more entries expected.
    ASSERT_FALSE(s_profiles.Step());
  }
}

// Tests that migrating from version 56 to version 57 adds the full_name
// column to autofill_profile_names table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion56ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_56.sql")));

  // Verify pre-conditions. These are expectations for version 56 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_FALSE(
        connection.DoesColumnExist("autofill_profile_names", "full_name"));

    // Verify the starting data.
    sql::Statement s_names(
        connection.GetUniqueStatement(
            "SELECT guid, first_name, middle_name, last_name "
            "FROM autofill_profile_names"));
    ASSERT_TRUE(s_names.Step());
    EXPECT_EQ("B41FE6E0-B13E-2A2A-BF0B-29FCE2C3ADBD", s_names.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Jon"), s_names.ColumnString16(1));
    EXPECT_EQ(base::string16(), s_names.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16("Smith"), s_names.ColumnString16(3));
  }

  DoMigration();

  // Verify post-conditions. These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The full_name column should have been added to autofill_profile_names
    // table.
    EXPECT_TRUE(
        connection.DoesColumnExist("autofill_profile_names", "full_name"));

    // Data should have been preserved. Full name should have been set to the
    // empty string.
    sql::Statement s_names(
        connection.GetUniqueStatement(
            "SELECT guid, first_name, middle_name, last_name, full_name "
            "FROM autofill_profile_names"));

    ASSERT_TRUE(s_names.Step());
    EXPECT_EQ("B41FE6E0-B13E-2A2A-BF0B-29FCE2C3ADBD", s_names.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Jon"), s_names.ColumnString16(1));
    EXPECT_EQ(base::string16(), s_names.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16("Smith"), s_names.ColumnString16(3));
    EXPECT_EQ(base::string16(), s_names.ColumnString16(4));

    // No more entries expected.
    ASSERT_FALSE(s_names.Step());
  }
}

// Tests that migrating from version 57 to version 58 drops the web_intents and
// web_apps tables.
TEST_F(WebDatabaseMigrationTest, MigrateVersion57ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_57.sql")));

  // Verify pre-conditions. These are expectations for version 57 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_TRUE(connection.DoesTableExist("web_apps"));
    EXPECT_TRUE(connection.DoesTableExist("web_app_icons"));
    EXPECT_TRUE(connection.DoesTableExist("web_intents"));
    EXPECT_TRUE(connection.DoesTableExist("web_intents_defaults"));
  }

  DoMigration();

  // Verify post-conditions. These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_FALSE(connection.DoesTableExist("web_apps"));
    EXPECT_FALSE(connection.DoesTableExist("web_app_icons"));
    EXPECT_FALSE(connection.DoesTableExist("web_intents"));
    EXPECT_FALSE(connection.DoesTableExist("web_intents_defaults"));
  }
}

// Tests that migrating from version 58 to version 59 drops the omnibox
// extension keywords.
TEST_F(WebDatabaseMigrationTest, MigrateVersion58ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_58.sql")));

  const char query_extensions[] = "SELECT * FROM keywords "
      "WHERE url='chrome-extension://iphchnegaodmijmkdlbhbanjhfphhikp/"
      "?q={searchTerms}'";
  // Verify pre-conditions.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 58, 58));

    sql::Statement s(connection.GetUniqueStatement(query_extensions));
    ASSERT_TRUE(s.is_valid());
    int count = 0;
    while (s.Step()) {
      ++count;
    }
    EXPECT_EQ(1, count);
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    sql::Statement s(connection.GetUniqueStatement(query_extensions));
    ASSERT_TRUE(s.is_valid());
    int count = 0;
    while (s.Step()) {
      ++count;
    }
    EXPECT_EQ(0, count);

    s.Assign(connection.GetUniqueStatement("SELECT * FROM keywords "
        "WHERE short_name='Google'"));
    ASSERT_TRUE(s.is_valid());
    count = 0;
    while (s.Step()) {
      ++count;
    }
    EXPECT_EQ(1, count);
  }
}

// Tests creation of the server_credit_cards table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion59ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_59.sql")));

  // Verify pre-conditions.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 59, 59));

    ASSERT_FALSE(connection.DoesTableExist("masked_credit_cards"));
    ASSERT_FALSE(connection.DoesTableExist("unmasked_credit_cards"));
    ASSERT_FALSE(connection.DoesTableExist("server_addresses"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    ASSERT_TRUE(connection.DoesTableExist("masked_credit_cards"));
    ASSERT_TRUE(connection.DoesTableExist("unmasked_credit_cards"));
    ASSERT_TRUE(connection.DoesTableExist("server_addresses"));
  }
}

// Tests addition of use_count and use_date fields to autofill profiles and
// credit cards.
TEST_F(WebDatabaseMigrationTest, MigrateVersion60ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_60.sql")));

  // Verify pre-conditions.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 60, 60));

    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "use_count"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "use_date"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "use_count"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "use_date"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "use_count"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "use_date"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "use_count"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "use_date"));
  }
}

// Tests addition of use_count and use_date fields to unmasked server cards.
TEST_F(WebDatabaseMigrationTest, MigrateVersion61ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_61.sql")));

  // Verify pre-conditions.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 61, 61));

    EXPECT_FALSE(connection.DoesColumnExist("unmasked_credit_cards",
                                            "use_count"));
    EXPECT_FALSE(connection.DoesColumnExist("unmasked_credit_cards",
                                            "use_date"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_TRUE(connection.DoesColumnExist("unmasked_credit_cards",
                                           "use_count"));
    EXPECT_TRUE(connection.DoesColumnExist("unmasked_credit_cards",
                                           "use_date"));
  }
}

// Tests addition of server metadata tables.
TEST_F(WebDatabaseMigrationTest, MigrateVersion64ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_64.sql")));

  // Verify pre-conditions.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 64, 64));

    EXPECT_FALSE(connection.DoesTableExist("server_card_metadata"));
    EXPECT_FALSE(connection.DoesTableExist("server_address_metadata"));

    // Add a server address --- make sure it gets an ID.
    sql::Statement insert_profiles(
        connection.GetUniqueStatement(
            "INSERT INTO server_addresses(id, postal_code) "
            "VALUES ('', 90210)"));
    insert_profiles.Run();
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_TRUE(connection.DoesTableExist("server_card_metadata"));
    EXPECT_TRUE(connection.DoesTableExist("server_address_metadata"));

    sql::Statement read_profiles(
        connection.GetUniqueStatement(
            "SELECT id, postal_code FROM server_addresses"));
    ASSERT_TRUE(read_profiles.Step());
    EXPECT_FALSE(read_profiles.ColumnString(0).empty());
    EXPECT_EQ("90210", read_profiles.ColumnString(1));
  }
}

// Tests addition of credit card billing address.
TEST_F(WebDatabaseMigrationTest, MigrateVersion65ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_65.sql")));

  // Verify pre-conditions.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 65, 65));

    EXPECT_FALSE(connection.DoesColumnExist("credit_cards",
                                            "billing_address_id"));

    EXPECT_TRUE(connection.Execute(
        "INSERT INTO credit_cards(guid, name_on_card) VALUES ('', 'Alice')"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_TRUE(connection.DoesColumnExist("credit_cards",
                                           "billing_address_id"));

    sql::Statement read_credit_cards(connection.GetUniqueStatement(
        "SELECT name_on_card, billing_address_id FROM credit_cards"));
    ASSERT_TRUE(read_credit_cards.Step());
    EXPECT_EQ("Alice", read_credit_cards.ColumnString(0));
    EXPECT_TRUE(read_credit_cards.ColumnString(1).empty());
  }
}

// Tests addition of masked server credit card billing address.
TEST_F(WebDatabaseMigrationTest, MigrateVersion66ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_66.sql")));

  // Verify pre-conditions.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 66, 66));

    EXPECT_FALSE(connection.DoesColumnExist("masked_credit_cards",
                                            "billing_address_id"));

    EXPECT_TRUE(connection.Execute(
        "INSERT INTO masked_credit_cards(id, name_on_card) "
        "VALUES ('id', 'Alice')"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards",
                                           "billing_address_id"));

    sql::Statement read_masked(connection.GetUniqueStatement(
        "SELECT name_on_card, billing_address_id FROM masked_credit_cards"));
    ASSERT_TRUE(read_masked.Step());
    EXPECT_EQ("Alice", read_masked.ColumnString(0));
    EXPECT_TRUE(read_masked.ColumnString(1).empty());
  }
}
