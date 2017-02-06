// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PUBLIC_CPP_REMOTE_ITERATOR_H_
#define COMPONENTS_LEVELDB_PUBLIC_CPP_REMOTE_ITERATOR_H_

#include "components/leveldb/public/interfaces/leveldb.mojom.h"
#include "mojo/public/cpp/bindings/array.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"

namespace leveldb {

// A wrapper around the raw iterator movement methods on the mojo leveldb
// interface to allow drop in replacement to current leveldb usage.
//
// Note: Next(), Prev() and all the Seek*() calls cause mojo sync calls.
class RemoteIterator : public Iterator {
 public:
  RemoteIterator(mojom::LevelDBDatabase* database, uint64_t iterator_id);
  ~RemoteIterator() override;

  // Overridden from leveldb::Iterator:
  bool Valid() const override;
  void SeekToFirst() override;
  void SeekToLast() override;
  void Seek(const Slice& target) override;
  void Next() override;
  void Prev() override;
  Slice key() const override;
  Slice value() const override;
  Status status() const override;

 private:
  mojom::LevelDBDatabase* database_;
  uint64_t iterator_id_;

  bool valid_;
  mojom::DatabaseError status_;
  mojo::Array<uint8_t> key_;
  mojo::Array<uint8_t> value_;

  DISALLOW_COPY_AND_ASSIGN(RemoteIterator);
};

}  // namespace leveldb

#endif  // COMPONENTS_LEVELDB_PUBLIC_CPP_REMOTE_ITERATOR_H_
