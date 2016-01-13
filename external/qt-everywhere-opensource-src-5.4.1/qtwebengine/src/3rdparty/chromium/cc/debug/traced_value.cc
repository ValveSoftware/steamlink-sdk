// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/traced_value.h"

#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace cc {

scoped_ptr<base::Value> TracedValue::CreateIDRef(const void* id) {
  scoped_ptr<base::DictionaryValue> res(new base::DictionaryValue());
  res->SetString("id_ref", base::StringPrintf("%p", id));
  return res.PassAs<base::Value>();
}

void TracedValue::MakeDictIntoImplicitSnapshot(
    base::DictionaryValue* dict, const char* object_name, const void* id) {
  dict->SetString("id", base::StringPrintf("%s/%p", object_name, id));
}

void TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
    const char* category,
    base::DictionaryValue* dict,
    const char* object_name,
    const void* id) {
  dict->SetString("cat", category);
  MakeDictIntoImplicitSnapshot(dict, object_name, id);
}

void TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
    const char* category,
    base::DictionaryValue* dict,
    const char* object_base_type_name,
    const char* object_name,
    const void* id) {
  dict->SetString("cat", category);
  dict->SetString("base_type", object_base_type_name);
  MakeDictIntoImplicitSnapshot(dict, object_name, id);
}

scoped_refptr<base::debug::ConvertableToTraceFormat> TracedValue::FromValue(
    base::Value* value) {
  return scoped_refptr<base::debug::ConvertableToTraceFormat>(
      new TracedValue(value));
}

TracedValue::TracedValue(base::Value* value)
  : value_(value) {
}

TracedValue::~TracedValue() {
}

void TracedValue::AppendAsTraceFormat(std::string* out) const {
  std::string tmp;
  base::JSONWriter::Write(value_.get(), &tmp);
  *out += tmp;
}

}  // namespace cc
