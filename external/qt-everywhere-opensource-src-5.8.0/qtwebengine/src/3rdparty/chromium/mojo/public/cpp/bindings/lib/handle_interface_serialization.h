// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_HANDLE_INTERFACE_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_HANDLE_INTERFACE_SERIALIZATION_H_

#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/associated_interface_request.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_context.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/system/handle.h"

namespace mojo {
namespace internal {

template <typename T>
struct Serializer<AssociatedInterfacePtrInfo<T>,
                  AssociatedInterfacePtrInfo<T>> {
  static void Serialize(AssociatedInterfacePtrInfo<T>& input,
                        AssociatedInterface_Data* output,
                        SerializationContext* context) {
    DCHECK(!input.handle().is_valid() || !input.handle().is_local());
    DCHECK_EQ(input.handle().group_controller(),
              context->group_controller.get());
    output->version = input.version();
    output->interface_id = input.PassHandle().release();
  }

  static bool Deserialize(AssociatedInterface_Data* input,
                          AssociatedInterfacePtrInfo<T>* output,
                          SerializationContext* context) {
    output->set_handle(context->group_controller->CreateLocalEndpointHandle(
        FetchAndReset(&input->interface_id)));
    output->set_version(input->version);
    return true;
  }
};

template <typename T>
struct Serializer<AssociatedInterfaceRequest<T>,
                  AssociatedInterfaceRequest<T>> {
  static void Serialize(AssociatedInterfaceRequest<T>& input,
                        AssociatedInterfaceRequest_Data* output,
                        SerializationContext* context) {
    DCHECK(!input.handle().is_valid() || !input.handle().is_local());
    DCHECK_EQ(input.handle().group_controller(),
              context->group_controller.get());
    output->interface_id = input.PassHandle().release();
  }

  static bool Deserialize(AssociatedInterfaceRequest_Data* input,
                          AssociatedInterfaceRequest<T>* output,
                          SerializationContext* context) {
    output->Bind(context->group_controller->CreateLocalEndpointHandle(
        FetchAndReset(&input->interface_id)));
    return true;
  }
};

template <typename T>
struct Serializer<InterfacePtr<T>, InterfacePtr<T>> {
  static void Serialize(InterfacePtr<T>& input,
                        Interface_Data* output,
                        SerializationContext* context) {
    InterfacePtrInfo<T> info = input.PassInterface();
    output->handle = context->handles.AddHandle(info.PassHandle().release());
    output->version = info.version();
  }

  static bool Deserialize(Interface_Data* input,
                          InterfacePtr<T>* output,
                          SerializationContext* context) {
    output->Bind(InterfacePtrInfo<T>(
        context->handles.TakeHandleAs<mojo::MessagePipeHandle>(input->handle),
        input->version));
    return true;
  }
};

template <typename T>
struct Serializer<InterfaceRequest<T>, InterfaceRequest<T>> {
  static void Serialize(InterfaceRequest<T>& input,
                        Handle_Data* output,
                        SerializationContext* context) {
    *output = context->handles.AddHandle(input.PassMessagePipe().release());
  }

  static bool Deserialize(Handle_Data* input,
                          InterfaceRequest<T>* output,
                          SerializationContext* context) {
    output->Bind(context->handles.TakeHandleAs<MessagePipeHandle>(*input));
    return true;
  }
};

template <typename T>
struct Serializer<ScopedHandleBase<T>, ScopedHandleBase<T>> {
  static void Serialize(ScopedHandleBase<T>& input,
                        Handle_Data* output,
                        SerializationContext* context) {
    *output = context->handles.AddHandle(input.release());
  }

  static bool Deserialize(Handle_Data* input,
                          ScopedHandleBase<T>* output,
                          SerializationContext* context) {
    *output = context->handles.TakeHandleAs<T>(*input);
    return true;
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_HANDLE_INTERFACE_SERIALIZATION_H_
