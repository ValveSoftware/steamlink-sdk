// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_STRUCT_DATA_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_STRUCT_DATA_H_

#include <vector>

#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/system/handle.h"

namespace mojo {
namespace internal {

class Buffer;
class ValidationContext;

class NativeStruct_Data {
 public:
  static bool Validate(const void* data, ValidationContext* validation_context);

  void EncodePointers() {}
  void DecodePointers() {}

  // Unlike normal structs, the memory layout is exactly the same as an array
  // of uint8_t.
  Array_Data<uint8_t> data;

 private:
  NativeStruct_Data() = delete;
  ~NativeStruct_Data() = delete;
};

static_assert(sizeof(Array_Data<uint8_t>) == sizeof(NativeStruct_Data),
              "Mismatched NativeStruct_Data and Array_Data<uint8_t> size");

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_STRUCT_DATA_H_
