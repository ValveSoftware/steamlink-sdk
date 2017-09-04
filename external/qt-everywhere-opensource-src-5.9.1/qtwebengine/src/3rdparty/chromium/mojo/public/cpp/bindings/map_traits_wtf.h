// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_WTF_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_WTF_H_

#include "base/logging.h"
#include "mojo/public/cpp/bindings/map_traits.h"
#include "mojo/public/cpp/bindings/wtf_map.h"

namespace mojo {

template <typename K, typename V>
struct MapTraits<WTFMap<K, V>> {
  using Key = K;
  using Value = V;
  using Iterator = typename WTFMap<K, V>::Iterator;
  using ConstIterator = typename WTFMap<K, V>::ConstIterator;

  static bool IsNull(const WTFMap<K, V>& input) { return input.is_null(); }
  static void SetToNull(WTFMap<K, V>* output) { *output = nullptr; }

  static size_t GetSize(const WTFMap<K, V>& input) { return input.size(); }

  static ConstIterator GetBegin(const WTFMap<K, V>& input) {
    return input.begin();
  }
  static Iterator GetBegin(WTFMap<K, V>& input) { return input.begin(); }

  static void AdvanceIterator(ConstIterator& iterator) { ++iterator; }
  static void AdvanceIterator(Iterator& iterator) { ++iterator; }

  static const K& GetKey(Iterator& iterator) { return iterator->key; }
  static const K& GetKey(ConstIterator& iterator) { return iterator->key; }

  static V& GetValue(Iterator& iterator) { return iterator->value; }
  static const V& GetValue(ConstIterator& iterator) { return iterator->value; }

  static bool Insert(WTFMap<K, V>& input, const K& key, V&& value) {
    if (!WTFMap<K, V>::IsValidKey(key)) {
      LOG(ERROR) << "The key value is disallowed by WTF::HashMap: " << key;
      return false;
    }
    input.insert(key, std::forward<V>(value));
    return true;
  }
  static bool Insert(WTFMap<K, V>& input, const K& key, const V& value) {
    if (!WTFMap<K, V>::IsValidKey(key)) {
      LOG(ERROR) << "The key value is disallowed by WTF::HashMap: " << key;
      return false;
    }
    input.insert(key, value);
    return true;
  }

  static void SetToEmpty(WTFMap<K, V>* output) { output->SetToEmpty(); }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_WTF_H_
