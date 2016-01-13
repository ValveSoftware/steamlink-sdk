// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRUCT_PTR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRUCT_PTR_H_

#include <assert.h>

#include <new>

#include "mojo/public/cpp/system/macros.h"

namespace mojo {
namespace internal {

template <typename Struct>
class StructHelper {
 public:
  template <typename Ptr>
  static void Initialize(Ptr* ptr) { ptr->Initialize(); }
};

}  // namespace internal

template <typename Struct>
class StructPtr {
  MOJO_MOVE_ONLY_TYPE_FOR_CPP_03(StructPtr, RValue);
 public:
  typedef typename Struct::Data_ Data_;

  StructPtr() : ptr_(NULL) {}
  ~StructPtr() {
    delete ptr_;
  }

  StructPtr(RValue other) : ptr_(NULL) { Take(other.object); }
  StructPtr& operator=(RValue other) {
    Take(other.object);
    return *this;
  }

  template <typename U>
  U To() const {
    return TypeConverter<StructPtr, U>::ConvertTo(*this);
  }

  void reset() {
    if (ptr_) {
      delete ptr_;
      ptr_ = NULL;
    }
  }

  bool is_null() const { return ptr_ == NULL; }

  Struct& operator*() const {
    assert(ptr_);
    return *ptr_;
  }
  Struct* operator->() const {
    assert(ptr_);
    return ptr_;
  }
  Struct* get() const { return ptr_; }

  void Swap(StructPtr* other) {
    std::swap(ptr_, other->ptr_);
  }

 private:
  typedef Struct* StructPtr::*Testable;

 public:
  operator Testable() const { return ptr_ ? &StructPtr::ptr_ : 0; }

 private:
  friend class internal::StructHelper<Struct>;
  void Initialize() { assert(!ptr_); ptr_ = new Struct(); }

  void Take(StructPtr* other) {
    reset();
    Swap(other);
  }

  Struct* ptr_;
};

// Designed to be used when Struct is small and copyable.
template <typename Struct>
class InlinedStructPtr {
  MOJO_MOVE_ONLY_TYPE_FOR_CPP_03(InlinedStructPtr, RValue);
 public:
  typedef typename Struct::Data_ Data_;

  InlinedStructPtr() : is_null_(true) {}
  ~InlinedStructPtr() {}

  InlinedStructPtr(RValue other) : is_null_(true) { Take(other.object); }
  InlinedStructPtr& operator=(RValue other) {
    Take(other.object);
    return *this;
  }

  template <typename U>
  U To() const {
    return TypeConverter<InlinedStructPtr, U>::ConvertTo(*this);
  }

  void reset() {
    is_null_ = true;
    value_.~Struct();
    new (&value_) Struct();
  }

  bool is_null() const { return is_null_; }

  Struct& operator*() const {
    assert(!is_null_);
    return value_;
  }
  Struct* operator->() const {
    assert(!is_null_);
    return &value_;
  }
  Struct* get() const { return &value_; }

  void Swap(InlinedStructPtr* other) {
    std::swap(value_, other->value_);
    std::swap(is_null_, other->is_null_);
  }

 private:
  typedef Struct InlinedStructPtr::*Testable;

 public:
  operator Testable() const { return is_null_ ? 0 : &InlinedStructPtr::value_; }

 private:
  friend class internal::StructHelper<Struct>;
  void Initialize() { is_null_ = false; }

  void Take(InlinedStructPtr* other) {
    reset();
    Swap(other);
  }

  mutable Struct value_;
  bool is_null_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRUCT_PTR_H_
