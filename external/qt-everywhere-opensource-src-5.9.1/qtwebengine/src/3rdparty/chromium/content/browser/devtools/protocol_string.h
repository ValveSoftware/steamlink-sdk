// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STRING_H
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STRING_H

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "content/common/content_export.h"

namespace content {
namespace protocol {

class Value;

using String = std::string;

class CONTENT_EXPORT StringBuilder {
 public:
  StringBuilder();
  ~StringBuilder();
  void append(const std::string&);
  void append(char);
  void append(const char*, size_t);
  std::string toString();
  void reserveCapacity(size_t);

 private:
  std::string string_;
};

class CONTENT_EXPORT StringUtil {
 public:
  static String substring(const String& s, unsigned pos, unsigned len) {
    return s.substr(pos, len);
  }
  static String fromInteger(int number) {
    return base::IntToString(number);
  }
  static String fromDouble(double number) {
    return base::DoubleToString(number);
  }
  static const size_t kNotFound = static_cast<size_t>(-1);
  static void builderReserve(StringBuilder& builder, unsigned capacity) {
    builder.reserveCapacity(capacity);
  }
  static std::unique_ptr<protocol::Value> parseJSON(const String&);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STRING_H
