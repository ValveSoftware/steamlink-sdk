// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/leveldb_database.h"

#include <inttypes.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
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

LevelDB::LevelDB(const char* client_name)
    : open_histogram_(nullptr), client_name_(client_name) {
  // Used in lieu of UMA_HISTOGRAM_ENUMERATION because the histogram name is
  // not a constant.
  open_histogram_ = base::LinearHistogram::FactoryGet(
      std::string("LevelDB.Open.") + client_name_, 1,
      leveldb_env::LEVELDB_STATUS_MAX, leveldb_env::LEVELDB_STATUS_MAX + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
}

LevelDB::~LevelDB() {
  DFAKE_SCOPED_LOCK(thread_checker_);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
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

    base::trace_event::MemoryDumpManager::GetInstance()
        ->RegisterDumpProviderWithSequencedTaskRunner(
            this, "LevelDB", base::SequencedTaskRunnerHandle::Get(),
            base::trace_event::MemoryDumpProvider::Options());
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

bool LevelDB::LoadKeys(std::vector<std::string>* keys) {
  DFAKE_SCOPED_LOCK(thread_checker_);
  if (!db_)
    return false;

  leveldb::ReadOptions options;
  options.fill_cache = false;
  std::unique_ptr<leveldb::Iterator> db_iterator(db_->NewIterator(options));
  for (db_iterator->SeekToFirst(); db_iterator->Valid(); db_iterator->Next()) {
    leveldb::Slice key_slice = db_iterator->key();
    keys->push_back(std::string(key_slice.data(), key_slice.size()));
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

bool LevelDB::OnMemoryDump(const base::trace_event::MemoryDumpArgs& dump_args,
                           base::trace_event::ProcessMemoryDump* pmd) {
  DFAKE_SCOPED_LOCK(thread_checker_);
  if (!db_)
    return false;

  std::string value;
  uint64_t size;
  bool res = db_->GetProperty("leveldb.approximate-memory-usage", &value);
  DCHECK(res);
  res = base::StringToUint64(value, &size);
  DCHECK(res);

  std::string dump_name = "leveldb/leveldb_proto";
  if (!client_name_.empty())
    dump_name += "/" + client_name_;
  base::trace_event::MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(
      base::StringPrintf("%s/0x%" PRIXPTR, dump_name.c_str(),
                         reinterpret_cast<uintptr_t>(db_.get())));
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes, size);

  // Memory is allocated from system allocator (malloc).
  const char* system_allocator_pool_name =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->system_allocator_pool_name();
  if (system_allocator_pool_name)
    pmd->AddSuballocation(dump->guid(), system_allocator_pool_name);

  return true;
}

}  // namespace leveldb_proto
