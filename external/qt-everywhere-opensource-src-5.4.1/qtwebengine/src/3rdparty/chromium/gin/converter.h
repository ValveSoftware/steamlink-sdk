// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_CONVERTER_H_
#define GIN_CONVERTER_H_

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "gin/gin_export.h"
#include "v8/include/v8.h"

namespace gin {

template<typename T, typename Enable = void>
struct Converter {};

template<>
struct GIN_EXPORT Converter<bool> {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    bool val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     bool* out);
};

template<>
struct GIN_EXPORT Converter<int32_t> {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    int32_t val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     int32_t* out);
};

template<>
struct GIN_EXPORT Converter<uint32_t> {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    uint32_t val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     uint32_t* out);
};

template<>
struct GIN_EXPORT Converter<int64_t> {
  // Warning: JavaScript cannot represent 64 integers precisely.
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    int64_t val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     int64_t* out);
};

template<>
struct GIN_EXPORT Converter<uint64_t> {
  // Warning: JavaScript cannot represent 64 integers precisely.
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    uint64_t val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     uint64_t* out);
};

template<>
struct GIN_EXPORT Converter<float> {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    float val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     float* out);
};

template<>
struct GIN_EXPORT Converter<double> {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    double val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     double* out);
};

template<>
struct GIN_EXPORT Converter<base::StringPiece> {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    const base::StringPiece& val);
  // No conversion out is possible because StringPiece does not contain storage.
};

template<>
struct GIN_EXPORT Converter<std::string> {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    const std::string& val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     std::string* out);
};

template<>
struct GIN_EXPORT Converter<v8::Handle<v8::Function> > {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     v8::Handle<v8::Function>* out);
};

template<>
struct GIN_EXPORT Converter<v8::Handle<v8::Object> > {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    v8::Handle<v8::Object> val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     v8::Handle<v8::Object>* out);
};

template<>
struct GIN_EXPORT Converter<v8::Handle<v8::ArrayBuffer> > {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    v8::Handle<v8::ArrayBuffer> val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     v8::Handle<v8::ArrayBuffer>* out);
};

template<>
struct GIN_EXPORT Converter<v8::Handle<v8::External> > {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    v8::Handle<v8::External> val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     v8::Handle<v8::External>* out);
};

template<>
struct GIN_EXPORT Converter<v8::Handle<v8::Value> > {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    v8::Handle<v8::Value> val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     v8::Handle<v8::Value>* out);
};

template<typename T>
struct Converter<std::vector<T> > {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    const std::vector<T>& val) {
    v8::Handle<v8::Array> result(
        v8::Array::New(isolate, static_cast<int>(val.size())));
    for (size_t i = 0; i < val.size(); ++i) {
      result->Set(static_cast<int>(i), Converter<T>::ToV8(isolate, val[i]));
    }
    return result;
  }

  static bool FromV8(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     std::vector<T>* out) {
    if (!val->IsArray())
      return false;

    std::vector<T> result;
    v8::Handle<v8::Array> array(v8::Handle<v8::Array>::Cast(val));
    uint32_t length = array->Length();
    for (uint32_t i = 0; i < length; ++i) {
      T item;
      if (!Converter<T>::FromV8(isolate, array->Get(i), &item))
        return false;
      result.push_back(item);
    }

    out->swap(result);
    return true;
  }
};

// Convenience functions that deduce T.
template<typename T>
v8::Handle<v8::Value> ConvertToV8(v8::Isolate* isolate, T input) {
  return Converter<T>::ToV8(isolate, input);
}

GIN_EXPORT inline v8::Handle<v8::String> StringToV8(
    v8::Isolate* isolate,
    const base::StringPiece& input) {
  return ConvertToV8(isolate, input).As<v8::String>();
}

GIN_EXPORT v8::Handle<v8::String> StringToSymbol(v8::Isolate* isolate,
                                                 const base::StringPiece& val);

template<typename T>
bool ConvertFromV8(v8::Isolate* isolate, v8::Handle<v8::Value> input,
                   T* result) {
  return Converter<T>::FromV8(isolate, input, result);
}

GIN_EXPORT std::string V8ToString(v8::Handle<v8::Value> value);

}  // namespace gin

#endif  // GIN_CONVERTER_H_
