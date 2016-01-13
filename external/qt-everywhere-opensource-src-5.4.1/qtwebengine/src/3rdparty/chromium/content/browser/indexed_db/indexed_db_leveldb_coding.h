// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_CODING_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_CODING_H_

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_path.h"

namespace content {

CONTENT_EXPORT extern const unsigned char kMinimumIndexId;

CONTENT_EXPORT std::string MaxIDBKey();
CONTENT_EXPORT std::string MinIDBKey();

// DatabaseId, BlobKey
typedef std::pair<int64_t, int64_t> BlobJournalEntryType;
typedef std::vector<BlobJournalEntryType> BlobJournalType;

CONTENT_EXPORT void EncodeByte(unsigned char value, std::string* into);
CONTENT_EXPORT void EncodeBool(bool value, std::string* into);
CONTENT_EXPORT void EncodeInt(int64 value, std::string* into);
CONTENT_EXPORT void EncodeVarInt(int64 value, std::string* into);
CONTENT_EXPORT void EncodeString(const base::string16& value,
                                 std::string* into);
CONTENT_EXPORT void EncodeStringWithLength(const base::string16& value,
                                           std::string* into);
CONTENT_EXPORT void EncodeBinary(const std::string& value, std::string* into);
CONTENT_EXPORT void EncodeDouble(double value, std::string* into);
CONTENT_EXPORT void EncodeIDBKey(const IndexedDBKey& value, std::string* into);
CONTENT_EXPORT void EncodeIDBKeyPath(const IndexedDBKeyPath& value,
                                     std::string* into);
CONTENT_EXPORT void EncodeBlobJournal(const BlobJournalType& journal,
                                      std::string* into);

CONTENT_EXPORT WARN_UNUSED_RESULT bool DecodeByte(base::StringPiece* slice,
                                                  unsigned char* value);
CONTENT_EXPORT WARN_UNUSED_RESULT bool DecodeBool(base::StringPiece* slice,
                                                  bool* value);
CONTENT_EXPORT WARN_UNUSED_RESULT bool DecodeInt(base::StringPiece* slice,
                                                 int64* value);
CONTENT_EXPORT WARN_UNUSED_RESULT bool DecodeVarInt(base::StringPiece* slice,
                                                    int64* value);
CONTENT_EXPORT WARN_UNUSED_RESULT bool DecodeString(base::StringPiece* slice,
                                                    base::string16* value);
CONTENT_EXPORT WARN_UNUSED_RESULT bool DecodeStringWithLength(
    base::StringPiece* slice,
    base::string16* value);
CONTENT_EXPORT WARN_UNUSED_RESULT bool DecodeBinary(base::StringPiece* slice,
                                                    std::string* value);
CONTENT_EXPORT WARN_UNUSED_RESULT bool DecodeDouble(base::StringPiece* slice,
                                                    double* value);
CONTENT_EXPORT WARN_UNUSED_RESULT bool DecodeIDBKey(
    base::StringPiece* slice,
    scoped_ptr<IndexedDBKey>* value);
CONTENT_EXPORT WARN_UNUSED_RESULT bool DecodeIDBKeyPath(
    base::StringPiece* slice,
    IndexedDBKeyPath* value);
CONTENT_EXPORT WARN_UNUSED_RESULT bool DecodeBlobJournal(
    base::StringPiece* slice,
    BlobJournalType* journal);

CONTENT_EXPORT int CompareEncodedStringsWithLength(base::StringPiece* slice1,
                                                   base::StringPiece* slice2,
                                                   bool* ok);

CONTENT_EXPORT WARN_UNUSED_RESULT bool ExtractEncodedIDBKey(
    base::StringPiece* slice,
    std::string* result);

CONTENT_EXPORT int CompareEncodedIDBKeys(base::StringPiece* slice1,
                                         base::StringPiece* slice2,
                                         bool* ok);

CONTENT_EXPORT int Compare(const base::StringPiece& a,
                           const base::StringPiece& b,
                           bool index_keys);

class KeyPrefix {
 public:
  KeyPrefix();
  explicit KeyPrefix(int64 database_id);
  KeyPrefix(int64 database_id, int64 object_store_id);
  KeyPrefix(int64 database_id, int64 object_store_id, int64 index_id);
  static KeyPrefix CreateWithSpecialIndex(int64 database_id,
                                          int64 object_store_id,
                                          int64 index_id);

  static bool Decode(base::StringPiece* slice, KeyPrefix* result);
  std::string Encode() const;
  static std::string EncodeEmpty();
  int Compare(const KeyPrefix& other) const;

  // These are serialized to disk; any new items must be appended, and none can
  // be deleted.
  enum Type {
    GLOBAL_METADATA,
    DATABASE_METADATA,
    OBJECT_STORE_DATA,
    EXISTS_ENTRY,
    INDEX_DATA,
    INVALID_TYPE,
    BLOB_ENTRY
  };

  static const size_t kMaxDatabaseIdSizeBits = 3;
  static const size_t kMaxObjectStoreIdSizeBits = 3;
  static const size_t kMaxIndexIdSizeBits = 2;

  static const size_t kMaxDatabaseIdSizeBytes =
      1ULL << kMaxDatabaseIdSizeBits;  // 8
  static const size_t kMaxObjectStoreIdSizeBytes =
      1ULL << kMaxObjectStoreIdSizeBits;                                   // 8
  static const size_t kMaxIndexIdSizeBytes = 1ULL << kMaxIndexIdSizeBits;  // 4

  static const size_t kMaxDatabaseIdBits =
      kMaxDatabaseIdSizeBytes * 8 - 1;  // 63
  static const size_t kMaxObjectStoreIdBits =
      kMaxObjectStoreIdSizeBytes * 8 - 1;                              // 63
  static const size_t kMaxIndexIdBits = kMaxIndexIdSizeBytes * 8 - 1;  // 31

  static const int64 kMaxDatabaseId =
      (1ULL << kMaxDatabaseIdBits) - 1;  // max signed int64
  static const int64 kMaxObjectStoreId =
      (1ULL << kMaxObjectStoreIdBits) - 1;  // max signed int64
  static const int64 kMaxIndexId =
      (1ULL << kMaxIndexIdBits) - 1;  // max signed int32

  CONTENT_EXPORT static bool IsValidDatabaseId(int64 database_id);
  static bool IsValidObjectStoreId(int64 index_id);
  static bool IsValidIndexId(int64 index_id);
  static bool ValidIds(int64 database_id,
                       int64 object_store_id,
                       int64 index_id) {
    return IsValidDatabaseId(database_id) &&
           IsValidObjectStoreId(object_store_id) && IsValidIndexId(index_id);
  }
  static bool ValidIds(int64 database_id, int64 object_store_id) {
    return IsValidDatabaseId(database_id) &&
           IsValidObjectStoreId(object_store_id);
  }

  Type type() const;

  int64 database_id_;
  int64 object_store_id_;
  int64 index_id_;

  static const int64 kInvalidId = -1;

 private:
  static std::string EncodeInternal(int64 database_id,
                                    int64 object_store_id,
                                    int64 index_id);
  // Special constructor for CreateWithSpecialIndex()
  KeyPrefix(enum Type,
            int64 database_id,
            int64 object_store_id,
            int64 index_id);
};

class SchemaVersionKey {
 public:
  CONTENT_EXPORT static std::string Encode();
};

class MaxDatabaseIdKey {
 public:
  CONTENT_EXPORT static std::string Encode();
};

class DataVersionKey {
 public:
  static std::string Encode();
};

class BlobJournalKey {
 public:
  static std::string Encode();
};

class LiveBlobJournalKey {
 public:
  static std::string Encode();
};

class DatabaseFreeListKey {
 public:
  DatabaseFreeListKey();
  static bool Decode(base::StringPiece* slice, DatabaseFreeListKey* result);
  CONTENT_EXPORT static std::string Encode(int64 database_id);
  static CONTENT_EXPORT std::string EncodeMaxKey();
  int64 DatabaseId() const;
  int Compare(const DatabaseFreeListKey& other) const;

 private:
  int64 database_id_;
};

class DatabaseNameKey {
 public:
  static bool Decode(base::StringPiece* slice, DatabaseNameKey* result);
  CONTENT_EXPORT static std::string Encode(const std::string& origin_identifier,
                                           const base::string16& database_name);
  static std::string EncodeMinKeyForOrigin(
      const std::string& origin_identifier);
  static std::string EncodeStopKeyForOrigin(
      const std::string& origin_identifier);
  base::string16 origin() const { return origin_; }
  base::string16 database_name() const { return database_name_; }
  int Compare(const DatabaseNameKey& other);

 private:
  base::string16 origin_;  // TODO(jsbell): Store encoded strings, or just
                           // pointers.
  base::string16 database_name_;
};

class DatabaseMetaDataKey {
 public:
  enum MetaDataType {
    ORIGIN_NAME = 0,
    DATABASE_NAME = 1,
    USER_VERSION = 2,
    MAX_OBJECT_STORE_ID = 3,
    USER_INT_VERSION = 4,
    BLOB_KEY_GENERATOR_CURRENT_NUMBER = 5,
    MAX_SIMPLE_METADATA_TYPE = 6
  };

  CONTENT_EXPORT static const int64 kAllBlobsKey;
  static const int64 kBlobKeyGeneratorInitialNumber;
  // All keys <= 0 are invalid.  This one's just a convenient example.
  static const int64 kInvalidBlobKey;

  static bool IsValidBlobKey(int64 blobKey);
  CONTENT_EXPORT static std::string Encode(int64 database_id,
                                           MetaDataType type);
};

class ObjectStoreMetaDataKey {
 public:
  enum MetaDataType {
    NAME = 0,
    KEY_PATH = 1,
    AUTO_INCREMENT = 2,
    EVICTABLE = 3,
    LAST_VERSION = 4,
    MAX_INDEX_ID = 5,
    HAS_KEY_PATH = 6,
    KEY_GENERATOR_CURRENT_NUMBER = 7
  };

  ObjectStoreMetaDataKey();
  static bool Decode(base::StringPiece* slice, ObjectStoreMetaDataKey* result);
  CONTENT_EXPORT static std::string Encode(int64 database_id,
                                           int64 object_store_id,
                                           unsigned char meta_data_type);
  CONTENT_EXPORT static std::string EncodeMaxKey(int64 database_id);
  CONTENT_EXPORT static std::string EncodeMaxKey(int64 database_id,
                                                 int64 object_store_id);
  int64 ObjectStoreId() const;
  unsigned char MetaDataType() const;
  int Compare(const ObjectStoreMetaDataKey& other);

 private:
  int64 object_store_id_;
  unsigned char meta_data_type_;
};

class IndexMetaDataKey {
 public:
  enum MetaDataType {
    NAME = 0,
    UNIQUE = 1,
    KEY_PATH = 2,
    MULTI_ENTRY = 3
  };

  IndexMetaDataKey();
  static bool Decode(base::StringPiece* slice, IndexMetaDataKey* result);
  CONTENT_EXPORT static std::string Encode(int64 database_id,
                                           int64 object_store_id,
                                           int64 index_id,
                                           unsigned char meta_data_type);
  CONTENT_EXPORT static std::string EncodeMaxKey(int64 database_id,
                                                 int64 object_store_id);
  CONTENT_EXPORT static std::string EncodeMaxKey(int64 database_id,
                                                 int64 object_store_id,
                                                 int64 index_id);
  int Compare(const IndexMetaDataKey& other);
  int64 IndexId() const;
  unsigned char meta_data_type() const { return meta_data_type_; }

 private:
  int64 object_store_id_;
  int64 index_id_;
  unsigned char meta_data_type_;
};

class ObjectStoreFreeListKey {
 public:
  ObjectStoreFreeListKey();
  static bool Decode(base::StringPiece* slice, ObjectStoreFreeListKey* result);
  CONTENT_EXPORT static std::string Encode(int64 database_id,
                                           int64 object_store_id);
  CONTENT_EXPORT static std::string EncodeMaxKey(int64 database_id);
  int64 ObjectStoreId() const;
  int Compare(const ObjectStoreFreeListKey& other);

 private:
  int64 object_store_id_;
};

class IndexFreeListKey {
 public:
  IndexFreeListKey();
  static bool Decode(base::StringPiece* slice, IndexFreeListKey* result);
  CONTENT_EXPORT static std::string Encode(int64 database_id,
                                           int64 object_store_id,
                                           int64 index_id);
  CONTENT_EXPORT static std::string EncodeMaxKey(int64 database_id,
                                                 int64 object_store_id);
  int Compare(const IndexFreeListKey& other);
  int64 ObjectStoreId() const;
  int64 IndexId() const;

 private:
  int64 object_store_id_;
  int64 index_id_;
};

class ObjectStoreNamesKey {
 public:
  // TODO(jsbell): We never use this to look up object store ids,
  // because a mapping is kept in the IndexedDBDatabase. Can the
  // mapping become unreliable?  Can we remove this?
  static bool Decode(base::StringPiece* slice, ObjectStoreNamesKey* result);
  CONTENT_EXPORT static std::string Encode(
      int64 database_id,
      const base::string16& object_store_name);
  int Compare(const ObjectStoreNamesKey& other);
  base::string16 object_store_name() const { return object_store_name_; }

 private:
  // TODO(jsbell): Store the encoded string, or just pointers to it.
  base::string16 object_store_name_;
};

class IndexNamesKey {
 public:
  IndexNamesKey();
  // TODO(jsbell): We never use this to look up index ids, because a mapping
  // is kept at a higher level.
  static bool Decode(base::StringPiece* slice, IndexNamesKey* result);
  CONTENT_EXPORT static std::string Encode(int64 database_id,
                                           int64 object_store_id,
                                           const base::string16& index_name);
  int Compare(const IndexNamesKey& other);
  base::string16 index_name() const { return index_name_; }

 private:
  int64 object_store_id_;
  base::string16 index_name_;
};

class ObjectStoreDataKey {
 public:
  static bool Decode(base::StringPiece* slice, ObjectStoreDataKey* result);
  CONTENT_EXPORT static std::string Encode(int64 database_id,
                                           int64 object_store_id,
                                           const std::string encoded_user_key);
  static std::string Encode(int64 database_id,
                            int64 object_store_id,
                            const IndexedDBKey& user_key);
  scoped_ptr<IndexedDBKey> user_key() const;
  static const int64 kSpecialIndexNumber;
  ObjectStoreDataKey();
  ~ObjectStoreDataKey();

 private:
  std::string encoded_user_key_;
};

class ExistsEntryKey {
 public:
  ExistsEntryKey();
  ~ExistsEntryKey();

  static bool Decode(base::StringPiece* slice, ExistsEntryKey* result);
  CONTENT_EXPORT static std::string Encode(int64 database_id,
                                           int64 object_store_id,
                                           const std::string& encoded_key);
  static std::string Encode(int64 database_id,
                            int64 object_store_id,
                            const IndexedDBKey& user_key);
  scoped_ptr<IndexedDBKey> user_key() const;

  static const int64 kSpecialIndexNumber;

 private:
  std::string encoded_user_key_;
  DISALLOW_COPY_AND_ASSIGN(ExistsEntryKey);
};

class BlobEntryKey {
 public:
  BlobEntryKey() : database_id_(0), object_store_id_(0) {}
  static bool Decode(base::StringPiece* slice, BlobEntryKey* result);
  static bool FromObjectStoreDataKey(base::StringPiece* slice,
                                     BlobEntryKey* result);
  static std::string ReencodeToObjectStoreDataKey(base::StringPiece* slice);
  static std::string EncodeMinKeyForObjectStore(int64 database_id,
                                                int64 object_store_id);
  static std::string EncodeStopKeyForObjectStore(int64 database_id,
                                                 int64 object_store_id);
  static std::string Encode(int64 database_id,
                            int64 object_store_id,
                            const IndexedDBKey& user_key);
  std::string Encode() const;
  int64 database_id() const { return database_id_; }
  int64 object_store_id() const { return object_store_id_; }

  static const int64 kSpecialIndexNumber;

 private:
  static std::string Encode(int64 database_id,
                            int64 object_store_id,
                            const std::string& encoded_user_key);
  int64 database_id_;
  int64 object_store_id_;
  // This is the user's ObjectStoreDataKey, not the BlobEntryKey itself.
  std::string encoded_user_key_;
};

class IndexDataKey {
 public:
  IndexDataKey();
  ~IndexDataKey();
  static bool Decode(base::StringPiece* slice, IndexDataKey* result);
  CONTENT_EXPORT static std::string Encode(
      int64 database_id,
      int64 object_store_id,
      int64 index_id,
      const std::string& encoded_user_key,
      const std::string& encoded_primary_key,
      int64 sequence_number);
  static std::string Encode(int64 database_id,
                            int64 object_store_id,
                            int64 index_id,
                            const IndexedDBKey& user_key);
  static std::string Encode(int64 database_id,
                            int64 object_store_id,
                            int64 index_id,
                            const IndexedDBKey& user_key,
                            const IndexedDBKey& user_primary_key);
  static std::string EncodeMinKey(int64 database_id,
                                  int64 object_store_id,
                                  int64 index_id);
  CONTENT_EXPORT static std::string EncodeMaxKey(int64 database_id,
                                                 int64 object_store_id,
                                                 int64 index_id);
  int64 DatabaseId() const;
  int64 ObjectStoreId() const;
  int64 IndexId() const;
  scoped_ptr<IndexedDBKey> user_key() const;
  scoped_ptr<IndexedDBKey> primary_key() const;

 private:
  int64 database_id_;
  int64 object_store_id_;
  int64 index_id_;
  std::string encoded_user_key_;
  std::string encoded_primary_key_;
  int64 sequence_number_;

  DISALLOW_COPY_AND_ASSIGN(IndexDataKey);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_CODING_H_
