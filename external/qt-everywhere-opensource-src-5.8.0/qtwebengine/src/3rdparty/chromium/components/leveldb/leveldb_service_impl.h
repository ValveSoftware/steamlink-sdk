// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_LEVELDB_SERVICE_IMPL_H_
#define COMPONENTS_LEVELDB_LEVELDB_SERVICE_IMPL_H_

#include "base/memory/ref_counted.h"
#include "components/leveldb/leveldb_mojo_proxy.h"
#include "components/leveldb/public/interfaces/leveldb.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace leveldb {

// Creates LevelDBDatabases based scoped to a |directory|/|dbname|.
class LevelDBServiceImpl : public mojom::LevelDBService {
 public:
  LevelDBServiceImpl(scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~LevelDBServiceImpl() override;

  // Overridden from LevelDBService:
  void Open(filesystem::mojom::DirectoryPtr directory,
            const mojo::String& dbname,
            leveldb::mojom::LevelDBDatabaseRequest database,
            const OpenCallback& callback) override;
  void OpenWithOptions(leveldb::mojom::OpenOptionsPtr open_options,
                       filesystem::mojom::DirectoryPtr directory,
                       const mojo::String& dbname,
                       leveldb::mojom::LevelDBDatabaseRequest database,
                       const OpenCallback& callback) override;
  void OpenInMemory(leveldb::mojom::LevelDBDatabaseRequest database,
                    const OpenInMemoryCallback& callback) override;

 private:
  // Thread to own the mojo message pipe. Because leveldb spawns multiple
  // threads that want to call file stuff, we create a dedicated thread to send
  // and receive mojo message calls.
  scoped_refptr<LevelDBMojoProxy> thread_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBServiceImpl);
};

}  // namespace leveldb

#endif  // COMPONENTS_LEVELDB_LEVELDB_SERVICE_IMPL_H_
