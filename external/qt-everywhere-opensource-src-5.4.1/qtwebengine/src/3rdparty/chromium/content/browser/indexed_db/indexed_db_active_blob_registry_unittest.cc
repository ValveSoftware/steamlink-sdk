// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/test/test_simple_task_runner.h"
#include "content/browser/indexed_db/indexed_db_active_blob_registry.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_fake_backing_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class MockIDBFactory : public IndexedDBFactory {
 public:
  MockIDBFactory() : IndexedDBFactory(NULL), duplicate_calls_(false) {}

  virtual void ReportOutstandingBlobs(const GURL& origin_url,
                                      bool blobs_outstanding) OVERRIDE {
    if (blobs_outstanding) {
      if (origins_.count(origin_url)) {
        duplicate_calls_ = true;
      } else {
        origins_.insert(origin_url);
      }
    } else {
      if (!origins_.count(origin_url)) {
        duplicate_calls_ = true;
      } else {
        origins_.erase(origin_url);
      }
    }
  }

  bool CheckNoOriginsInUse() const {
    return !duplicate_calls_ && !origins_.size();
  }
  bool CheckSingleOriginInUse(const GURL& origin) const {
    return !duplicate_calls_ && origins_.size() == 1 && origins_.count(origin);
  }

 protected:
  virtual ~MockIDBFactory() {}

 private:
  std::set<GURL> origins_;
  bool duplicate_calls_;

  DISALLOW_COPY_AND_ASSIGN(MockIDBFactory);
};

class MockIDBBackingStore : public IndexedDBFakeBackingStore {
 public:
  MockIDBBackingStore(IndexedDBFactory* factory, base::TaskRunner* task_runner)
      : IndexedDBFakeBackingStore(factory, task_runner),
        duplicate_calls_(false) {}

  virtual void ReportBlobUnused(int64 database_id, int64 blob_key) OVERRIDE {
    unused_blobs_.insert(std::make_pair(database_id, blob_key));
  }

  typedef std::pair<int64, int64> KeyPair;
  typedef std::set<KeyPair> KeyPairSet;
  bool CheckUnusedBlobsEmpty() const {
    return !duplicate_calls_ && !unused_blobs_.size();
  }
  bool CheckSingleUnusedBlob(int64 database_id, int64 blob_key) const {
    return !duplicate_calls_ && unused_blobs_.size() == 1 &&
           unused_blobs_.count(std::make_pair(database_id, blob_key));
  }

  const KeyPairSet& unused_blobs() const { return unused_blobs_; }

 protected:
  virtual ~MockIDBBackingStore() {}

 private:
  KeyPairSet unused_blobs_;
  bool duplicate_calls_;

  DISALLOW_COPY_AND_ASSIGN(MockIDBBackingStore);
};

// Base class for our test fixtures.
class IndexedDBActiveBlobRegistryTest : public testing::Test {
 public:
  IndexedDBActiveBlobRegistryTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        factory_(new MockIDBFactory),
        backing_store_(new MockIDBBackingStore(factory_, task_runner_)),
        registry_(new IndexedDBActiveBlobRegistry(backing_store_.get())) {}

  void RunUntilIdle() { task_runner_->RunUntilIdle(); }
  MockIDBFactory* factory() const { return factory_.get(); }
  MockIDBBackingStore* backing_store() const { return backing_store_.get(); }
  IndexedDBActiveBlobRegistry* registry() const { return registry_.get(); }

  static const int64 kDatabaseId0 = 7;
  static const int64 kDatabaseId1 = 12;
  static const int64 kBlobKey0 = 77;
  static const int64 kBlobKey1 = 14;

  typedef webkit_blob::ShareableFileReference::FinalReleaseCallback
      ReleaseCallback;

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_refptr<MockIDBFactory> factory_;
  scoped_refptr<MockIDBBackingStore> backing_store_;
  scoped_ptr<IndexedDBActiveBlobRegistry> registry_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBActiveBlobRegistryTest);
};

TEST_F(IndexedDBActiveBlobRegistryTest, DeleteUnused) {
  EXPECT_TRUE(factory()->CheckNoOriginsInUse());
  EXPECT_TRUE(backing_store()->CheckUnusedBlobsEmpty());

  EXPECT_FALSE(registry()->MarkDeletedCheckIfUsed(kDatabaseId0, kBlobKey0));
  RunUntilIdle();

  EXPECT_TRUE(factory()->CheckNoOriginsInUse());
  EXPECT_TRUE(backing_store()->CheckUnusedBlobsEmpty());
}

TEST_F(IndexedDBActiveBlobRegistryTest, SimpleUse) {
  EXPECT_TRUE(factory()->CheckNoOriginsInUse());
  EXPECT_TRUE(backing_store()->CheckUnusedBlobsEmpty());

  base::Closure add_ref =
      registry()->GetAddBlobRefCallback(kDatabaseId0, kBlobKey0);
  ReleaseCallback release =
      registry()->GetFinalReleaseCallback(kDatabaseId0, kBlobKey0);
  add_ref.Run();
  RunUntilIdle();

  EXPECT_TRUE(factory()->CheckSingleOriginInUse(backing_store()->origin_url()));
  EXPECT_TRUE(backing_store()->CheckUnusedBlobsEmpty());

  release.Run(base::FilePath());
  RunUntilIdle();

  EXPECT_TRUE(factory()->CheckNoOriginsInUse());
  EXPECT_TRUE(backing_store()->CheckUnusedBlobsEmpty());
}

TEST_F(IndexedDBActiveBlobRegistryTest, DeleteWhileInUse) {
  EXPECT_TRUE(factory()->CheckNoOriginsInUse());
  EXPECT_TRUE(backing_store()->CheckUnusedBlobsEmpty());

  base::Closure add_ref =
      registry()->GetAddBlobRefCallback(kDatabaseId0, kBlobKey0);
  ReleaseCallback release =
      registry()->GetFinalReleaseCallback(kDatabaseId0, kBlobKey0);

  add_ref.Run();
  RunUntilIdle();

  EXPECT_TRUE(factory()->CheckSingleOriginInUse(backing_store()->origin_url()));
  EXPECT_TRUE(backing_store()->CheckUnusedBlobsEmpty());

  EXPECT_TRUE(registry()->MarkDeletedCheckIfUsed(kDatabaseId0, kBlobKey0));
  RunUntilIdle();

  EXPECT_TRUE(factory()->CheckSingleOriginInUse(backing_store()->origin_url()));
  EXPECT_TRUE(backing_store()->CheckUnusedBlobsEmpty());

  release.Run(base::FilePath());
  RunUntilIdle();

  EXPECT_TRUE(factory()->CheckNoOriginsInUse());
  EXPECT_TRUE(backing_store()->CheckSingleUnusedBlob(kDatabaseId0, kBlobKey0));
}

TEST_F(IndexedDBActiveBlobRegistryTest, MultipleBlobs) {
  EXPECT_TRUE(factory()->CheckNoOriginsInUse());
  EXPECT_TRUE(backing_store()->CheckUnusedBlobsEmpty());

  base::Closure add_ref_00 =
      registry()->GetAddBlobRefCallback(kDatabaseId0, kBlobKey0);
  ReleaseCallback release_00 =
      registry()->GetFinalReleaseCallback(kDatabaseId0, kBlobKey0);
  base::Closure add_ref_01 =
      registry()->GetAddBlobRefCallback(kDatabaseId0, kBlobKey1);
  ReleaseCallback release_01 =
      registry()->GetFinalReleaseCallback(kDatabaseId0, kBlobKey1);
  base::Closure add_ref_10 =
      registry()->GetAddBlobRefCallback(kDatabaseId1, kBlobKey0);
  ReleaseCallback release_10 =
      registry()->GetFinalReleaseCallback(kDatabaseId1, kBlobKey0);
  base::Closure add_ref_11 =
      registry()->GetAddBlobRefCallback(kDatabaseId1, kBlobKey1);
  ReleaseCallback release_11 =
      registry()->GetFinalReleaseCallback(kDatabaseId1, kBlobKey1);

  add_ref_00.Run();
  add_ref_01.Run();
  RunUntilIdle();

  EXPECT_TRUE(factory()->CheckSingleOriginInUse(backing_store()->origin_url()));
  EXPECT_TRUE(backing_store()->CheckUnusedBlobsEmpty());

  release_00.Run(base::FilePath());
  add_ref_10.Run();
  add_ref_11.Run();
  RunUntilIdle();

  EXPECT_TRUE(factory()->CheckSingleOriginInUse(backing_store()->origin_url()));
  EXPECT_TRUE(backing_store()->CheckUnusedBlobsEmpty());

  EXPECT_TRUE(registry()->MarkDeletedCheckIfUsed(kDatabaseId0, kBlobKey1));
  RunUntilIdle();

  EXPECT_TRUE(factory()->CheckSingleOriginInUse(backing_store()->origin_url()));
  EXPECT_TRUE(backing_store()->CheckUnusedBlobsEmpty());

  release_01.Run(base::FilePath());
  release_11.Run(base::FilePath());
  RunUntilIdle();

  EXPECT_TRUE(factory()->CheckSingleOriginInUse(backing_store()->origin_url()));
  EXPECT_TRUE(backing_store()->CheckSingleUnusedBlob(kDatabaseId0, kBlobKey1));

  release_10.Run(base::FilePath());
  RunUntilIdle();

  EXPECT_TRUE(factory()->CheckNoOriginsInUse());
  EXPECT_TRUE(backing_store()->CheckSingleUnusedBlob(kDatabaseId0, kBlobKey1));
}

TEST_F(IndexedDBActiveBlobRegistryTest, ForceShutdown) {
  EXPECT_TRUE(factory()->CheckNoOriginsInUse());
  EXPECT_TRUE(backing_store()->CheckUnusedBlobsEmpty());

  base::Closure add_ref_0 =
      registry()->GetAddBlobRefCallback(kDatabaseId0, kBlobKey0);
  ReleaseCallback release_0 =
      registry()->GetFinalReleaseCallback(kDatabaseId0, kBlobKey0);
  base::Closure add_ref_1 =
      registry()->GetAddBlobRefCallback(kDatabaseId0, kBlobKey1);
  ReleaseCallback release_1 =
      registry()->GetFinalReleaseCallback(kDatabaseId0, kBlobKey1);

  add_ref_0.Run();
  RunUntilIdle();

  EXPECT_TRUE(factory()->CheckSingleOriginInUse(backing_store()->origin_url()));
  EXPECT_TRUE(backing_store()->CheckUnusedBlobsEmpty());

  registry()->ForceShutdown();

  add_ref_1.Run();
  RunUntilIdle();

  // Nothing changes.
  EXPECT_TRUE(factory()->CheckSingleOriginInUse(backing_store()->origin_url()));
  EXPECT_TRUE(backing_store()->CheckUnusedBlobsEmpty());

  release_0.Run(base::FilePath());
  release_1.Run(base::FilePath());
  RunUntilIdle();

  // Nothing changes.
  EXPECT_TRUE(factory()->CheckSingleOriginInUse(backing_store()->origin_url()));
  EXPECT_TRUE(backing_store()->CheckUnusedBlobsEmpty());
}

}  // namespace

}  // namespace content
