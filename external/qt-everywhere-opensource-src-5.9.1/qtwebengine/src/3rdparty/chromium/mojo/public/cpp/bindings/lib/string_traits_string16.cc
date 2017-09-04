// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/string_traits_string16.h"

#include <string>

#include "base/strings/utf_string_conversions.h"

namespace mojo {

// static
void* StringTraits<base::string16>::SetUpContext(const base::string16& input) {
  return new std::string(base::UTF16ToUTF8(input));
}

// static
void StringTraits<base::string16>::TearDownContext(const base::string16& input,
                                                   void* context) {
  delete static_cast<std::string*>(context);
}

// static
size_t StringTraits<base::string16>::GetSize(const base::string16& input,
                                             void* context) {
  return static_cast<std::string*>(context)->size();
}

// static
const char* StringTraits<base::string16>::GetData(const base::string16& input,
                                                  void* context) {
  return static_cast<std::string*>(context)->data();
}

// static
bool StringTraits<base::string16>::Read(StringDataView input,
                                        base::string16* output) {
  return base::UTF8ToUTF16(input.storage(), input.size(), output);
}

}  // namespace mojo
