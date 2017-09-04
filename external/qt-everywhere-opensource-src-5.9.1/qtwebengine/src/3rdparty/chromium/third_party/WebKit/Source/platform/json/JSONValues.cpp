/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/json/JSONValues.h"

#include "platform/Decimal.h"
#include "wtf/MathExtras.h"
#include "wtf/text/StringBuilder.h"
#include <algorithm>
#include <cmath>

namespace blink {

namespace {

const char* const nullString = "null";
const char* const trueString = "true";
const char* const falseString = "false";

inline bool escapeChar(UChar c, StringBuilder* dst) {
  switch (c) {
    case '\b':
      dst->append("\\b");
      break;
    case '\f':
      dst->append("\\f");
      break;
    case '\n':
      dst->append("\\n");
      break;
    case '\r':
      dst->append("\\r");
      break;
    case '\t':
      dst->append("\\t");
      break;
    case '\\':
      dst->append("\\\\");
      break;
    case '"':
      dst->append("\\\"");
      break;
    default:
      return false;
  }
  return true;
}

const LChar hexDigits[17] = "0123456789ABCDEF";

void appendUnsignedAsHex(UChar number, StringBuilder* dst) {
  dst->append("\\u");
  for (size_t i = 0; i < 4; ++i) {
    dst->append(hexDigits[(number & 0xF000) >> 12]);
    number <<= 4;
  }
}

void writeIndent(int depth, StringBuilder* output) {
  for (int i = 0; i < depth; ++i)
    output->append("  ");
}

}  // anonymous namespace

void escapeStringForJSON(const String& str, StringBuilder* dst) {
  for (unsigned i = 0; i < str.length(); ++i) {
    UChar c = str[i];
    if (!escapeChar(c, dst)) {
      if (c < 32 || c > 126 || c == '<' || c == '>') {
        // 1. Escaping <, > to prevent script execution.
        // 2. Technically, we could also pass through c > 126 as UTF8, but this
        //    is also optional. It would also be a pain to implement here.
        appendUnsignedAsHex(c, dst);
      } else {
        dst->append(c);
      }
    }
  }
}

void doubleQuoteStringForJSON(const String& str, StringBuilder* dst) {
  dst->append('"');
  escapeStringForJSON(str, dst);
  dst->append('"');
}

String JSONValue::quoteString(const String& input) {
  StringBuilder builder;
  doubleQuoteStringForJSON(input, &builder);
  return builder.toString();
}

bool JSONValue::asBoolean(bool*) const {
  return false;
}

bool JSONValue::asDouble(double*) const {
  return false;
}

bool JSONValue::asInteger(int*) const {
  return false;
}

bool JSONValue::asString(String*) const {
  return false;
}

String JSONValue::toJSONString() const {
  StringBuilder result;
  result.reserveCapacity(512);
  writeJSON(&result);
  return result.toString();
}

String JSONValue::toPrettyJSONString() const {
  StringBuilder result;
  result.reserveCapacity(512);
  prettyWriteJSON(&result);
  return result.toString();
}

void JSONValue::writeJSON(StringBuilder* output) const {
  DCHECK(m_type == TypeNull);
  output->append(nullString, 4);
}

void JSONValue::prettyWriteJSON(StringBuilder* output) const {
  prettyWriteJSONInternal(output, 0);
  output->append('\n');
}

void JSONValue::prettyWriteJSONInternal(StringBuilder* output,
                                        int depth) const {
  writeJSON(output);
}

std::unique_ptr<JSONValue> JSONValue::clone() const {
  return JSONValue::null();
}

bool JSONBasicValue::asBoolean(bool* output) const {
  if (getType() != TypeBoolean)
    return false;
  *output = m_boolValue;
  return true;
}

bool JSONBasicValue::asDouble(double* output) const {
  if (getType() == TypeDouble) {
    *output = m_doubleValue;
    return true;
  }
  if (getType() == TypeInteger) {
    *output = m_integerValue;
    return true;
  }
  return false;
}

bool JSONBasicValue::asInteger(int* output) const {
  if (getType() != TypeInteger)
    return false;
  *output = m_integerValue;
  return true;
}

void JSONBasicValue::writeJSON(StringBuilder* output) const {
  DCHECK(getType() == TypeBoolean || getType() == TypeInteger ||
         getType() == TypeDouble);
  if (getType() == TypeBoolean) {
    if (m_boolValue)
      output->append(trueString, 4);
    else
      output->append(falseString, 5);
  } else if (getType() == TypeDouble) {
    if (!std::isfinite(m_doubleValue)) {
      output->append(nullString, 4);
      return;
    }
    output->append(Decimal::fromDouble(m_doubleValue).toString());
  } else if (getType() == TypeInteger) {
    output->append(String::number(m_integerValue));
  }
}

std::unique_ptr<JSONValue> JSONBasicValue::clone() const {
  switch (getType()) {
    case TypeDouble:
      return JSONBasicValue::create(m_doubleValue);
    case TypeInteger:
      return JSONBasicValue::create(m_integerValue);
    case TypeBoolean:
      return JSONBasicValue::create(m_boolValue);
    default:
      NOTREACHED();
  }
  return nullptr;
}

bool JSONString::asString(String* output) const {
  *output = m_stringValue;
  return true;
}

void JSONString::writeJSON(StringBuilder* output) const {
  DCHECK(getType() == TypeString);
  doubleQuoteStringForJSON(m_stringValue, output);
}

std::unique_ptr<JSONValue> JSONString::clone() const {
  return JSONString::create(m_stringValue);
}

JSONObject::~JSONObject() {}

void JSONObject::setBoolean(const String& name, bool value) {
  setValue(name, JSONBasicValue::create(value));
}

void JSONObject::setInteger(const String& name, int value) {
  setValue(name, JSONBasicValue::create(value));
}

void JSONObject::setDouble(const String& name, double value) {
  setValue(name, JSONBasicValue::create(value));
}

void JSONObject::setString(const String& name, const String& value) {
  setValue(name, JSONString::create(value));
}

void JSONObject::setValue(const String& name,
                          std::unique_ptr<JSONValue> value) {
  set(name, value);
}

void JSONObject::setObject(const String& name,
                           std::unique_ptr<JSONObject> value) {
  set(name, value);
}

void JSONObject::setArray(const String& name,
                          std::unique_ptr<JSONArray> value) {
  set(name, value);
}

bool JSONObject::getBoolean(const String& name, bool* output) const {
  JSONValue* value = get(name);
  if (!value)
    return false;
  return value->asBoolean(output);
}

bool JSONObject::getInteger(const String& name, int* output) const {
  JSONValue* value = get(name);
  if (!value)
    return false;
  return value->asInteger(output);
}

bool JSONObject::getDouble(const String& name, double* output) const {
  JSONValue* value = get(name);
  if (!value)
    return false;
  return value->asDouble(output);
}

bool JSONObject::getString(const String& name, String* output) const {
  JSONValue* value = get(name);
  if (!value)
    return false;
  return value->asString(output);
}

JSONObject* JSONObject::getObject(const String& name) const {
  return JSONObject::cast(get(name));
}

JSONArray* JSONObject::getArray(const String& name) const {
  return JSONArray::cast(get(name));
}

JSONValue* JSONObject::get(const String& name) const {
  Dictionary::const_iterator it = m_data.find(name);
  if (it == m_data.end())
    return nullptr;
  return it->value.get();
}

JSONObject::Entry JSONObject::at(size_t index) const {
  const String key = m_order[index];
  return std::make_pair(key, m_data.find(key)->value.get());
}

bool JSONObject::booleanProperty(const String& name, bool defaultValue) const {
  bool result = defaultValue;
  getBoolean(name, &result);
  return result;
}

int JSONObject::integerProperty(const String& name, int defaultValue) const {
  int result = defaultValue;
  getInteger(name, &result);
  return result;
}

double JSONObject::doubleProperty(const String& name,
                                  double defaultValue) const {
  double result = defaultValue;
  getDouble(name, &result);
  return result;
}

void JSONObject::remove(const String& name) {
  m_data.remove(name);
  for (size_t i = 0; i < m_order.size(); ++i) {
    if (m_order[i] == name) {
      m_order.remove(i);
      break;
    }
  }
}

void JSONObject::writeJSON(StringBuilder* output) const {
  output->append('{');
  for (size_t i = 0; i < m_order.size(); ++i) {
    Dictionary::const_iterator it = m_data.find(m_order[i]);
    CHECK(it != m_data.end());
    if (i)
      output->append(',');
    doubleQuoteStringForJSON(it->key, output);
    output->append(':');
    it->value->writeJSON(output);
  }
  output->append('}');
}

void JSONObject::prettyWriteJSONInternal(StringBuilder* output,
                                         int depth) const {
  output->append("{\n");
  for (size_t i = 0; i < m_order.size(); ++i) {
    Dictionary::const_iterator it = m_data.find(m_order[i]);
    CHECK(it != m_data.end());
    if (i)
      output->append(",\n");
    writeIndent(depth + 1, output);
    doubleQuoteStringForJSON(it->key, output);
    output->append(": ");
    it->value->prettyWriteJSONInternal(output, depth + 1);
  }
  output->append('\n');
  writeIndent(depth, output);
  output->append('}');
}

std::unique_ptr<JSONValue> JSONObject::clone() const {
  std::unique_ptr<JSONObject> result = JSONObject::create();
  for (size_t i = 0; i < m_order.size(); ++i) {
    String key = m_order[i];
    Dictionary::const_iterator value = m_data.find(key);
    DCHECK(value != m_data.end() && value->value);
    result->setValue(key, value->value->clone());
  }
  return std::move(result);
}

JSONObject::JSONObject() : JSONValue(TypeObject), m_data(), m_order() {}

JSONArray::~JSONArray() {}

void JSONArray::writeJSON(StringBuilder* output) const {
  output->append('[');
  bool first = true;
  for (const std::unique_ptr<JSONValue>& value : m_data) {
    if (!first)
      output->append(',');
    value->writeJSON(output);
    first = false;
  }
  output->append(']');
}

void JSONArray::prettyWriteJSONInternal(StringBuilder* output,
                                        int depth) const {
  output->append('[');
  bool first = true;
  bool lastInsertedNewLine = false;
  for (const std::unique_ptr<JSONValue>& value : m_data) {
    bool insertNewLine = value->getType() == JSONValue::TypeObject ||
                         value->getType() == JSONValue::TypeArray ||
                         value->getType() == JSONValue::TypeString;
    if (first) {
      if (insertNewLine) {
        output->append('\n');
        writeIndent(depth + 1, output);
      }
      first = false;
    } else {
      output->append(',');
      if (lastInsertedNewLine) {
        output->append('\n');
        writeIndent(depth + 1, output);
      } else {
        output->append(' ');
      }
    }
    value->prettyWriteJSONInternal(output, depth + 1);
    lastInsertedNewLine = insertNewLine;
  }
  if (lastInsertedNewLine) {
    output->append('\n');
    writeIndent(depth, output);
  }
  output->append(']');
}

std::unique_ptr<JSONValue> JSONArray::clone() const {
  std::unique_ptr<JSONArray> result = JSONArray::create();
  for (const std::unique_ptr<JSONValue>& value : m_data)
    result->pushValue(value->clone());
  return std::move(result);
}

JSONArray::JSONArray() : JSONValue(TypeArray) {}

void JSONArray::pushBoolean(bool value) {
  m_data.append(JSONBasicValue::create(value));
}

void JSONArray::pushInteger(int value) {
  m_data.append(JSONBasicValue::create(value));
}

void JSONArray::pushDouble(double value) {
  m_data.append(JSONBasicValue::create(value));
}

void JSONArray::pushString(const String& value) {
  m_data.append(JSONString::create(value));
}

void JSONArray::pushValue(std::unique_ptr<JSONValue> value) {
  DCHECK(value);
  m_data.append(std::move(value));
}

void JSONArray::pushObject(std::unique_ptr<JSONObject> value) {
  DCHECK(value);
  m_data.append(std::move(value));
}

void JSONArray::pushArray(std::unique_ptr<JSONArray> value) {
  DCHECK(value);
  m_data.append(std::move(value));
}

JSONValue* JSONArray::at(size_t index) {
  DCHECK_LT(index, m_data.size());
  return m_data[index].get();
}

}  // namespace blink
