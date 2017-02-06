// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_DATABASE_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_DATABASE_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"
#include "components/safe_browsing_db/v4_store.h"

namespace safe_browsing {

class V4Database;

typedef base::Callback<void(std::unique_ptr<V4Database>)>
    NewDatabaseReadyCallback;

// This callback is scheduled once the database has finished processing the
// update requests for all stores and is ready to process the next set of update
// requests.
typedef base::Callback<void()> DatabaseUpdatedCallback;

// This hash_map maps the UpdateListIdentifiers to their corresponding in-memory
// stores, which contain the hash prefixes for that UpdateListIdentifier as well
// as manage their storage on disk.
typedef base::hash_map<UpdateListIdentifier, std::unique_ptr<V4Store>> StoreMap;

// Factory for creating V4Database. Tests implement this factory to create fake
// databases for testing.
class V4DatabaseFactory {
 public:
  virtual ~V4DatabaseFactory() {}
  virtual V4Database* CreateV4Database(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      const base::FilePath& base_dir_path,
      const StoreFileNameMap& store_file_name_map) = 0;
};

// The on-disk databases are shared among all profiles, as it doesn't contain
// user-specific data. This object is not thread-safe, i.e. all its methods
// should be used on the same thread that it was created on, unless specified
// otherwise.
// The hash-prefixes of each type are managed by a V4Store (including saving to
// and reading from disk).
// The V4Database serves as a single place to manage all the V4Stores.
class V4Database {
 public:
  // Factory method to create a V4Database. It creates the database on the
  // provided |db_task_runner| containing stores in |store_file_name_map|. When
  // the database creation is complete, it runs the NewDatabaseReadyCallback on
  // the same thread as it was called.
  static void Create(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      const base::FilePath& base_path,
      const StoreFileNameMap& store_file_name_map,
      NewDatabaseReadyCallback callback);

  // Destroys the provided v4_database on its task_runner since this may be a
  // long operation.
  static void Destroy(std::unique_ptr<V4Database> v4_database);

  virtual ~V4Database();

  // Updates the stores with the response received from the SafeBrowsing service
  // and calls the db_updated_callback when done.
  void ApplyUpdate(std::unique_ptr<ParsedServerResponse> parsed_server_response,
                   DatabaseUpdatedCallback db_updated_callback);

  // Returns the current state of each of the stores being managed.
  std::unique_ptr<StoreStateMap> GetStoreStateMap();

  // Deletes the current database and creates a new one.
  virtual bool ResetDatabase();

 protected:
  V4Database(const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
             std::unique_ptr<StoreMap> store_map);

 private:
  friend class V4DatabaseTest;
  FRIEND_TEST_ALL_PREFIXES(V4DatabaseTest, TestSetupDatabaseWithFakeStores);
  FRIEND_TEST_ALL_PREFIXES(V4DatabaseTest,
                           TestSetupDatabaseWithFakeStoresFailsReset);
  FRIEND_TEST_ALL_PREFIXES(V4DatabaseTest, TestApplyUpdateWithNewStates);
  FRIEND_TEST_ALL_PREFIXES(V4DatabaseTest, TestApplyUpdateWithNoNewState);
  FRIEND_TEST_ALL_PREFIXES(V4DatabaseTest, TestApplyUpdateWithEmptyUpdate);

  // Makes the passed |factory| the factory used to instantiate a V4Store. Only
  // for tests.
  static void RegisterStoreFactoryForTest(V4StoreFactory* factory) {
    factory_ = factory;
  }

  // Factory method to create a V4Database. When the database creation is
  // complete, it calls the NewDatabaseReadyCallback on |callback_task_runner|.
  static void CreateOnTaskRunner(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      const base::FilePath& base_path,
      const StoreFileNameMap& store_file_name_map,
      const scoped_refptr<base::SingleThreadTaskRunner>& callback_task_runner,
      NewDatabaseReadyCallback callback);

  // Callback called when a new store has been created and is ready to be used.
  // This method updates the store_map_ to point to the new store, which causes
  // the old store to get deleted.
  void UpdatedStoreReady(UpdateListIdentifier identifier,
                         std::unique_ptr<V4Store> store);

  const scoped_refptr<base::SequencedTaskRunner> db_task_runner_;

  // Map of UpdateListIdentifier to the V4Store.
  const std::unique_ptr<StoreMap> store_map_;

  DatabaseUpdatedCallback db_updated_callback_;

  // The factory that controls the creation of V4Store objects.
  static V4StoreFactory* factory_;

  // The number of stores for which the update request is pending. When this
  // goes down to 0, that indicates that the database has updated all the stores
  // that needed updating and is ready for the next update. It should only be
  // accessed on the IO thread.
  int pending_store_updates_;

  DISALLOW_COPY_AND_ASSIGN(V4Database);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_DATABASE_H_
