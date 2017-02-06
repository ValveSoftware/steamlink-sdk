// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_WTF_ARRAY_H_
#define MOJO_PUBLIC_CPP_BINDINGS_WTF_ARRAY_H_

#include <stddef.h>
#include <utility>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/WebKit/Source/wtf/Vector.h"

namespace mojo {

// Represents an array backed by WTF::Vector. Comparing with WTF::Vector,
// mojo::WTFArray is move-only and can be null.
// It is easy to convert between WTF::Vector<T> and mojo::WTFArray<T>:
//   - constructor WTFArray(WTF::Vector<T>&&) takes the contents of a
//     WTF::Vector<T>;
//   - method PassStorage() passes the underlying WTF::Vector.
template <typename T>
class WTFArray {
 public:
  // Constructs an empty array.
  WTFArray() : is_null_(false) {}
  // Constructs a null array.
  WTFArray(std::nullptr_t null_pointer) : is_null_(true) {}

  // Constructs a new non-null array of the specified size. The elements will
  // be value-initialized (meaning that they will be initialized by their
  // default constructor, if any, or else zero-initialized).
  explicit WTFArray(size_t size) : vec_(size), is_null_(false) {}
  ~WTFArray() {}

  // Moves the contents of |other| into this array.
  WTFArray(WTF::Vector<T>&& other) : vec_(std::move(other)), is_null_(false) {}
  WTFArray(WTFArray&& other) : is_null_(true) { Take(&other); }

  WTFArray& operator=(WTF::Vector<T>&& other) {
    vec_ = std::move(other);
    is_null_ = false;
    return *this;
  }
  WTFArray& operator=(WTFArray&& other) {
    Take(&other);
    return *this;
  }

  WTFArray& operator=(std::nullptr_t null_pointer) {
    is_null_ = true;
    vec_.clear();
    return *this;
  }

  // Creates a non-null array of the specified size. The elements will be
  // value-initialized (meaning that they will be initialized by their default
  // constructor, if any, or else zero-initialized).
  static WTFArray New(size_t size) { return WTFArray(size); }

  // Creates a new array with a copy of the contents of |other|.
  template <typename U>
  static WTFArray From(const U& other) {
    return TypeConverter<WTFArray, U>::Convert(other);
  }

  // Copies the contents of this array to a new object of type |U|.
  template <typename U>
  U To() const {
    return TypeConverter<U, WTFArray>::Convert(*this);
  }

  // Indicates whether the array is null (which is distinct from empty).
  bool is_null() const {
    // When the array is set to null, the underlying storage |vec_| shouldn't
    // contain any elements.
    DCHECK(!is_null_ || vec_.isEmpty());
    return is_null_;
  }

  // Indicates whether the array is empty (which is distinct from null).
  bool empty() const { return vec_.isEmpty() && !is_null_; }

  // Returns a reference to the first element of the array. Calling this on a
  // null or empty array causes undefined behavior.
  const T& front() const { return vec_.first(); }
  T& front() { return vec_.first(); }

  // Returns the size of the array, which will be zero if the array is null.
  size_t size() const { return vec_.size(); }

  // Returns a reference to the element at zero-based |offset|. Calling this on
  // an array with size less than |offset|+1 causes undefined behavior.
  const T& at(size_t offset) const { return vec_.at(offset); }
  const T& operator[](size_t offset) const { return at(offset); }
  T& at(size_t offset) { return vec_.at(offset); }
  T& operator[](size_t offset) { return at(offset); }

  // Resizes the array to |size| and makes it non-null. Otherwise, works just
  // like the resize method of |WTF::Vector|.
  void resize(size_t size) {
    is_null_ = false;
    vec_.resize(size);
  }

  // Sets the array to empty (even if previously it was null.)
  void SetToEmpty() { resize(0); }

  // Returns a const reference to the |WTF::Vector| managed by this class. If
  // the array is null, this will be an empty vector.
  const WTF::Vector<T>& storage() const { return vec_; }

  // Passes the underlying storage and resets this array to null.
  //
  // TODO(yzshen): Consider changing this to a rvalue-ref-qualified conversion
  // to WTF::Vector<T> after we move to MSVC 2015.
  WTF::Vector<T> PassStorage() {
    is_null_ = true;
    return std::move(vec_);
  }

  void Swap(WTFArray* other) {
    std::swap(is_null_, other->is_null_);
    vec_.swap(other->vec_);
  }

  // Swaps the contents of this array with the specified vector, making this
  // array non-null. Since the vector cannot represent null, it will just be
  // made empty if this array is null.
  void Swap(WTF::Vector<T>* other) {
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
  WTFArray Clone() const {
    WTFArray result;
    result.is_null_ = is_null_;
    result.vec_.reserveCapacity(vec_.size());
    for (const auto& element : vec_)
      result.vec_.append(internal::Clone(element));
    return result;
  }

  // Indicates whether the contents of this array are equal to |other|. A null
  // array is only equal to another null array. If the element type defines an
  // Equals() method, it will be used; otherwise == operator will be used.
  bool Equals(const WTFArray& other) const {
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
  // TODO(dcheng): Use an explicit conversion operator.
  typedef WTF::Vector<T> WTFArray::*Testable;

 public:
  operator Testable() const {
    // When the array is set to null, the underlying storage |vec_| shouldn't
    // contain any elements.
    DCHECK(!is_null_ || vec_.isEmpty());
    return is_null_ ? 0 : &WTFArray::vec_;
  }

 private:
  // Forbid the == and != operators explicitly, otherwise WTFArray will be
  // converted to Testable to do == or != comparison.
  template <typename U>
  bool operator==(const WTFArray<U>& other) const = delete;
  template <typename U>
  bool operator!=(const WTFArray<U>& other) const = delete;

  void Take(WTFArray* other) {
    operator=(nullptr);
    Swap(other);
  }

  WTF::Vector<T> vec_;
  bool is_null_;

  DISALLOW_COPY_AND_ASSIGN(WTFArray);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_WTF_ARRAY_H_
