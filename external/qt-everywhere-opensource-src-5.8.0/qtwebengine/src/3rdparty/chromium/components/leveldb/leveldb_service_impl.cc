// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb/leveldb_service_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "components/leveldb/env_mojo.h"
#include "components/leveldb/leveldb_database_impl.h"
#include "components/leveldb/public/cpp/util.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/filter_policy.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

namespace leveldb {

LevelDBServiceImpl::LevelDBServiceImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : thread_(new LevelDBMojoProxy(std::move(task_runner))) {}

LevelDBServiceImpl::~LevelDBServiceImpl() {}

void LevelDBServiceImpl::Open(filesystem::mojom::DirectoryPtr directory,
                              const mojo::String& dbname,
                              leveldb::mojom::LevelDBDatabaseRequest database,
                              const OpenCallback& callback) {
  OpenWithOptions(leveldb::mojom::OpenOptions::New(), std::move(directory),
                  dbname, std::move(database), callback);
}

void LevelDBServiceImpl::OpenWithOptions(
    leveldb::mojom::OpenOptionsPtr open_options,
    filesystem::mojom::DirectoryPtr directory,
    const mojo::String& dbname,
    leveldb::mojom::LevelDBDatabaseRequest database,
    const OpenCallback& callback) {
  leveldb::Options options;
  options.create_if_missing = open_options->create_if_missing;
  options.error_if_exists = open_options->error_if_exists;
  options.paranoid_checks = open_options->paranoid_checks;
  options.write_buffer_size = open_options->write_buffer_size;
  options.max_open_files = open_options->max_open_files;

  options.reuse_logs = leveldb_env::kDefaultLogReuseOptionValue;
  options.compression = leveldb::kSnappyCompression;

  // Register our directory with the file thread.
  LevelDBMojoProxy::OpaqueDir* dir =
      thread_->RegisterDirectory(std::move(directory));

  std::unique_ptr<MojoEnv> env_mojo(new MojoEnv(thread_, dir));
  options.env = env_mojo.get();

  leveldb::DB* db = nullptr;
  leveldb::Status s = leveldb::DB::Open(options, dbname.To<std::string>(), &db);

  if (s.ok()) {
    new LevelDBDatabaseImpl(std::move(database), std::move(env_mojo),
                            base::WrapUnique(db));
  }

  callback.Run(LeveldbStatusToError(s));
}

void LevelDBServiceImpl::OpenInMemory(
    leveldb::mojom::LevelDBDatabaseRequest database,
    const OpenCallback& callback) {
  leveldb::Options options;
  options.create_if_missing = true;
  options.max_open_files = 0;  // Use minimum.

  std::unique_ptr<leveldb::Env> env(
      leveldb::NewMemEnv(leveldb::Env::Default()));
  options.env = env.get();

  leveldb::DB* db = nullptr;
  leveldb::Status s = leveldb::DB::Open(options, "", &db);

  if (s.ok()) {
    new LevelDBDatabaseImpl(std::move(database), std::move(env),
                            base::WrapUnique(db));
  }

  callback.Run(LeveldbStatusToError(s));
}

}  // namespace leveldb
