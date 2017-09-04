// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8InspectorString_h
#define V8InspectorString_h

#include "core/CoreExport.h"
#include "platform/Decimal.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/StringView.h"
#include "wtf/text/WTFString.h"

#include <memory>
#include <v8-inspector.h>

namespace blink {

// Note that passed string must outlive the resulting StringView. This implies
// it must not be a temporary object.
CORE_EXPORT v8_inspector::StringView toV8InspectorStringView(const StringView&);
CORE_EXPORT std::unique_ptr<v8_inspector::StringBuffer>
toV8InspectorStringBuffer(const StringView&);
CORE_EXPORT String toCoreString(const v8_inspector::StringView&);
CORE_EXPORT String toCoreString(std::unique_ptr<v8_inspector::StringBuffer>);

namespace protocol {

class Value;

using String = WTF::String;
using StringBuilder = WTF::StringBuilder;

class CORE_EXPORT StringUtil {
  STATIC_ONLY(StringUtil);

 public:
  static String substring(const String& s, unsigned pos, unsigned len) {
    return s.substring(pos, len);
  }
  static String fromInteger(int number) { return String::number(number); }
  static String fromDouble(double number) {
    return Decimal::fromDouble(number).toString();
  }
  static const size_t kNotFound = WTF::kNotFound;
  static void builderReserve(StringBuilder& builder, unsigned capacity) {
    builder.reserveCapacity(capacity);
  }
  static std::unique_ptr<protocol::Value> parseJSON(const String&);
};

}  // namespace protocol

}  // namespace blink

// TODO(dgozman): migrate core/inspector/protocol to wtf::HashMap.
namespace std {
template <>
struct hash<WTF::String> {
  std::size_t operator()(const WTF::String& string) const {
    return StringHash::hash(string);
  }
};
}  // namespace std

#endif  // V8InspectorString_h
