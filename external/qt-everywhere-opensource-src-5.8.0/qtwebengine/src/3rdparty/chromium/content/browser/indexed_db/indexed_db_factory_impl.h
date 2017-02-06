// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_IMPL_H_

#include <stddef.h>

#include <map>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "content/browser/indexed_db/indexed_db_factory.h"

namespace url {
class Origin;
}

namespace content {

class IndexedDBContextImpl;

class CONTENT_EXPORT IndexedDBFactoryImpl : public IndexedDBFactory {
 public:
  explicit IndexedDBFactoryImpl(IndexedDBContextImpl* context);

  // content::IndexedDBFactory overrides:
  void ReleaseDatabase(const IndexedDBDatabase::Identifier& identifier,
                       bool forced_close) override;

  void GetDatabaseNames(scoped_refptr<IndexedDBCallbacks> callbacks,
                        const url::Origin& origin,
                        const base::FilePath& data_directory,
                        net::URLRequestContext* request_context) override;
  void Open(const base::string16& name,
            const IndexedDBPendingConnection& connection,
            net::URLRequestContext* request_context,
            const url::Origin& origin,
            const base::FilePath& data_directory) override;

  void DeleteDatabase(const base::string16& name,
                      net::URLRequestContext* request_context,
                      scoped_refptr<IndexedDBCallbacks> callbacks,
                      const url::Origin& origin,
                      const base::FilePath& data_directory) override;

  void HandleBackingStoreFailure(const url::Origin& origin) override;
  void HandleBackingStoreCorruption(
      const url::Origin& origin,
      const IndexedDBDatabaseError& error) override;

  OriginDBs GetOpenDatabasesForOrigin(const url::Origin& origin) const override;

  void ForceClose(const url::Origin& origin) override;

  // Called by the IndexedDBContext destructor so the factory can do cleanup.
  void ContextDestroyed() override;

  // Called by the IndexedDBActiveBlobRegistry.
  void ReportOutstandingBlobs(const url::Origin& origin,
                              bool blobs_outstanding) override;

  // Called by an IndexedDBDatabase when it is actually deleted.
  void DatabaseDeleted(
      const IndexedDBDatabase::Identifier& identifier) override;

  size_t GetConnectionCount(const url::Origin& origin) const override;

 protected:
  ~IndexedDBFactoryImpl() override;

  scoped_refptr<IndexedDBBackingStore> OpenBackingStore(
      const url::Origin& origin,
      const base::FilePath& data_directory,
      net::URLRequestContext* request_context,
      blink::WebIDBDataLoss* data_loss,
      std::string* data_loss_reason,
      bool* disk_full,
      leveldb::Status* s) override;

  scoped_refptr<IndexedDBBackingStore> OpenBackingStoreHelper(
      const url::Origin& origin,
      const base::FilePath& data_directory,
      net::URLRequestContext* request_context,
      blink::WebIDBDataLoss* data_loss,
      std::string* data_loss_message,
      bool* disk_full,
      bool first_time,
      leveldb::Status* s) override;

  void ReleaseBackingStore(const url::Origin& origin, bool immediate);
  void CloseBackingStore(const url::Origin& origin);
  IndexedDBContextImpl* context() const { return context_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           BackingStoreReleasedOnForcedClose);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           BackingStoreReleaseDelayedOnClose);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest, DatabaseFailedOpen);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           DeleteDatabaseClosesBackingStore);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           ForceCloseReleasesBackingStore);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           GetDatabaseNamesClosesBackingStore);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest,
                           ForceCloseOpenDatabasesOnCommitFailure);

  typedef std::map<IndexedDBDatabase::Identifier, IndexedDBDatabase*>
      IndexedDBDatabaseMap;
  typedef std::map<url::Origin, scoped_refptr<IndexedDBBackingStore>>
      IndexedDBBackingStoreMap;

  // Called internally after a database is closed, with some delay. If this
  // factory has the last reference, it will be released.
  void MaybeCloseBackingStore(const url::Origin& origin);
  bool HasLastBackingStoreReference(const url::Origin& origin) const;

  // Testing helpers, so unit tests don't need to grovel through internal state.
  bool IsDatabaseOpen(const url::Origin& origin,
                      const base::string16& name) const;
  bool IsBackingStoreOpen(const url::Origin& origin) const;
  bool IsBackingStorePendingClose(const url::Origin& origin) const;
  void RemoveDatabaseFromMaps(const IndexedDBDatabase::Identifier& identifier);

  IndexedDBContextImpl* context_;

  IndexedDBDatabaseMap database_map_;
  OriginDBMap origin_dbs_;
  IndexedDBBackingStoreMap backing_store_map_;

  std::set<scoped_refptr<IndexedDBBackingStore> > session_only_backing_stores_;
  IndexedDBBackingStoreMap backing_stores_with_active_blobs_;
  std::set<url::Origin> backends_opened_since_boot_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBFactoryImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_IMPL_H_
