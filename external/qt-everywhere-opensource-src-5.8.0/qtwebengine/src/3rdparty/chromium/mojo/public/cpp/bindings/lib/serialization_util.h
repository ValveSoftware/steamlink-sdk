// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include <queue>

#include "base/logging.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_context.h"
#include "mojo/public/cpp/system/handle.h"

namespace mojo {

class AssociatedGroupController;

namespace internal {

size_t Align(size_t size);
char* AlignPointer(char* ptr);

bool IsAligned(const void* ptr);

// Pointers are encoded as relative offsets. The offsets are relative to the
// address of where the offset value is stored, such that the pointer may be
// recovered with the expression:
//
//   ptr = reinterpret_cast<char*>(offset) + *offset
//
// A null pointer is encoded as an offset value of 0.
//
void EncodePointer(const void* ptr, uint64_t* offset);
// Note: This function doesn't validate the encoded pointer value.
const void* DecodePointerRaw(const uint64_t* offset);

// Note: This function doesn't validate the encoded pointer value.
template <typename T>
inline void DecodePointer(const uint64_t* offset, T** ptr) {
  *ptr = reinterpret_cast<T*>(const_cast<void*>(DecodePointerRaw(offset)));
}

// The following 2 functions are used to encode/decode all objects (structs and
// arrays) in a consistent manner.

template <typename T>
inline void Encode(T* obj) {
  if (obj->ptr)
    obj->ptr->EncodePointers();
  EncodePointer(obj->ptr, &obj->offset);
}

// Note: This function doesn't validate the encoded pointer and handle values.
template <typename T>
inline void Decode(T* obj) {
  DecodePointer(&obj->offset, &obj->ptr);
  if (obj->ptr)
    obj->ptr->DecodePointers();
}

template <typename T>
inline void AssociatedInterfacePtrInfoToData(
    AssociatedInterfacePtrInfo<T> input,
    AssociatedInterface_Data* output) {
  output->version = input.version();
  output->interface_id = input.PassHandle().release();
}

template <typename T>
inline void AssociatedInterfaceDataToPtrInfo(
    AssociatedInterface_Data* input,
    AssociatedInterfacePtrInfo<T>* output,
    AssociatedGroupController* group_controller) {
  output->set_handle(group_controller->CreateLocalEndpointHandle(
      FetchAndReset(&input->interface_id)));
  output->set_version(input->version);
}

template <typename T>
inline void InterfacePointerToData(InterfacePtr<T> input,
                                   Interface_Data* output,
                                   SerializationContext* context) {
  InterfacePtrInfo<T> info = input.PassInterface();
  output->handle = context->handles.AddHandle(info.PassHandle().release());
  output->version = info.version();
}

template <typename T>
inline void InterfaceDataToPointer(Interface_Data* input,
                                   InterfacePtr<T>* output,
                                   SerializationContext* context) {
  output->Bind(InterfacePtrInfo<T>(
      context->handles.TakeHandleAs<mojo::MessagePipeHandle>(input->handle),
      input->version));
}

template <typename T>
struct HasIsNullMethod {
  template <typename U>
  static char Test(decltype(U::IsNull)*);
  template <typename U>
  static int Test(...);
  static const bool value = sizeof(Test<T>(0)) == sizeof(char);

 private:
  EnsureTypeIsComplete<T> check_t_;
};

template <
    typename Traits,
    typename UserType,
    typename std::enable_if<HasIsNullMethod<Traits>::value>::type* = nullptr>
bool CallIsNullIfExists(const UserType& input) {
  return Traits::IsNull(input);
}

template <
    typename Traits,
    typename UserType,
    typename std::enable_if<!HasIsNullMethod<Traits>::value>::type* = nullptr>
bool CallIsNullIfExists(const UserType& input) {
  return false;
}
template <typename T>
struct HasSetToNullMethod {
  template <typename U>
  static char Test(decltype(U::SetToNull)*);
  template <typename U>
  static int Test(...);
  static const bool value = sizeof(Test<T>(0)) == sizeof(char);

 private:
  EnsureTypeIsComplete<T> check_t_;
};

template <
    typename Traits,
    typename UserType,
    typename std::enable_if<HasSetToNullMethod<Traits>::value>::type* = nullptr>
bool CallSetToNullIfExists(UserType* output) {
  Traits::SetToNull(output);
  return true;
}

template <typename Traits,
          typename UserType,
          typename std::enable_if<!HasSetToNullMethod<Traits>::value>::type* =
              nullptr>
bool CallSetToNullIfExists(UserType* output) {
  LOG(ERROR) << "A null value is received. But the Struct/Array/StringTraits "
             << "class doesn't define a SetToNull() function and therefore is "
             << "unable to deserialize the value.";
  return false;
}

template <typename T>
struct HasSetUpContextMethod {
  template <typename U>
  static char Test(decltype(U::SetUpContext)*);
  template <typename U>
  static int Test(...);
  static const bool value = sizeof(Test<T>(0)) == sizeof(char);

 private:
  EnsureTypeIsComplete<T> check_t_;
};

template <typename Traits,
          bool has_context = HasSetUpContextMethod<Traits>::value>
struct CustomContextHelper;

template <typename Traits>
struct CustomContextHelper<Traits, true> {
  template <typename MaybeConstUserType>
  static void* SetUp(MaybeConstUserType& input, SerializationContext* context) {
    void* custom_context = Traits::SetUpContext(input);
    if (!context->custom_contexts)
      context->custom_contexts.reset(new std::queue<void*>());
    context->custom_contexts->push(custom_context);
    return custom_context;
  }

  static void* GetNext(SerializationContext* context) {
    void* custom_context = context->custom_contexts->front();
    context->custom_contexts->pop();
    return custom_context;
  }

  template <typename MaybeConstUserType>
  static void TearDown(MaybeConstUserType& input, void* custom_context) {
    Traits::TearDownContext(input, custom_context);
  }
};

template <typename Traits>
struct CustomContextHelper<Traits, false> {
  template <typename MaybeConstUserType>
  static void* SetUp(MaybeConstUserType& input, SerializationContext* context) {
    return nullptr;
  }

  static void* GetNext(SerializationContext* context) { return nullptr; }

  template <typename MaybeConstUserType>
  static void TearDown(MaybeConstUserType& input, void* custom_context) {
    DCHECK(!custom_context);
  }
};

template <typename ReturnType, typename ParamType, typename InputUserType>
ReturnType CallWithContext(ReturnType (*f)(ParamType, void*),
                           InputUserType&& input,
                           void* context) {
  return f(std::forward<InputUserType>(input), context);
}

template <typename ReturnType, typename ParamType, typename InputUserType>
ReturnType CallWithContext(ReturnType (*f)(ParamType),
                           InputUserType&& input,
                           void* context) {
  return f(std::forward<InputUserType>(input));
}

template <typename T, typename MaybeConstUserType>
struct HasGetBeginMethod {
  template <typename U>
  static char Test(decltype(U::GetBegin(std::declval<MaybeConstUserType&>()))*);
  template <typename U>
  static int Test(...);
  static const bool value = sizeof(Test<T>(0)) == sizeof(char);

 private:
  EnsureTypeIsComplete<T> check_t_;
};

template <
    typename Traits,
    typename MaybeConstUserType,
    typename std::enable_if<
        HasGetBeginMethod<Traits, MaybeConstUserType>::value>::type* = nullptr>
decltype(Traits::GetBegin(std::declval<MaybeConstUserType&>()))
CallGetBeginIfExists(MaybeConstUserType& input) {
  return Traits::GetBegin(input);
}

template <
    typename Traits,
    typename MaybeConstUserType,
    typename std::enable_if<
        !HasGetBeginMethod<Traits, MaybeConstUserType>::value>::type* = nullptr>
size_t CallGetBeginIfExists(MaybeConstUserType& input) {
  return 0;
}

template <typename T, typename MaybeConstUserType>
struct HasGetDataMethod {
  template <typename U>
  static char Test(decltype(U::GetData(std::declval<MaybeConstUserType&>()))*);
  template <typename U>
  static int Test(...);
  static const bool value = sizeof(Test<T>(0)) == sizeof(char);

 private:
  EnsureTypeIsComplete<T> check_t_;
};

template <
    typename Traits,
    typename MaybeConstUserType,
    typename std::enable_if<
        HasGetDataMethod<Traits, MaybeConstUserType>::value>::type* = nullptr>
decltype(Traits::GetData(std::declval<MaybeConstUserType&>()))
CallGetDataIfExists(MaybeConstUserType& input) {
  return Traits::GetData(input);
}

template <
    typename Traits,
    typename MaybeConstUserType,
    typename std::enable_if<
        !HasGetDataMethod<Traits, MaybeConstUserType>::value>::type* = nullptr>
void* CallGetDataIfExists(MaybeConstUserType& input) {
  return nullptr;
}

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_UTIL_H_
