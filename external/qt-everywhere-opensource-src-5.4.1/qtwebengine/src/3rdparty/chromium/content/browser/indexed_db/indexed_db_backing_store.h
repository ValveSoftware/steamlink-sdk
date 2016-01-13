// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BACKING_STORE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BACKING_STORE_H_

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/indexed_db/indexed_db.h"
#include "content/browser/indexed_db/indexed_db_active_blob_registry.h"
#include "content/browser/indexed_db/indexed_db_blob_info.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_metadata.h"
#include "content/browser/indexed_db/leveldb/leveldb_iterator.h"
#include "content/browser/indexed_db/leveldb/leveldb_transaction.h"
#include "content/common/content_export.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_path.h"
#include "content/common/indexed_db/indexed_db_key_range.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "url/gurl.h"
#include "webkit/browser/blob/blob_data_handle.h"

namespace base {
class TaskRunner;
}

namespace fileapi {
class FileWriterDelegate;
}

namespace net {
class URLRequestContext;
}

namespace content {

class IndexedDBFactory;
class LevelDBComparator;
class LevelDBDatabase;
struct IndexedDBValue;

class LevelDBFactory {
 public:
  virtual ~LevelDBFactory() {}
  virtual leveldb::Status OpenLevelDB(const base::FilePath& file_name,
                                      const LevelDBComparator* comparator,
                                      scoped_ptr<LevelDBDatabase>* db,
                                      bool* is_disk_full) = 0;
  virtual leveldb::Status DestroyLevelDB(const base::FilePath& file_name) = 0;
};

class CONTENT_EXPORT IndexedDBBackingStore
    : public base::RefCounted<IndexedDBBackingStore> {
 public:
  class CONTENT_EXPORT Transaction;

  class CONTENT_EXPORT Comparator : public LevelDBComparator {
   public:
    virtual int Compare(const base::StringPiece& a,
                        const base::StringPiece& b) const OVERRIDE;
    virtual const char* Name() const OVERRIDE;
  };

  const GURL& origin_url() const { return origin_url_; }
  IndexedDBFactory* factory() const { return indexed_db_factory_; }
  base::TaskRunner* task_runner() const { return task_runner_; }
  base::OneShotTimer<IndexedDBBackingStore>* close_timer() {
    return &close_timer_;
  }
  IndexedDBActiveBlobRegistry* active_blob_registry() {
    return &active_blob_registry_;
  }

  static scoped_refptr<IndexedDBBackingStore> Open(
      IndexedDBFactory* indexed_db_factory,
      const GURL& origin_url,
      const base::FilePath& path_base,
      net::URLRequestContext* request_context,
      blink::WebIDBDataLoss* data_loss,
      std::string* data_loss_message,
      bool* disk_full,
      base::TaskRunner* task_runner,
      bool clean_journal);
  static scoped_refptr<IndexedDBBackingStore> Open(
      IndexedDBFactory* indexed_db_factory,
      const GURL& origin_url,
      const base::FilePath& path_base,
      net::URLRequestContext* request_context,
      blink::WebIDBDataLoss* data_loss,
      std::string* data_loss_message,
      bool* disk_full,
      LevelDBFactory* leveldb_factory,
      base::TaskRunner* task_runner,
      bool clean_journal);
  static scoped_refptr<IndexedDBBackingStore> OpenInMemory(
      const GURL& origin_url,
      base::TaskRunner* task_runner);
  static scoped_refptr<IndexedDBBackingStore> OpenInMemory(
      const GURL& origin_url,
      LevelDBFactory* leveldb_factory,
      base::TaskRunner* task_runner);

  void GrantChildProcessPermissions(int child_process_id);

  // Compact is public for testing.
  virtual void Compact();
  virtual std::vector<base::string16> GetDatabaseNames(leveldb::Status*);
  virtual leveldb::Status GetIDBDatabaseMetaData(
      const base::string16& name,
      IndexedDBDatabaseMetadata* metadata,
      bool* success) WARN_UNUSED_RESULT;
  virtual leveldb::Status CreateIDBDatabaseMetaData(
      const base::string16& name,
      const base::string16& version,
      int64 int_version,
      int64* row_id);
  virtual bool UpdateIDBDatabaseIntVersion(
      IndexedDBBackingStore::Transaction* transaction,
      int64 row_id,
      int64 int_version);
  virtual leveldb::Status DeleteDatabase(const base::string16& name);

  // Assumes caller has already closed the backing store.
  static leveldb::Status DestroyBackingStore(const base::FilePath& path_base,
                                             const GURL& origin_url);
  static bool RecordCorruptionInfo(const base::FilePath& path_base,
                                   const GURL& origin_url,
                                   const std::string& message);
  leveldb::Status GetObjectStores(
      int64 database_id,
      IndexedDBDatabaseMetadata::ObjectStoreMap* map) WARN_UNUSED_RESULT;
  virtual leveldb::Status CreateObjectStore(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      const base::string16& name,
      const IndexedDBKeyPath& key_path,
      bool auto_increment);
  virtual leveldb::Status DeleteObjectStore(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id) WARN_UNUSED_RESULT;

  class CONTENT_EXPORT RecordIdentifier {
   public:
    RecordIdentifier(const std::string& primary_key, int64 version);
    RecordIdentifier();
    ~RecordIdentifier();

    const std::string& primary_key() const { return primary_key_; }
    int64 version() const { return version_; }
    void Reset(const std::string& primary_key, int64 version) {
      primary_key_ = primary_key;
      version_ = version;
    }

   private:
    // TODO(jsbell): Make it more clear that this is the *encoded* version of
    // the key.
    std::string primary_key_;
    int64 version_;
    DISALLOW_COPY_AND_ASSIGN(RecordIdentifier);
  };

  class BlobWriteCallback : public base::RefCounted<BlobWriteCallback> {
   public:
    virtual void Run(bool succeeded) = 0;

   protected:
    virtual ~BlobWriteCallback() {}
    friend class base::RefCounted<BlobWriteCallback>;
  };

  virtual leveldb::Status GetRecord(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      const IndexedDBKey& key,
      IndexedDBValue* record) WARN_UNUSED_RESULT;
  virtual leveldb::Status PutRecord(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      const IndexedDBKey& key,
      IndexedDBValue& value,
      ScopedVector<webkit_blob::BlobDataHandle>* handles,
      RecordIdentifier* record) WARN_UNUSED_RESULT;
  virtual leveldb::Status ClearObjectStore(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id) WARN_UNUSED_RESULT;
  virtual leveldb::Status DeleteRecord(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      const RecordIdentifier& record) WARN_UNUSED_RESULT;
  virtual leveldb::Status DeleteRange(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      const IndexedDBKeyRange&) WARN_UNUSED_RESULT;
  virtual leveldb::Status GetKeyGeneratorCurrentNumber(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      int64* current_number) WARN_UNUSED_RESULT;
  virtual leveldb::Status MaybeUpdateKeyGeneratorCurrentNumber(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      int64 new_state,
      bool check_current) WARN_UNUSED_RESULT;
  virtual leveldb::Status KeyExistsInObjectStore(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      const IndexedDBKey& key,
      RecordIdentifier* found_record_identifier,
      bool* found) WARN_UNUSED_RESULT;

  virtual leveldb::Status CreateIndex(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      int64 index_id,
      const base::string16& name,
      const IndexedDBKeyPath& key_path,
      bool is_unique,
      bool is_multi_entry) WARN_UNUSED_RESULT;
  virtual leveldb::Status DeleteIndex(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      int64 index_id) WARN_UNUSED_RESULT;
  virtual leveldb::Status PutIndexDataForRecord(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      int64 index_id,
      const IndexedDBKey& key,
      const RecordIdentifier& record) WARN_UNUSED_RESULT;
  virtual leveldb::Status GetPrimaryKeyViaIndex(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      int64 index_id,
      const IndexedDBKey& key,
      scoped_ptr<IndexedDBKey>* primary_key) WARN_UNUSED_RESULT;
  virtual leveldb::Status KeyExistsInIndex(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      int64 index_id,
      const IndexedDBKey& key,
      scoped_ptr<IndexedDBKey>* found_primary_key,
      bool* exists) WARN_UNUSED_RESULT;

  // Public for IndexedDBActiveBlobRegistry::ReleaseBlobRef.
  virtual void ReportBlobUnused(int64 database_id, int64 blob_key);

  base::FilePath GetBlobFileName(int64 database_id, int64 key);

  class Cursor {
   public:
    virtual ~Cursor();

    enum IteratorState {
      READY = 0,
      SEEK
    };

    struct CursorOptions {
      CursorOptions();
      ~CursorOptions();
      int64 database_id;
      int64 object_store_id;
      int64 index_id;
      std::string low_key;
      bool low_open;
      std::string high_key;
      bool high_open;
      bool forward;
      bool unique;
    };

    const IndexedDBKey& key() const { return *current_key_; }
    bool Continue(leveldb::Status* s) { return Continue(NULL, NULL, SEEK, s); }
    bool Continue(const IndexedDBKey* key,
                  IteratorState state,
                  leveldb::Status* s) {
      return Continue(key, NULL, state, s);
    }
    bool Continue(const IndexedDBKey* key,
                  const IndexedDBKey* primary_key,
                  IteratorState state,
                  leveldb::Status*);
    bool Advance(uint32 count, leveldb::Status*);
    bool FirstSeek(leveldb::Status*);

    virtual Cursor* Clone() = 0;
    virtual const IndexedDBKey& primary_key() const;
    virtual IndexedDBValue* value() = 0;
    virtual const RecordIdentifier& record_identifier() const;
    virtual bool LoadCurrentRow() = 0;

   protected:
    Cursor(scoped_refptr<IndexedDBBackingStore> backing_store,
           Transaction* transaction,
           int64 database_id,
           const CursorOptions& cursor_options);
    explicit Cursor(const IndexedDBBackingStore::Cursor* other);

    virtual std::string EncodeKey(const IndexedDBKey& key) = 0;
    virtual std::string EncodeKey(const IndexedDBKey& key,
                                  const IndexedDBKey& primary_key) = 0;

    bool IsPastBounds() const;
    bool HaveEnteredRange() const;

    IndexedDBBackingStore* backing_store_;
    Transaction* transaction_;
    int64 database_id_;
    const CursorOptions cursor_options_;
    scoped_ptr<LevelDBIterator> iterator_;
    scoped_ptr<IndexedDBKey> current_key_;
    IndexedDBBackingStore::RecordIdentifier record_identifier_;

   private:
    DISALLOW_COPY_AND_ASSIGN(Cursor);
  };

  virtual scoped_ptr<Cursor> OpenObjectStoreKeyCursor(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      const IndexedDBKeyRange& key_range,
      indexed_db::CursorDirection,
      leveldb::Status*);
  virtual scoped_ptr<Cursor> OpenObjectStoreCursor(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      const IndexedDBKeyRange& key_range,
      indexed_db::CursorDirection,
      leveldb::Status*);
  virtual scoped_ptr<Cursor> OpenIndexKeyCursor(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      int64 index_id,
      const IndexedDBKeyRange& key_range,
      indexed_db::CursorDirection,
      leveldb::Status*);
  virtual scoped_ptr<Cursor> OpenIndexCursor(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      int64 index_id,
      const IndexedDBKeyRange& key_range,
      indexed_db::CursorDirection,
      leveldb::Status*);

  class BlobChangeRecord {
   public:
    BlobChangeRecord(const std::string& key, int64 object_store_id);
    ~BlobChangeRecord();
    const std::string& key() const { return key_; }
    int64 object_store_id() const { return object_store_id_; }
    void SetBlobInfo(std::vector<IndexedDBBlobInfo>* blob_info);
    std::vector<IndexedDBBlobInfo>& mutable_blob_info() { return blob_info_; }
    const std::vector<IndexedDBBlobInfo>& blob_info() const {
      return blob_info_;
    }
    void SetHandles(ScopedVector<webkit_blob::BlobDataHandle>* handles);
    scoped_ptr<BlobChangeRecord> Clone() const;

   private:
    std::string key_;
    int64 object_store_id_;
    std::vector<IndexedDBBlobInfo> blob_info_;
    ScopedVector<webkit_blob::BlobDataHandle> handles_;
    DISALLOW_COPY_AND_ASSIGN(BlobChangeRecord);
  };
  typedef std::map<std::string, BlobChangeRecord*> BlobChangeMap;

  class Transaction {
   public:
    explicit Transaction(IndexedDBBackingStore* backing_store);
    virtual ~Transaction();
    virtual void Begin();
    // The callback will be called eventually on success or failure, or
    // immediately if phase one is complete due to lack of any blobs to write.
    virtual leveldb::Status CommitPhaseOne(scoped_refptr<BlobWriteCallback>);
    virtual leveldb::Status CommitPhaseTwo();
    virtual void Rollback();
    void Reset() {
      backing_store_ = NULL;
      transaction_ = NULL;
    }
    leveldb::Status PutBlobInfoIfNeeded(
        int64 database_id,
        int64 object_store_id,
        const std::string& object_store_data_key,
        std::vector<IndexedDBBlobInfo>*,
        ScopedVector<webkit_blob::BlobDataHandle>* handles);
    void PutBlobInfo(int64 database_id,
                     int64 object_store_id,
                     const std::string& object_store_data_key,
                     std::vector<IndexedDBBlobInfo>*,
                     ScopedVector<webkit_blob::BlobDataHandle>* handles);

    LevelDBTransaction* transaction() { return transaction_; }

    leveldb::Status GetBlobInfoForRecord(
        int64 database_id,
        const std::string& object_store_data_key,
        IndexedDBValue* value);

    // This holds a BlobEntryKey and the encoded IndexedDBBlobInfo vector stored
    // under that key.
    typedef std::vector<std::pair<BlobEntryKey, std::string> >
        BlobEntryKeyValuePairVec;

    class WriteDescriptor {
     public:
      WriteDescriptor(const GURL& url, int64_t key, int64_t size);
      WriteDescriptor(const base::FilePath& path,
                      int64_t key,
                      int64_t size,
                      base::Time last_modified);

      bool is_file() const { return is_file_; }
      const GURL& url() const {
        DCHECK(!is_file_);
        return url_;
      }
      const base::FilePath& file_path() const {
        DCHECK(is_file_);
        return file_path_;
      }
      int64_t key() const { return key_; }
      int64_t size() const { return size_; }
      base::Time last_modified() const { return last_modified_; }

     private:
      bool is_file_;
      GURL url_;
      base::FilePath file_path_;
      int64_t key_;
      int64_t size_;
      base::Time last_modified_;
    };

    class ChainedBlobWriter : public base::RefCounted<ChainedBlobWriter> {
     public:
      virtual void set_delegate(
          scoped_ptr<fileapi::FileWriterDelegate> delegate) = 0;

      // TODO(ericu): Add a reason in the event of failure.
      virtual void ReportWriteCompletion(bool succeeded,
                                         int64 bytes_written) = 0;

      virtual void Abort() = 0;

     protected:
      virtual ~ChainedBlobWriter() {}
      friend class base::RefCounted<ChainedBlobWriter>;
    };

    class ChainedBlobWriterImpl;

    typedef std::vector<WriteDescriptor> WriteDescriptorVec;

   private:
    class BlobWriteCallbackWrapper;

    leveldb::Status HandleBlobPreTransaction(
        BlobEntryKeyValuePairVec* new_blob_entries,
        WriteDescriptorVec* new_files_to_write);
    // Returns true on success, false on failure.
    bool CollectBlobFilesToRemove();
    // The callback will be called eventually on success or failure.
    void WriteNewBlobs(BlobEntryKeyValuePairVec& new_blob_entries,
                       WriteDescriptorVec& new_files_to_write,
                       scoped_refptr<BlobWriteCallback> callback);
    leveldb::Status SortBlobsToRemove();

    IndexedDBBackingStore* backing_store_;
    scoped_refptr<LevelDBTransaction> transaction_;
    BlobChangeMap blob_change_map_;
    BlobChangeMap incognito_blob_map_;
    int64 database_id_;
    BlobJournalType blobs_to_remove_;
    scoped_refptr<ChainedBlobWriter> chained_blob_writer_;
  };

 protected:
  IndexedDBBackingStore(IndexedDBFactory* indexed_db_factory,
                        const GURL& origin_url,
                        const base::FilePath& blob_path,
                        net::URLRequestContext* request_context,
                        scoped_ptr<LevelDBDatabase> db,
                        scoped_ptr<LevelDBComparator> comparator,
                        base::TaskRunner* task_runner);
  virtual ~IndexedDBBackingStore();
  friend class base::RefCounted<IndexedDBBackingStore>;

  bool is_incognito() const { return !indexed_db_factory_; }

  bool SetUpMetadata();

  virtual bool WriteBlobFile(
      int64 database_id,
      const Transaction::WriteDescriptor& descriptor,
      Transaction::ChainedBlobWriter* chained_blob_writer);
  virtual bool RemoveBlobFile(int64 database_id, int64 key);
  virtual void StartJournalCleaningTimer();
  void CleanPrimaryJournalIgnoreReturn();

 private:
  static scoped_refptr<IndexedDBBackingStore> Create(
      IndexedDBFactory* indexed_db_factory,
      const GURL& origin_url,
      const base::FilePath& blob_path,
      net::URLRequestContext* request_context,
      scoped_ptr<LevelDBDatabase> db,
      scoped_ptr<LevelDBComparator> comparator,
      base::TaskRunner* task_runner);

  static bool ReadCorruptionInfo(const base::FilePath& path_base,
                                 const GURL& origin_url,
                                 std::string& message);

  leveldb::Status FindKeyInIndex(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      int64 index_id,
      const IndexedDBKey& key,
      std::string* found_encoded_primary_key,
      bool* found);
  leveldb::Status GetIndexes(int64 database_id,
                             int64 object_store_id,
                             IndexedDBObjectStoreMetadata::IndexMap* map)
      WARN_UNUSED_RESULT;
  bool RemoveBlobDirectory(int64 database_id);
  leveldb::Status CleanUpBlobJournal(const std::string& level_db_key);

  IndexedDBFactory* indexed_db_factory_;
  const GURL origin_url_;
  base::FilePath blob_path_;

  // The origin identifier is a key prefix unique to the origin used in the
  // leveldb backing store to partition data by origin. It is a normalized
  // version of the origin URL with a versioning suffix appended, e.g.
  // "http_localhost_81@1" Since only one origin is stored per backing store
  // this is redundant but necessary for backwards compatibility; the suffix
  // provides for future flexibility.
  const std::string origin_identifier_;

  net::URLRequestContext* request_context_;
  base::TaskRunner* task_runner_;
  std::set<int> child_process_ids_granted_;
  BlobChangeMap incognito_blob_map_;
  base::OneShotTimer<IndexedDBBackingStore> journal_cleaning_timer_;

  scoped_ptr<LevelDBDatabase> db_;
  scoped_ptr<LevelDBComparator> comparator_;
  // Whenever blobs are registered in active_blob_registry_, indexed_db_factory_
  // will hold a reference to this backing store.
  IndexedDBActiveBlobRegistry active_blob_registry_;
  base::OneShotTimer<IndexedDBBackingStore> close_timer_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBBackingStore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BACKING_STORE_H_
