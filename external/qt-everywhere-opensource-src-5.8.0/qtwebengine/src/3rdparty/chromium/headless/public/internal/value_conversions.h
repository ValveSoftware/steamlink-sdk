// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_INTERNAL_VALUE_CONVERSIONS_H_
#define HEADLESS_PUBLIC_INTERNAL_VALUE_CONVERSIONS_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "headless/public/util/error_reporter.h"

namespace headless {
namespace internal {

// Generic conversion from a type to a base::Value. Implemented in
// type_conversions.h after all type-specific ToValueImpls have been defined.
template <typename T>
std::unique_ptr<base::Value> ToValue(const T& value);

// Generic conversion from a base::Value to a type. Note that this generic
// variant is never defined. Instead, we declare a specific template
// specialization for all the used types.
template <typename T>
struct FromValue {
  static std::unique_ptr<T> Parse(const base::Value& value,
                                  ErrorReporter* errors);
};

// ToValueImpl is a helper used by the ToValue template for dispatching into
// type-specific serializers. It uses a dummy |T*| argument as a way to
// partially specialize vector types.
template <typename T>
std::unique_ptr<base::Value> ToValueImpl(int value, T*) {
  return base::WrapUnique(new base::FundamentalValue(value));
}

template <typename T>
std::unique_ptr<base::Value> ToValueImpl(double value, T*) {
  return base::WrapUnique(new base::FundamentalValue(value));
}

template <typename T>
std::unique_ptr<base::Value> ToValueImpl(bool value, T*) {
  return base::WrapUnique(new base::FundamentalValue(value));
}

template <typename T>
std::unique_ptr<base::Value> ToValueImpl(const std::string& value, T*) {
  return base::WrapUnique(new base::StringValue(value));
}

template <typename T>
std::unique_ptr<base::Value> ToValueImpl(const base::Value& value, T*) {
  return value.CreateDeepCopy();
}

template <typename T>
std::unique_ptr<base::Value> ToValueImpl(const std::vector<T>& vector,
                                         const std::vector<T>*) {
  std::unique_ptr<base::ListValue> result(new base::ListValue());
  for (const auto& it : vector)
    result->Append(ToValue(it));
  return std::move(result);
}

template <typename T>
std::unique_ptr<base::Value> ToValueImpl(const std::unique_ptr<T>& value,
                                         std::unique_ptr<T>*) {
  return ToValue(value.get());
}

// FromValue specializations for basic types.
template <>
struct FromValue<bool> {
  static bool Parse(const base::Value& value, ErrorReporter* errors) {
    bool result = false;
    if (!value.GetAsBoolean(&result))
      errors->AddError("boolean value expected");
    return result;
  }
};

template <>
struct FromValue<int> {
  static int Parse(const base::Value& value, ErrorReporter* errors) {
    int result = 0;
    if (!value.GetAsInteger(&result))
      errors->AddError("integer value expected");
    return result;
  }
};

template <>
struct FromValue<double> {
  static double Parse(const base::Value& value, ErrorReporter* errors) {
    double result = 0;
    if (!value.GetAsDouble(&result))
      errors->AddError("double value expected");
    return result;
  }
};

template <>
struct FromValue<std::string> {
  static std::string Parse(const base::Value& value, ErrorReporter* errors) {
    std::string result;
    if (!value.GetAsString(&result))
      errors->AddError("string value expected");
    return result;
  }
};

template <>
struct FromValue<base::DictionaryValue> {
  static std::unique_ptr<base::DictionaryValue> Parse(const base::Value& value,
                                                      ErrorReporter* errors) {
    const base::DictionaryValue* result;
    if (!value.GetAsDictionary(&result)) {
      errors->AddError("dictionary value expected");
      return nullptr;
    }
    return result->CreateDeepCopy();
  }
};

template <>
struct FromValue<base::Value> {
  static std::unique_ptr<base::Value> Parse(const base::Value& value,
                                            ErrorReporter* errors) {
    return value.CreateDeepCopy();
  }
};

template <typename T>
struct FromValue<std::unique_ptr<T>> {
  static std::unique_ptr<T> Parse(const base::Value& value,
                                  ErrorReporter* errors) {
    return FromValue<T>::Parse(value, errors);
  }
};

template <typename T>
struct FromValue<std::vector<T>> {
  static std::vector<T> Parse(const base::Value& value, ErrorReporter* errors) {
    std::vector<T> result;
    const base::ListValue* list;
    if (!value.GetAsList(&list)) {
      errors->AddError("list value expected");
      return result;
    }
    errors->Push();
    for (const auto& item : *list)
      result.push_back(FromValue<T>::Parse(*item, errors));
    errors->Pop();
    return result;
  }
};

}  // namespace internal
}  // namespace headless

#endif  // HEADLESS_PUBLIC_INTERNAL_VALUE_CONVERSIONS_H_
