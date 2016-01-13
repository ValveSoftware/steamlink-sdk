// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/common_type_converters.h"

#include <string>

#include "base/strings/utf_string_conversions.h"

namespace mojo {

// static
String TypeConverter<String, base::StringPiece>::ConvertFrom(
    const base::StringPiece& input) {
  if (input.empty()) {
    char c = 0;
    return String(&c, 0);
  }
  return String(input.data(), input.size());
}
// static
base::StringPiece TypeConverter<String, base::StringPiece>::ConvertTo(
    const String& input) {
  return input.get();
}

// static
String TypeConverter<String, base::string16>::ConvertFrom(
    const base::string16& input) {
  return TypeConverter<String, base::StringPiece>::ConvertFrom(
      base::UTF16ToUTF8(input));
}
// static
base::string16 TypeConverter<String, base::string16>::ConvertTo(
    const String& input) {
  return base::UTF8ToUTF16(TypeConverter<String, base::StringPiece>::ConvertTo(
      input));
}

}  // namespace mojo
