// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/leveldb_database.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_checker.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace leveldb_proto {

// static
bool LevelDB::Destroy(const base::FilePath& database_dir) {
  const leveldb::Status s =
      leveldb::DestroyDB(database_dir.AsUTF8Unsafe(), leveldb::Options());
  return s.ok();
}

LevelDB::LevelDB(const char* client_name) : open_histogram_(nullptr) {
  // Used in lieu of UMA_HISTOGRAM_ENUMERATION because the histogram name is
  // not a constant.
  open_histogram_ = base::LinearHistogram::FactoryGet(
      std::string("LevelDB.Open.") + client_name, 1,
      leveldb_env::LEVELDB_STATUS_MAX, leveldb_env::LEVELDB_STATUS_MAX + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
}

LevelDB::~LevelDB() {
  DFAKE_SCOPED_LOCK(thread_checker_);
}

bool LevelDB::InitWithOptions(const base::FilePath& database_dir,
                              const leveldb::Options& options) {
  DFAKE_SCOPED_LOCK(thread_checker_);

  std::string path = database_dir.AsUTF8Unsafe();

  leveldb::DB* db = NULL;
  leveldb::Status status = leveldb::DB::Open(options, path, &db);
  if (open_histogram_)
    open_histogram_->Add(leveldb_env::GetLevelDBStatusUMAValue(status));
  if (status.IsCorruption()) {
    base::DeleteFile(database_dir, true);
    status = leveldb::DB::Open(options, path, &db);
  }

  if (status.ok()) {
    CHECK(db);
    db_.reset(db);
    return true;
  }

  LOG(WARNING) << "Unable to open " << database_dir.value() << ": "
               << status.ToString();
  return false;
}

bool LevelDB::Init(const base::FilePath& database_dir) {
  leveldb::Options options;
  options.create_if_missing = true;
  options.max_open_files = 0;  // Use minimum.
  options.reuse_logs = leveldb_env::kDefaultLogReuseOptionValue;
  if (database_dir.empty()) {
    env_.reset(leveldb::NewMemEnv(leveldb::Env::Default()));
    options.env = env_.get();
  }

  return InitWithOptions(database_dir, options);
}

bool LevelDB::Save(const base::StringPairs& entries_to_save,
                   const std::vector<std::string>& keys_to_remove) {
  DFAKE_SCOPED_LOCK(thread_checker_);
  if (!db_)
    return false;

  leveldb::WriteBatch updates;
  for (const auto& pair : entries_to_save)
    updates.Put(leveldb::Slice(pair.first), leveldb::Slice(pair.second));

  for (const auto& key : keys_to_remove)
    updates.Delete(leveldb::Slice(key));

  leveldb::WriteOptions options;
  options.sync = true;

  leveldb::Status status = db_->Write(options, &updates);
  if (status.ok())
    return true;

  DLOG(WARNING) << "Failed writing leveldb_proto entries: "
                << status.ToString();
  return false;
}

bool LevelDB::Load(std::vector<std::string>* entries) {
  DFAKE_SCOPED_LOCK(thread_checker_);
  if (!db_)
    return false;

  leveldb::ReadOptions options;
  std::unique_ptr<leveldb::Iterator> db_iterator(db_->NewIterator(options));
  for (db_iterator->SeekToFirst(); db_iterator->Valid(); db_iterator->Next()) {
    leveldb::Slice value_slice = db_iterator->value();
    std::string entry(value_slice.data(), value_slice.size());
    entries->push_back(entry);
  }
  return true;
}

bool LevelDB::Get(const std::string& key, bool* found, std::string* entry) {
  DFAKE_SCOPED_LOCK(thread_checker_);
  if (!db_)
    return false;

  leveldb::ReadOptions options;
  leveldb::Status status = db_->Get(options, key, entry);
  if (status.ok()) {
    *found = true;
    return true;
  }
  if (status.IsNotFound()) {
    *found = false;
    return true;
  }

  DLOG(WARNING) << "Failed loading leveldb_proto entry with key \"" << key
                << "\": " << status.ToString();
  return false;
}

}  // namespace leveldb_proto
