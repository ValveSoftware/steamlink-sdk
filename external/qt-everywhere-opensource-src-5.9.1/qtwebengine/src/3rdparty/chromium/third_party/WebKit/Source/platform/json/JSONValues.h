/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef JSONValues_h
#define JSONValues_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/TypeTraits.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

#include <memory>

namespace blink {

class JSONValue;

}  // namespace blink

namespace blink {

class JSONArray;
class JSONObject;

class PLATFORM_EXPORT JSONValue {
  USING_FAST_MALLOC(JSONValue);
  WTF_MAKE_NONCOPYABLE(JSONValue);

 public:
  static const int maxDepth = 1000;

  virtual ~JSONValue() {}

  static std::unique_ptr<JSONValue> null() {
    return wrapUnique(new JSONValue());
  }

  enum ValueType {
    TypeNull = 0,
    TypeBoolean,
    TypeInteger,
    TypeDouble,
    TypeString,
    TypeObject,
    TypeArray
  };

  ValueType getType() const { return m_type; }

  bool isNull() const { return m_type == TypeNull; }

  virtual bool asBoolean(bool* output) const;
  virtual bool asDouble(double* output) const;
  virtual bool asInteger(int* output) const;
  virtual bool asString(String* output) const;

  String toJSONString() const;
  String toPrettyJSONString() const;
  virtual void writeJSON(StringBuilder* output) const;
  virtual void prettyWriteJSON(StringBuilder* output) const;
  virtual std::unique_ptr<JSONValue> clone() const;

  static String quoteString(const String&);

 protected:
  JSONValue() : m_type(TypeNull) {}
  explicit JSONValue(ValueType type) : m_type(type) {}
  virtual void prettyWriteJSONInternal(StringBuilder* output, int depth) const;

 private:
  friend class JSONObject;
  friend class JSONArray;

  ValueType m_type;
};

class PLATFORM_EXPORT JSONBasicValue : public JSONValue {
 public:
  static std::unique_ptr<JSONBasicValue> create(bool value) {
    return wrapUnique(new JSONBasicValue(value));
  }

  static std::unique_ptr<JSONBasicValue> create(int value) {
    return wrapUnique(new JSONBasicValue(value));
  }

  static std::unique_ptr<JSONBasicValue> create(double value) {
    return wrapUnique(new JSONBasicValue(value));
  }

  bool asBoolean(bool* output) const override;
  bool asDouble(double* output) const override;
  bool asInteger(int* output) const override;
  void writeJSON(StringBuilder* output) const override;
  std::unique_ptr<JSONValue> clone() const override;

 private:
  explicit JSONBasicValue(bool value)
      : JSONValue(TypeBoolean), m_boolValue(value) {}
  explicit JSONBasicValue(int value)
      : JSONValue(TypeInteger), m_integerValue(value) {}
  explicit JSONBasicValue(double value)
      : JSONValue(TypeDouble), m_doubleValue(value) {}

  union {
    bool m_boolValue;
    double m_doubleValue;
    int m_integerValue;
  };
};

class PLATFORM_EXPORT JSONString : public JSONValue {
 public:
  static std::unique_ptr<JSONString> create(const String& value) {
    return wrapUnique(new JSONString(value));
  }

  static std::unique_ptr<JSONString> create(const char* value) {
    return wrapUnique(new JSONString(value));
  }

  bool asString(String* output) const override;
  void writeJSON(StringBuilder* output) const override;
  std::unique_ptr<JSONValue> clone() const override;

 private:
  explicit JSONString(const String& value)
      : JSONValue(TypeString), m_stringValue(value) {}
  explicit JSONString(const char* value)
      : JSONValue(TypeString), m_stringValue(value) {}

  String m_stringValue;
};

class PLATFORM_EXPORT JSONObject : public JSONValue {
 public:
  using Entry = std::pair<String, JSONValue*>;
  static std::unique_ptr<JSONObject> create() {
    return wrapUnique(new JSONObject());
  }

  static JSONObject* cast(JSONValue* value) {
    if (!value || value->getType() != TypeObject)
      return nullptr;
    return static_cast<JSONObject*>(value);
  }

  static std::unique_ptr<JSONObject> from(std::unique_ptr<JSONValue> value) {
    auto maybeObject = wrapUnique(JSONObject::cast(value.get()));
    if (maybeObject)
      value.release();
    return maybeObject;
  }

  static void cast(JSONObject*) = delete;
  static void cast(std::unique_ptr<JSONObject>) = delete;

  void writeJSON(StringBuilder* output) const override;
  std::unique_ptr<JSONValue> clone() const override;

  size_t size() const { return m_data.size(); }

  void setBoolean(const String& name, bool);
  void setInteger(const String& name, int);
  void setDouble(const String& name, double);
  void setString(const String& name, const String&);
  void setValue(const String& name, std::unique_ptr<JSONValue>);
  void setObject(const String& name, std::unique_ptr<JSONObject>);
  void setArray(const String& name, std::unique_ptr<JSONArray>);

  bool getBoolean(const String& name, bool* output) const;
  bool getInteger(const String& name, int* output) const;
  bool getDouble(const String& name, double* output) const;
  bool getString(const String& name, String* output) const;

  JSONObject* getObject(const String& name) const;
  JSONArray* getArray(const String& name) const;
  JSONValue* get(const String& name) const;
  Entry at(size_t index) const;

  bool booleanProperty(const String& name, bool defaultValue) const;
  int integerProperty(const String& name, int defaultValue) const;
  double doubleProperty(const String& name, double defaultValue) const;
  void remove(const String& name);

  ~JSONObject() override;

 protected:
  void prettyWriteJSONInternal(StringBuilder* output, int depth) const override;

 private:
  JSONObject();
  template <typename T>
  void set(const String& key, std::unique_ptr<T>& value) {
    DCHECK(value);
    if (m_data.set(key, std::move(value)).isNewEntry)
      m_order.append(key);
  }

  using Dictionary = HashMap<String, std::unique_ptr<JSONValue>>;
  Dictionary m_data;
  Vector<String> m_order;
};

class PLATFORM_EXPORT JSONArray : public JSONValue {
 public:
  static std::unique_ptr<JSONArray> create() {
    return wrapUnique(new JSONArray());
  }

  static JSONArray* cast(JSONValue* value) {
    if (!value || value->getType() != TypeArray)
      return nullptr;
    return static_cast<JSONArray*>(value);
  }

  static std::unique_ptr<JSONArray> from(std::unique_ptr<JSONValue> value) {
    auto maybeArray = wrapUnique(JSONArray::cast(value.get()));
    if (maybeArray)
      value.release();
    return maybeArray;
  }

  static void cast(JSONArray*) = delete;
  static void cast(std::unique_ptr<JSONArray>) = delete;

  ~JSONArray() override;

  void writeJSON(StringBuilder* output) const override;
  std::unique_ptr<JSONValue> clone() const override;

  void pushBoolean(bool);
  void pushInteger(int);
  void pushDouble(double);
  void pushString(const String&);
  void pushValue(std::unique_ptr<JSONValue>);
  void pushObject(std::unique_ptr<JSONObject>);
  void pushArray(std::unique_ptr<JSONArray>);

  JSONValue* at(size_t index);
  size_t size() const { return m_data.size(); }

 protected:
  void prettyWriteJSONInternal(StringBuilder* output, int depth) const override;

 private:
  JSONArray();
  Vector<std::unique_ptr<JSONValue>> m_data;
};

PLATFORM_EXPORT void escapeStringForJSON(const String&, StringBuilder*);
void doubleQuoteStringForJSON(const String&, StringBuilder*);

}  // namespace blink

#endif  // JSONValues_h
