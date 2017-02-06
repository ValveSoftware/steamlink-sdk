// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/leveldb/leveldb_iterator_impl.h"

#include <memory>
#include <utility>

#include "base/logging.h"

static leveldb::Slice MakeSlice(const base::StringPiece& s) {
  return leveldb::Slice(s.begin(), s.size());
}

static base::StringPiece MakeStringPiece(const leveldb::Slice& s) {
  return base::StringPiece(s.data(), s.size());
}

namespace content {

LevelDBIteratorImpl::~LevelDBIteratorImpl() {
}

LevelDBIteratorImpl::LevelDBIteratorImpl(std::unique_ptr<leveldb::Iterator> it)
    : iterator_(std::move(it)) {}

void LevelDBIteratorImpl::CheckStatus() {
  const leveldb::Status& s = iterator_->status();
  if (!s.ok())
    LOG(ERROR) << "LevelDB iterator error: " << s.ToString();
}

bool LevelDBIteratorImpl::IsValid() const {
  return iterator_->Valid();
}

leveldb::Status LevelDBIteratorImpl::SeekToLast() {
  iterator_->SeekToLast();
  CheckStatus();
  return iterator_->status();
}

leveldb::Status LevelDBIteratorImpl::Seek(const base::StringPiece& target) {
  iterator_->Seek(MakeSlice(target));
  CheckStatus();
  return iterator_->status();
}

leveldb::Status LevelDBIteratorImpl::Next() {
  DCHECK(IsValid());
  iterator_->Next();
  CheckStatus();
  return iterator_->status();
}

leveldb::Status LevelDBIteratorImpl::Prev() {
  DCHECK(IsValid());
  iterator_->Prev();
  CheckStatus();
  return iterator_->status();
}

base::StringPiece LevelDBIteratorImpl::Key() const {
  DCHECK(IsValid());
  return MakeStringPiece(iterator_->key());
}

base::StringPiece LevelDBIteratorImpl::Value() const {
  DCHECK(IsValid());
  return MakeStringPiece(iterator_->value());
}

}  // namespace content
