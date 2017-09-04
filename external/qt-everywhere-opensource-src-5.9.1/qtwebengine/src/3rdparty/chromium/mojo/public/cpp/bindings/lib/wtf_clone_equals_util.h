// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_WTF_CLONE_EQUALS_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_WTF_CLONE_EQUALS_UTIL_H_

#include <type_traits>

#include "mojo/public/cpp/bindings/lib/clone_equals_util.h"
#include "third_party/WebKit/Source/wtf/HashMap.h"
#include "third_party/WebKit/Source/wtf/Optional.h"
#include "third_party/WebKit/Source/wtf/Vector.h"
#include "third_party/WebKit/Source/wtf/text/WTFString.h"

namespace mojo {
namespace internal {

template <typename T>
struct CloneTraits<WTF::Vector<T>, false> {
  static WTF::Vector<T> Clone(const WTF::Vector<T>& input) {
    WTF::Vector<T> result;
    result.reserveCapacity(input.size());
    for (const auto& element : input)
      result.append(internal::Clone(element));

    return result;
  }
};

template <typename K, typename V>
struct CloneTraits<WTF::HashMap<K, V>, false> {
  static WTF::HashMap<K, V> Clone(const WTF::HashMap<K, V>& input) {
    WTF::HashMap<K, V> result;
    auto input_end = input.end();
    for (auto it = input.begin(); it != input_end; ++it)
      result.add(internal::Clone(it->key), internal::Clone(it->value));
    return result;
  }
};

template <typename T>
struct EqualsTraits<WTF::Vector<T>, false> {
  static bool Equals(const WTF::Vector<T>& a, const WTF::Vector<T>& b) {
    if (a.size() != b.size())
      return false;
    for (size_t i = 0; i < a.size(); ++i) {
      if (!internal::Equals(a[i], b[i]))
        return false;
    }
    return true;
  }
};

template <typename K, typename V>
struct EqualsTraits<WTF::HashMap<K, V>, false> {
  static bool Equals(const WTF::HashMap<K, V>& a, const WTF::HashMap<K, V>& b) {
    if (a.size() != b.size())
      return false;

    auto a_end = a.end();
    auto b_end = b.end();

    for (auto iter = a.begin(); iter != a_end; ++iter) {
      auto b_iter = b.find(iter->key);
      if (b_iter == b_end || !internal::Equals(iter->value, b_iter->value))
        return false;
    }
    return true;
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_WTF_CLONE_EQUALS_UTIL_H_
