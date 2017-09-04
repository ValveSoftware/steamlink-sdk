// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/argument_spec.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "content/public/child/v8_value_converter.h"
#include "gin/converter.h"
#include "gin/dictionary.h"

namespace extensions {

namespace {

template <class T>
std::unique_ptr<base::Value> GetFundamentalConvertedValueHelper(
    v8::Local<v8::Value> arg,
    v8::Local<v8::Context> context,
    const base::Optional<int>& minimum) {
  T val;
  if (!gin::Converter<T>::FromV8(context->GetIsolate(), arg, &val))
    return nullptr;
  if (minimum && val < minimum.value())
    return nullptr;
  return base::MakeUnique<base::FundamentalValue>(val);
}

}  // namespace

ArgumentSpec::ArgumentSpec(const base::Value& value)
    : type_(ArgumentType::INTEGER), optional_(false) {
  const base::DictionaryValue* dict = nullptr;
  CHECK(value.GetAsDictionary(&dict));
  dict->GetBoolean("optional", &optional_);
  dict->GetString("name", &name_);

  InitializeType(dict);
}

void ArgumentSpec::InitializeType(const base::DictionaryValue* dict) {
  std::string ref_string;
  if (dict->GetString("$ref", &ref_string)) {
    ref_ = std::move(ref_string);
    type_ = ArgumentType::REF;
    return;
  }

  std::string type_string;
  CHECK(dict->GetString("type", &type_string));
  if (type_string == "integer")
    type_ = ArgumentType::INTEGER;
  else if (type_string == "number")
    type_ = ArgumentType::DOUBLE;
  else if (type_string == "object")
    type_ = ArgumentType::OBJECT;
  else if (type_string == "array")
    type_ = ArgumentType::LIST;
  else if (type_string == "boolean")
    type_ = ArgumentType::BOOLEAN;
  else if (type_string == "string")
    type_ = ArgumentType::STRING;
  else if (type_string == "any")
    type_ = ArgumentType::ANY;
  else if (type_string == "function")
    type_ = ArgumentType::FUNCTION;
  else
    NOTREACHED();

  int min = 0;
  if (dict->GetInteger("minimum", &min))
    minimum_ = min;

  const base::DictionaryValue* properties_value = nullptr;
  if (type_ == ArgumentType::OBJECT &&
      dict->GetDictionary("properties", &properties_value)) {
    for (base::DictionaryValue::Iterator iter(*properties_value);
         !iter.IsAtEnd(); iter.Advance()) {
      properties_[iter.key()] = base::MakeUnique<ArgumentSpec>(iter.value());
    }
  } else if (type_ == ArgumentType::LIST) {
    const base::DictionaryValue* item_value = nullptr;
    CHECK(dict->GetDictionary("items", &item_value));
    list_element_type_ = base::MakeUnique<ArgumentSpec>(*item_value);
  } else if (type_ == ArgumentType::STRING) {
    // Technically, there's no reason enums couldn't be other objects (e.g.
    // numbers), but right now they seem to be exclusively strings. We could
    // always update this if need be.
    const base::ListValue* enums = nullptr;
    if (dict->GetList("enum", &enums)) {
      size_t size = enums->GetSize();
      CHECK_GT(size, 0u);
      for (size_t i = 0; i < size; ++i) {
        std::string enum_value;
        // Enum entries come in two versions: a list of possible strings, and
        // a dictionary with a field 'name'.
        if (!enums->GetString(i, &enum_value)) {
          const base::DictionaryValue* enum_value_dictionary = nullptr;
          CHECK(enums->GetDictionary(i, &enum_value_dictionary));
          CHECK(enum_value_dictionary->GetString("name", &enum_value));
        }
        enum_values_.insert(std::move(enum_value));
      }
    }
  }
}

ArgumentSpec::~ArgumentSpec() {}

std::unique_ptr<base::Value> ArgumentSpec::ConvertArgument(
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> value,
    const RefMap& refs,
    std::string* error) const {
  // TODO(devlin): Support functions?
  DCHECK_NE(type_, ArgumentType::FUNCTION);
  if (type_ == ArgumentType::REF) {
    DCHECK(ref_);
    auto iter = refs.find(ref_.value());
    DCHECK(iter != refs.end()) << ref_.value();
    return iter->second->ConvertArgument(context, value, refs, error);
  }

  if (IsFundamentalType())
    return ConvertArgumentToFundamental(context, value, error);
  if (type_ == ArgumentType::OBJECT) {
    // TODO(devlin): Currently, this would accept an array (if that array had
    // all the requisite properties). Is that the right thing to do?
    if (!value->IsObject()) {
      *error = "Wrong type";
      return nullptr;
    }
    v8::Local<v8::Object> object = value.As<v8::Object>();
    return ConvertArgumentToObject(context, object, refs, error);
  }
  if (type_ == ArgumentType::LIST) {
    if (!value->IsArray()) {
      *error = "Wrong type";
      return nullptr;
    }
    v8::Local<v8::Array> array = value.As<v8::Array>();
    return ConvertArgumentToArray(context, array, refs, error);
  }
  if (type_ == ArgumentType::ANY)
    return ConvertArgumentToAny(context, value, error);
  NOTREACHED();
  return nullptr;
}

bool ArgumentSpec::IsFundamentalType() const {
  return type_ == ArgumentType::INTEGER || type_ == ArgumentType::DOUBLE ||
         type_ == ArgumentType::BOOLEAN || type_ == ArgumentType::STRING;
}

std::unique_ptr<base::Value> ArgumentSpec::ConvertArgumentToFundamental(
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> value,
    std::string* error) const {
  DCHECK(IsFundamentalType());
  switch (type_) {
    case ArgumentType::INTEGER:
      return GetFundamentalConvertedValueHelper<int32_t>(value, context,
                                                         minimum_);
    case ArgumentType::DOUBLE:
      return GetFundamentalConvertedValueHelper<double>(value, context,
                                                        minimum_);
    case ArgumentType::STRING: {
      std::string s;
      // TODO(devlin): If base::StringValue ever takes a std::string&&, we could
      // use std::move to construct.
      if (!gin::Converter<std::string>::FromV8(context->GetIsolate(),
                                               value, &s) ||
          (!enum_values_.empty() && enum_values_.count(s) == 0)) {
        return nullptr;
      }
      return base::MakeUnique<base::StringValue>(s);
    }
    case ArgumentType::BOOLEAN: {
      bool b = false;
      if (value->IsBoolean() &&
          gin::Converter<bool>::FromV8(context->GetIsolate(), value, &b)) {
        return base::MakeUnique<base::FundamentalValue>(b);
      }
      return nullptr;
    }
    default:
      NOTREACHED();
  }
  return nullptr;
}

std::unique_ptr<base::Value> ArgumentSpec::ConvertArgumentToObject(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> object,
    const RefMap& refs,
    std::string* error) const {
  DCHECK_EQ(ArgumentType::OBJECT, type_);
  auto result = base::MakeUnique<base::DictionaryValue>();
  gin::Dictionary dictionary(context->GetIsolate(), object);
  for (const auto& kv : properties_) {
    v8::Local<v8::Value> subvalue;
    // See comment in ConvertArgumentToArray() about passing in custom crazy
    // values here.
    // TODO(devlin): gin::Dictionary::Get() uses Isolate::GetCurrentContext() -
    // is that always right here, or should we use the v8::Object APIs and
    // pass in |context|?
    // TODO(devlin): Hyper-optimization - Dictionary::Get() also creates a new
    // v8::String for each call. Hypothetically, we could cache these, or at
    // least use an internalized string.
    if (!dictionary.Get(kv.first, &subvalue))
      return nullptr;

    if (subvalue.IsEmpty() || subvalue->IsNull() || subvalue->IsUndefined()) {
      if (!kv.second->optional_) {
        *error = "Missing key: " + kv.first;
        return nullptr;
      }
      continue;
    }
    std::unique_ptr<base::Value> property =
        kv.second->ConvertArgument(context, subvalue, refs, error);
    if (!property)
      return nullptr;
    result->Set(kv.first, std::move(property));
  }
  return std::move(result);
}

std::unique_ptr<base::Value> ArgumentSpec::ConvertArgumentToArray(
    v8::Local<v8::Context> context,
    v8::Local<v8::Array> value,
    const RefMap& refs,
    std::string* error) const {
  DCHECK_EQ(ArgumentType::LIST, type_);
  auto result = base::MakeUnique<base::ListValue>();
  uint32_t length = value->Length();
  for (uint32_t i = 0; i < length; ++i) {
    v8::MaybeLocal<v8::Value> maybe_subvalue = value->Get(context, i);
    v8::Local<v8::Value> subvalue;
    // Note: This can fail in the case of a developer passing in the following:
    // var a = [];
    // Object.defineProperty(a, 0, { get: () => { throw new Error('foo'); } });
    // Currently, this will cause the developer-specified error ('foo') to be
    // thrown.
    // TODO(devlin): This is probably fine, but it's worth contemplating
    // catching the error and throwing our own.
    if (!maybe_subvalue.ToLocal(&subvalue))
      return nullptr;
    std::unique_ptr<base::Value> item =
        list_element_type_->ConvertArgument(context, subvalue, refs, error);
    if (!item)
      return nullptr;
    result->Append(std::move(item));
  }
  return std::move(result);
}

std::unique_ptr<base::Value> ArgumentSpec::ConvertArgumentToAny(
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> value,
    std::string* error) const {
  DCHECK_EQ(ArgumentType::ANY, type_);
  std::unique_ptr<content::V8ValueConverter> converter(
      content::V8ValueConverter::create());
  std::unique_ptr<base::Value> converted(
      converter->FromV8Value(value, context));
  if (!converted)
    *error = "Could not convert to 'any'.";
  return converted;
}

}  // namespace extensions
