// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_STANDARD_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_STANDARD_H_

#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/array_traits.h"

namespace mojo {

template <typename T>
struct ArrayTraits<Array<T>> {
  using Element = T;

  static bool IsNull(const Array<T>& input) { return input.is_null(); }
  static void SetToNull(Array<T>* output) { *output = nullptr; }

  static size_t GetSize(const Array<T>& input) { return input.size(); }

  static T* GetData(Array<T>& input) { return &input.front(); }

  static const T* GetData(const Array<T>& input) { return &input.front(); }

  static typename Array<T>::RefType GetAt(Array<T>& input, size_t index) {
    return input[index];
  }

  static typename Array<T>::ConstRefType GetAt(const Array<T>& input,
                                               size_t index) {
    return input[index];
  }

  static bool Resize(Array<T>& input, size_t size) {
    input.resize(size);
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_STANDARD_H_
