// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PtrUtil_h
#define PtrUtil_h

#include "base/memory/ptr_util.h"
#include "wtf/TypeTraits.h"

#include <memory>

namespace WTF {

template <typename T>
std::unique_ptr<T> wrapUnique(T* ptr) {
  static_assert(
      !WTF::IsGarbageCollectedType<T>::value,
      "Garbage collected types should not be stored in std::unique_ptr!");
  return std::unique_ptr<T>(ptr);
}

template <typename T>
std::unique_ptr<T[]> wrapArrayUnique(T* ptr) {
  static_assert(
      !WTF::IsGarbageCollectedType<T>::value,
      "Garbage collected types should not be stored in std::unique_ptr!");
  return std::unique_ptr<T[]>(ptr);
}

// WTF::makeUnique is base::MakeUnique. See base/ptr_util.h for documentation.
template <typename T, typename... Args>
auto makeUnique(Args&&... args)
    -> decltype(base::MakeUnique<T>(std::forward<Args>(args)...)) {
  static_assert(
      !WTF::IsGarbageCollectedType<T>::value,
      "Garbage collected types should not be stored in std::unique_ptr!");
  return base::MakeUnique<T>(std::forward<Args>(args)...);
}

template <typename T>
auto makeUnique(size_t size) -> decltype(base::MakeUnique<T>(size)) {
  static_assert(
      !WTF::IsGarbageCollectedType<T>::value,
      "Garbage collected types should not be stored in std::unique_ptr!");
  return base::MakeUnique<T>(size);
}

}  // namespace WTF

using WTF::makeUnique;
using WTF::wrapUnique;
using WTF::wrapArrayUnique;

#endif  // PtrUtil_h
