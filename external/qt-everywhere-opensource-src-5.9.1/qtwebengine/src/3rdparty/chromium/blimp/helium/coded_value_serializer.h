// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_CODED_VALUE_SERIALIZER_H_
#define BLIMP_HELIUM_CODED_VALUE_SERIALIZER_H_

#include <string>

#include "blimp/helium/blimp_helium_export.h"

namespace google {
namespace protobuf {
namespace io {
class CodedInputStream;
class CodedOutputStream;
}  // namespace io
}  // namespace protobuf
}  // namespace google

namespace blimp {
namespace helium {

class VersionVector;

// Syncable CRDTs need to serialize their data into CodedOutputStreams. This
// helper class centralizes that logic into two overloaded functions, one for
// serialization and one for deserialization that can be called for any
// supported primitive type.
class BLIMP_HELIUM_EXPORT CodedValueSerializer {
 public:
  static void Serialize(const int32_t& value,
                        google::protobuf::io::CodedOutputStream* output_stream);
  static bool Deserialize(google::protobuf::io::CodedInputStream* input_stream,
                          int32_t* value);

  static void Serialize(const std::string& value,
                        google::protobuf::io::CodedOutputStream* output_stream);
  static bool Deserialize(google::protobuf::io::CodedInputStream* input_stream,
                          std::string* value);

  static void Serialize(const VersionVector& value,
                        google::protobuf::io::CodedOutputStream* output_stream);
  static bool Deserialize(google::protobuf::io::CodedInputStream* input_stream,
                          VersionVector* value);

  static void Serialize(const uint64_t& value,
                        google::protobuf::io::CodedOutputStream* output_stream);
  static bool Deserialize(google::protobuf::io::CodedInputStream* input_stream,
                          uint64_t* value);
};

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_CODED_VALUE_SERIALIZER_H_
