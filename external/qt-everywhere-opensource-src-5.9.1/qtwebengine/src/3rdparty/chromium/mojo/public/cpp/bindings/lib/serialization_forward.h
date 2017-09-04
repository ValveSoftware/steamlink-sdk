// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_FORWARD_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_FORWARD_H_

#include "base/optional.h"
#include "mojo/public/cpp/bindings/array_traits.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "mojo/public/cpp/bindings/map_traits.h"
#include "mojo/public/cpp/bindings/string_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"

// This file is included by serialization implementation files to avoid circular
// includes.
// Users of the serialization funtions should include serialization.h (and also
// wtf_serialization.h if necessary).

namespace mojo {
namespace internal {

template <typename MojomType, typename MaybeConstUserType>
struct Serializer;

template <typename T>
struct IsOptionalWrapper {
  static const bool value = IsSpecializationOf<
      base::Optional,
      typename std::remove_const<
          typename std::remove_reference<T>::type>::type>::value;
};

// PrepareToSerialize() must be matched by a Serialize() for the same input
// later. Moreover, within the same SerializationContext if PrepareToSerialize()
// is called for |input_1|, ..., |input_n|, Serialize() must be called for
// those objects in the exact same order.
template <typename MojomType,
          typename InputUserType,
          typename... Args,
          typename std::enable_if<
              !IsOptionalWrapper<InputUserType>::value>::type* = nullptr>
size_t PrepareToSerialize(InputUserType&& input, Args&&... args) {
  return Serializer<MojomType,
                    typename std::remove_reference<InputUserType>::type>::
      PrepareToSerialize(std::forward<InputUserType>(input),
                         std::forward<Args>(args)...);
}

template <typename MojomType,
          typename InputUserType,
          typename... Args,
          typename std::enable_if<
              !IsOptionalWrapper<InputUserType>::value>::type* = nullptr>
void Serialize(InputUserType&& input, Args&&... args) {
  Serializer<MojomType, typename std::remove_reference<InputUserType>::type>::
      Serialize(std::forward<InputUserType>(input),
                std::forward<Args>(args)...);
}

template <typename MojomType,
          typename DataType,
          typename InputUserType,
          typename... Args,
          typename std::enable_if<
              !IsOptionalWrapper<InputUserType>::value>::type* = nullptr>
bool Deserialize(DataType&& input, InputUserType* output, Args&&... args) {
  return Serializer<MojomType, InputUserType>::Deserialize(
      std::forward<DataType>(input), output, std::forward<Args>(args)...);
}

// Specialization that unwraps base::Optional<>.
template <typename MojomType,
          typename InputUserType,
          typename... Args,
          typename std::enable_if<
              IsOptionalWrapper<InputUserType>::value>::type* = nullptr>
size_t PrepareToSerialize(InputUserType&& input, Args&&... args) {
  if (!input)
    return 0;
  return PrepareToSerialize<MojomType>(*input, std::forward<Args>(args)...);
}

template <typename MojomType,
          typename InputUserType,
          typename DataType,
          typename... Args,
          typename std::enable_if<
              IsOptionalWrapper<InputUserType>::value>::type* = nullptr>
void Serialize(InputUserType&& input,
               Buffer* buffer,
               DataType** output,
               Args&&... args) {
  if (!input) {
    *output = nullptr;
    return;
  }
  Serialize<MojomType>(*input, buffer, output, std::forward<Args>(args)...);
}

template <typename MojomType,
          typename DataType,
          typename InputUserType,
          typename... Args,
          typename std::enable_if<
              IsOptionalWrapper<InputUserType>::value>::type* = nullptr>
bool Deserialize(DataType&& input, InputUserType* output, Args&&... args) {
  if (!input) {
    *output = base::nullopt;
    return true;
  }
  if (!*output)
    output->emplace();
  return Deserialize<MojomType>(std::forward<DataType>(input), &output->value(),
                                std::forward<Args>(args)...);
}

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_FORWARD_H_
