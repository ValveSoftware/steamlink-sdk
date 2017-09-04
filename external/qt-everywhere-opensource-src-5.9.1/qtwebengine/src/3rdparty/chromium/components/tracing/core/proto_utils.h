// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_CORE_PROTO_UTILS_H_
#define COMPONENTS_TRACING_CORE_PROTO_UTILS_H_

#include <inttypes.h>

#include <type_traits>

#include "base/logging.h"
#include "components/tracing/tracing_export.h"

namespace tracing {
namespace v2 {
namespace proto {

// See https://developers.google.com/protocol-buffers/docs/encoding wire types.

enum FieldType : uint32_t {
  kFieldTypeVarInt = 0,
  kFieldTypeFixed64 = 1,
  kFieldTypeLengthDelimited = 2,
  kFieldTypeFixed32 = 5,
};

class ProtoFieldDescriptor {
 public:
  enum Type {
    TYPE_INVALID = 0,
    TYPE_DOUBLE = 1,
    TYPE_FLOAT = 2,
    TYPE_INT64 = 3,
    TYPE_UINT64 = 4,
    TYPE_INT32 = 5,
    TYPE_FIXED64 = 6,
    TYPE_FIXED32 = 7,
    TYPE_BOOL = 8,
    TYPE_STRING = 9,
    TYPE_MESSAGE = 11,
    // TYPE_GROUP = 10 is not supported.
    TYPE_BYTES = 12,
    TYPE_UINT32 = 13,
    TYPE_ENUM = 14,
    TYPE_SFIXED32 = 15,
    TYPE_SFIXED64 = 16,
    TYPE_SINT32 = 17,
    TYPE_SINT64 = 18,
  };

  ProtoFieldDescriptor(const char* name,
                       Type type,
                       uint32_t number,
                       bool is_repeated)
      : name_(name), type_(type), number_(number), is_repeated_(is_repeated) {
  }

  const char* name() const {
    return name_;
  }

  Type type() const {
    return type_;
  }

  uint32_t number() const {
    return number_;
  }

  bool is_repeated() const {
    return is_repeated_;
  }

  bool is_valid() const {
    return type_ != Type::TYPE_INVALID;
  }

 private:
  const char* const name_;
  const Type type_;
  const uint32_t number_;
  const bool is_repeated_;

  DISALLOW_COPY_AND_ASSIGN(ProtoFieldDescriptor);
};

// Maximum message size supported: 256 MiB (4 x 7-bit due to varint encoding).
constexpr size_t kMessageLengthFieldSize = 4;
constexpr size_t kMaxMessageLength = (1u << (kMessageLengthFieldSize * 7)) - 1;

// Field tag is encoded as 32-bit varint (5 bytes at most).
// Largest value of simple (not length-delimited) field is 64-bit varint
// (10 bytes at most). 15 bytes buffer is enough to store a simple field.
constexpr size_t kMaxTagEncodedSize = 5;
constexpr size_t kMaxSimpleFieldEncodedSize = kMaxTagEncodedSize + 10;

// Proto types: (int|uint|sint)(32|64), bool, enum.
constexpr uint32_t MakeTagVarInt(uint32_t field_id) {
  return (field_id << 3) | kFieldTypeVarInt;
}

// Proto types: fixed64, sfixed64, fixed32, sfixed32, double, float.
template <typename T>
constexpr uint32_t MakeTagFixed(uint32_t field_id) {
  static_assert(sizeof(T) == 8 || sizeof(T) == 4, "Value must be 4 or 8 bytes");
  return (field_id << 3) |
         (sizeof(T) == 8 ? kFieldTypeFixed64 : kFieldTypeFixed32);
}

// Proto types: string, bytes, embedded messages.
constexpr uint32_t MakeTagLengthDelimited(uint32_t field_id) {
  return (field_id << 3) | kFieldTypeLengthDelimited;
}

// Proto tipes: sint64, sint32.
template <typename T>
inline typename std::make_unsigned<T>::type ZigZagEncode(T value) {
  return (value << 1) ^ (value >> (sizeof(T) * 8 - 1));
}

template <typename T>
inline uint8_t* WriteVarInt(T value, uint8_t* target) {
  // Avoid arithmetic (sign expanding) shifts.
  typedef typename std::make_unsigned<T>::type unsigned_T;
  unsigned_T unsigned_value = static_cast<unsigned_T>(value);

  while (unsigned_value >= 0x80) {
    *target++ = static_cast<uint8_t>(unsigned_value) | 0x80;
    unsigned_value >>= 7;
  }
  *target = static_cast<uint8_t>(unsigned_value);
  return target + 1;
}

// Writes a fixed-size redundant encoding of the given |value|. This is
// used to backfill fixed-size reservations for the length field using a
// non-canonical varint encoding (e.g. \x81\x80\x80\x00 instead of \x01).
// See https://github.com/google/protobuf/issues/1530.
// In particular, this is used for nested messages. The size of a nested message
// is not known until all its field have been written. |kMessageLengthFieldSize|
// bytes are reserved to encode the size field and backfilled at the end.
inline void WriteRedundantVarInt(uint32_t value, uint8_t* buf) {
  for (size_t i = 0; i < kMessageLengthFieldSize; ++i) {
    const uint8_t msb = (i < kMessageLengthFieldSize - 1) ? 0x80 : 0;
    buf[i] = static_cast<uint8_t>(value) | msb;
    value >>= 7;
  }
  DCHECK_EQ(0u, value) << "Message is too long";
}

template <uint32_t field_id>
void StaticAssertSingleBytePreamble() {
  static_assert(field_id < 16,
                "Proto field id too big to fit in a single byte preamble");
};

// Parses a VarInt from the encoded buffer [start, end). |end| is STL-style and
// points one byte past the end of buffer.
// The parsed int value is stored in the output arg |value|. Returns a pointer
// to the next unconsumed byte (so start < retval <= end).
TRACING_EXPORT const uint8_t* ParseVarInt(const uint8_t* start,
                                          const uint8_t* end,
                                          uint64_t* value);

// Parses a protobuf field and computes its id, type and value.
// Returns a pointer to the next unconsumed byte (|start| < retval <= end) that
// is either the beginning of the next field or the end of the parent message.
// In the case of a kFieldTypeLengthDelimited field, |field_intvalue| will store
// the length of the payload (either a string or a nested message). In this
// case, the start of the payload will be at (return value) - |field_intvalue|.
TRACING_EXPORT const uint8_t* ParseField(const uint8_t* start,
                                         const uint8_t* end,
                                         uint32_t* field_id,
                                         FieldType* field_type,
                                         uint64_t* field_intvalue);

}  // namespace proto
}  // namespace v2
}  // namespace tracing

#endif  // COMPONENTS_TRACING_CORE_PROTO_UTILS_H_
