// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb/public/cpp/remote_iterator.h"

#include "components/leveldb/public/cpp/util.h"

namespace leveldb {

RemoteIterator::RemoteIterator(mojom::LevelDBDatabase* database,
                               uint64_t iterator_id)
    : database_(database),
      iterator_id_(iterator_id),
      valid_(false),
      status_(mojom::DatabaseError::OK) {}

RemoteIterator::~RemoteIterator() {
  database_->ReleaseIterator(iterator_id_);
}

bool RemoteIterator::Valid() const {
  return valid_;
}

void RemoteIterator::SeekToFirst() {
  database_->IteratorSeekToFirst(iterator_id_, &valid_, &status_, &key_,
                                 &value_);
}

void RemoteIterator::SeekToLast() {
  database_->IteratorSeekToLast(iterator_id_, &valid_, &status_, &key_,
                                &value_);
}

void RemoteIterator::Seek(const Slice& target) {
  database_->IteratorSeek(iterator_id_, GetArrayFor(target), &valid_, &status_,
                          &key_, &value_);
}

void RemoteIterator::Next() {
  database_->IteratorNext(iterator_id_, &valid_, &status_, &key_, &value_);
}

void RemoteIterator::Prev() {
  database_->IteratorPrev(iterator_id_, &valid_, &status_, &key_, &value_);
}

Slice RemoteIterator::key() const {
  return GetSliceFor(key_);
}

Slice RemoteIterator::value() const {
  return GetSliceFor(value_);
}

Status RemoteIterator::status() const {
  return DatabaseErrorToStatus(status_, GetSliceFor(key_), GetSliceFor(value_));
}

}  // namespace leveldb
