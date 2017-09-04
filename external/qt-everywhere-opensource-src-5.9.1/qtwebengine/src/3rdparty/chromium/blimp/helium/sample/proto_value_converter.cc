// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/sample/proto_value_converter.h"

#include <stdint.h>

#include <string>

#include "base/logging.h"
#include "blimp/helium/sample/sample.pb.h"
#include "blimp/helium/version_vector.h"

namespace blimp {
namespace helium {

// -----------------------------
// int32_t serialization methods

void ProtoValueConverter::ToMessage(const int32_t& value,
                                    ValueMessage* output_message) {
  DCHECK(output_message);
  output_message->set_int32_value(value);
}

bool ProtoValueConverter::FromMessage(const ValueMessage& input_message,
                                      int32_t* value) {
  DCHECK(value);
  DCHECK_EQ(ValueMessage::kInt32Value, input_message.value_case());
  if (input_message.value_case() != ValueMessage::kInt32Value) {
    return false;
  }
  *value = input_message.int32_value();
  return true;
}

// ---------------------------------
// std::string serialization methods

// These could be made more efficient using string::swap() to re-use allocated
// data.
void ProtoValueConverter::ToMessage(const std::string& value,
                                    ValueMessage* output_message) {
  DCHECK(output_message);
  output_message->set_string_value(value);
}

bool ProtoValueConverter::FromMessage(const ValueMessage& input_message,
                                      std::string* value) {
  DCHECK(value);
  DCHECK_EQ(ValueMessage::kStringValue, input_message.value_case());
  if (input_message.value_case() != ValueMessage::kStringValue) {
    return false;
  }
  *value = input_message.string_value();
  return true;
}

// -----------------------------------
// VersionVector serialization methods

void ProtoValueConverter::ToMessage(const VersionVector& value,
                                    VersionVectorMessage* output_message) {
  DCHECK(output_message);
  output_message->set_local_revision(value.local_revision());
  output_message->set_remote_revision(value.remote_revision());
}

bool ProtoValueConverter::FromMessage(const VersionVectorMessage& input_message,
                                      VersionVector* value) {
  DCHECK(value);
  value->set_local_revision(input_message.local_revision());
  value->set_remote_revision(input_message.remote_revision());
  return true;
}

}  // namespace helium
}  // namespace blimp
