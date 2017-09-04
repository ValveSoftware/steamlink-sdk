// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/coded_value_serializer.h"

#include <stdint.h>

#include <string>

#include "base/logging.h"
#include "blimp/helium/version_vector.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"

namespace blimp {
namespace helium {
namespace {
// Large (and arbitrary) size for sanity check on string deserialization.
constexpr uint32_t kMaxStringLength = (10 << 20);  // 10MB
}

// -----------------------------
// int32_t serialization methods

void CodedValueSerializer::Serialize(
    const int32_t& value,
    google::protobuf::io::CodedOutputStream* output_stream) {
  output_stream->WriteVarint32(static_cast<uint32_t>(value));
}

bool CodedValueSerializer::Deserialize(
    google::protobuf::io::CodedInputStream* input_stream,
    int32_t* value) {
  return input_stream->ReadVarint32(reinterpret_cast<uint32_t*>(value));
}

// ---------------------------------
// std::string serialization methods

void CodedValueSerializer::Serialize(
    const std::string& value,
    google::protobuf::io::CodedOutputStream* output_stream) {
  DCHECK_LT(value.length(), kMaxStringLength);
  output_stream->WriteVarint32(value.length());
  output_stream->WriteString(value);
}

bool CodedValueSerializer::Deserialize(
    google::protobuf::io::CodedInputStream* input_stream,
    std::string* value) {
  uint32_t length;
  if (input_stream->ReadVarint32(&length) && length < kMaxStringLength) {
    return input_stream->ReadString(value, length);
  }
  return false;
}

// -----------------------------------
// VersionVector serialization methods

void CodedValueSerializer::Serialize(
    const VersionVector& value,
    google::protobuf::io::CodedOutputStream* output_stream) {
  output_stream->WriteVarint64(value.local_revision());
  output_stream->WriteVarint64(value.remote_revision());
}

bool CodedValueSerializer::Deserialize(
    google::protobuf::io::CodedInputStream* input_stream,
    VersionVector* value) {
  Revision local_revision;
  Revision remote_revision;
  if (!input_stream->ReadVarint64(&local_revision) ||
      !input_stream->ReadVarint64(&remote_revision)) {
    return false;
  }
  value->set_local_revision(local_revision);
  value->set_remote_revision(remote_revision);
  return true;
}

// -----------------------------------
// uint64_t serialization methods

void CodedValueSerializer::Serialize(
    const uint64_t& value,
    google::protobuf::io::CodedOutputStream* output_stream) {
  output_stream->WriteVarint64(value);
}

bool CodedValueSerializer::Deserialize(
    google::protobuf::io::CodedInputStream* input_stream,
    uint64_t* value) {
  return input_stream->ReadVarint64(value);
}

}  // namespace helium
}  // namespace blimp
