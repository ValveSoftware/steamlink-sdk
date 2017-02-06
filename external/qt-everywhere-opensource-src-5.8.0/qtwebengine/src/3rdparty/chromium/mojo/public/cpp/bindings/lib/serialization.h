// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_H_

#include <string.h>

#include "mojo/public/cpp/bindings/array_traits_carray.h"
#include "mojo/public/cpp/bindings/array_traits_standard.h"
#include "mojo/public/cpp/bindings/array_traits_stl.h"
#include "mojo/public/cpp/bindings/lib/array_serialization.h"
#include "mojo/public/cpp/bindings/lib/fixed_buffer.h"
#include "mojo/public/cpp/bindings/lib/handle_interface_serialization.h"
#include "mojo/public/cpp/bindings/lib/map_serialization.h"
#include "mojo/public/cpp/bindings/lib/native_enum_serialization.h"
#include "mojo/public/cpp/bindings/lib/native_struct_serialization.h"
#include "mojo/public/cpp/bindings/lib/string_serialization.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "mojo/public/cpp/bindings/map_traits_standard.h"
#include "mojo/public/cpp/bindings/map_traits_stl.h"
#include "mojo/public/cpp/bindings/string_traits_standard.h"
#include "mojo/public/cpp/bindings/string_traits_stl.h"
#include "mojo/public/cpp/bindings/string_traits_string16.h"
#include "mojo/public/cpp/bindings/string_traits_string_piece.h"

namespace mojo {
namespace internal {

template <typename MojomType, typename DataArrayType, typename UserType>
DataArrayType StructSerializeImpl(UserType* input) {
  static_assert(BelongsTo<MojomType, MojomTypeCategory::STRUCT>::value,
                "Unexpected type.");

  SerializationContext context;
  size_t size = PrepareToSerialize<MojomType>(*input, &context);
  DCHECK_EQ(size, Align(size));

  DataArrayType result(size);
  if (size == 0)
    return result;

  void* result_buffer = &result.front();
  // The serialization logic requires that the buffer is 8-byte aligned. If the
  // result buffer is not properly aligned, we have to do an extra copy. In
  // practice, this should never happen for mojo::Array (backed by std::vector).
  bool need_copy = !IsAligned(result_buffer);

  if (need_copy) {
    // calloc sets the memory to all zero.
    result_buffer = calloc(size, 1);
    DCHECK(IsAligned(result_buffer));
  }

  FixedBuffer buffer;
  buffer.Initialize(result_buffer, size);
  typename MojomType::Struct::Data_* data = nullptr;
  Serialize<MojomType>(*input, &buffer, &data, &context);

  data->EncodePointers();

  if (need_copy) {
    memcpy(&result.front(), result_buffer, size);
    free(result_buffer);
  }

  return result;
}

template <typename MojomType, typename DataArrayType, typename UserType>
bool StructDeserializeImpl(DataArrayType input, UserType* output) {
  static_assert(BelongsTo<MojomType, MojomTypeCategory::STRUCT>::value,
                "Unexpected type.");
  using DataType = typename MojomType::Struct::Data_;

  if (input.is_null())
    return false;

  void* input_buffer = input.empty() ? nullptr : &input.front();

  // Please see comments in StructSerializeImpl.
  bool need_copy = !IsAligned(input_buffer);

  if (need_copy) {
    input_buffer = malloc(input.size());
    DCHECK(IsAligned(input_buffer));
    memcpy(input_buffer, &input.front(), input.size());
  }

  ValidationContext validation_context(input_buffer, input.size(), 0);
  bool result = false;
  if (DataType::Validate(input_buffer, &validation_context)) {
    auto data = reinterpret_cast<DataType*>(input_buffer);
    if (data)
      data->DecodePointers();

    SerializationContext context;
    result = Deserialize<MojomType>(data, output, &context);
  }

  if (need_copy)
    free(input_buffer);

  return result;
}

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_H_
