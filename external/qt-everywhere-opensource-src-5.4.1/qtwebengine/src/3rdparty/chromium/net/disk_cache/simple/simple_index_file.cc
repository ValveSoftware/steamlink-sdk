// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_index_file.h"

#include <vector>

#include "base/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "net/disk_cache/simple/simple_backend_version.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_histogram_macros.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_synchronous_entry.h"
#include "net/disk_cache/simple/simple_util.h"
#include "third_party/zlib/zlib.h"

namespace disk_cache {
namespace {

const int kEntryFilesHashLength = 16;
const int kEntryFilesSuffixLength = 2;

const uint64 kMaxEntiresInIndex = 100000000;

uint32 CalculatePickleCRC(const Pickle& pickle) {
  return crc32(crc32(0, Z_NULL, 0),
               reinterpret_cast<const Bytef*>(pickle.payload()),
               pickle.payload_size());
}

// Used in histograms. Please only add new values at the end.
enum IndexFileState {
  INDEX_STATE_CORRUPT = 0,
  INDEX_STATE_STALE = 1,
  INDEX_STATE_FRESH = 2,
  INDEX_STATE_FRESH_CONCURRENT_UPDATES = 3,
  INDEX_STATE_MAX = 4,
};

void UmaRecordIndexFileState(IndexFileState state, net::CacheType cache_type) {
  SIMPLE_CACHE_UMA(ENUMERATION,
                   "IndexFileStateOnLoad", cache_type, state, INDEX_STATE_MAX);
}

// Used in histograms. Please only add new values at the end.
enum IndexInitMethod {
  INITIALIZE_METHOD_RECOVERED = 0,
  INITIALIZE_METHOD_LOADED = 1,
  INITIALIZE_METHOD_NEWCACHE = 2,
  INITIALIZE_METHOD_MAX = 3,
};

void UmaRecordIndexInitMethod(IndexInitMethod method,
                              net::CacheType cache_type) {
  SIMPLE_CACHE_UMA(ENUMERATION,
                   "IndexInitializeMethod", cache_type,
                   method, INITIALIZE_METHOD_MAX);
}

bool WritePickleFile(Pickle* pickle, const base::FilePath& file_name) {
  int bytes_written = base::WriteFile(
      file_name, static_cast<const char*>(pickle->data()), pickle->size());
  if (bytes_written != implicit_cast<int>(pickle->size())) {
    base::DeleteFile(file_name, /* recursive = */ false);
    return false;
  }
  return true;
}

// Called for each cache directory traversal iteration.
void ProcessEntryFile(SimpleIndex::EntrySet* entries,
                      const base::FilePath& file_path) {
  static const size_t kEntryFilesLength =
      kEntryFilesHashLength + kEntryFilesSuffixLength;
  // Converting to std::string is OK since we never use UTF8 wide chars in our
  // file names.
  const base::FilePath::StringType base_name = file_path.BaseName().value();
  const std::string file_name(base_name.begin(), base_name.end());
  if (file_name.size() != kEntryFilesLength)
    return;
  const base::StringPiece hash_string(
      file_name.begin(), file_name.begin() + kEntryFilesHashLength);
  uint64 hash_key = 0;
  if (!simple_util::GetEntryHashKeyFromHexString(hash_string, &hash_key)) {
    LOG(WARNING) << "Invalid entry hash key filename while restoring index from"
                 << " disk: " << file_name;
    return;
  }

  base::File::Info file_info;
  if (!base::GetFileInfo(file_path, &file_info)) {
    LOG(ERROR) << "Could not get file info for " << file_path.value();
    return;
  }
  base::Time last_used_time;
#if defined(OS_POSIX)
  // For POSIX systems, a last access time is available. However, it's not
  // guaranteed to be more accurate than mtime. It is no worse though.
  last_used_time = file_info.last_accessed;
#endif
  if (last_used_time.is_null())
    last_used_time = file_info.last_modified;

  int64 file_size = file_info.size;
  SimpleIndex::EntrySet::iterator it = entries->find(hash_key);
  if (it == entries->end()) {
    SimpleIndex::InsertInEntrySet(
        hash_key,
        EntryMetadata(last_used_time, file_size),
        entries);
  } else {
    // Summing up the total size of the entry through all the *_[0-1] files
    it->second.SetEntrySize(it->second.GetEntrySize() + file_size);
  }
}

}  // namespace

SimpleIndexLoadResult::SimpleIndexLoadResult() : did_load(false),
                                                 flush_required(false) {
}

SimpleIndexLoadResult::~SimpleIndexLoadResult() {
}

void SimpleIndexLoadResult::Reset() {
  did_load = false;
  flush_required = false;
  entries.clear();
}

// static
const char SimpleIndexFile::kIndexFileName[] = "the-real-index";
// static
const char SimpleIndexFile::kIndexDirectory[] = "index-dir";
// static
const char SimpleIndexFile::kTempIndexFileName[] = "temp-index";

SimpleIndexFile::IndexMetadata::IndexMetadata()
    : magic_number_(kSimpleIndexMagicNumber),
      version_(kSimpleVersion),
      number_of_entries_(0),
      cache_size_(0) {}

SimpleIndexFile::IndexMetadata::IndexMetadata(
    uint64 number_of_entries, uint64 cache_size)
    : magic_number_(kSimpleIndexMagicNumber),
      version_(kSimpleVersion),
      number_of_entries_(number_of_entries),
      cache_size_(cache_size) {}

void SimpleIndexFile::IndexMetadata::Serialize(Pickle* pickle) const {
  DCHECK(pickle);
  pickle->WriteUInt64(magic_number_);
  pickle->WriteUInt32(version_);
  pickle->WriteUInt64(number_of_entries_);
  pickle->WriteUInt64(cache_size_);
}

// static
bool SimpleIndexFile::SerializeFinalData(base::Time cache_modified,
                                         Pickle* pickle) {
  if (!pickle->WriteInt64(cache_modified.ToInternalValue()))
    return false;
  SimpleIndexFile::PickleHeader* header_p = pickle->headerT<PickleHeader>();
  header_p->crc = CalculatePickleCRC(*pickle);
  return true;
}

bool SimpleIndexFile::IndexMetadata::Deserialize(PickleIterator* it) {
  DCHECK(it);
  return it->ReadUInt64(&magic_number_) &&
      it->ReadUInt32(&version_) &&
      it->ReadUInt64(&number_of_entries_)&&
      it->ReadUInt64(&cache_size_);
}

void SimpleIndexFile::SyncWriteToDisk(net::CacheType cache_type,
                                      const base::FilePath& cache_directory,
                                      const base::FilePath& index_filename,
                                      const base::FilePath& temp_index_filename,
                                      scoped_ptr<Pickle> pickle,
                                      const base::TimeTicks& start_time,
                                      bool app_on_background) {
  // There is a chance that the index containing all the necessary data about
  // newly created entries will appear to be stale. This can happen if on-disk
  // part of a Create operation does not fit into the time budget for the index
  // flush delay. This simple approach will be reconsidered if it does not allow
  // for maintaining freshness.
  base::Time cache_dir_mtime;
  if (!simple_util::GetMTime(cache_directory, &cache_dir_mtime)) {
    LOG(ERROR) << "Could obtain information about cache age";
    return;
  }
  SerializeFinalData(cache_dir_mtime, pickle.get());
  if (!WritePickleFile(pickle.get(), temp_index_filename)) {
    if (!base::CreateDirectory(temp_index_filename.DirName())) {
      LOG(ERROR) << "Could not create a directory to hold the index file";
      return;
    }
    if (!WritePickleFile(pickle.get(), temp_index_filename)) {
      LOG(ERROR) << "Failed to write the temporary index file";
      return;
    }
  }

  // Atomically rename the temporary index file to become the real one.
  bool result = base::ReplaceFile(temp_index_filename, index_filename, NULL);
  DCHECK(result);

  if (app_on_background) {
    SIMPLE_CACHE_UMA(TIMES,
                     "IndexWriteToDiskTime.Background", cache_type,
                     (base::TimeTicks::Now() - start_time));
  } else {
    SIMPLE_CACHE_UMA(TIMES,
                     "IndexWriteToDiskTime.Foreground", cache_type,
                     (base::TimeTicks::Now() - start_time));
  }
}

bool SimpleIndexFile::IndexMetadata::CheckIndexMetadata() {
  return number_of_entries_ <= kMaxEntiresInIndex &&
      magic_number_ == kSimpleIndexMagicNumber &&
      version_ == kSimpleVersion;
}

SimpleIndexFile::SimpleIndexFile(
    base::SingleThreadTaskRunner* cache_thread,
    base::TaskRunner* worker_pool,
    net::CacheType cache_type,
    const base::FilePath& cache_directory)
    : cache_thread_(cache_thread),
      worker_pool_(worker_pool),
      cache_type_(cache_type),
      cache_directory_(cache_directory),
      index_file_(cache_directory_.AppendASCII(kIndexDirectory)
                      .AppendASCII(kIndexFileName)),
      temp_index_file_(cache_directory_.AppendASCII(kIndexDirectory)
                           .AppendASCII(kTempIndexFileName)) {
}

SimpleIndexFile::~SimpleIndexFile() {}

void SimpleIndexFile::LoadIndexEntries(base::Time cache_last_modified,
                                       const base::Closure& callback,
                                       SimpleIndexLoadResult* out_result) {
  base::Closure task = base::Bind(&SimpleIndexFile::SyncLoadIndexEntries,
                                  cache_type_,
                                  cache_last_modified, cache_directory_,
                                  index_file_, out_result);
  worker_pool_->PostTaskAndReply(FROM_HERE, task, callback);
}

void SimpleIndexFile::WriteToDisk(const SimpleIndex::EntrySet& entry_set,
                                  uint64 cache_size,
                                  const base::TimeTicks& start,
                                  bool app_on_background) {
  IndexMetadata index_metadata(entry_set.size(), cache_size);
  scoped_ptr<Pickle> pickle = Serialize(index_metadata, entry_set);
  cache_thread_->PostTask(FROM_HERE, base::Bind(
      &SimpleIndexFile::SyncWriteToDisk,
      cache_type_,
      cache_directory_,
      index_file_,
      temp_index_file_,
      base::Passed(&pickle),
      base::TimeTicks::Now(),
      app_on_background));
}

// static
void SimpleIndexFile::SyncLoadIndexEntries(
    net::CacheType cache_type,
    base::Time cache_last_modified,
    const base::FilePath& cache_directory,
    const base::FilePath& index_file_path,
    SimpleIndexLoadResult* out_result) {
  // Load the index and find its age.
  base::Time last_cache_seen_by_index;
  SyncLoadFromDisk(index_file_path, &last_cache_seen_by_index, out_result);

  // Consider the index loaded if it is fresh.
  const bool index_file_existed = base::PathExists(index_file_path);
  if (!out_result->did_load) {
    if (index_file_existed)
      UmaRecordIndexFileState(INDEX_STATE_CORRUPT, cache_type);
  } else {
    if (cache_last_modified <= last_cache_seen_by_index) {
      base::Time latest_dir_mtime;
      simple_util::GetMTime(cache_directory, &latest_dir_mtime);
      if (LegacyIsIndexFileStale(latest_dir_mtime, index_file_path)) {
        UmaRecordIndexFileState(INDEX_STATE_FRESH_CONCURRENT_UPDATES,
                                cache_type);
      } else {
        UmaRecordIndexFileState(INDEX_STATE_FRESH, cache_type);
      }
      UmaRecordIndexInitMethod(INITIALIZE_METHOD_LOADED, cache_type);
      return;
    }
    UmaRecordIndexFileState(INDEX_STATE_STALE, cache_type);
  }

  // Reconstruct the index by scanning the disk for entries.
  const base::TimeTicks start = base::TimeTicks::Now();
  SyncRestoreFromDisk(cache_directory, index_file_path, out_result);
  SIMPLE_CACHE_UMA(MEDIUM_TIMES, "IndexRestoreTime", cache_type,
                   base::TimeTicks::Now() - start);
  SIMPLE_CACHE_UMA(COUNTS, "IndexEntriesRestored", cache_type,
                   out_result->entries.size());
  if (index_file_existed) {
    UmaRecordIndexInitMethod(INITIALIZE_METHOD_RECOVERED, cache_type);
  } else {
    UmaRecordIndexInitMethod(INITIALIZE_METHOD_NEWCACHE, cache_type);
    SIMPLE_CACHE_UMA(COUNTS,
                     "IndexCreatedEntryCount", cache_type,
                     out_result->entries.size());
  }
}

// static
void SimpleIndexFile::SyncLoadFromDisk(const base::FilePath& index_filename,
                                       base::Time* out_last_cache_seen_by_index,
                                       SimpleIndexLoadResult* out_result) {
  out_result->Reset();

  base::MemoryMappedFile index_file_map;
  if (!index_file_map.Initialize(index_filename)) {
    LOG(WARNING) << "Could not map Simple Index file.";
    base::DeleteFile(index_filename, false);
    return;
  }

  SimpleIndexFile::Deserialize(
      reinterpret_cast<const char*>(index_file_map.data()),
      index_file_map.length(),
      out_last_cache_seen_by_index,
      out_result);

  if (!out_result->did_load)
    base::DeleteFile(index_filename, false);
}

// static
scoped_ptr<Pickle> SimpleIndexFile::Serialize(
    const SimpleIndexFile::IndexMetadata& index_metadata,
    const SimpleIndex::EntrySet& entries) {
  scoped_ptr<Pickle> pickle(new Pickle(sizeof(SimpleIndexFile::PickleHeader)));

  index_metadata.Serialize(pickle.get());
  for (SimpleIndex::EntrySet::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    pickle->WriteUInt64(it->first);
    it->second.Serialize(pickle.get());
  }
  return pickle.Pass();
}

// static
void SimpleIndexFile::Deserialize(const char* data, int data_len,
                                  base::Time* out_cache_last_modified,
                                  SimpleIndexLoadResult* out_result) {
  DCHECK(data);

  out_result->Reset();
  SimpleIndex::EntrySet* entries = &out_result->entries;

  Pickle pickle(data, data_len);
  if (!pickle.data()) {
    LOG(WARNING) << "Corrupt Simple Index File.";
    return;
  }

  PickleIterator pickle_it(pickle);
  SimpleIndexFile::PickleHeader* header_p =
      pickle.headerT<SimpleIndexFile::PickleHeader>();
  const uint32 crc_read = header_p->crc;
  const uint32 crc_calculated = CalculatePickleCRC(pickle);

  if (crc_read != crc_calculated) {
    LOG(WARNING) << "Invalid CRC in Simple Index file.";
    return;
  }

  SimpleIndexFile::IndexMetadata index_metadata;
  if (!index_metadata.Deserialize(&pickle_it)) {
    LOG(ERROR) << "Invalid index_metadata on Simple Cache Index.";
    return;
  }

  if (!index_metadata.CheckIndexMetadata()) {
    LOG(ERROR) << "Invalid index_metadata on Simple Cache Index.";
    return;
  }

#if !defined(OS_WIN)
  // TODO(gavinp): Consider using std::unordered_map.
  entries->resize(index_metadata.GetNumberOfEntries() + kExtraSizeForMerge);
#endif
  while (entries->size() < index_metadata.GetNumberOfEntries()) {
    uint64 hash_key;
    EntryMetadata entry_metadata;
    if (!pickle_it.ReadUInt64(&hash_key) ||
        !entry_metadata.Deserialize(&pickle_it)) {
      LOG(WARNING) << "Invalid EntryMetadata in Simple Index file.";
      entries->clear();
      return;
    }
    SimpleIndex::InsertInEntrySet(hash_key, entry_metadata, entries);
  }

  int64 cache_last_modified;
  if (!pickle_it.ReadInt64(&cache_last_modified)) {
    entries->clear();
    return;
  }
  DCHECK(out_cache_last_modified);
  *out_cache_last_modified = base::Time::FromInternalValue(cache_last_modified);

  out_result->did_load = true;
}

// static
void SimpleIndexFile::SyncRestoreFromDisk(
    const base::FilePath& cache_directory,
    const base::FilePath& index_file_path,
    SimpleIndexLoadResult* out_result) {
  VLOG(1) << "Simple Cache Index is being restored from disk.";
  base::DeleteFile(index_file_path, /* recursive = */ false);
  out_result->Reset();
  SimpleIndex::EntrySet* entries = &out_result->entries;

  const bool did_succeed = TraverseCacheDirectory(
      cache_directory, base::Bind(&ProcessEntryFile, entries));
  if (!did_succeed) {
    LOG(ERROR) << "Could not reconstruct index from disk";
    return;
  }
  out_result->did_load = true;
  // When we restore from disk we write the merged index file to disk right
  // away, this might save us from having to restore again next time.
  out_result->flush_required = true;
}

// static
bool SimpleIndexFile::LegacyIsIndexFileStale(
    base::Time cache_last_modified,
    const base::FilePath& index_file_path) {
  base::Time index_mtime;
  if (!simple_util::GetMTime(index_file_path, &index_mtime))
    return true;
  return index_mtime < cache_last_modified;
}

}  // namespace disk_cache
