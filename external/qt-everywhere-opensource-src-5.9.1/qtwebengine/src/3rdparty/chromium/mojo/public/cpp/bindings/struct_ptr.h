// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRUCT_PTR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRUCT_PTR_H_

#include <functional>
#include <new>

#include "base/logging.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/lib/hash_util.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {
namespace internal {

constexpr size_t kHashSeed = 31;

template <typename Struct>
class StructHelper {
 public:
  template <typename Ptr>
  static void Initialize(Ptr* ptr) {
    ptr->Initialize();
  }
};

template <typename Struct>
class StructPtrWTFHelper;

template <typename Struct>
class InlinedStructPtrWTFHelper;

}  // namespace internal

// Smart pointer wrapping a mojom structure with move-only semantics.
template <typename S>
class StructPtr {
 public:
  using Struct = S;

  StructPtr() : ptr_(nullptr) {}
  StructPtr(decltype(nullptr)) : ptr_(nullptr) {}

  ~StructPtr() { delete ptr_; }

  StructPtr& operator=(decltype(nullptr)) {
    reset();
    return *this;
  }

  StructPtr(StructPtr&& other) : ptr_(nullptr) { Take(&other); }
  StructPtr& operator=(StructPtr&& other) {
    Take(&other);
    return *this;
  }

  template <typename U>
  U To() const {
    return TypeConverter<U, StructPtr>::Convert(*this);
  }

  void reset() {
    if (ptr_) {
      delete ptr_;
      ptr_ = nullptr;
    }
  }

  bool is_null() const { return ptr_ == nullptr; }

  Struct& operator*() const {
    DCHECK(ptr_);
    return *ptr_;
  }
  Struct* operator->() const {
    DCHECK(ptr_);
    return ptr_;
  }
  Struct* get() const { return ptr_; }

  void Swap(StructPtr* other) { std::swap(ptr_, other->ptr_); }

  // Please note that calling this method will fail compilation if the value
  // type |Struct| doesn't have a Clone() method defined (which usually means
  // that it contains Mojo handles).
  StructPtr Clone() const { return is_null() ? StructPtr() : ptr_->Clone(); }

  // Compares the pointees (which might both be null).
  // TODO(tibell): Get rid of Equals in favor of the operator. Same for Hash.
  bool Equals(const StructPtr& other) const {
    if (is_null() || other.is_null())
      return is_null() && other.is_null();
    return ptr_->Equals(*other.ptr_);
  }

  // Hashes based on the pointee (which might be null).
  size_t Hash(size_t seed) const {
    if (is_null())
      return internal::HashCombine(seed, 0);
    return ptr_->Hash(seed);
  }

  explicit operator bool() const { return !is_null(); }

 private:
  friend class internal::StructHelper<Struct>;
  friend class internal::StructPtrWTFHelper<Struct>;

  void Initialize() {
    DCHECK(!ptr_);
    ptr_ = new Struct();
  }

  void Take(StructPtr* other) {
    reset();
    Swap(other);
  }

  Struct* ptr_;

  DISALLOW_COPY_AND_ASSIGN(StructPtr);
};

template <typename T>
bool operator==(const StructPtr<T>& lhs, const StructPtr<T>& rhs) {
  return lhs.Equals(rhs);
}
template <typename T>
bool operator!=(const StructPtr<T>& lhs, const StructPtr<T>& rhs) {
  return !(lhs == rhs);
}

// Designed to be used when Struct is small and copyable.
template <typename S>
class InlinedStructPtr {
 public:
  using Struct = S;

  InlinedStructPtr() : state_(NIL) {}
  InlinedStructPtr(decltype(nullptr)) : state_(NIL) {}

  ~InlinedStructPtr() {}

  InlinedStructPtr& operator=(decltype(nullptr)) {
    reset();
    return *this;
  }

  InlinedStructPtr(InlinedStructPtr&& other) : state_(NIL) { Take(&other); }
  InlinedStructPtr& operator=(InlinedStructPtr&& other) {
    Take(&other);
    return *this;
  }

  template <typename U>
  U To() const {
    return TypeConverter<U, InlinedStructPtr>::Convert(*this);
  }

  void reset() {
    state_ = NIL;
    value_. ~Struct();
    new (&value_) Struct();
  }

  bool is_null() const { return state_ == NIL; }

  Struct& operator*() const {
    DCHECK(state_ == VALID);
    return value_;
  }
  Struct* operator->() const {
    DCHECK(state_ == VALID);
    return &value_;
  }
  Struct* get() const { return &value_; }

  void Swap(InlinedStructPtr* other) {
    std::swap(value_, other->value_);
    std::swap(state_, other->state_);
  }

  InlinedStructPtr Clone() const {
    return is_null() ? InlinedStructPtr() : value_.Clone();
  }

  // Compares the pointees (which might both be null).
  bool Equals(const InlinedStructPtr& other) const {
    if (is_null() || other.is_null())
      return is_null() && other.is_null();
    return value_.Equals(other.value_);
  }

  // Hashes based on the pointee (which might be null).
  size_t Hash(size_t seed) const {
    if (is_null())
      return internal::HashCombine(seed, 0);
    return value_.Hash(seed);
  }

  explicit operator bool() const { return !is_null(); }

 private:
  friend class internal::StructHelper<Struct>;
  friend class internal::InlinedStructPtrWTFHelper<Struct>;

  void Initialize() { state_ = VALID; }

  void Take(InlinedStructPtr* other) {
    reset();
    Swap(other);
  }

  enum State {
    VALID,
    NIL,
    DELETED,  // For use in WTF::HashMap only
  };

  mutable Struct value_;
  State state_;

  DISALLOW_COPY_AND_ASSIGN(InlinedStructPtr);
};

template <typename T>
bool operator==(const InlinedStructPtr<T>& lhs,
                const InlinedStructPtr<T>& rhs) {
  return lhs.Equals(rhs);
}
template <typename T>
bool operator!=(const InlinedStructPtr<T>& lhs,
                const InlinedStructPtr<T>& rhs) {
  return !(lhs == rhs);
}

namespace internal {

template <typename Struct>
class StructPtrWTFHelper {
 public:
  static bool IsHashTableDeletedValue(const StructPtr<Struct>& value) {
    return value.ptr_ == reinterpret_cast<Struct*>(1u);
  }

  static void ConstructDeletedValue(mojo::StructPtr<Struct>& slot) {
    // |slot| refers to a previous, real value that got deleted and had its
    // destructor run, so this is the first time the "deleted value" has its
    // constructor called.
    //
    // Dirty trick: implant an invalid pointer in |ptr_|. Destructor isn't
    // called for deleted buckets, so this is okay.
    new (&slot) StructPtr<Struct>();
    slot.ptr_ = reinterpret_cast<Struct*>(1u);
  }
};

template <typename Struct>
class InlinedStructPtrWTFHelper {
 public:
  static bool IsHashTableDeletedValue(const InlinedStructPtr<Struct>& value) {
    return value.state_ == InlinedStructPtr<Struct>::DELETED;
  }

  static void ConstructDeletedValue(mojo::InlinedStructPtr<Struct>& slot) {
    // |slot| refers to a previous, real value that got deleted and had its
    // destructor run, so this is the first time the "deleted value" has its
    // constructor called.
    new (&slot) InlinedStructPtr<Struct>();
    slot.state_ = InlinedStructPtr<Struct>::DELETED;
  }
};

}  // namespace internal
}  // namespace mojo

namespace std {

template <typename T>
struct hash<mojo::StructPtr<T>> {
  size_t operator()(const mojo::StructPtr<T>& value) const {
    return value.Hash(mojo::internal::kHashSeed);
  }
};

template <typename T>
struct hash<mojo::InlinedStructPtr<T>> {
  size_t operator()(const mojo::InlinedStructPtr<T>& value) const {
    return value.Hash(mojo::internal::kHashSeed);
  }
};

}  // namespace std

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRUCT_PTR_H_
