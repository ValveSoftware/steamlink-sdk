// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/common_type_converters.h"

#include <stdint.h>

#include <string>

#include "base/strings/utf_string_conversions.h"

namespace mojo {

// static
String TypeConverter<String, base::StringPiece>::Convert(
    const base::StringPiece& input) {
  if (input.empty()) {
    char c = 0;
    return String(&c, 0);
  }
  return String(input.data(), input.size());
}
// static
base::StringPiece TypeConverter<base::StringPiece, String>::Convert(
    const String& input) {
  return input.get();
}

// static
String TypeConverter<String, base::string16>::Convert(
    const base::string16& input) {
  return TypeConverter<String, base::StringPiece>::Convert(
      base::UTF16ToUTF8(input));
}
// static
base::string16 TypeConverter<base::string16, String>::Convert(
    const String& input) {
  return base::UTF8ToUTF16(input.To<base::StringPiece>());
}

std::string TypeConverter<std::string, Array<uint8_t>>::Convert(
    const Array<uint8_t>& input) {
  if (input.is_null() || input.empty())
    return std::string();

  return std::string(reinterpret_cast<const char*>(&input.front()),
                     input.size());
}

Array<uint8_t> TypeConverter<Array<uint8_t>, std::string>::Convert(
    const std::string& input) {
  Array<uint8_t> result(input.size());
  if (!input.empty())
    memcpy(&result.front(), input.c_str(), input.size());
  return result;
}

Array<uint8_t> TypeConverter<Array<uint8_t>, base::StringPiece>::Convert(
    const base::StringPiece& input) {
  Array<uint8_t> result(input.size());
  if (!input.empty())
    memcpy(&result.front(), input.data(), input.size());
  return result;
}

base::string16 TypeConverter<base::string16, Array<uint8_t>>::Convert(
    const Array<uint8_t>& input) {
  if (input.is_null() || input.empty())
    return base::string16();

  return base::string16(reinterpret_cast<const base::char16*>(&input.front()),
                        input.size() / sizeof(base::char16));
}

Array<uint8_t> TypeConverter<Array<uint8_t>, base::string16>::Convert(
    const base::string16& input) {
  Array<uint8_t> result(input.size() * sizeof(base::char16));
  if (!input.empty())
    memcpy(&result.front(), input.c_str(), input.size() * sizeof(base::char16));
  return result;
}

}  // namespace mojo
