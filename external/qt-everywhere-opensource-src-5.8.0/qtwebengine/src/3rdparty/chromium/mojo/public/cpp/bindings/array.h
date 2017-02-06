// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ARRAY_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ARRAY_H_

#include <stddef.h>
#include <string.h>
#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

// Represents a moveable array with contents of type |T|. The array can be null,
// meaning that no value has been assigned to it. Null is distinct from empty.
template <typename T>
class Array {
 public:
  using ConstRefType = typename std::vector<T>::const_reference;
  using RefType = typename std::vector<T>::reference;

  using Element = T;

  using iterator = typename std::vector<T>::iterator;
  using const_iterator = typename std::vector<T>::const_iterator;

  // Constructs an empty array.
  Array() : is_null_(false) {}
  // Constructs a null array.
  Array(std::nullptr_t null_pointer) : is_null_(true) {}

  // Constructs a new non-null array of the specified size. The elements will
  // be value-initialized (meaning that they will be initialized by their
  // default constructor, if any, or else zero-initialized).
  explicit Array(size_t size) : vec_(size), is_null_(false) {}
  ~Array() {}

  // Copies the contents of |other| into this array.
  Array(const std::vector<T>& other) : vec_(other), is_null_(false) {}

  // Moves the contents of |other| into this array.
  Array(std::vector<T>&& other) : vec_(std::move(other)), is_null_(false) {}
  Array(Array&& other) : is_null_(true) { Take(&other); }

  Array& operator=(std::vector<T>&& other) {
    vec_ = std::move(other);
    is_null_ = false;
    return *this;
  }
  Array& operator=(Array&& other) {
    Take(&other);
    return *this;
  }

  Array& operator=(std::nullptr_t null_pointer) {
    is_null_ = true;
    vec_.clear();
    return *this;
  }

  // Creates a non-null array of the specified size. The elements will be
  // value-initialized (meaning that they will be initialized by their default
  // constructor, if any, or else zero-initialized).
  static Array New(size_t size) { return Array(size); }

  // Creates a new array with a copy of the contents of |other|.
  template <typename U>
  static Array From(const U& other) {
    return TypeConverter<Array, U>::Convert(other);
  }

  // Copies the contents of this array to a new object of type |U|.
  template <typename U>
  U To() const {
    return TypeConverter<U, Array>::Convert(*this);
  }

  // Indicates whether the array is null (which is distinct from empty).
  bool is_null() const { return is_null_; }

  // Indicates whether the array is empty (which is distinct from null).
  bool empty() const { return vec_.empty() && !is_null_; }

  // Returns a reference to the first element of the array. Calling this on a
  // null or empty array causes undefined behavior.
  ConstRefType front() const { return vec_.front(); }
  RefType front() { return vec_.front(); }

  iterator begin() { return vec_.begin(); }
  const_iterator begin() const { return vec_.begin(); }
  iterator end() { return vec_.end(); }
  const_iterator end() const { return vec_.end(); }

  // Returns the size of the array, which will be zero if the array is null.
  size_t size() const { return vec_.size(); }

  // Returns a reference to the element at zero-based |offset|. Calling this on
  // an array with size less than |offset|+1 causes undefined behavior.
  ConstRefType at(size_t offset) const { return vec_.at(offset); }
  ConstRefType operator[](size_t offset) const { return at(offset); }
  RefType at(size_t offset) { return vec_.at(offset); }
  RefType operator[](size_t offset) { return at(offset); }

  // Pushes |value| onto the back of the array. If this array was null, it will
  // become non-null with a size of 1.
  void push_back(const T& value) {
    is_null_ = false;
    vec_.push_back(value);
  }
  void push_back(T&& value) {
    is_null_ = false;
    vec_.push_back(std::move(value));
  }

  // Resizes the array to |size| and makes it non-null. Otherwise, works just
  // like the resize method of |std::vector|.
  void resize(size_t size) {
    is_null_ = false;
    vec_.resize(size);
  }

  // Sets the array to empty (even if previously it was null.)
  void SetToEmpty() { resize(0); }

  // Returns a const reference to the |std::vector| managed by this class. If
  // the array is null, this will be an empty vector.
  const std::vector<T>& storage() const { return vec_; }

  // Passes the underlying storage and resets this array to null.
  //
  // TODO(yzshen): Consider changing this to a rvalue-ref-qualified conversion
  // to std::vector<T> after we move to MSVC 2015.
  std::vector<T> PassStorage() {
    is_null_ = true;
    return std::move(vec_);
  }

  void Swap(Array* other) {
    std::swap(is_null_, other->is_null_);
    vec_.swap(other->vec_);
  }

  // Swaps the contents of this array with the specified vector, making this
  // array non-null. Since the vector cannot represent null, it will just be
  // made empty if this array is null.
  void Swap(std::vector<T>* other) {
    is_null_ = false;
    vec_.swap(*other);
  }

  // Returns a copy of the array where each value of the new array has been
  // "cloned" from the corresponding value of this array. If the element type
  // defines a Clone() method, it will be used; otherwise copy
  // constructor/assignment will be used.
  //
  // Please note that calling this method will fail compilation if the element
  // type cannot be cloned (which usually means that it is a Mojo handle type or
  // a type containing Mojo handles).
  Array Clone() const {
    Array result;
    result.is_null_ = is_null_;
    result.vec_.reserve(vec_.size());
    for (const auto& element : vec_)
      result.vec_.push_back(internal::Clone(element));
    return std::move(result);
  }

  // Indicates whether the contents of this array are equal to |other|. A null
  // array is only equal to another null array. If the element type defines an
  // Equals() method, it will be used; otherwise == operator will be used.
  bool Equals(const Array& other) const {
    if (is_null() != other.is_null())
      return false;
    if (size() != other.size())
      return false;
    for (size_t i = 0; i < size(); ++i) {
      if (!internal::Equals(at(i), other.at(i)))
        return false;
    }
    return true;
  }

 private:
  typedef std::vector<T> Array::*Testable;

 public:
  operator Testable() const { return is_null_ ? 0 : &Array::vec_; }

 private:
  // Forbid the == and != operators explicitly, otherwise Array will be
  // converted to Testable to do == or != comparison.
  template <typename U>
  bool operator==(const Array<U>& other) const = delete;
  template <typename U>
  bool operator!=(const Array<U>& other) const = delete;

  void Take(Array* other) {
    operator=(nullptr);
    Swap(other);
  }

  std::vector<T> vec_;
  bool is_null_;

  DISALLOW_COPY_AND_ASSIGN(Array);
};

// A |TypeConverter| that will create an |Array<T>| containing a copy of the
// contents of an |std::vector<E>|, using |TypeConverter<T, E>| to copy each
// element. The returned array will always be non-null.
template <typename T, typename E>
struct TypeConverter<Array<T>, std::vector<E>> {
  static Array<T> Convert(const std::vector<E>& input) {
    Array<T> result(input.size());
    for (size_t i = 0; i < input.size(); ++i)
      result[i] = TypeConverter<T, E>::Convert(input[i]);
    return std::move(result);
  }
};

// A |TypeConverter| that will create an |std::vector<E>| containing a copy of
// the contents of an |Array<T>|, using |TypeConverter<E, T>| to copy each
// element. If the input array is null, the output vector will be empty.
template <typename E, typename T>
struct TypeConverter<std::vector<E>, Array<T>> {
  static std::vector<E> Convert(const Array<T>& input) {
    std::vector<E> result;
    if (!input.is_null()) {
      result.resize(input.size());
      for (size_t i = 0; i < input.size(); ++i)
        result[i] = TypeConverter<E, T>::Convert(input[i]);
    }
    return result;
  }
};

// A |TypeConverter| that will create an |Array<T>| containing a copy of the
// contents of an |std::set<E>|, using |TypeConverter<T, E>| to copy each
// element. The returned array will always be non-null.
template <typename T, typename E>
struct TypeConverter<Array<T>, std::set<E>> {
  static Array<T> Convert(const std::set<E>& input) {
    Array<T> result;
    for (auto i : input)
      result.push_back(TypeConverter<T, E>::Convert(i));
    return std::move(result);
  }
};

// A |TypeConverter| that will create an |std::set<E>| containing a copy of
// the contents of an |Array<T>|, using |TypeConverter<E, T>| to copy each
// element. If the input array is null, the output set will be empty.
template <typename E, typename T>
struct TypeConverter<std::set<E>, Array<T>> {
  static std::set<E> Convert(const Array<T>& input) {
    std::set<E> result;
    if (!input.is_null()) {
      for (size_t i = 0; i < input.size(); ++i)
        result.insert(TypeConverter<E, T>::Convert(input[i]));
    }
    return result;
  }
};

// Less than operator to allow Arrays as keys in std maps and sets.
template <typename T>
inline bool operator<(const Array<T>& a, const Array<T>& b) {
  if (a.is_null())
    return !b.is_null();
  if (b.is_null())
    return false;
  return a.storage() < b.storage();
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ARRAY_H_
