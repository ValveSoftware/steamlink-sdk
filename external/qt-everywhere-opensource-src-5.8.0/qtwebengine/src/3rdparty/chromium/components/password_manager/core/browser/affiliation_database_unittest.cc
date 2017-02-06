// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation_database.h"

#include <stdint.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace password_manager {

namespace {

const char kTestFacetURI1[] = "https://alpha.example.com";
const char kTestFacetURI2[] = "https://beta.example.com";
const char kTestFacetURI3[] = "https://gamma.example.com";
const char kTestFacetURI4[] = "https://delta.example.com";
const char kTestFacetURI5[] = "https://epsilon.example.com";
const char kTestFacetURI6[] = "https://zeta.example.com";

const char kTestAndroidFacetURI[] = "android://hash@com.example.android";

const int64_t kTestTimeUs1 = 1000000;
const int64_t kTestTimeUs2 = 2000000;
const int64_t kTestTimeUs3 = 3000000;

void ExpectEquivalenceClassesAreEqual(
    const AffiliatedFacetsWithUpdateTime& expectation,
    const AffiliatedFacetsWithUpdateTime& reality) {
  EXPECT_EQ(expectation.last_update_time, reality.last_update_time);
  EXPECT_THAT(reality.facets,
              testing::UnorderedElementsAreArray(reality.facets));
}

AffiliatedFacetsWithUpdateTime TestEquivalenceClass1() {
  AffiliatedFacetsWithUpdateTime affiliation;
  affiliation.last_update_time = base::Time::FromInternalValue(kTestTimeUs1);
  affiliation.facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI1));
  affiliation.facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI2));
  affiliation.facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI3));
  return affiliation;
}

AffiliatedFacetsWithUpdateTime TestEquivalenceClass2() {
  AffiliatedFacetsWithUpdateTime affiliation;
  affiliation.last_update_time = base::Time::FromInternalValue(kTestTimeUs2);
  affiliation.facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI4));
  affiliation.facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI5));
  return affiliation;
}

AffiliatedFacetsWithUpdateTime TestEquivalenceClass3() {
  AffiliatedFacetsWithUpdateTime affiliation;
  affiliation.last_update_time = base::Time::FromInternalValue(kTestTimeUs3);
  affiliation.facets.push_back(
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURI));
  return affiliation;
}

}  // namespace

class AffiliationDatabaseTest : public testing::Test {
 public:
  AffiliationDatabaseTest() {}
  ~AffiliationDatabaseTest() override {}

  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    OpenDatabase();
  }

  void OpenDatabase() {
    db_.reset(new AffiliationDatabase);
    ASSERT_TRUE(db_->Init(db_path()));
  }

  void CloseDatabase() { db_.reset(); }

  void StoreInitialTestData() {
    ASSERT_TRUE(db_->Store(TestEquivalenceClass1()));
    ASSERT_TRUE(db_->Store(TestEquivalenceClass2()));
    ASSERT_TRUE(db_->Store(TestEquivalenceClass3()));
  }

  AffiliationDatabase& db() { return *db_; }

  base::FilePath db_path() {
    return temp_directory_.path().Append(
        FILE_PATH_LITERAL("Test Affiliation Database"));
  }

 private:
  base::ScopedTempDir temp_directory_;
  std::unique_ptr<AffiliationDatabase> db_;

  DISALLOW_COPY_AND_ASSIGN(AffiliationDatabaseTest);
};

TEST_F(AffiliationDatabaseTest, Store) {
  LOG(ERROR) << "During this test, SQL errors (number 19) will be logged to "
                "the console. This is expected.";

  ASSERT_NO_FATAL_FAILURE(StoreInitialTestData());

  // Verify that duplicate equivalence classes are not allowed to be stored.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CONSTRAINT);
    AffiliatedFacetsWithUpdateTime duplicate = TestEquivalenceClass1();
    EXPECT_FALSE(db().Store(duplicate));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }

  // Verify that intersecting equivalence classes are not allowed to be stored.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CONSTRAINT);
    AffiliatedFacetsWithUpdateTime intersecting;
    intersecting.facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI3));
    intersecting.facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI4));
    EXPECT_FALSE(db().Store(intersecting));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
}

TEST_F(AffiliationDatabaseTest, GetAllAffiliations) {
  std::vector<AffiliatedFacetsWithUpdateTime> affiliations;

  // Empty database should not return any equivalence classes.
  db().GetAllAffiliations(&affiliations);
  EXPECT_EQ(0u, affiliations.size());

  ASSERT_NO_FATAL_FAILURE(StoreInitialTestData());

  // The test data should be returned in order.
  db().GetAllAffiliations(&affiliations);
  ASSERT_EQ(3u, affiliations.size());
  ExpectEquivalenceClassesAreEqual(TestEquivalenceClass1(), affiliations[0]);
  ExpectEquivalenceClassesAreEqual(TestEquivalenceClass2(), affiliations[1]);
  ExpectEquivalenceClassesAreEqual(TestEquivalenceClass3(), affiliations[2]);
}

TEST_F(AffiliationDatabaseTest, GetAffiliationForFacet) {
  ASSERT_NO_FATAL_FAILURE(StoreInitialTestData());

  // Verify that querying any element in the first equivalence class yields that
  // class.
  for (const auto& facet_uri : TestEquivalenceClass1().facets) {
    AffiliatedFacetsWithUpdateTime affiliation;
    EXPECT_TRUE(db().GetAffiliationsForFacet(facet_uri, &affiliation));
    ExpectEquivalenceClassesAreEqual(TestEquivalenceClass1(), affiliation);
  }

  // Verify that querying the sole element in the third equivalence class yields
  // that class.
  {
    AffiliatedFacetsWithUpdateTime affiliation;
    EXPECT_TRUE(db().GetAffiliationsForFacet(
        FacetURI::FromCanonicalSpec(kTestAndroidFacetURI), &affiliation));
    ExpectEquivalenceClassesAreEqual(TestEquivalenceClass3(), affiliation);
  }

  // Verify that querying a facet not in the database yields no result.
  {
    AffiliatedFacetsWithUpdateTime affiliation;
    EXPECT_FALSE(db().GetAffiliationsForFacet(
        FacetURI::FromCanonicalSpec(kTestFacetURI6), &affiliation));
    ExpectEquivalenceClassesAreEqual(AffiliatedFacetsWithUpdateTime(),
                                     affiliation);
  }
}

TEST_F(AffiliationDatabaseTest, StoreAndRemoveConflicting) {
  ASSERT_NO_FATAL_FAILURE(StoreInitialTestData());

  AffiliatedFacetsWithUpdateTime updated = TestEquivalenceClass1();
  updated.last_update_time = base::Time::FromInternalValue(4000000);

  // Verify that duplicate equivalence classes are now allowed to be stored, and
  // the last update timestamp is updated.
  {
    std::vector<AffiliatedFacetsWithUpdateTime> removed;
    db().StoreAndRemoveConflicting(updated, &removed);
    EXPECT_EQ(0u, removed.size());

    AffiliatedFacetsWithUpdateTime affiliation;
    EXPECT_TRUE(db().GetAffiliationsForFacet(
        FacetURI::FromCanonicalSpec(kTestFacetURI1), &affiliation));
    ExpectEquivalenceClassesAreEqual(updated, affiliation);
  }

  // Verify that intersecting equivalence classes are now allowed to be stored,
  // the conflicting classes are removed, but unaffected classes are retained.
  {
    AffiliatedFacetsWithUpdateTime intersecting;
    std::vector<AffiliatedFacetsWithUpdateTime> removed;
    intersecting.last_update_time = base::Time::FromInternalValue(5000000);
    intersecting.facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI3));
    intersecting.facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI4));
    db().StoreAndRemoveConflicting(intersecting, &removed);

    ASSERT_EQ(2u, removed.size());
    ExpectEquivalenceClassesAreEqual(updated, removed[0]);
    ExpectEquivalenceClassesAreEqual(TestEquivalenceClass2(), removed[1]);

    std::vector<AffiliatedFacetsWithUpdateTime> affiliations;
    db().GetAllAffiliations(&affiliations);
    ASSERT_EQ(2u, affiliations.size());
    ExpectEquivalenceClassesAreEqual(TestEquivalenceClass3(), affiliations[0]);
    ExpectEquivalenceClassesAreEqual(intersecting, affiliations[1]);
  }
}

TEST_F(AffiliationDatabaseTest, DeleteAllAffiliations) {
  db().DeleteAllAffiliations();

  ASSERT_NO_FATAL_FAILURE(StoreInitialTestData());

  db().DeleteAllAffiliations();

  std::vector<AffiliatedFacetsWithUpdateTime> affiliations;
  db().GetAllAffiliations(&affiliations);
  ASSERT_EQ(0u, affiliations.size());
}

TEST_F(AffiliationDatabaseTest, DeleteAffiliationsOlderThan) {
  db().DeleteAffiliationsOlderThan(base::Time::FromInternalValue(0));

  ASSERT_NO_FATAL_FAILURE(StoreInitialTestData());

  db().DeleteAffiliationsOlderThan(base::Time::FromInternalValue(kTestTimeUs2));

  std::vector<AffiliatedFacetsWithUpdateTime> affiliations;
  db().GetAllAffiliations(&affiliations);
  ASSERT_EQ(2u, affiliations.size());
  ExpectEquivalenceClassesAreEqual(TestEquivalenceClass2(), affiliations[0]);
  ExpectEquivalenceClassesAreEqual(TestEquivalenceClass3(), affiliations[1]);

  db().DeleteAffiliationsOlderThan(base::Time::Max());

  db().GetAllAffiliations(&affiliations);
  ASSERT_EQ(0u, affiliations.size());
}

// Verify that an existing DB can be reopened, and data is retained.
TEST_F(AffiliationDatabaseTest, DBRetainsDataAfterReopening) {
  ASSERT_NO_FATAL_FAILURE(StoreInitialTestData());

  CloseDatabase();
  OpenDatabase();

  // The test data should be returned in order.
  std::vector<AffiliatedFacetsWithUpdateTime> affiliations;
  db().GetAllAffiliations(&affiliations);
  ASSERT_EQ(3u, affiliations.size());
  ExpectEquivalenceClassesAreEqual(TestEquivalenceClass1(), affiliations[0]);
  ExpectEquivalenceClassesAreEqual(TestEquivalenceClass2(), affiliations[1]);
  ExpectEquivalenceClassesAreEqual(TestEquivalenceClass3(), affiliations[2]);
}

// Verify that when it is discovered during opening that a DB is corrupt, it
// gets razed, and then an empty (but again usable) DB is produced.
TEST_F(AffiliationDatabaseTest, CorruptDBIsRazedThenOpened) {
  ASSERT_NO_FATAL_FAILURE(StoreInitialTestData());

  CloseDatabase();
  ASSERT_TRUE(sql::test::CorruptSizeInHeader(db_path()));
  ASSERT_NO_FATAL_FAILURE(OpenDatabase());

  std::vector<AffiliatedFacetsWithUpdateTime> affiliations;
  db().GetAllAffiliations(&affiliations);
  EXPECT_EQ(0u, affiliations.size());

  ASSERT_NO_FATAL_FAILURE(StoreInitialTestData());
  db().GetAllAffiliations(&affiliations);
  EXPECT_EQ(3u, affiliations.size());
}

// Verify that when the DB becomes corrupt after it has been opened, it gets
// poisoned so that operations fail silently without side effects.
TEST_F(AffiliationDatabaseTest, CorruptDBGetsPoisoned) {
  ASSERT_TRUE(db().Store(TestEquivalenceClass1()));

  ASSERT_TRUE(sql::test::CorruptSizeInHeader(db_path()));

  EXPECT_FALSE(db().Store(TestEquivalenceClass2()));
  std::vector<AffiliatedFacetsWithUpdateTime> affiliations;
  db().GetAllAffiliations(&affiliations);
  EXPECT_EQ(0u, affiliations.size());
}

// Verify that all files get deleted.
TEST_F(AffiliationDatabaseTest, Delete) {
  ASSERT_NO_FATAL_FAILURE(StoreInitialTestData());
  CloseDatabase();

  AffiliationDatabase::Delete(db_path());
  EXPECT_TRUE(base::IsDirectoryEmpty(db_path().DirName()));
}

}  // namespace password_manager
