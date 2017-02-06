// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MAP_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MAP_H_

#include <stddef.h>
#include <map>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/lib/map_data_internal.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

// A move-only map that can handle move-only values. Map has the following
// characteristics:
//   - The map itself can be null, and this is distinct from empty.
//   - Keys must not be move-only.
//   - The Key-type's "<" operator is used to sort the entries, and also is
//     used to determine equality of the key values.
//   - There can only be one entry per unique key.
//   - Values of move-only types will be moved into the Map when they are added
//     using the insert() method.
template <typename K, typename V>
class Map {
 public:
  using Key = K;
  using Value = V;

  // Map keys cannot be move only classes.
  static_assert(!internal::IsMoveOnlyType<Key>::value,
                "Map keys cannot be move only types.");

  using Iterator = typename std::map<Key, Value>::iterator;
  using ConstIterator = typename std::map<Key, Value>::const_iterator;

  // Constructs an empty map.
  Map() : is_null_(false) {}
  // Constructs a null map.
  Map(std::nullptr_t null_pointer) : is_null_(true) {}

  // Constructs a non-null Map containing the specified |keys| mapped to the
  // corresponding |values|.
  Map(mojo::Array<Key> keys, mojo::Array<Value> values) : is_null_(false) {
    DCHECK(keys.size() == values.size());
    for (size_t i = 0; i < keys.size(); ++i)
      map_.insert(std::make_pair(keys[i], std::move(values[i])));
  }

  ~Map() {}

  Map(std::map<Key, Value>&& other) : map_(std::move(other)), is_null_(false) {}
  Map(Map&& other) : is_null_(true) { Take(&other); }

  Map& operator=(std::map<Key, Value>&& other) {
    is_null_ = false;
    map_ = std::move(other);
    return *this;
  }
  Map& operator=(Map&& other) {
    Take(&other);
    return *this;
  }

  Map& operator=(std::nullptr_t null_pointer) {
    is_null_ = true;
    map_.clear();
    return *this;
  }

  // Copies the contents of some other type of map into a new Map using a
  // TypeConverter. A TypeConverter for std::map to Map is defined below.
  template <typename U>
  static Map From(const U& other) {
    return TypeConverter<Map, U>::Convert(other);
  }

  // Copies the contents of the Map into some other type of map. A TypeConverter
  // for Map to std::map is defined below.
  template <typename U>
  U To() const {
    return TypeConverter<U, Map>::Convert(*this);
  }

  // Indicates whether the map is null (which is distinct from empty).
  bool is_null() const { return is_null_; }

  // Indicates whether the map is empty (which is distinct from null).
  bool empty() const { return map_.empty() && !is_null_; }

  // Indicates the number of keys in the map, which will be zero if the map is
  // null.
  size_t size() const { return map_.size(); }

  // Inserts a key-value pair into the map. Like std::map, this does not insert
  // |value| if |key| is already a member of the map.
  void insert(const Key& key, const Value& value) {
    is_null_ = false;
    map_.insert(std::make_pair(key, value));
  }
  void insert(const Key& key, Value&& value) {
    is_null_ = false;
    map_.insert(std::make_pair(key, std::move(value)));
  }

  // Returns a reference to the value associated with the specified key,
  // crashing the process if the key is not present in the map.
  Value& at(const Key& key) { return map_.at(key); }
  const Value& at(const Key& key) const { return map_.at(key); }

  // Returns a reference to the value associated with the specified key,
  // creating a new entry if the key is not already present in the map. A
  // newly-created value will be value-initialized (meaning that it will be
  // initialized by the default constructor of the value type, if any, or else
  // will be zero-initialized).
  Value& operator[](const Key& key) {
    is_null_ = false;
    return map_[key];
  }

  // Sets the map to empty (even if previously it was null).
  void SetToEmpty() {
    is_null_ = false;
    map_.clear();
  }

  // Returns a const reference to the std::map managed by this class. If this
  // object is null, the return value will be an empty map.
  const std::map<Key, Value>& storage() const { return map_; }

  // Passes the underlying storage and resets this map to null.
  //
  // TODO(yzshen): Consider changing this to a rvalue-ref-qualified conversion
  // to std::map<Key, Value> after we move to MSVC 2015.
  std::map<Key, Value> PassStorage() {
    is_null_ = true;
    return std::move(map_);
  }

  // Swaps the contents of this Map with another Map of the same type (including
  // nullness).
  void Swap(Map<Key, Value>* other) {
    std::swap(is_null_, other->is_null_);
    map_.swap(other->map_);
  }

  // Swaps the contents of this Map with an std::map containing keys and values
  // of the same type. Since std::map cannot represent the null state, the
  // std::map will be empty if Map is null. The Map will always be left in a
  // non-null state.
  void Swap(std::map<Key, Value>* other) {
    is_null_ = false;
    map_.swap(*other);
  }

  // Removes all contents from the Map and places them into parallel key/value
  // arrays. Each key will be copied from the source to the destination, and
  // values will be copied unless their type is designated move-only, in which
  // case they will be moved. Either way, the Map will be left in a null state.
  void DecomposeMapTo(mojo::Array<Key>* keys, mojo::Array<Value>* values) {
    std::vector<Key> key_vector;
    key_vector.reserve(map_.size());
    std::vector<Value> value_vector;
    value_vector.reserve(map_.size());

    for (auto& entry : map_) {
      key_vector.push_back(entry.first);
      value_vector.push_back(std::move(entry.second));
    }

    map_.clear();
    is_null_ = true;

    keys->Swap(&key_vector);
    values->Swap(&value_vector);
  }

  // Returns a new Map that contains a copy of the contents of this map. If the
  // key/value type defines a Clone() method, it will be used; otherwise copy
  // constructor/assignment will be used.
  //
  // Please note that calling this method will fail compilation if the key/value
  // type cannot be cloned (which usually means that it is a Mojo handle type or
  // a type containing Mojo handles).
  Map Clone() const {
    Map result;
    result.is_null_ = is_null_;
    for (auto it = map_.begin(); it != map_.end(); ++it) {
      result.map_.insert(std::make_pair(internal::Clone(it->first),
                                        internal::Clone(it->second)));
    }
    return result;
  }

  // Indicates whether the contents of this map are equal to those of another
  // Map (including nullness). If the key/value type defines an Equals() method,
  // it will be used; otherwise == operator will be used.
  bool Equals(const Map& other) const {
    if (is_null() != other.is_null())
      return false;
    if (size() != other.size())
      return false;
    auto i = begin();
    auto j = other.begin();
    while (i != end()) {
      if (!internal::Equals(i->first, j->first))
        return false;
      if (!internal::Equals(i->second, j->second))
        return false;
      ++i;
      ++j;
    }
    return true;
  }

  // Provide read-only iteration over map members in a way similar to STL
  // collections.
  ConstIterator begin() const { return map_.begin(); }
  Iterator begin() { return map_.begin(); }

  ConstIterator end() const { return map_.end(); }
  Iterator end() { return map_.end(); }

  // Returns the iterator pointing to the entry for |key|, if present, or else
  // returns end().
  ConstIterator find(const Key& key) const { return map_.find(key); }
  Iterator find(const Key& key) { return map_.find(key); }

 private:
  typedef std::map<Key, Value> Map::*Testable;

 public:
  // The Map may be used in boolean expressions to determine if it is non-null,
  // but is not implicitly convertible to an actual bool value (which would be
  // dangerous).
  operator Testable() const { return is_null_ ? 0 : &Map::map_; }

 private:
  // Forbid the == and != operators explicitly, otherwise Map will be converted
  // to Testable to do == or != comparison.
  template <typename T, typename U>
  bool operator==(const Map<T, U>& other) const = delete;
  template <typename T, typename U>
  bool operator!=(const Map<T, U>& other) const = delete;

  void Take(Map* other) {
    operator=(nullptr);
    Swap(other);
  }

  std::map<Key, Value> map_;
  bool is_null_;

  DISALLOW_COPY_AND_ASSIGN(Map);
};

// Copies the contents of an std::map to a new Map, optionally changing the
// types of the keys and values along the way using TypeConverter.
template <typename MojoKey,
          typename MojoValue,
          typename STLKey,
          typename STLValue>
struct TypeConverter<Map<MojoKey, MojoValue>, std::map<STLKey, STLValue>> {
  static Map<MojoKey, MojoValue> Convert(
      const std::map<STLKey, STLValue>& input) {
    Map<MojoKey, MojoValue> result;
    for (auto& pair : input) {
      result.insert(TypeConverter<MojoKey, STLKey>::Convert(pair.first),
                    TypeConverter<MojoValue, STLValue>::Convert(pair.second));
    }
    return result;
  }
};

// Copies the contents of a Map to an std::map, optionally changing the types of
// the keys and values along the way using TypeConverter.
template <typename MojoKey,
          typename MojoValue,
          typename STLKey,
          typename STLValue>
struct TypeConverter<std::map<STLKey, STLValue>, Map<MojoKey, MojoValue>> {
  static std::map<STLKey, STLValue> Convert(
      const Map<MojoKey, MojoValue>& input) {
    std::map<STLKey, STLValue> result;
    if (!input.is_null()) {
      for (auto it = input.begin(); it != input.end(); ++it) {
        result.insert(std::make_pair(
            TypeConverter<STLKey, MojoKey>::Convert(it->first),
            TypeConverter<STLValue, MojoValue>::Convert(it->second)));
      }
    }
    return result;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MAP_H_
