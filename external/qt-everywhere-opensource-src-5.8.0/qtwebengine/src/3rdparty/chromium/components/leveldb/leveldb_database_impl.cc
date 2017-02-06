// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb/leveldb_database_impl.h"

#include <map>
#include <string>

#include "base/rand_util.h"
#include "components/leveldb/env_mojo.h"
#include "components/leveldb/public/cpp/util.h"
#include "mojo/common/common_type_converters.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace leveldb {
namespace {

template <typename T>
uint64_t GetSafeRandomId(const std::map<uint64_t, T>& m) {
  // Associate a random unsigned 64 bit handle to |s|, checking for the highly
  // improbable id duplication or castability to null.
  uint64_t new_id = base::RandUint64();
  while ((new_id == 0) || (m.find(new_id) != m.end()))
    new_id = base::RandUint64();

  return new_id;
}

template <typename FunctionType>
leveldb::Status ForEachWithPrefix(leveldb::DB* db,
                                  const leveldb::Slice& key_prefix,
                                  FunctionType function) {
  std::unique_ptr<leveldb::Iterator> it(
      db->NewIterator(leveldb::ReadOptions()));
  it->Seek(key_prefix);
  for (; it->Valid(); it->Next()) {
    if (!it->key().starts_with(key_prefix))
      break;
    function(it->key(), it->value());
  }
  return it->status();
}

}  // namespace

LevelDBDatabaseImpl::LevelDBDatabaseImpl(
    leveldb::mojom::LevelDBDatabaseRequest request,
    std::unique_ptr<leveldb::Env> environment,
    std::unique_ptr<leveldb::DB> db)
    : binding_(this, std::move(request)),
      environment_(std::move(environment)),
      db_(std::move(db)) {}

LevelDBDatabaseImpl::~LevelDBDatabaseImpl() {
  for (auto& p : iterator_map_)
    delete p.second;
  for (auto& p : snapshot_map_)
    db_->ReleaseSnapshot(p.second);
}

void LevelDBDatabaseImpl::Put(mojo::Array<uint8_t> key,
                              mojo::Array<uint8_t> value,
                              const PutCallback& callback) {
  leveldb::Status status =
      db_->Put(leveldb::WriteOptions(), GetSliceFor(key), GetSliceFor(value));
  callback.Run(LeveldbStatusToError(status));
}

void LevelDBDatabaseImpl::Delete(mojo::Array<uint8_t> key,
                                 const DeleteCallback& callback) {
  leveldb::Status status =
      db_->Delete(leveldb::WriteOptions(), GetSliceFor(key));
  callback.Run(LeveldbStatusToError(status));
}

void LevelDBDatabaseImpl::DeletePrefixed(
    mojo::Array<uint8_t> key_prefix,
    const DeletePrefixedCallback& callback) {
  leveldb::WriteBatch batch;
  leveldb::Status status = DeletePrefixedHelper(
      GetSliceFor(key_prefix), &batch);
  if (status.ok())
    status = db_->Write(leveldb::WriteOptions(), &batch);
  callback.Run(LeveldbStatusToError(status));
}

void LevelDBDatabaseImpl::Write(
    mojo::Array<mojom::BatchedOperationPtr> operations,
    const WriteCallback& callback) {
  leveldb::WriteBatch batch;

  for (size_t i = 0; i < operations.size(); ++i) {
    switch (operations[i]->type) {
      case mojom::BatchOperationType::PUT_KEY: {
        batch.Put(GetSliceFor(operations[i]->key),
                  GetSliceFor(operations[i]->value));
        break;
      }
      case mojom::BatchOperationType::DELETE_KEY: {
        batch.Delete(GetSliceFor(operations[i]->key));
        break;
      }
      case mojom::BatchOperationType::DELETE_PREFIXED_KEY: {
        DeletePrefixedHelper(GetSliceFor(operations[i]->key), &batch);
        break;
      }
    }
  }

  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  callback.Run(LeveldbStatusToError(status));
}

void LevelDBDatabaseImpl::Get(mojo::Array<uint8_t> key,
                              const GetCallback& callback) {
  std::string value;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), GetSliceFor(key), &value);
  callback.Run(LeveldbStatusToError(status), mojo::Array<uint8_t>::From(value));
}

void LevelDBDatabaseImpl::GetPrefixed(mojo::Array<uint8_t> key_prefix,
                                      const GetPrefixedCallback& callback) {
  mojo::Array<mojom::KeyValuePtr> data;
  leveldb::Status status = ForEachWithPrefix(
      db_.get(), GetSliceFor(key_prefix),
      [&data](const leveldb::Slice& key, const leveldb::Slice& value) {
        mojom::KeyValuePtr kv = mojom::KeyValue::New();
        kv->key = GetArrayFor(key);
        kv->value = GetArrayFor(value);
        data.push_back(std::move(kv));
      });
  callback.Run(LeveldbStatusToError(status), std::move(data));
}

void LevelDBDatabaseImpl::GetSnapshot(const GetSnapshotCallback& callback) {
  const Snapshot* s = db_->GetSnapshot();
  uint64_t new_id = GetSafeRandomId(snapshot_map_);
  snapshot_map_.insert(std::make_pair(new_id, s));
  callback.Run(new_id);
}

void LevelDBDatabaseImpl::ReleaseSnapshot(uint64_t snapshot_id) {
  auto it = snapshot_map_.find(snapshot_id);
  if (it != snapshot_map_.end()) {
    db_->ReleaseSnapshot(it->second);
    snapshot_map_.erase(it);
  }
}

void LevelDBDatabaseImpl::GetFromSnapshot(uint64_t snapshot_id,
                                          mojo::Array<uint8_t> key,
                                          const GetCallback& callback) {
  // If the snapshot id is invalid, send back invalid argument
  auto it = snapshot_map_.find(snapshot_id);
  if (it == snapshot_map_.end()) {
    callback.Run(mojom::DatabaseError::INVALID_ARGUMENT,
                 mojo::Array<uint8_t>());
    return;
  }

  std::string value;
  leveldb::ReadOptions options;
  options.snapshot = it->second;
  leveldb::Status status = db_->Get(options, GetSliceFor(key), &value);
  callback.Run(LeveldbStatusToError(status), mojo::Array<uint8_t>::From(value));
}

void LevelDBDatabaseImpl::NewIterator(const NewIteratorCallback& callback) {
  Iterator* iterator = db_->NewIterator(leveldb::ReadOptions());
  uint64_t new_id = GetSafeRandomId(iterator_map_);
  iterator_map_.insert(std::make_pair(new_id, iterator));
  callback.Run(new_id);
}

void LevelDBDatabaseImpl::NewIteratorFromSnapshot(
    uint64_t snapshot_id,
    const NewIteratorCallback& callback) {
  // If the snapshot id is invalid, send back invalid argument
  auto it = snapshot_map_.find(snapshot_id);
  if (it == snapshot_map_.end()) {
    callback.Run(0);
    return;
  }

  leveldb::ReadOptions options;
  options.snapshot = it->second;

  Iterator* iterator = db_->NewIterator(options);
  uint64_t new_id = GetSafeRandomId(iterator_map_);
  iterator_map_.insert(std::make_pair(new_id, iterator));
  callback.Run(new_id);
}

void LevelDBDatabaseImpl::ReleaseIterator(uint64_t iterator_id) {
  auto it = iterator_map_.find(iterator_id);
  if (it != iterator_map_.end()) {
    delete it->second;
    iterator_map_.erase(it);
  }
}

void LevelDBDatabaseImpl::IteratorSeekToFirst(
    uint64_t iterator_id,
    const IteratorSeekToFirstCallback& callback) {
  auto it = iterator_map_.find(iterator_id);
  if (it == iterator_map_.end()) {
    callback.Run(false, mojom::DatabaseError::INVALID_ARGUMENT, nullptr,
                 nullptr);
    return;
  }

  it->second->SeekToFirst();

  ReplyToIteratorMessage(it->second, callback);
}

void LevelDBDatabaseImpl::IteratorSeekToLast(
    uint64_t iterator_id,
    const IteratorSeekToLastCallback& callback) {
  auto it = iterator_map_.find(iterator_id);
  if (it == iterator_map_.end()) {
    callback.Run(false, mojom::DatabaseError::INVALID_ARGUMENT, nullptr,
                 nullptr);
    return;
  }

  it->second->SeekToLast();

  ReplyToIteratorMessage(it->second, callback);
}

void LevelDBDatabaseImpl::IteratorSeek(
    uint64_t iterator_id,
    mojo::Array<uint8_t> target,
    const IteratorSeekToLastCallback& callback) {
  auto it = iterator_map_.find(iterator_id);
  if (it == iterator_map_.end()) {
    callback.Run(false, mojom::DatabaseError::INVALID_ARGUMENT, nullptr,
                 nullptr);
    return;
  }

  it->second->Seek(GetSliceFor(target));

  ReplyToIteratorMessage(it->second, callback);
}

void LevelDBDatabaseImpl::IteratorNext(uint64_t iterator_id,
                                       const IteratorNextCallback& callback) {
  auto it = iterator_map_.find(iterator_id);
  if (it == iterator_map_.end()) {
    callback.Run(false, mojom::DatabaseError::INVALID_ARGUMENT, nullptr,
                 nullptr);
    return;
  }

  it->second->Next();

  ReplyToIteratorMessage(it->second, callback);
}

void LevelDBDatabaseImpl::IteratorPrev(uint64_t iterator_id,
                                       const IteratorPrevCallback& callback) {
  auto it = iterator_map_.find(iterator_id);
  if (it == iterator_map_.end()) {
    callback.Run(false, mojom::DatabaseError::INVALID_ARGUMENT, nullptr,
                 nullptr);
    return;
  }

  it->second->Prev();

  ReplyToIteratorMessage(it->second, callback);
}

void LevelDBDatabaseImpl::ReplyToIteratorMessage(
    leveldb::Iterator* it,
    const IteratorSeekToFirstCallback& callback) {
  if (!it->Valid()) {
    callback.Run(false, LeveldbStatusToError(it->status()), nullptr, nullptr);
    return;
  }

  callback.Run(true, LeveldbStatusToError(it->status()), GetArrayFor(it->key()),
               GetArrayFor(it->value()));
}

leveldb::Status LevelDBDatabaseImpl::DeletePrefixedHelper(
    const leveldb::Slice& key_prefix,
    leveldb::WriteBatch* batch) {
  leveldb::Status status = ForEachWithPrefix(db_.get(), key_prefix,
      [batch](const leveldb::Slice& key, const leveldb::Slice& value) {
        batch->Delete(key);
      });
  return status;
}

}  // namespace leveldb
