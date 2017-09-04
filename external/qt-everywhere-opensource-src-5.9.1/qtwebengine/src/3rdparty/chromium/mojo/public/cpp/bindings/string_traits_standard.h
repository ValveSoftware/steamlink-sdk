// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STANDARD_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STANDARD_H_

#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/bindings/string_traits.h"

namespace mojo {

template <>
struct StringTraits<String> {
  static bool IsNull(const String& input) { return input.is_null(); }
  static void SetToNull(String* output) { *output = nullptr; }

  static size_t GetSize(const String& input) { return input.size(); }

  static const char* GetData(const String& input) { return input.data(); }

  static bool Read(StringDataView input, String* output) {
    String result(input.storage(), input.size());
    result.Swap(output);
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STANDARD_H_
