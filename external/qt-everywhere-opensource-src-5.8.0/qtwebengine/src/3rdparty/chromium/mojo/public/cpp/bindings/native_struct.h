// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_NATIVE_STRUCT_H_
#define MOJO_PUBLIC_CPP_BINDINGS_NATIVE_STRUCT_H_

#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/lib/native_struct_data.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

class NativeStruct;
using NativeStructPtr = StructPtr<NativeStruct>;

// Native-only structs correspond to "[Native] struct Foo;" definitions in
// mojom.
class NativeStruct {
 public:
  using Data_ = internal::NativeStruct_Data;

  static NativeStructPtr New();

  template <typename U>
  static NativeStructPtr From(const U& u) {
    return TypeConverter<NativeStructPtr, U>::Convert(u);
  }

  template <typename U>
  U To() const {
    return TypeConverter<U, NativeStruct>::Convert(*this);
  }

  NativeStruct();
  ~NativeStruct();

  NativeStructPtr Clone() const;
  bool Equals(const NativeStruct& other) const;

  Array<uint8_t> data;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_NATIVE_STRUCT_H_
