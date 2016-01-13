// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/session_storage_database.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "url/gurl.h"


namespace {

const char session_storage_uma_name[] = "SessionStorageDatabase.Open";

enum SessionStorageUMA {
  SESSION_STORAGE_UMA_SUCCESS,
  SESSION_STORAGE_UMA_RECREATED,
  SESSION_STORAGE_UMA_FAIL,
  SESSION_STORAGE_UMA_MAX
};

}  // namespace

// Layout of the database:
// | key                            | value                              |
// -----------------------------------------------------------------------
// | map-1-                         | 2 (refcount, start of map-1-* keys)|
// | map-1-a                        | b (a = b in map 1)                 |
// | ...                            |                                    |
// | namespace-                     | dummy (start of namespace-* keys)  |
// | namespace-1- (1 = namespace id)| dummy (start of namespace-1-* keys)|
// | namespace-1-origin1            | 1 (mapid)                          |
// | namespace-1-origin2            | 2                                  |
// | namespace-2-                   | dummy                              |
// | namespace-2-origin1            | 1 (shallow copy)                   |
// | namespace-2-origin2            | 2 (shallow copy)                   |
// | namespace-3-                   | dummy                              |
// | namespace-3-origin1            | 3 (deep copy)                      |
// | namespace-3-origin2            | 2 (shallow copy)                   |
// | next-map-id                    | 4                                  |

namespace content {

// This class keeps track of ongoing operations across different threads. When
// DB inconsistency is detected, we need to 1) make sure no new operations start
// 2) wait until all current operations finish, and let the last one of them
// close the DB and delete the data. The DB will remain empty for the rest of
// the run, and will be recreated during the next run. We cannot hope to recover
// during this run, since the upper layer will have a different idea about what
// should be in the database.
class SessionStorageDatabase::DBOperation {
 public:
  DBOperation(SessionStorageDatabase* session_storage_database)
      : session_storage_database_(session_storage_database) {
    base::AutoLock auto_lock(session_storage_database_->db_lock_);
    ++session_storage_database_->operation_count_;
  }

  ~DBOperation() {
    base::AutoLock auto_lock(session_storage_database_->db_lock_);
    --session_storage_database_->operation_count_;
    if ((session_storage_database_->is_inconsistent_ ||
         session_storage_database_->db_error_) &&
        session_storage_database_->operation_count_ == 0 &&
        !session_storage_database_->invalid_db_deleted_) {
      // No other operations are ongoing and the data is bad -> delete it now.
      session_storage_database_->db_.reset();
#if defined(OS_WIN)
      leveldb::DestroyDB(
          base::WideToUTF8(session_storage_database_->file_path_.value()),
          leveldb::Options());
#else
      leveldb::DestroyDB(session_storage_database_->file_path_.value(),
                         leveldb::Options());
#endif
      session_storage_database_->invalid_db_deleted_ = true;
    }
  }

 private:
  SessionStorageDatabase* session_storage_database_;
};


SessionStorageDatabase::SessionStorageDatabase(const base::FilePath& file_path)
    : file_path_(file_path),
      db_error_(false),
      is_inconsistent_(false),
      invalid_db_deleted_(false),
      operation_count_(0) {
}

SessionStorageDatabase::~SessionStorageDatabase() {
}

void SessionStorageDatabase::ReadAreaValues(const std::string& namespace_id,
                                            const GURL& origin,
                                            DOMStorageValuesMap* result) {
  // We don't create a database if it doesn't exist. In that case, there is
  // nothing to be added to the result.
  if (!LazyOpen(false))
    return;
  DBOperation operation(this);

  // While ReadAreaValues is in progress, another thread can call
  // CommitAreaChanges. CommitAreaChanges might update map ref count key while
  // this thread is iterating over the map ref count key. To protect the reading
  // operation, create a snapshot and read from it.
  leveldb::ReadOptions options;
  options.snapshot = db_->GetSnapshot();

  std::string map_id;
  bool exists;
  if (GetMapForArea(namespace_id, origin.spec(), options, &exists, &map_id) &&
      exists)
    ReadMap(map_id, options, result, false);
  db_->ReleaseSnapshot(options.snapshot);
}

bool SessionStorageDatabase::CommitAreaChanges(
    const std::string& namespace_id,
    const GURL& origin,
    bool clear_all_first,
    const DOMStorageValuesMap& changes) {
  // Even if |changes| is empty, we need to write the appropriate placeholders
  // in the database, so that it can be later shallow-copied succssfully.
  if (!LazyOpen(true))
    return false;
  DBOperation operation(this);

  leveldb::WriteBatch batch;
  // Ensure that the keys "namespace-" "namespace-N" (see the schema above)
  // exist.
  const bool kOkIfExists = true;
  if (!CreateNamespace(namespace_id, kOkIfExists, &batch))
    return false;

  std::string map_id;
  bool exists;
  if (!GetMapForArea(namespace_id, origin.spec(), leveldb::ReadOptions(),
                     &exists, &map_id))
    return false;
  if (exists) {
    int64 ref_count;
    if (!GetMapRefCount(map_id, &ref_count))
      return false;
    if (ref_count > 1) {
      if (!DeepCopyArea(namespace_id, origin, !clear_all_first,
                        &map_id, &batch))
        return false;
    }
    else if (clear_all_first) {
      if (!ClearMap(map_id, &batch))
        return false;
    }
  } else {
    // Map doesn't exist, create it now if needed.
    if (!changes.empty()) {
      if (!CreateMapForArea(namespace_id, origin, &map_id, &batch))
        return false;
    }
  }

  WriteValuesToMap(map_id, changes, &batch);

  leveldb::Status s = db_->Write(leveldb::WriteOptions(), &batch);
  return DatabaseErrorCheck(s.ok());
}

bool SessionStorageDatabase::CloneNamespace(
    const std::string& namespace_id, const std::string& new_namespace_id) {
  // Go through all origins in the namespace |namespace_id|, create placeholders
  // for them in |new_namespace_id|, and associate them with the existing maps.

  // Example, data before shallow copy:
  // | map-1-                         | 1 (refcount)        |
  // | map-1-a                        | b                   |
  // | namespace-1- (1 = namespace id)| dummy               |
  // | namespace-1-origin1            | 1 (mapid)           |

  // Example, data after shallow copy:
  // | map-1-                         | 2 (inc. refcount)   |
  // | map-1-a                        | b                   |
  // | namespace-1-(1 = namespace id) | dummy               |
  // | namespace-1-origin1            | 1 (mapid)           |
  // | namespace-2-                   | dummy               |
  // | namespace-2-origin1            | 1 (mapid) << references the same map

  if (!LazyOpen(true))
    return false;
  DBOperation operation(this);

  leveldb::WriteBatch batch;
  const bool kOkIfExists = false;
  if (!CreateNamespace(new_namespace_id, kOkIfExists, &batch))
    return false;

  std::map<std::string, std::string> areas;
  if (!GetAreasInNamespace(namespace_id, &areas))
    return false;

  for (std::map<std::string, std::string>::const_iterator it = areas.begin();
       it != areas.end(); ++it) {
    const std::string& origin = it->first;
    const std::string& map_id = it->second;
    if (!IncreaseMapRefCount(map_id, &batch))
      return false;
    AddAreaToNamespace(new_namespace_id, origin, map_id, &batch);
  }
  leveldb::Status s = db_->Write(leveldb::WriteOptions(), &batch);
  return DatabaseErrorCheck(s.ok());
}

bool SessionStorageDatabase::DeleteArea(const std::string& namespace_id,
                                        const GURL& origin) {
  if (!LazyOpen(false)) {
    // No need to create the database if it doesn't exist.
    return true;
  }
  DBOperation operation(this);
  leveldb::WriteBatch batch;
  if (!DeleteAreaHelper(namespace_id, origin.spec(), &batch))
    return false;
  leveldb::Status s = db_->Write(leveldb::WriteOptions(), &batch);
  return DatabaseErrorCheck(s.ok());
}

bool SessionStorageDatabase::DeleteNamespace(const std::string& namespace_id) {
  if (!LazyOpen(false)) {
    // No need to create the database if it doesn't exist.
    return true;
  }
  DBOperation operation(this);
  // Itereate through the areas in the namespace.
  leveldb::WriteBatch batch;
  std::map<std::string, std::string> areas;
  if (!GetAreasInNamespace(namespace_id, &areas))
    return false;
  for (std::map<std::string, std::string>::const_iterator it = areas.begin();
       it != areas.end(); ++it) {
    const std::string& origin = it->first;
    if (!DeleteAreaHelper(namespace_id, origin, &batch))
      return false;
  }
  batch.Delete(NamespaceStartKey(namespace_id));
  leveldb::Status s = db_->Write(leveldb::WriteOptions(), &batch);
  return DatabaseErrorCheck(s.ok());
}

bool SessionStorageDatabase::ReadNamespacesAndOrigins(
    std::map<std::string, std::vector<GURL> >* namespaces_and_origins) {
  if (!LazyOpen(true))
    return false;
  DBOperation operation(this);

  // While ReadNamespacesAndOrigins is in progress, another thread can call
  // CommitAreaChanges. To protect the reading operation, create a snapshot and
  // read from it.
  leveldb::ReadOptions options;
  options.snapshot = db_->GetSnapshot();

  std::string namespace_prefix = NamespacePrefix();
  scoped_ptr<leveldb::Iterator> it(db_->NewIterator(options));
  it->Seek(namespace_prefix);
  // If the key is not found, the status of the iterator won't be IsNotFound(),
  // but the iterator will be invalid.
  if (!it->Valid()) {
    db_->ReleaseSnapshot(options.snapshot);
    return true;
  }

  if (!DatabaseErrorCheck(it->status().ok())) {
    db_->ReleaseSnapshot(options.snapshot);
    return false;
  }

  // Skip the dummy entry "namespace-" and iterate the namespaces.
  std::string current_namespace_start_key;
  std::string current_namespace_id;
  for (it->Next(); it->Valid(); it->Next()) {
    std::string key = it->key().ToString();
    if (key.find(namespace_prefix) != 0) {
      // Iterated past the "namespace-" keys.
      break;
    }
    // For each namespace, the first key is "namespace-<namespaceid>-", and the
    // subsequent keys are "namespace-<namespaceid>-<origin>". Read the unique
    // "<namespaceid>" parts from the keys.
    if (current_namespace_start_key.empty() ||
        key.substr(0, current_namespace_start_key.length()) !=
        current_namespace_start_key) {
      // The key is of the form "namespace-<namespaceid>-" for a new
      // <namespaceid>.
      current_namespace_start_key = key;
      current_namespace_id =
          key.substr(namespace_prefix.length(),
                     key.length() - namespace_prefix.length() - 1);
      // Ensure that we keep track of the namespace even if it doesn't contain
      // any origins.
      namespaces_and_origins->insert(
          std::make_pair(current_namespace_id, std::vector<GURL>()));
    } else {
      // The key is of the form "namespace-<namespaceid>-<origin>".
      std::string origin = key.substr(current_namespace_start_key.length());
      (*namespaces_and_origins)[current_namespace_id].push_back(GURL(origin));
    }
  }
  db_->ReleaseSnapshot(options.snapshot);
  return true;
}

bool SessionStorageDatabase::LazyOpen(bool create_if_needed) {
  base::AutoLock auto_lock(db_lock_);
  if (db_error_ || is_inconsistent_) {
    // Don't try to open a database that we know has failed already.
    return false;
  }
  if (IsOpen())
    return true;

  if (!create_if_needed &&
      (!base::PathExists(file_path_) || base::IsDirectoryEmpty(file_path_))) {
    // If the directory doesn't exist already and we haven't been asked to
    // create a file on disk, then we don't bother opening the database. This
    // means we wait until we absolutely need to put something onto disk before
    // we do so.
    return false;
  }

  leveldb::DB* db;
  leveldb::Status s = TryToOpen(&db);
  if (!s.ok()) {
    LOG(WARNING) << "Failed to open leveldb in " << file_path_.value()
                 << ", error: " << s.ToString();
    DCHECK(db == NULL);

    // Clear the directory and try again.
    base::DeleteFile(file_path_, true);
    s = TryToOpen(&db);
    if (!s.ok()) {
      LOG(WARNING) << "Failed to open leveldb in " << file_path_.value()
                   << ", error: " << s.ToString();
      UMA_HISTOGRAM_ENUMERATION(session_storage_uma_name,
                                SESSION_STORAGE_UMA_FAIL,
                                SESSION_STORAGE_UMA_MAX);
      DCHECK(db == NULL);
      db_error_ = true;
      return false;
    }
    UMA_HISTOGRAM_ENUMERATION(session_storage_uma_name,
                              SESSION_STORAGE_UMA_RECREATED,
                              SESSION_STORAGE_UMA_MAX);
  } else {
    UMA_HISTOGRAM_ENUMERATION(session_storage_uma_name,
                              SESSION_STORAGE_UMA_SUCCESS,
                              SESSION_STORAGE_UMA_MAX);
  }
  db_.reset(db);
  return true;
}

leveldb::Status SessionStorageDatabase::TryToOpen(leveldb::DB** db) {
  leveldb::Options options;
  // The directory exists but a valid leveldb database might not exist inside it
  // (e.g., a subset of the needed files might be missing). Handle this
  // situation gracefully by creating the database now.
  options.max_open_files = 0;  // Use minimum.
  options.create_if_missing = true;
#if defined(OS_WIN)
  return leveldb::DB::Open(options, base::WideToUTF8(file_path_.value()), db);
#elif defined(OS_POSIX)
  return leveldb::DB::Open(options, file_path_.value(), db);
#endif
}

bool SessionStorageDatabase::IsOpen() const {
  return db_.get() != NULL;
}

bool SessionStorageDatabase::CallerErrorCheck(bool ok) const {
  DCHECK(ok);
  return ok;
}

bool SessionStorageDatabase::ConsistencyCheck(bool ok) {
  if (ok)
    return true;
  base::AutoLock auto_lock(db_lock_);
  // We cannot recover the database during this run, e.g., the upper layer can
  // have a different understanding of the database state (shallow and deep
  // copies). Make further operations fail. The next operation that finishes
  // will delete the data, and next run will recerate the database.
  is_inconsistent_ = true;
  return false;
}

bool SessionStorageDatabase::DatabaseErrorCheck(bool ok) {
  if (ok)
    return true;
  base::AutoLock auto_lock(db_lock_);
  // Make further operations fail. The next operation that finishes
  // will delete the data, and next run will recerate the database.
  db_error_ = true;
  return false;
}

bool SessionStorageDatabase::CreateNamespace(const std::string& namespace_id,
                                             bool ok_if_exists,
                                             leveldb::WriteBatch* batch) {
  leveldb::Slice namespace_prefix = NamespacePrefix();
  std::string dummy;
  leveldb::Status s = db_->Get(leveldb::ReadOptions(), namespace_prefix,
                               &dummy);
  if (!DatabaseErrorCheck(s.ok() || s.IsNotFound()))
    return false;
  if (s.IsNotFound())
    batch->Put(namespace_prefix, "");

  std::string namespace_start_key = NamespaceStartKey(namespace_id);
  s = db_->Get(leveldb::ReadOptions(), namespace_start_key, &dummy);
  if (!DatabaseErrorCheck(s.ok() || s.IsNotFound()))
    return false;
  if (s.IsNotFound()) {
    batch->Put(namespace_start_key, "");
    return true;
  }
  return CallerErrorCheck(ok_if_exists);
}

bool SessionStorageDatabase::GetAreasInNamespace(
    const std::string& namespace_id,
    std::map<std::string, std::string>* areas) {
  std::string namespace_start_key = NamespaceStartKey(namespace_id);
  scoped_ptr<leveldb::Iterator> it(db_->NewIterator(leveldb::ReadOptions()));
  it->Seek(namespace_start_key);
  // If the key is not found, the status of the iterator won't be IsNotFound(),
  // but the iterator will be invalid.
  if (!it->Valid()) {
    // The namespace_start_key is not found when the namespace doesn't contain
    // any areas. We don't need to do anything.
    return true;
  }
  if (!DatabaseErrorCheck(it->status().ok()))
    return false;

  // Skip the dummy entry "namespace-<namespaceid>-" and iterate the origins.
  for (it->Next(); it->Valid(); it->Next()) {
    std::string key = it->key().ToString();
    if (key.find(namespace_start_key) != 0) {
      // Iterated past the origins for this namespace.
      break;
    }
    std::string origin = key.substr(namespace_start_key.length());
    std::string map_id = it->value().ToString();
    (*areas)[origin] = map_id;
  }
  return true;
}

void SessionStorageDatabase::AddAreaToNamespace(const std::string& namespace_id,
                                                const std::string& origin,
                                                const std::string& map_id,
                                                leveldb::WriteBatch* batch) {
  std::string namespace_key = NamespaceKey(namespace_id, origin);
  batch->Put(namespace_key, map_id);
}

bool SessionStorageDatabase::DeleteAreaHelper(
    const std::string& namespace_id,
    const std::string& origin,
    leveldb::WriteBatch* batch) {
  std::string map_id;
  bool exists;
  if (!GetMapForArea(namespace_id, origin, leveldb::ReadOptions(), &exists,
                     &map_id))
    return false;
  if (!exists)
    return true;  // Nothing to delete.
  if (!DecreaseMapRefCount(map_id, 1, batch))
    return false;
  std::string namespace_key = NamespaceKey(namespace_id, origin);
  batch->Delete(namespace_key);

  // If this was the only area in the namespace, delete the namespace start key,
  // too.
  std::string namespace_start_key = NamespaceStartKey(namespace_id);
  scoped_ptr<leveldb::Iterator> it(db_->NewIterator(leveldb::ReadOptions()));
  it->Seek(namespace_start_key);
  if (!ConsistencyCheck(it->Valid()))
    return false;
  // Advance the iterator 2 times (we still haven't really deleted
  // namespace_key).
  it->Next();
  if (!ConsistencyCheck(it->Valid()))
    return false;
  it->Next();
  if (!it->Valid())
    return true;
  std::string key = it->key().ToString();
  if (key.find(namespace_start_key) != 0)
    batch->Delete(namespace_start_key);
  return true;
}

bool SessionStorageDatabase::GetMapForArea(const std::string& namespace_id,
                                           const std::string& origin,
                                           const leveldb::ReadOptions& options,
                                           bool* exists, std::string* map_id) {
  std::string namespace_key = NamespaceKey(namespace_id, origin);
  leveldb::Status s = db_->Get(options, namespace_key, map_id);
  if (s.IsNotFound()) {
    *exists = false;
    return true;
  }
  *exists = true;
  return DatabaseErrorCheck(s.ok());
}

bool SessionStorageDatabase::CreateMapForArea(const std::string& namespace_id,
                                              const GURL& origin,
                                              std::string* map_id,
                                              leveldb::WriteBatch* batch) {
  leveldb::Slice next_map_id_key = NextMapIdKey();
  leveldb::Status s = db_->Get(leveldb::ReadOptions(), next_map_id_key, map_id);
  if (!DatabaseErrorCheck(s.ok() || s.IsNotFound()))
    return false;
  int64 next_map_id = 0;
  if (s.IsNotFound()) {
    *map_id = "0";
  } else {
    bool conversion_ok = base::StringToInt64(*map_id, &next_map_id);
    if (!ConsistencyCheck(conversion_ok))
      return false;
  }
  batch->Put(next_map_id_key, base::Int64ToString(++next_map_id));
  std::string namespace_key = NamespaceKey(namespace_id, origin.spec());
  batch->Put(namespace_key, *map_id);
  batch->Put(MapRefCountKey(*map_id), "1");
  return true;
}

bool SessionStorageDatabase::ReadMap(const std::string& map_id,
                                     const leveldb::ReadOptions& options,
                                     DOMStorageValuesMap* result,
                                     bool only_keys) {
  scoped_ptr<leveldb::Iterator> it(db_->NewIterator(options));
  std::string map_start_key = MapRefCountKey(map_id);
  it->Seek(map_start_key);
  // If the key is not found, the status of the iterator won't be IsNotFound(),
  // but the iterator will be invalid. The map needs to exist, otherwise we have
  // a stale map_id in the database.
  if (!ConsistencyCheck(it->Valid()))
    return false;
  if (!DatabaseErrorCheck(it->status().ok()))
    return false;
  // Skip the dummy entry "map-<mapid>-".
  for (it->Next(); it->Valid(); it->Next()) {
    std::string key = it->key().ToString();
    if (key.find(map_start_key) != 0) {
      // Iterated past the keys in this map.
      break;
    }
    // Key is of the form "map-<mapid>-<key>".
    base::string16 key16 =
        base::UTF8ToUTF16(key.substr(map_start_key.length()));
    if (only_keys) {
      (*result)[key16] = base::NullableString16();
    } else {
      // Convert the raw data stored in std::string (it->value()) to raw data
      // stored in base::string16.
      size_t len = it->value().size() / sizeof(base::char16);
      const base::char16* data_ptr =
          reinterpret_cast<const base::char16*>(it->value().data());
      (*result)[key16] =
          base::NullableString16(base::string16(data_ptr, len), false);
    }
  }
  return true;
}

void SessionStorageDatabase::WriteValuesToMap(const std::string& map_id,
                                              const DOMStorageValuesMap& values,
                                              leveldb::WriteBatch* batch) {
  for (DOMStorageValuesMap::const_iterator it = values.begin();
       it != values.end();
       ++it) {
    base::NullableString16 value = it->second;
    std::string key = MapKey(map_id, base::UTF16ToUTF8(it->first));
    if (value.is_null()) {
      batch->Delete(key);
    } else {
      // Convert the raw data stored in base::string16 to raw data stored in
      // std::string.
      const char* data = reinterpret_cast<const char*>(value.string().data());
      size_t size = value.string().size() * 2;
      batch->Put(key, leveldb::Slice(data, size));
    }
  }
}

bool SessionStorageDatabase::GetMapRefCount(const std::string& map_id,
                                            int64* ref_count) {
  std::string ref_count_string;
  leveldb::Status s = db_->Get(leveldb::ReadOptions(),
                               MapRefCountKey(map_id), &ref_count_string);
  if (!ConsistencyCheck(s.ok()))
    return false;
  bool conversion_ok = base::StringToInt64(ref_count_string, ref_count);
  return ConsistencyCheck(conversion_ok);
}

bool SessionStorageDatabase::IncreaseMapRefCount(const std::string& map_id,
                                                 leveldb::WriteBatch* batch) {
  // Increase the ref count for the map.
  int64 old_ref_count;
  if (!GetMapRefCount(map_id, &old_ref_count))
    return false;
  batch->Put(MapRefCountKey(map_id), base::Int64ToString(++old_ref_count));
  return true;
}

bool SessionStorageDatabase::DecreaseMapRefCount(const std::string& map_id,
                                                 int decrease,
                                                 leveldb::WriteBatch* batch) {
  // Decrease the ref count for the map.
  int64 ref_count;
  if (!GetMapRefCount(map_id, &ref_count))
    return false;
  if (!ConsistencyCheck(decrease <= ref_count))
    return false;
  ref_count -= decrease;
  if (ref_count > 0) {
    batch->Put(MapRefCountKey(map_id), base::Int64ToString(ref_count));
  } else {
    // Clear all keys in the map.
    if (!ClearMap(map_id, batch))
      return false;
    batch->Delete(MapRefCountKey(map_id));
  }
  return true;
}

bool SessionStorageDatabase::ClearMap(const std::string& map_id,
                                      leveldb::WriteBatch* batch) {
  DOMStorageValuesMap values;
  if (!ReadMap(map_id, leveldb::ReadOptions(), &values, true))
    return false;
  for (DOMStorageValuesMap::const_iterator it = values.begin();
       it != values.end(); ++it)
    batch->Delete(MapKey(map_id, base::UTF16ToUTF8(it->first)));
  return true;
}

bool SessionStorageDatabase::DeepCopyArea(
    const std::string& namespace_id, const GURL& origin, bool copy_data,
    std::string* map_id, leveldb::WriteBatch* batch) {
  // Example, data before deep copy:
  // | namespace-1- (1 = namespace id)| dummy               |
  // | namespace-1-origin1            | 1 (mapid)           |
  // | namespace-2-                   | dummy               |
  // | namespace-2-origin1            | 1 (mapid) << references the same map
  // | map-1-                         | 2 (refcount)        |
  // | map-1-a                        | b                   |

  // Example, data after deep copy copy:
  // | namespace-1-(1 = namespace id) | dummy               |
  // | namespace-1-origin1            | 1 (mapid)           |
  // | namespace-2-                   | dummy               |
  // | namespace-2-origin1            | 2 (mapid) << references the new map
  // | map-1-                         | 1 (dec. refcount)   |
  // | map-1-a                        | b                   |
  // | map-2-                         | 1 (refcount)        |
  // | map-2-a                        | b                   |

  // Read the values from the old map here. If we don't need to copy the data,
  // this can stay empty.
  DOMStorageValuesMap values;
  if (copy_data && !ReadMap(*map_id, leveldb::ReadOptions(), &values, false))
    return false;
  if (!DecreaseMapRefCount(*map_id, 1, batch))
    return false;
  // Create a new map (this will also break the association to the old map) and
  // write the old data into it. This will write the id of the created map into
  // |map_id|.
  if (!CreateMapForArea(namespace_id, origin, map_id, batch))
    return false;
  WriteValuesToMap(*map_id, values, batch);
  return true;
}

std::string SessionStorageDatabase::NamespaceStartKey(
    const std::string& namespace_id) {
  return base::StringPrintf("namespace-%s-", namespace_id.c_str());
}

std::string SessionStorageDatabase::NamespaceKey(
    const std::string& namespace_id, const std::string& origin) {
  return base::StringPrintf("namespace-%s-%s", namespace_id.c_str(),
                            origin.c_str());
}

const char* SessionStorageDatabase::NamespacePrefix() {
  return "namespace-";
}

std::string SessionStorageDatabase::MapRefCountKey(const std::string& map_id) {
  return base::StringPrintf("map-%s-", map_id.c_str());
}

std::string SessionStorageDatabase::MapKey(const std::string& map_id,
                                           const std::string& key) {
  return base::StringPrintf("map-%s-%s", map_id.c_str(), key.c_str());
}

const char* SessionStorageDatabase::NextMapIdKey() {
  return "next-map-id";
}

}  // namespace content
