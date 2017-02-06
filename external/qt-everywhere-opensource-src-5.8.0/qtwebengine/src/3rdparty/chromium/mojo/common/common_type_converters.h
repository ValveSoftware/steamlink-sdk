// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_COMMON_TYPE_CONVERTERS_H_
#define MOJO_COMMON_COMMON_TYPE_CONVERTERS_H_

#include <stdint.h>

#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "mojo/common/mojo_common_export.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

template <>
struct MOJO_COMMON_EXPORT TypeConverter<String, base::StringPiece> {
  static String Convert(const base::StringPiece& input);
};

template <>
struct MOJO_COMMON_EXPORT TypeConverter<base::StringPiece, String> {
  static base::StringPiece Convert(const String& input);
};

template <>
struct MOJO_COMMON_EXPORT TypeConverter<String, base::string16> {
  static String Convert(const base::string16& input);
};

template <>
struct MOJO_COMMON_EXPORT TypeConverter<base::string16, String> {
  static base::string16 Convert(const String& input);
};

template <>
struct MOJO_COMMON_EXPORT TypeConverter<std::string, Array<uint8_t>> {
  static std::string Convert(const Array<uint8_t>& input);
};

template <>
struct MOJO_COMMON_EXPORT TypeConverter<Array<uint8_t>, std::string> {
  static Array<uint8_t> Convert(const std::string& input);
};

template <>
struct MOJO_COMMON_EXPORT TypeConverter<Array<uint8_t>, base::StringPiece> {
  static Array<uint8_t> Convert(const base::StringPiece& input);
};

template <>
struct MOJO_COMMON_EXPORT TypeConverter<base::string16, Array<uint8_t>> {
  static base::string16 Convert(const Array<uint8_t>& input);
};

template <>
struct MOJO_COMMON_EXPORT TypeConverter<Array<uint8_t>, base::string16> {
  static Array<uint8_t> Convert(const base::string16& input);
};

}  // namespace mojo

#endif  // MOJO_COMMON_COMMON_TYPE_CONVERTERS_H_
