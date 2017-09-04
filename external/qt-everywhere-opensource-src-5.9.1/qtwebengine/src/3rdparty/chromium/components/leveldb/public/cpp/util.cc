// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb/public/cpp/util.h"

#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace leveldb {

mojom::DatabaseError LeveldbStatusToError(const leveldb::Status& s) {
  if (s.ok())
    return mojom::DatabaseError::OK;
  if (s.IsNotFound())
    return mojom::DatabaseError::NOT_FOUND;
  if (s.IsCorruption())
    return mojom::DatabaseError::CORRUPTION;
  if (s.IsNotSupportedError())
    return mojom::DatabaseError::NOT_SUPPORTED;
  if (s.IsIOError())
    return mojom::DatabaseError::IO_ERROR;
  return mojom::DatabaseError::INVALID_ARGUMENT;
}

leveldb::Status DatabaseErrorToStatus(mojom::DatabaseError e,
                                      const Slice& msg,
                                      const Slice& msg2) {
  switch (e) {
    case mojom::DatabaseError::OK:
      return leveldb::Status::OK();
    case mojom::DatabaseError::NOT_FOUND:
      return leveldb::Status::NotFound(msg, msg2);
    case mojom::DatabaseError::CORRUPTION:
      return leveldb::Status::Corruption(msg, msg2);
    case mojom::DatabaseError::NOT_SUPPORTED:
      return leveldb::Status::NotSupported(msg, msg2);
    case mojom::DatabaseError::INVALID_ARGUMENT:
      return leveldb::Status::InvalidArgument(msg, msg2);
    case mojom::DatabaseError::IO_ERROR:
      return leveldb::Status::IOError(msg, msg2);
  }

  // This will never be reached, but we still have configurations which don't
  // do switch enum checking.
  return leveldb::Status::InvalidArgument(msg, msg2);
}

leveldb::Slice GetSliceFor(const std::vector<uint8_t>& key) {
  if (key.size() == 0)
    return leveldb::Slice();
  return leveldb::Slice(reinterpret_cast<const char*>(&key.front()),
                        key.size());
}

std::vector<uint8_t> GetVectorFor(const leveldb::Slice& s) {
  if (s.size() == 0)
    return std::vector<uint8_t>();
  return std::vector<uint8_t>(
      reinterpret_cast<const uint8_t*>(s.data()),
      reinterpret_cast<const uint8_t*>(s.data() + s.size()));
}

std::string Uint8VectorToStdString(const std::vector<uint8_t>& input) {
  return std::string(reinterpret_cast<const char*>(input.data()), input.size());
}

std::vector<uint8_t> StdStringToUint8Vector(const std::string& input) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(input.data());
  return std::vector<uint8_t>(data, data + input.size());
}

}  // namespace leveldb
