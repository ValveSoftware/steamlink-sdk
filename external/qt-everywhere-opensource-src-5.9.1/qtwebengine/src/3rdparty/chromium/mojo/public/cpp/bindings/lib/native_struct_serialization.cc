// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/native_struct_serialization.h"

#include "mojo/public/cpp/bindings/lib/serialization.h"

namespace mojo {
namespace internal {

// static
size_t UnmappedNativeStructSerializerImpl::PrepareToSerialize(
    const NativeStructPtr& input,
    SerializationContext* context) {
  if (!input)
    return 0;
  return internal::PrepareToSerialize<ArrayDataView<uint8_t>>(input->data,
                                                              context);
}

// static
void UnmappedNativeStructSerializerImpl::Serialize(
    const NativeStructPtr& input,
    Buffer* buffer,
    NativeStruct_Data** output,
    SerializationContext* context) {
  if (!input) {
    *output = nullptr;
    return;
  }

  Array_Data<uint8_t>* data = nullptr;
  const ContainerValidateParams params(0, false, nullptr);
  internal::Serialize<ArrayDataView<uint8_t>>(input->data, buffer, &data,
                                              &params, context);
  *output = reinterpret_cast<NativeStruct_Data*>(data);
}

// static
bool UnmappedNativeStructSerializerImpl::Deserialize(
    NativeStruct_Data* input,
    NativeStructPtr* output,
    SerializationContext* context) {
  Array_Data<uint8_t>* data = reinterpret_cast<Array_Data<uint8_t>*>(input);

  NativeStructPtr result(NativeStruct::New());
  if (!internal::Deserialize<ArrayDataView<uint8_t>>(data, &result->data,
                                                     context)) {
    output = nullptr;
    return false;
  }
  if (!result->data)
    *output = nullptr;
  else
    result.Swap(output);
  return true;
}

}  // namespace internal
}  // namespace mojo
