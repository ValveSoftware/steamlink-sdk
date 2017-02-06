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

#include "platform/JSONValues.h"

#include "platform/Decimal.h"
#include "wtf/MathExtras.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace {

const char* const nullString = "null";
const char* const trueString = "true";
const char* const falseString = "false";

inline bool escapeChar(UChar c, StringBuilder* dst)
{
    switch (c) {
    case '\b': dst->append("\\b"); break;
    case '\f': dst->append("\\f"); break;
    case '\n': dst->append("\\n"); break;
    case '\r': dst->append("\\r"); break;
    case '\t': dst->append("\\t"); break;
    case '\\': dst->append("\\\\"); break;
    case '"': dst->append("\\\""); break;
    default:
        return false;
    }
    return true;
}

void writeIndent(int depth, StringBuilder* output)
{
    for (int i = 0; i < depth; ++i)
        output->append("  ");
}

} // anonymous namespace

void escapeStringForJSON(const String& str, StringBuilder* dst)
{
    for (unsigned i = 0; i < str.length(); ++i) {
        UChar c = str[i];
        if (!escapeChar(c, dst)) {
            if (c < 32 || c > 126 || c == '<' || c == '>') {
                // 1. Escaping <, > to prevent script execution.
                // 2. Technically, we could also pass through c > 126 as UTF8, but this
                //    is also optional. It would also be a pain to implement here.
                unsigned symbol = static_cast<unsigned>(c);
                String symbolCode = String::format("\\u%04X", symbol);
                dst->append(symbolCode);
            } else {
                dst->append(c);
            }
        }
    }
}

void doubleQuoteStringForJSON(const String& str, StringBuilder* dst)
{
    dst->append('"');
    escapeStringForJSON(str, dst);
    dst->append('"');
}

String JSONValue::quoteString(const String& input)
{
    StringBuilder builder;
    doubleQuoteStringForJSON(input, &builder);
    return builder.toString();
}

bool JSONValue::asBoolean(bool*) const
{
    return false;
}

bool JSONValue::asNumber(double*) const
{
    return false;
}

bool JSONValue::asNumber(long*) const
{
    return false;
}

bool JSONValue::asNumber(int*) const
{
    return false;
}

bool JSONValue::asNumber(unsigned long*) const
{
    return false;
}

bool JSONValue::asNumber(unsigned*) const
{
    return false;
}

bool JSONValue::asString(String*) const
{
    return false;
}

String JSONValue::toJSONString() const
{
    StringBuilder result;
    result.reserveCapacity(512);
    writeJSON(&result);
    return result.toString();
}

String JSONValue::toPrettyJSONString() const
{
    StringBuilder result;
    result.reserveCapacity(512);
    prettyWriteJSON(&result);
    return result.toString();
}

void JSONValue::writeJSON(StringBuilder* output) const
{
    ASSERT(m_type == TypeNull);
    output->append(nullString, 4);
}

void JSONValue::prettyWriteJSON(StringBuilder* output) const
{
    prettyWriteJSONInternal(output, 0);
    output->append('\n');
}

void JSONValue::prettyWriteJSONInternal(StringBuilder* output, int depth) const
{
    writeJSON(output);
}

bool JSONBasicValue::asBoolean(bool* output) const
{
    if (getType() != TypeBoolean)
        return false;
    *output = m_boolValue;
    return true;
}

bool JSONBasicValue::asNumber(double* output) const
{
    if (getType() != TypeNumber)
        return false;
    *output = m_doubleValue;
    return true;
}

bool JSONBasicValue::asNumber(long* output) const
{
    if (getType() != TypeNumber)
        return false;
    *output = static_cast<long>(m_doubleValue);
    return true;
}

bool JSONBasicValue::asNumber(int* output) const
{
    if (getType() != TypeNumber)
        return false;
    *output = static_cast<int>(m_doubleValue);
    return true;
}

bool JSONBasicValue::asNumber(unsigned long* output) const
{
    if (getType() != TypeNumber)
        return false;
    *output = static_cast<unsigned long>(m_doubleValue);
    return true;
}

bool JSONBasicValue::asNumber(unsigned* output) const
{
    if (getType() != TypeNumber)
        return false;
    *output = static_cast<unsigned>(m_doubleValue);
    return true;
}

void JSONBasicValue::writeJSON(StringBuilder* output) const
{
    ASSERT(getType() == TypeBoolean || getType() == TypeNumber);
    if (getType() == TypeBoolean) {
        if (m_boolValue)
            output->append(trueString, 4);
        else
            output->append(falseString, 5);
    } else if (getType() == TypeNumber) {
        if (!std::isfinite(m_doubleValue)) {
            output->append(nullString, 4);
            return;
        }
        output->append(Decimal::fromDouble(m_doubleValue).toString());
    }
}

bool JSONString::asString(String* output) const
{
    *output = m_stringValue;
    return true;
}

void JSONString::writeJSON(StringBuilder* output) const
{
    ASSERT(getType() == TypeString);
    doubleQuoteStringForJSON(m_stringValue, output);
}

JSONObject::~JSONObject()
{
}

void JSONObject::setBoolean(const String& name, bool value)
{
    setValue(name, JSONBasicValue::create(value));
}

void JSONObject::setNumber(const String& name, double value)
{
    setValue(name, JSONBasicValue::create(value));
}

void JSONObject::setString(const String& name, const String& value)
{
    setValue(name, JSONString::create(value));
}

void JSONObject::setValue(const String& name, PassRefPtr<JSONValue> value)
{
    ASSERT(value);
    if (m_data.set(name, value).isNewEntry)
        m_order.append(name);
}

void JSONObject::setObject(const String& name, PassRefPtr<JSONObject> value)
{
    ASSERT(value);
    if (m_data.set(name, value).isNewEntry)
        m_order.append(name);
}

void JSONObject::setArray(const String& name, PassRefPtr<JSONArray> value)
{
    ASSERT(value);
    if (m_data.set(name, value).isNewEntry)
        m_order.append(name);
}

JSONObject::iterator JSONObject::find(const String& name)
{
    return m_data.find(name);
}

JSONObject::const_iterator JSONObject::find(const String& name) const
{
    return m_data.find(name);
}

bool JSONObject::getBoolean(const String& name, bool* output) const
{
    RefPtr<JSONValue> value = get(name);
    if (!value)
        return false;
    return value->asBoolean(output);
}

bool JSONObject::getString(const String& name, String* output) const
{
    RefPtr<JSONValue> value = get(name);
    if (!value)
        return false;
    return value->asString(output);
}

PassRefPtr<JSONObject> JSONObject::getObject(const String& name) const
{
    return JSONObject::cast(get(name));
}

PassRefPtr<JSONArray> JSONObject::getArray(const String& name) const
{
    return JSONArray::cast(get(name));
}

PassRefPtr<JSONValue> JSONObject::get(const String& name) const
{
    Dictionary::const_iterator it = m_data.find(name);
    if (it == m_data.end())
        return nullptr;
    return it->value;
}

bool JSONObject::booleanProperty(const String& name, bool defaultValue) const
{
    bool result = defaultValue;
    getBoolean(name, &result);
    return result;
}

void JSONObject::remove(const String& name)
{
    m_data.remove(name);
    for (size_t i = 0; i < m_order.size(); ++i) {
        if (m_order[i] == name) {
            m_order.remove(i);
            break;
        }
    }
}

void JSONObject::writeJSON(StringBuilder* output) const
{
    output->append('{');
    for (size_t i = 0; i < m_order.size(); ++i) {
        Dictionary::const_iterator it = m_data.find(m_order[i]);
        ASSERT_WITH_SECURITY_IMPLICATION(it != m_data.end());
        if (i)
            output->append(',');
        doubleQuoteStringForJSON(it->key, output);
        output->append(':');
        it->value->writeJSON(output);
    }
    output->append('}');
}

void JSONObject::prettyWriteJSONInternal(StringBuilder* output, int depth) const
{
    output->append("{\n");
    for (size_t i = 0; i < m_order.size(); ++i) {
        Dictionary::const_iterator it = m_data.find(m_order[i]);
        ASSERT_WITH_SECURITY_IMPLICATION(it != m_data.end());
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

JSONObject::JSONObject()
    : JSONValue(TypeObject)
    , m_data()
    , m_order()
{
}

JSONArray::~JSONArray()
{
}

void JSONArray::writeJSON(StringBuilder* output) const
{
    output->append('[');
    for (Vector<RefPtr<JSONValue>>::const_iterator it = m_data.begin(); it != m_data.end(); ++it) {
        if (it != m_data.begin())
            output->append(',');
        (*it)->writeJSON(output);
    }
    output->append(']');
}

void JSONArray::prettyWriteJSONInternal(StringBuilder* output, int depth) const
{
    output->append('[');
    bool lastInsertedNewLine = false;
    for (Vector<RefPtr<JSONValue>>::const_iterator it = m_data.begin(); it != m_data.end(); ++it) {
        bool insertNewLine = (*it)->getType() == JSONValue::TypeObject || (*it)->getType() == JSONValue::TypeArray || (*it)->getType() == JSONValue::TypeString;
        if (it == m_data.begin()) {
            if (insertNewLine) {
                output->append('\n');
                writeIndent(depth + 1, output);
            }
        } else {
            output->append(',');
            if (lastInsertedNewLine) {
                output->append('\n');
                writeIndent(depth + 1, output);
            } else {
                output->append(' ');
            }
        }
        (*it)->prettyWriteJSONInternal(output, depth + 1);
        lastInsertedNewLine = insertNewLine;
    }
    if (lastInsertedNewLine) {
        output->append('\n');
        writeIndent(depth, output);
    }
    output->append(']');
}

JSONArray::JSONArray()
    : JSONValue(TypeArray)
    , m_data()
{
}

void JSONArray::pushBoolean(bool value)
{
    m_data.append(JSONBasicValue::create(value));
}

void JSONArray::pushInt(int value)
{
    m_data.append(JSONBasicValue::create(value));
}

void JSONArray::pushNumber(double value)
{
    m_data.append(JSONBasicValue::create(value));
}

void JSONArray::pushString(const String& value)
{
    m_data.append(JSONString::create(value));
}

void JSONArray::pushValue(PassRefPtr<JSONValue> value)
{
    ASSERT(value);
    m_data.append(value);
}

void JSONArray::pushObject(PassRefPtr<JSONObject> value)
{
    ASSERT(value);
    m_data.append(value);
}

void JSONArray::pushArray(PassRefPtr<JSONArray> value)
{
    ASSERT(value);
    m_data.append(value);
}

PassRefPtr<JSONValue> JSONArray::get(size_t index)
{
    ASSERT_WITH_SECURITY_IMPLICATION(index < m_data.size());
    return m_data[index];
}

} // namespace blink
