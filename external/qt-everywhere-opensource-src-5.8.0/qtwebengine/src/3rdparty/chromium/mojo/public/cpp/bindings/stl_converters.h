// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STL_CONVERTERS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STL_CONVERTERS_H_

#include <map>
#include <string>
#include <type_traits>
#include <vector>

#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/map.h"
#include "mojo/public/cpp/bindings/string.h"

// Two functions are defined to facilitate conversion between
// mojo::Array/Map/String and std::vector/map/string: mojo::UnwrapToSTLType()
// recursively convert mojo types to STL types; mojo::WrapSTLType() does the
// opposite. For example:
//   mojo::Array<mojo::Map<mojo::String, mojo::Array<int32_t>>> mojo_obj;
//
//   std::vector<std::map<std::string, std::vector<int32_t>>> stl_obj =
//       mojo::UnwrapToSTLType(std::move(mojo_obj));
//
//   mojo_obj = mojo::WrapSTLType(std::move(stl_obj));
//
// Notes:
//   - The conversion moves as much contents as possible. The two functions both
//     take an rvalue ref as input in order to avoid accidental copies.
//   - Because std::vector/map/string cannot express null, UnwrapToSTLType()
//     converts null mojo::Array/Map/String to empty.
//   - The recursive conversion stops at any types that are not the types listed
//     above. For example, unwrapping mojo::Array<StructContainingMojoMap> will
//     result in std::vector<StructContainingMojoMap>. It won't convert
//     mojo::Map inside the struct.

namespace mojo {
namespace internal {

template <typename T>
struct UnwrapTraits;

template <typename T>
struct UnwrapShouldGoDeeper {
 public:
  static const bool value =
      !std::is_same<T, typename UnwrapTraits<T>::Type>::value;
};

template <typename T>
struct UnwrapTraits {
 public:
  using Type = T;
  static Type Unwrap(T input) { return input; }
};

template <typename T>
struct UnwrapTraits<Array<T>> {
 public:
  using Type = std::vector<typename UnwrapTraits<T>::Type>;

  static Type Unwrap(Array<T> input) {
    return Helper<T>::Run(std::move(input));
  }

 private:
  template <typename U, bool should_go_deeper = UnwrapShouldGoDeeper<U>::value>
  struct Helper {};

  template <typename U>
  struct Helper<U, true> {
   public:
    static Type Run(Array<T> input) {
      Type output;
      output.reserve(input.size());
      for (size_t i = 0; i < input.size(); ++i)
        output.push_back(UnwrapTraits<T>::Unwrap(std::move(input[i])));
      return output;
    }
  };

  template <typename U>
  struct Helper<U, false> {
   public:
    static Type Run(Array<T> input) { return input.PassStorage(); }
  };
};

template <typename K, typename V>
struct UnwrapTraits<Map<K, V>> {
 public:
  using Type =
      std::map<typename UnwrapTraits<K>::Type, typename UnwrapTraits<V>::Type>;

  static Type Unwrap(Map<K, V> input) {
    return Helper<K, V>::Run(std::move(input));
  }

 private:
  template <typename X,
            typename Y,
            bool should_go_deeper = UnwrapShouldGoDeeper<X>::value ||
                                    UnwrapShouldGoDeeper<Y>::value>
  struct Helper {};

  template <typename X, typename Y>
  struct Helper<X, Y, true> {
   public:
    static Type Run(Map<K, V> input) {
      std::map<K, V> input_storage = input.PassStorage();
      Type output;
      for (auto& pair : input_storage) {
        output.insert(
            std::make_pair(UnwrapTraits<K>::Unwrap(pair.first),
                           UnwrapTraits<V>::Unwrap(std::move(pair.second))));
      }
      return output;
    }
  };

  template <typename X, typename Y>
  struct Helper<X, Y, false> {
   public:
    static Type Run(Map<K, V> input) { return input.PassStorage(); }
  };
};

template <>
struct UnwrapTraits<String> {
 public:
  using Type = std::string;

  static std::string Unwrap(const String& input) { return input; }
};

template <typename T>
struct WrapTraits;

template <typename T>
struct WrapShouldGoDeeper {
 public:
  static const bool value =
      !std::is_same<T, typename WrapTraits<T>::Type>::value;
};

template <typename T>
struct WrapTraits {
 public:
  using Type = T;

  static T Wrap(T input) { return input; }
};

template <typename T>
struct WrapTraits<std::vector<T>> {
 public:
  using Type = Array<typename WrapTraits<T>::Type>;

  static Type Wrap(std::vector<T> input) {
    return Helper<T>::Run(std::move(input));
  }

 private:
  template <typename U, bool should_go_deeper = WrapShouldGoDeeper<U>::value>
  struct Helper {};

  template <typename U>
  struct Helper<U, true> {
   public:
    static Type Run(std::vector<T> input) {
      std::vector<typename WrapTraits<T>::Type> output_storage;
      output_storage.reserve(input.size());
      for (auto& element : input)
        output_storage.push_back(WrapTraits<T>::Wrap(std::move(element)));
      return Type(std::move(output_storage));
    }
  };

  template <typename U>
  struct Helper<U, false> {
   public:
    static Type Run(std::vector<T> input) { return Type(std::move(input)); }
  };
};

template <typename K, typename V>
struct WrapTraits<std::map<K, V>> {
 public:
  using Type = Map<typename WrapTraits<K>::Type, typename WrapTraits<V>::Type>;

  static Type Wrap(std::map<K, V> input) {
    return Helper<K, V>::Run(std::move(input));
  }

 private:
  template <typename X,
            typename Y,
            bool should_go_deeper =
                WrapShouldGoDeeper<X>::value || WrapShouldGoDeeper<Y>::value>
  struct Helper {};

  template <typename X, typename Y>
  struct Helper<X, Y, true> {
   public:
    static Type Run(std::map<K, V> input) {
      Type output;
      for (auto& pair : input) {
        output.insert(WrapTraits<K>::Wrap(pair.first),
                      WrapTraits<V>::Wrap(std::move(pair.second)));
      }
      return output;
    }
  };

  template <typename X, typename Y>
  struct Helper<X, Y, false> {
   public:
    static Type Run(std::map<K, V> input) { return Type(std::move(input)); }
  };
};

template <>
struct WrapTraits<std::string> {
 public:
  using Type = String;

  static String Wrap(const std::string& input) { return input; }
};

}  // namespace internal

template <typename T>
typename internal::UnwrapTraits<T>::Type UnwrapToSTLType(T&& input) {
  return internal::UnwrapTraits<T>::Unwrap(std::move(input));
}

template <typename T>
typename internal::WrapTraits<T>::Type WrapSTLType(T&& input) {
  return internal::WrapTraits<T>::Wrap(std::move(input));
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STL_CONVERTERS_H_
