// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/util.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace json_schema_compiler {
namespace util {

bool PopulateItem(const base::Value& from, int* out) {
  return from.GetAsInteger(out);
}

bool PopulateItem(const base::Value& from, int* out, base::string16* error) {
  if (!from.GetAsInteger(out)) {
    if (error->length()) {
      error->append(base::UTF8ToUTF16("; "));
    }
    error->append(base::UTF8ToUTF16("expected integer, got " +
                                    ValueTypeToString(from.GetType())));
    return false;
  }
  return true;
}

bool PopulateItem(const base::Value& from, bool* out) {
  return from.GetAsBoolean(out);
}

bool PopulateItem(const base::Value& from, bool* out, base::string16* error) {
  if (!from.GetAsBoolean(out)) {
    if (error->length()) {
      error->append(base::UTF8ToUTF16("; "));
    }
    error->append(base::UTF8ToUTF16("expected boolean, got " +
                                    ValueTypeToString(from.GetType())));
    return false;
  }
  return true;
}

bool PopulateItem(const base::Value& from, double* out) {
  return from.GetAsDouble(out);
}

bool PopulateItem(const base::Value& from, double* out, base::string16* error) {
  if (!from.GetAsDouble(out)) {
    if (error->length()) {
      error->append(base::UTF8ToUTF16("; "));
    }
    error->append(base::UTF8ToUTF16("expected double, got " +
                                    ValueTypeToString(from.GetType())));
    return false;
  }
  return true;
}

bool PopulateItem(const base::Value& from, std::string* out) {
  return from.GetAsString(out);
}

bool PopulateItem(const base::Value& from,
                  std::string* out,
                  base::string16* error) {
  if (!from.GetAsString(out)) {
    if (error->length()) {
      error->append(base::UTF8ToUTF16("; "));
    }
    error->append(base::UTF8ToUTF16("expected string, got " +
                                    ValueTypeToString(from.GetType())));
    return false;
  }
  return true;
}

bool PopulateItem(const base::Value& from, std::vector<char>* out) {
  const base::BinaryValue* binary = nullptr;
  if (!from.GetAsBinary(&binary))
    return false;
  out->assign(binary->GetBuffer(), binary->GetBuffer() + binary->GetSize());
  return true;
}

bool PopulateItem(const base::Value& from,
                  std::vector<char>* out,
                  base::string16* error) {
  const base::BinaryValue* binary = nullptr;
  if (!from.GetAsBinary(&binary)) {
    if (error->length()) {
      error->append(base::UTF8ToUTF16("; "));
    }
    error->append(base::UTF8ToUTF16("expected binary, got " +
                                    ValueTypeToString(from.GetType())));
    return false;
  }
  out->assign(binary->GetBuffer(), binary->GetBuffer() + binary->GetSize());
  return true;
}

bool PopulateItem(const base::Value& from, std::unique_ptr<base::Value>* out) {
  *out = from.CreateDeepCopy();
  return true;
}

bool PopulateItem(const base::Value& from,
                  std::unique_ptr<base::Value>* out,
                  base::string16* error) {
  *out = from.CreateDeepCopy();
  return true;
}

bool PopulateItem(const base::Value& from,
                  std::unique_ptr<base::DictionaryValue>* out) {
  const base::DictionaryValue* dict = nullptr;
  if (!from.GetAsDictionary(&dict))
    return false;
  *out = dict->CreateDeepCopy();
  return true;
}

bool PopulateItem(const base::Value& from,
                  std::unique_ptr<base::DictionaryValue>* out,
                  base::string16* error) {
  const base::DictionaryValue* dict = nullptr;
  if (!from.GetAsDictionary(&dict)) {
    if (error->length()) {
      error->append(base::UTF8ToUTF16("; "));
    }
    error->append(base::UTF8ToUTF16("expected dictionary, got " +
                                    ValueTypeToString(from.GetType())));
    return false;
  }
  *out = dict->CreateDeepCopy();
  return true;
}

void AddItemToList(const int from, base::ListValue* out) {
  out->AppendInteger(from);
}

void AddItemToList(const bool from, base::ListValue* out) {
  out->AppendBoolean(from);
}

void AddItemToList(const double from, base::ListValue* out) {
  out->AppendDouble(from);
}

void AddItemToList(const std::string& from, base::ListValue* out) {
  out->AppendString(from);
}

void AddItemToList(const std::vector<char>& from, base::ListValue* out) {
  out->Append(
      base::BinaryValue::CreateWithCopiedBuffer(from.data(), from.size()));
}

void AddItemToList(const std::unique_ptr<base::Value>& from,
                   base::ListValue* out) {
  out->Append(from->CreateDeepCopy());
}

void AddItemToList(const std::unique_ptr<base::DictionaryValue>& from,
                   base::ListValue* out) {
  out->Append(from->CreateDeepCopy());
}

std::string ValueTypeToString(base::Value::Type type) {
  switch (type) {
    case base::Value::TYPE_NULL:
      return "null";
    case base::Value::TYPE_BOOLEAN:
      return "boolean";
    case base::Value::TYPE_INTEGER:
      return "integer";
    case base::Value::TYPE_DOUBLE:
      return "number";
    case base::Value::TYPE_STRING:
      return "string";
    case base::Value::TYPE_BINARY:
      return "binary";
    case base::Value::TYPE_DICTIONARY:
      return "dictionary";
    case base::Value::TYPE_LIST:
      return "list";
  }
  NOTREACHED();
  return "";
}

}  // namespace util
}  // namespace json_schema_compiler
