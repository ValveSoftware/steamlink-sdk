// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_WTF_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_WTF_H_

#include "mojo/public/cpp/bindings/array_traits.h"
#include "mojo/public/cpp/bindings/wtf_array.h"

namespace mojo {

template <typename U>
struct ArrayTraits<WTFArray<U>> {
  using Element = U;

  static bool IsNull(const WTFArray<U>& input) { return input.is_null(); }
  static void SetToNull(WTFArray<U>* output) { *output = nullptr; }

  static size_t GetSize(const WTFArray<U>& input) { return input.size(); }

  static U* GetData(WTFArray<U>& input) { return &input.front(); }

  static const U* GetData(const WTFArray<U>& input) { return &input.front(); }

  static U& GetAt(WTFArray<U>& input, size_t index) { return input[index]; }

  static const U& GetAt(const WTFArray<U>& input, size_t index) {
    return input[index];
  }

  static bool Resize(WTFArray<U>& input, size_t size) {
    input.resize(size);
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_WTF_H_
