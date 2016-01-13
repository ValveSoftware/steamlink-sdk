// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ARRAY_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ARRAY_H_

#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

// Provides read-only access to array data.
template <typename T>
class Array {
  MOJO_MOVE_ONLY_TYPE_FOR_CPP_03(Array, RValue)
 public:
  typedef internal::ArrayTraits<T, internal::IsMoveOnlyType<T>::value>
      Traits;
  typedef typename Traits::ConstRefType ConstRefType;
  typedef typename Traits::RefType RefType;
  typedef typename Traits::StorageType StorageType;
  typedef typename Traits::ForwardType ForwardType;

  typedef internal::Array_Data<typename internal::WrapperTraits<T>::DataType>
      Data_;

  Array() : is_null_(true) {}
  explicit Array(size_t size) : vec_(size), is_null_(false) {
    Traits::Initialize(&vec_);
  }
  ~Array() { Traits::Finalize(&vec_); }

  Array(RValue other) : is_null_(true) { Take(other.object); }
  Array& operator=(RValue other) {
    Take(other.object);
    return *this;
  }

  static Array New(size_t size) {
    return Array(size).Pass();
  }

  template <typename U>
  static Array From(const U& other) {
    return TypeConverter<Array, U>::ConvertFrom(other);
  }

  template <typename U>
  U To() const {
    return TypeConverter<Array, U>::ConvertTo(*this);
  }

  void reset() {
    if (!vec_.empty()) {
      Traits::Finalize(&vec_);
      vec_.clear();
    }
    is_null_ = true;
  }

  bool is_null() const { return is_null_; }

  size_t size() const { return vec_.size(); }

  ConstRefType at(size_t offset) const { return Traits::at(&vec_, offset); }
  ConstRefType operator[](size_t offset) const { return at(offset); }

  RefType at(size_t offset) { return Traits::at(&vec_, offset); }
  RefType operator[](size_t offset) { return at(offset); }

  void push_back(ForwardType value) {
    is_null_ = false;
    Traits::PushBack(&vec_, value);
  }

  void resize(size_t size) {
    is_null_ = false;
    Traits::Resize(&vec_, size);
  }

  const std::vector<StorageType>& storage() const {
    return vec_;
  }
  operator const std::vector<StorageType>&() const {
    return vec_;
  }

  void Swap(Array* other) {
    std::swap(is_null_, other->is_null_);
    vec_.swap(other->vec_);
  }
  void Swap(std::vector<StorageType>* other) {
    is_null_ = false;
    vec_.swap(*other);
  }

 private:
  typedef std::vector<StorageType> Array::*Testable;

 public:
  operator Testable() const { return is_null_ ? 0 : &Array::vec_; }

 private:
  void Take(Array* other) {
    reset();
    Swap(other);
  }

  std::vector<StorageType> vec_;
  bool is_null_;
};

template <typename T, typename E>
class TypeConverter<Array<T>, std::vector<E> > {
 public:
  static Array<T> ConvertFrom(const std::vector<E>& input) {
    Array<T> result(input.size());
    for (size_t i = 0; i < input.size(); ++i)
      result[i] = TypeConverter<T, E>::ConvertFrom(input[i]);
    return result.Pass();
  }
  static std::vector<E> ConvertTo(const Array<T>& input) {
    std::vector<E> result;
    if (!input.is_null()) {
      result.resize(input.size());
      for (size_t i = 0; i < input.size(); ++i)
        result[i] = TypeConverter<T, E>::ConvertTo(input[i]);
    }
    return result;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ARRAY_H_
