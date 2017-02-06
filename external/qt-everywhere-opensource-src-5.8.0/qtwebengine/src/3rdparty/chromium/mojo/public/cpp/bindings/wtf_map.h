// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_WTF_MAP_H_
#define MOJO_PUBLIC_CPP_BINDINGS_WTF_MAP_H_

#include <stddef.h>
#include <utility>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/WebKit/Source/wtf/HashMap.h"
#include "third_party/WebKit/Source/wtf/text/StringHash.h"

namespace mojo {

// Represents a map backed by WTF::HashMap. Comparing with WTF::HashMap,
// mojo::WTFMap is move-only and can be null.
//
// It is easy to convert between WTF::HashMap<K, V> and mojo::WTFMap<K, V>:
//   - constructor WTFMap(WTF::HashMap<K, V>&&) takes the contents of a
//     WTF::HashMap<K, V>;
//   - method PassStorage() passes the underlying WTF::HashMap.
//
// NOTE: WTF::HashMap disallows certain key values. For integer types, those are
// 0 and -1 (max value instead of -1 for unsigned). For string, that is null.
template <typename Key, typename Value>
class WTFMap {
 public:
  using Iterator = typename WTF::HashMap<Key, Value>::iterator;
  using ConstIterator = typename WTF::HashMap<Key, Value>::const_iterator;

  // Constructs an empty map.
  WTFMap() : is_null_(false) {}
  // Constructs a null map.
  WTFMap(std::nullptr_t null_pointer) : is_null_(true) {}

  ~WTFMap() {}

  WTFMap(WTF::HashMap<Key, Value>&& other)
      : map_(std::move(other)), is_null_(false) {}
  WTFMap(WTFMap&& other) : is_null_(true) { Take(&other); }

  WTFMap& operator=(WTF::HashMap<Key, Value>&& other) {
    is_null_ = false;
    map_ = std::move(other);
    return *this;
  }
  WTFMap& operator=(WTFMap&& other) {
    Take(&other);
    return *this;
  }

  WTFMap& operator=(std::nullptr_t null_pointer) {
    is_null_ = true;
    map_.clear();
    return *this;
  }

  static bool IsValidKey(const Key& key) {
    return WTF::HashMap<Key, Value>::isValidKey(key);
  }

  // Copies the contents of some other type of map into a new WTFMap using a
  // TypeConverter.
  template <typename U>
  static WTFMap From(const U& other) {
    return TypeConverter<WTFMap, U>::Convert(other);
  }

  // Copies the contents of the WTFMap into some other type of map.
  template <typename U>
  U To() const {
    return TypeConverter<U, WTFMap>::Convert(*this);
  }

  // Indicates whether the map is null (which is distinct from empty).
  bool is_null() const { return is_null_; }

  // Indicates whether the map is empty (which is distinct from null).
  bool empty() const { return map_.isEmpty() && !is_null_; }

  // Indicates the number of keys in the map, which will be zero if the map is
  // null.
  size_t size() const { return map_.size(); }

  // Inserts a key-value pair into the map. Like WTF::HashMap::add(), this does
  // not insert |value| if |key| is already a member of the map.
  void insert(const Key& key, const Value& value) {
    is_null_ = false;
    map_.add(key, value);
  }
  void insert(const Key& key, Value&& value) {
    is_null_ = false;
    map_.add(key, std::move(value));
  }

  // Returns a reference to the value associated with the specified key,
  // crashing the process if the key is not present in the map.
  Value& at(const Key& key) { return map_.find(key)->value; }
  const Value& at(const Key& key) const { return map_.find(key)->value; }

  // Returns a reference to the value associated with the specified key,
  // creating a new entry if the key is not already present in the map. A
  // newly-created value will be value-initialized (meaning that it will be
  // initialized by the default constructor of the value type, if any, or else
  // will be zero-initialized).
  Value& operator[](const Key& key) {
    is_null_ = false;
    if (!map_.contains(key))
      map_.add(key, Value());
    return at(key);
  }

  // Sets the map to empty (even if previously it was null).
  void SetToEmpty() {
    is_null_ = false;
    map_.clear();
  }

  // Returns a const reference to the WTF::HashMap managed by this class. If
  // this object is null, the return value will be an empty map.
  const WTF::HashMap<Key, Value>& storage() const { return map_; }

  // Passes the underlying storage and resets this map to null.
  WTF::HashMap<Key, Value> PassStorage() {
    is_null_ = true;
    return std::move(map_);
  }

  // Swaps the contents of this WTFMap with another WTFMap of the same type
  // (including nullness).
  void Swap(WTFMap<Key, Value>* other) {
    std::swap(is_null_, other->is_null_);
    map_.swap(other->map_);
  }

  // Swaps the contents of this WTFMap with an WTF::HashMap containing keys and
  // values of the same type. Since WTF::HashMap cannot represent the null
  // state, the WTF::HashMap will be empty if WTFMap is null. The WTFMap will
  // always be left in a non-null state.
  void Swap(WTF::HashMap<Key, Value>* other) {
    is_null_ = false;
    map_.swap(*other);
  }

  // Returns a new WTFMap that contains a copy of the contents of this map. If
  // the key/value type defines a Clone() method, it will be used; otherwise
  // copy constructor/assignment will be used.
  //
  // Please note that calling this method will fail compilation if the key/value
  // type cannot be cloned (which usually means that it is a Mojo handle type or
  // a type containing Mojo handles).
  WTFMap Clone() const {
    WTFMap result;
    result.is_null_ = is_null_;
    auto map_end = map_.end();
    for (auto it = map_.begin(); it != map_end; ++it)
      result.map_.add(internal::Clone(it->key), internal::Clone(it->value));
    return result;
  }

  // Indicates whether the contents of this map are equal to those of another
  // WTFMap (including nullness). If the key/value type defines an Equals()
  // method, it will be used; otherwise == operator will be used.
  bool Equals(const WTFMap& other) const {
    if (is_null() != other.is_null())
      return false;
    if (size() != other.size())
      return false;

    auto this_end = map_.end();
    auto other_end = other.map_.end();

    for (auto iter = map_.begin(); iter != this_end; ++iter) {
      auto other_iter = other.map_.find(iter->key);
      if (other_iter == other_end ||
          !internal::Equals(iter->value, other_iter->value)) {
        return false;
      }
    }
    return true;
  }

  ConstIterator begin() const { return map_.begin(); }
  Iterator begin() { return map_.begin(); }

  ConstIterator end() const { return map_.end(); }
  Iterator end() { return map_.end(); }

  // Returns the iterator pointing to the entry for |key|, if present, or else
  // returns end().
  ConstIterator find(const Key& key) const { return map_.find(key); }
  Iterator find(const Key& key) { return map_.find(key); }

  explicit operator bool() const { return !is_null_; }

 private:
  void Take(WTFMap* other) {
    operator=(nullptr);
    Swap(other);
  }

  WTF::HashMap<Key, Value> map_;
  bool is_null_;

  DISALLOW_COPY_AND_ASSIGN(WTFMap);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_WTF_MAP_H_
