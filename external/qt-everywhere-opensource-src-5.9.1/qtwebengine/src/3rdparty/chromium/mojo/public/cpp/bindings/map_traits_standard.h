// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_STANDARD_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_STANDARD_H_

#include "mojo/public/cpp/bindings/map.h"
#include "mojo/public/cpp/bindings/map_traits.h"

namespace mojo {

template <typename K, typename V>
struct MapTraits<Map<K, V>> {
  using Key = K;
  using Value = V;
  using Iterator = typename Map<K, V>::Iterator;
  using ConstIterator = typename Map<K, V>::ConstIterator;

  static bool IsNull(const Map<K, V>& input) { return input.is_null(); }
  static void SetToNull(Map<K, V>* output) { *output = nullptr; }

  static size_t GetSize(const Map<K, V>& input) { return input.size(); }

  static ConstIterator GetBegin(const Map<K, V>& input) {
    return input.begin();
  }
  static Iterator GetBegin(Map<K, V>& input) { return input.begin(); }

  static void AdvanceIterator(ConstIterator& iterator) { iterator++; }
  static void AdvanceIterator(Iterator& iterator) { iterator++; }

  static const K& GetKey(Iterator& iterator) { return iterator->first; }
  static const K& GetKey(ConstIterator& iterator) { return iterator->first; }

  static V& GetValue(Iterator& iterator) { return iterator->second; }
  static const V& GetValue(ConstIterator& iterator) { return iterator->second; }

  static bool Insert(Map<K, V>& input, const K& key, V&& value) {
    input.insert(key, std::forward<V>(value));
    return true;
  }
  static bool Insert(Map<K, V>& input, const K& key, const V& value) {
    input.insert(key, value);
    return true;
  }

  static void SetToEmpty(Map<K, V>* output) { output->SetToEmpty(); }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_STANDARD_H_
