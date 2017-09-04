// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/V8InspectorString.h"

#include "core/inspector/protocol/Protocol.h"

namespace blink {

v8_inspector::StringView toV8InspectorStringView(const StringView& string) {
  if (string.isNull())
    return v8_inspector::StringView();
  if (string.is8Bit())
    return v8_inspector::StringView(
        reinterpret_cast<const uint8_t*>(string.characters8()),
        string.length());
  return v8_inspector::StringView(
      reinterpret_cast<const uint16_t*>(string.characters16()),
      string.length());
}

std::unique_ptr<v8_inspector::StringBuffer> toV8InspectorStringBuffer(
    const StringView& string) {
  return v8_inspector::StringBuffer::create(toV8InspectorStringView(string));
}

String toCoreString(const v8_inspector::StringView& string) {
  if (string.is8Bit())
    return String(reinterpret_cast<const LChar*>(string.characters8()),
                  string.length());
  return String(reinterpret_cast<const UChar*>(string.characters16()),
                string.length());
}

String toCoreString(std::unique_ptr<v8_inspector::StringBuffer> buffer) {
  if (!buffer)
    return String();
  return toCoreString(buffer->string());
}

namespace protocol {

// static
std::unique_ptr<protocol::Value> StringUtil::parseJSON(const String& string) {
  if (string.isNull())
    return nullptr;
  if (string.is8Bit()) {
    return parseJSONCharacters(
        reinterpret_cast<const uint8_t*>(string.characters8()),
        string.length());
  }
  return parseJSONCharacters(
      reinterpret_cast<const uint16_t*>(string.characters16()),
      string.length());
}

}  // namespace protocol

}  // namespace blink
