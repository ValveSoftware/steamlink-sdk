// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PUBLIC_CPP_UTIL_H_
#define COMPONENTS_LEVELDB_PUBLIC_CPP_UTIL_H_

#include "components/leveldb/public/interfaces/leveldb.mojom.h"

namespace leveldb {

class Slice;
class Status;

// Builds a mojo mojom::DatabaseError from a leveldb::Status object.
mojom::DatabaseError LeveldbStatusToError(const leveldb::Status& s);

// Creates a leveldb Status object form a database error and two optional
// messages. A mojoification of the various static leveldb::Status
// constructors.
leveldb::Status DatabaseErrorToStatus(mojom::DatabaseError e,
                                      const Slice& msg,
                                      const Slice& msg2);

// Builds a Slice pointing to the data inside |a|. This is not a type-converter
// as it is not a copy operation; the returned Slice points into |a| and must
// outlive |a|.
leveldb::Slice GetSliceFor(const mojo::Array<uint8_t>& a);

// Copies the data that |s| points to into a mojo::Array.
mojo::Array<uint8_t> GetArrayFor(const leveldb::Slice& s);

}  // namespace leveldb

#endif  // COMPONENTS_LEVELDB_PUBLIC_CPP_UTIL_H_
