// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_STRING_UTIL_H_
#define COMPONENTS_UI_DEVTOOLS_STRING_UTIL_H_

#include <memory>

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"

namespace ui {
namespace devtools {

using String = std::string;

namespace protocol {

class Value;

class CustomStringBuilder {
  String s_;

 public:
  CustomStringBuilder() {}
  CustomStringBuilder(String& s) : s_(s) {}
  void reserveCapacity(std::size_t size) { s_.reserve(size); }
  void append(const String& s) { s_ += s; }
  void append(char c) { s_ += c; }
  void append(const char* data, unsigned int length) {
    s_.append(data, length);
  }
  String toString() { return s_; }
};

using StringBuilder = CustomStringBuilder;

class StringUtil {
 public:
  static String substring(const String& s, unsigned pos, unsigned len) {
    return s.substr(pos, len);
  }
  static String fromInteger(int number) { return base::IntToString(number); }
  static String fromDouble(double number) {
    return base::DoubleToString(number);
  }
  static void builderReserve(StringBuilder& builder, unsigned capacity) {
    builder.reserveCapacity(capacity);
  }
  static const size_t kNotFound = static_cast<size_t>(-1);
  static std::unique_ptr<Value> parseJSON(const String& string);
};

}  // namespace protocol
}  // namespace devtools
}  // namespace ui

#endif  // COMPONENTS_UI_DEVTOOLS_STRING_UTIL_H_
