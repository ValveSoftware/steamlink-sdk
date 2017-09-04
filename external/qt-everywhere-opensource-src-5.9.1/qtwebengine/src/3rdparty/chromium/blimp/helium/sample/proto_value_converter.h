// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_SAMPLE_PROTO_VALUE_CONVERTER_H_
#define BLIMP_HELIUM_SAMPLE_PROTO_VALUE_CONVERTER_H_

#include <string>

#include "blimp/helium/blimp_helium_export.h"

namespace blimp {
namespace helium {

class ValueMessage;
class VersionVector;
class VersionVectorMessage;

class BLIMP_HELIUM_EXPORT ProtoValueConverter {
 public:
  static void ToMessage(const int32_t& value, ValueMessage* output_message);
  static bool FromMessage(const ValueMessage& input_message, int32_t* value);

  static void ToMessage(const std::string& value, ValueMessage* output_message);
  static bool FromMessage(const ValueMessage& input_message,
                          std::string* value);

  static void ToMessage(const VersionVector& value,
                        VersionVectorMessage* output_message);
  static bool FromMessage(const VersionVectorMessage& input_message,
                          VersionVector* value);
};

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_SAMPLE_PROTO_VALUE_CONVERTER_H_
