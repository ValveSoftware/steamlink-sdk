// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_H_

#include <string>

#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/string_serialization.h"

namespace mojo {

template <typename E>
inline size_t GetSerializedSize_(const Array<E>& input);

template <typename E, typename F>
inline void Serialize_(Array<E> input, internal::Buffer* buf,
                       internal::Array_Data<F>** output);
template <typename E, typename F>
inline void Deserialize_(internal::Array_Data<F>* data, Array<E>* output);

namespace internal {

template <typename E, typename F, bool move_only = IsMoveOnlyType<E>::value>
struct ArraySerializer;

template <typename E, typename F> struct ArraySerializer<E, F, false> {
  MOJO_COMPILE_ASSERT(sizeof(E) == sizeof(F), wrong_array_serializer);
  static size_t GetSerializedSize(const Array<E>& input) {
    return sizeof(Array_Data<F>) + Align(input.size() * sizeof(E));
  }
  static void SerializeElements(
      Array<E> input, Buffer* buf, Array_Data<F>* output) {
    memcpy(output->storage(), &input.storage()[0], input.size() * sizeof(E));
  }
  static void DeserializeElements(
      Array_Data<F>* input, Array<E>* output) {
    std::vector<E> result(input->size());
    memcpy(&result[0], input->storage(), input->size() * sizeof(E));
    output->Swap(&result);
  }
};

template <> struct ArraySerializer<bool, bool, false> {
  static size_t GetSerializedSize(const Array<bool>& input) {
    return sizeof(Array_Data<bool>) + Align((input.size() + 7) / 8);
  }
  static void SerializeElements(
      Array<bool> input, Buffer* buf, Array_Data<bool>* output) {
    // TODO(darin): Can this be a memcpy somehow instead of a bit-by-bit copy?
    for (size_t i = 0; i < input.size(); ++i)
      output->at(i) = input[i];
  }
  static void DeserializeElements(
      Array_Data<bool>* input, Array<bool>* output) {
    Array<bool> result(input->size());
    // TODO(darin): Can this be a memcpy somehow instead of a bit-by-bit copy?
    for (size_t i = 0; i < input->size(); ++i)
      result.at(i) = input->at(i);
    output->Swap(&result);
  }
};

template <typename H> struct ArraySerializer<ScopedHandleBase<H>, H, true> {
  static size_t GetSerializedSize(const Array<ScopedHandleBase<H> >& input) {
    return sizeof(Array_Data<H>) + Align(input.size() * sizeof(H));
  }
  static void SerializeElements(
      Array<ScopedHandleBase<H> > input,
      Buffer* buf,
      Array_Data<H>* output) {
    for (size_t i = 0; i < input.size(); ++i)
      output->at(i) = input[i].release();  // Transfer ownership of the handle.
  }
  static void DeserializeElements(
      Array_Data<H>* input, Array<ScopedHandleBase<H> >* output) {
    Array<ScopedHandleBase<H> > result(input->size());
    for (size_t i = 0; i < input->size(); ++i)
      result.at(i) = MakeScopedHandle(FetchAndReset(&input->at(i)));
    output->Swap(&result);
  }
};

template <typename S> struct ArraySerializer<S, typename S::Data_*, true> {
  static size_t GetSerializedSize(const Array<S>& input) {
    size_t size = sizeof(Array_Data<typename S::Data_*>) +
        input.size() * sizeof(internal::StructPointer<typename S::Data_>);
    for (size_t i = 0; i < input.size(); ++i)
      size += GetSerializedSize_(input[i]);
    return size;
  }
  static void SerializeElements(
      Array<S> input,
      Buffer* buf,
      Array_Data<typename S::Data_*>* output) {
    for (size_t i = 0; i < input.size(); ++i) {
      typename S::Data_* element;
      Serialize_(input[i].Pass(), buf, &element);
      output->at(i) = element;
    }
  }
  static void DeserializeElements(
      Array_Data<typename S::Data_*>* input, Array<S>* output) {
    Array<S> result(input->size());
    for (size_t i = 0; i < input->size(); ++i) {
      S element;
      Deserialize_(input->at(i), &element);
      result[i] = element.Pass();
    }
    output->Swap(&result);
  }
};

template <> struct ArraySerializer<String, String_Data*, false> {
  static size_t GetSerializedSize(const Array<String>& input) {
    size_t size = sizeof(Array_Data<String_Data*>) +
        input.size() * sizeof(internal::StringPointer);
    for (size_t i = 0; i < input.size(); ++i)
      size += GetSerializedSize_(input[i]);
    return size;
  }
  static void SerializeElements(
      Array<String> input,
      Buffer* buf,
      Array_Data<String_Data*>* output) {
    for (size_t i = 0; i < input.size(); ++i) {
      String_Data* element;
      Serialize_(input[i], buf, &element);
      output->at(i) = element;
    }
  }
  static void DeserializeElements(
      Array_Data<String_Data*>* input, Array<String>* output) {
    Array<String> result(input->size());
    for (size_t i = 0; i < input->size(); ++i)
      Deserialize_(input->at(i), &result[i]);
    output->Swap(&result);
  }
};

}  // namespace internal

template <typename E>
inline size_t GetSerializedSize_(const Array<E>& input) {
  if (!input)
    return 0;
  typedef typename internal::WrapperTraits<E>::DataType F;
  return internal::ArraySerializer<E, F>::GetSerializedSize(input);
}

template <typename E, typename F>
inline void Serialize_(Array<E> input, internal::Buffer* buf,
                       internal::Array_Data<F>** output) {
  if (input) {
    internal::Array_Data<F>* result =
        internal::Array_Data<F>::New(input.size(), buf);
    internal::ArraySerializer<E, F>::SerializeElements(
        internal::Forward(input), buf, result);
    *output = result;
  } else {
    *output = NULL;
  }
}

template <typename E, typename F>
inline void Deserialize_(internal::Array_Data<F>* input,
                         Array<E>* output) {
  if (input) {
    internal::ArraySerializer<E, F>::DeserializeElements(input, output);
  } else {
    output->reset();
  }
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_H_
