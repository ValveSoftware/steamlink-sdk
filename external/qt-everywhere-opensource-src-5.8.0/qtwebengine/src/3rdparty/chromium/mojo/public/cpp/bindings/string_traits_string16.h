// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STRING16_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STRING16_H_

#include "base/strings/string16.h"
#include "mojo/public/cpp/bindings/string_traits.h"

namespace mojo {

template <>
struct StringTraits<base::string16> {
  static bool IsNull(const base::string16& input) {
    // base::string16 is always converted to non-null mojom string.
    return false;
  }

  static void SetToNull(base::string16* output) {
    // Convert null to an "empty" base::string16.
    output->clear();
  }

  static void* SetUpContext(const base::string16& input);
  static void TearDownContext(const base::string16& input, void* context);

  static size_t GetSize(const base::string16& input, void* context);
  static const char* GetData(const base::string16& input, void* context);

  static bool Read(StringDataView input, base::string16* output);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STRING16_H_
