// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_INTERNAL_H_

#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {
class String;

namespace internal {
template <typename T> class Array_Data;

#pragma pack(push, 1)

struct StructHeader {
  uint32_t num_bytes;
  uint32_t num_fields;
};
MOJO_COMPILE_ASSERT(sizeof(StructHeader) == 8, bad_sizeof_StructHeader);

struct ArrayHeader {
  uint32_t num_bytes;
  uint32_t num_elements;
};
MOJO_COMPILE_ASSERT(sizeof(ArrayHeader) == 8, bad_sizeof_ArrayHeader);

template <typename T>
union StructPointer {
  uint64_t offset;
  T* ptr;
};
MOJO_COMPILE_ASSERT(sizeof(StructPointer<char>) == 8, bad_sizeof_StructPointer);

template <typename T>
union ArrayPointer {
  uint64_t offset;
  Array_Data<T>* ptr;
};
MOJO_COMPILE_ASSERT(sizeof(ArrayPointer<char>) == 8, bad_sizeof_ArrayPointer);

union StringPointer {
  uint64_t offset;
  Array_Data<char>* ptr;
};
MOJO_COMPILE_ASSERT(sizeof(StringPointer) == 8, bad_sizeof_StringPointer);

#pragma pack(pop)

template <typename T>
void ResetIfNonNull(T* ptr) {
  if (ptr)
    *ptr = T();
}

template <typename T>
T FetchAndReset(T* ptr) {
  T temp = *ptr;
  *ptr = T();
  return temp;
}

template <typename H> struct IsHandle {
  static const bool value = IsBaseOf<Handle, H>::value;
};

template <typename T, bool move_only = IsMoveOnlyType<T>::value>
struct WrapperTraits;

template <typename T> struct WrapperTraits<T, false> {
  typedef T DataType;
};
template <typename H> struct WrapperTraits<ScopedHandleBase<H>, true> {
  typedef H DataType;
};
template <typename S> struct WrapperTraits<S, true> {
  typedef typename S::Data_* DataType;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_INTERNAL_H_
