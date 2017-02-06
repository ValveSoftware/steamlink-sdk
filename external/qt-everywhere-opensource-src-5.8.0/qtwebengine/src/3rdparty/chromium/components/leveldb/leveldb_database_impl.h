// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_LEVELDB_DATABASE_IMPL_H_
#define COMPONENTS_LEVELDB_LEVELDB_DATABASE_IMPL_H_

#include <memory>

#include "components/leveldb/public/interfaces/leveldb.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace leveldb {

class MojoEnv;

// The backing to a database object that we pass to our called.
class LevelDBDatabaseImpl : public mojom::LevelDBDatabase {
 public:
  LevelDBDatabaseImpl(leveldb::mojom::LevelDBDatabaseRequest request,
                      std::unique_ptr<leveldb::Env> environment,
                      std::unique_ptr<leveldb::DB> db);
  ~LevelDBDatabaseImpl() override;

  // Overridden from LevelDBDatabase:
  void Put(mojo::Array<uint8_t> key,
           mojo::Array<uint8_t> value,
           const PutCallback& callback) override;
  void Delete(mojo::Array<uint8_t> key,
              const DeleteCallback& callback) override;
  void DeletePrefixed(mojo::Array<uint8_t> key_prefix,
                      const DeletePrefixedCallback& callback) override;
  void Write(mojo::Array<mojom::BatchedOperationPtr> operations,
             const WriteCallback& callback) override;
  void Get(mojo::Array<uint8_t> key, const GetCallback& callback) override;
  void GetPrefixed(mojo::Array<uint8_t> key_prefix,
                   const GetPrefixedCallback& callback) override;
  void GetSnapshot(const GetSnapshotCallback& callback) override;
  void ReleaseSnapshot(uint64_t snapshot_id) override;
  void GetFromSnapshot(uint64_t snapshot_id,
                       mojo::Array<uint8_t> key,
                       const GetCallback& callback) override;
  void NewIterator(const NewIteratorCallback& callback) override;
  void NewIteratorFromSnapshot(uint64_t snapshot_id,
                               const NewIteratorCallback& callback) override;
  void ReleaseIterator(uint64_t iterator_id) override;
  void IteratorSeekToFirst(
      uint64_t iterator_id,
      const IteratorSeekToFirstCallback& callback) override;
  void IteratorSeekToLast(uint64_t iterator_id,
                          const IteratorSeekToLastCallback& callback) override;
  void IteratorSeek(uint64_t iterator_id,
                    mojo::Array<uint8_t> target,
                    const IteratorSeekToLastCallback& callback) override;
  void IteratorNext(uint64_t iterator_id,
                    const IteratorNextCallback& callback) override;
  void IteratorPrev(uint64_t iterator_id,
                    const IteratorPrevCallback& callback) override;

 private:
  // Returns the state of |it| to a caller. Note: This assumes that all the
  // iterator movement methods have the same callback signature. We don't
  // directly reference the underlying type in case of bindings change.
  void ReplyToIteratorMessage(leveldb::Iterator* it,
                              const IteratorSeekToFirstCallback& callback);

  leveldb::Status DeletePrefixedHelper(const leveldb::Slice& key_prefix,
                                       leveldb::WriteBatch* batch);

  mojo::StrongBinding<mojom::LevelDBDatabase> binding_;
  std::unique_ptr<leveldb::Env> environment_;
  std::unique_ptr<leveldb::DB> db_;

  std::map<uint64_t, const Snapshot*> snapshot_map_;

  // TODO(erg): If we have an existing iterator which depends on a snapshot,
  // and delete the snapshot from the client side, that shouldn't delete the
  // snapshot maybe? At worse it's a DDoS if there's multiple users of the
  // system, but this maybe should be fixed...

  std::map<uint64_t, Iterator*> iterator_map_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBDatabaseImpl);
};

}  // namespace leveldb

#endif  // COMPONENTS_LEVELDB_LEVELDB_DATABASE_IMPL_H_
