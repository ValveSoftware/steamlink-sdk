// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRING_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRING_H_

#include <assert.h>

#include <string>

#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {

class String {
 public:
  typedef internal::String_Data Data_;

  String() : is_null_(true) {}
  String(const std::string& str) : value_(str), is_null_(false) {}
  String(const char* chars) : is_null_(!chars) {
    if (chars)
      value_ = chars;
  }
  String(const char* chars, size_t num_chars)
      : value_(chars, num_chars),
        is_null_(false) {
  }
  template <size_t N>
  String(const char chars[N]) : value_(chars, N-1), is_null_(false) {}

  template <typename U>
  static String From(const U& other) {
    return TypeConverter<String, U>::ConvertFrom(other);
  }

  template <typename U>
  U To() const {
    return TypeConverter<String, U>::ConvertTo(*this);
  }

  String& operator=(const std::string& str) {
    value_ = str;
    is_null_ = false;
    return *this;
  }
  String& operator=(const char* chars) {
    is_null_ = !chars;
    if (chars) {
      value_ = chars;
    } else {
      value_.clear();
    }
    return *this;
  }

  void reset() {
    value_.clear();
    is_null_ = true;
  }

  bool is_null() const { return is_null_; }

  size_t size() const { return value_.size(); }

  const char* data() const { return value_.data(); }

  const char& at(size_t offset) const { return value_.at(offset); }
  const char& operator[](size_t offset) const { return value_[offset]; }

  const std::string& get() const { return value_; }
  operator const std::string&() const { return value_; }

  void Swap(String* other) {
    std::swap(is_null_, other->is_null_);
    value_.swap(other->value_);
  }

  void Swap(std::string* other) {
    is_null_ = false;
    value_.swap(*other);
  }

 private:
  typedef std::string String::*Testable;

 public:
  operator Testable() const { return is_null_ ? 0 : &String::value_; }

 private:
  std::string value_;
  bool is_null_;
};

inline bool operator==(const String& a, const String& b) {
  return a.is_null() == b.is_null() && a.get() == b.get();
}
inline bool operator==(const char* a, const String& b) {
  return !b.is_null() && a == b.get();
}
inline bool operator==(const String& a, const char* b) {
  return !a.is_null() && a.get() == b;
}
inline bool operator!=(const String& a, const String& b) { return !(a == b); }
inline bool operator!=(const char* a, const String& b) { return !(a == b); }
inline bool operator!=(const String& a, const char* b) { return !(a == b); }

// TODO(darin): Add similar variants of operator<,<=,>,>=

template <>
class TypeConverter<String, std::string> {
 public:
  static String ConvertFrom(const std::string& input) {
    return String(input);
  }
  static std::string ConvertTo(const String& input) {
    return input;
  }
};

template <size_t N>
class TypeConverter<String, char[N]> {
 public:
  static String ConvertFrom(const char input[N]) {
    assert(input);
    return String(input, N-1);
  }
};

// Appease MSVC.
template <size_t N>
class TypeConverter<String, const char[N]> {
 public:
  static String ConvertFrom(const char input[N]) {
    assert(input);
    return String(input, N-1);
  }
};

template <>
class TypeConverter<String, const char*> {
 public:
  // |input| may be null, in which case a null String will be returned.
  static String ConvertFrom(const char* input) {
    return String(input);
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRING_H_
