// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_ITERATOR_H_
#define CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_ITERATOR_H_

#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace content {

class CONTENT_EXPORT LevelDBIterator {
 public:
  virtual ~LevelDBIterator() {}
  virtual bool IsValid() const = 0;
  virtual leveldb::Status SeekToLast() = 0;
  virtual leveldb::Status Seek(const base::StringPiece& target) = 0;
  virtual leveldb::Status Next() = 0;
  virtual leveldb::Status Prev() = 0;
  virtual base::StringPiece Key() const = 0;
  virtual base::StringPiece Value() const = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_ITERATOR_H_
