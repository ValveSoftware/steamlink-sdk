// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_CLONE_EQUALS_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_CLONE_EQUALS_UTIL_H_

#include <type_traits>
#include <unordered_map>
#include <vector>

#include "base/optional.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"

namespace mojo {
namespace internal {

template <typename T>
struct HasCloneMethod {
  template <typename U>
  static char Test(decltype(&U::Clone));
  template <typename U>
  static int Test(...);
  static const bool value = sizeof(Test<T>(0)) == sizeof(char);

 private:
  EnsureTypeIsComplete<T> check_t_;
};

template <typename T, bool has_clone_method = HasCloneMethod<T>::value>
struct CloneTraits;

template <typename T>
T Clone(const T& input);

template <typename T>
struct CloneTraits<T, true> {
  static T Clone(const T& input) { return input.Clone(); }
};

template <typename T>
struct CloneTraits<T, false> {
  static T Clone(const T& input) { return input; }
};

template <typename T>
struct CloneTraits<base::Optional<T>, false> {
  static base::Optional<T> Clone(const base::Optional<T>& input) {
    if (!input)
      return base::nullopt;

    return base::Optional<T>(internal::Clone(*input));
  }
};

template <typename T>
struct CloneTraits<std::vector<T>, false> {
  static std::vector<T> Clone(const std::vector<T>& input) {
    std::vector<T> result;
    result.reserve(input.size());
    for (const auto& element : input)
      result.push_back(internal::Clone(element));

    return result;
  }
};

template <typename K, typename V>
struct CloneTraits<std::unordered_map<K, V>, false> {
  static std::unordered_map<K, V> Clone(const std::unordered_map<K, V>& input) {
    std::unordered_map<K, V> result;
    for (const auto& element : input) {
      result.insert(std::make_pair(internal::Clone(element.first),
                                   internal::Clone(element.second)));
    }
    return result;
  }
};

template <typename T>
T Clone(const T& input) {
  return CloneTraits<T>::Clone(input);
};

template <typename T>
struct HasEqualsMethod {
  template <typename U>
  static char Test(decltype(&U::Equals));
  template <typename U>
  static int Test(...);
  static const bool value = sizeof(Test<T>(0)) == sizeof(char);

 private:
  EnsureTypeIsComplete<T> check_t_;
};

template <typename T, bool has_equals_method = HasEqualsMethod<T>::value>
struct EqualsTraits;

template <typename T>
bool Equals(const T& a, const T& b);

template <typename T>
struct EqualsTraits<T, true> {
  static bool Equals(const T& a, const T& b) { return a.Equals(b); }
};

template <typename T>
struct EqualsTraits<T, false> {
  static bool Equals(const T& a, const T& b) { return a == b; }
};

template <typename T>
struct EqualsTraits<base::Optional<T>, false> {
  static bool Equals(const base::Optional<T>& a, const base::Optional<T>& b) {
    if (!a && !b)
      return true;
    if (!a || !b)
      return false;

    return internal::Equals(*a, *b);
  }
};

template <typename T>
struct EqualsTraits<std::vector<T>, false> {
  static bool Equals(const std::vector<T>& a, const std::vector<T>& b) {
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
struct EqualsTraits<std::unordered_map<K, V>, false> {
  static bool Equals(const std::unordered_map<K, V>& a,
                     const std::unordered_map<K, V>& b) {
    if (a.size() != b.size())
      return false;
    for (const auto& element : a) {
      auto iter = b.find(element.first);
      if (iter == b.end() || !internal::Equals(element.second, iter->second))
        return false;
    }
    return true;
  }
};

template <typename T>
bool Equals(const T& a, const T& b) {
  return EqualsTraits<T>::Equals(a, b);
}

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_CLONE_EQUALS_UTIL_H_
