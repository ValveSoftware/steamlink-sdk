// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_SERIALIZATION_CONTEXT_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_SERIALIZATION_CONTEXT_H_

#include <stddef.h>

#include <memory>
#include <queue>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/system/handle.h"

namespace mojo {

class AssociatedGroupController;

namespace internal {

// A container for handles during serialization/deserialization.
class SerializedHandleVector {
 public:
  SerializedHandleVector();
  ~SerializedHandleVector();

  size_t size() const { return handles_.size(); }

  // Adds a handle to the handle list and returns its index for encoding.
  Handle_Data AddHandle(mojo::Handle handle);

  // Takes a handle from the list of serialized handle data.
  mojo::Handle TakeHandle(const Handle_Data& encoded_handle);

  // Takes a handle from the list of serialized handle data and returns it in
  // |*out_handle| as a specific scoped handle type.
  template <typename T>
  ScopedHandleBase<T> TakeHandleAs(const Handle_Data& encoded_handle) {
    return MakeScopedHandle(T(TakeHandle(encoded_handle).value()));
  }

  // Swaps all owned handles out with another Handle vector.
  void Swap(std::vector<mojo::Handle>* other);

 private:
  // Handles are owned by this object.
  std::vector<mojo::Handle> handles_;

  DISALLOW_COPY_AND_ASSIGN(SerializedHandleVector);
};

// Context information for serialization/deserialization routines.
struct SerializationContext {
  SerializationContext();
  explicit SerializationContext(
      scoped_refptr<AssociatedGroupController> in_group_controller);

  ~SerializationContext();

  // Used to serialize/deserialize associated interface pointers and requests.
  scoped_refptr<AssociatedGroupController> group_controller;

  // Opaque context pointers returned by StringTraits::SetUpContext().
  std::unique_ptr<std::queue<void*>> custom_contexts;

  // Stashes handles encoded in a message by index.
  SerializedHandleVector handles;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_SERIALIZATION_CONTEXT_H_
