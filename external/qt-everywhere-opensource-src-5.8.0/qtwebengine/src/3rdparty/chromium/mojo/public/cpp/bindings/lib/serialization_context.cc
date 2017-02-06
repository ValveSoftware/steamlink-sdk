// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/serialization_context.h"

#include <limits>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {
namespace internal {

SerializedHandleVector::SerializedHandleVector() {}

SerializedHandleVector::~SerializedHandleVector() {
  for (auto handle : handles_) {
    if (handle.is_valid()) {
      MojoResult rv = MojoClose(handle.value());
      DCHECK_EQ(rv, MOJO_RESULT_OK);
    }
  }
}

Handle_Data SerializedHandleVector::AddHandle(mojo::Handle handle) {
  Handle_Data data;
  if (!handle.is_valid()) {
    data.value = kEncodedInvalidHandleValue;
  } else {
    DCHECK_LT(handles_.size(), std::numeric_limits<uint32_t>::max());
    data.value = static_cast<uint32_t>(handles_.size());
    handles_.push_back(handle);
  }
  return data;
}

mojo::Handle SerializedHandleVector::TakeHandle(
    const Handle_Data& encoded_handle) {
  if (!encoded_handle.is_valid())
    return mojo::Handle();
  DCHECK_LT(encoded_handle.value, handles_.size());
  return FetchAndReset(&handles_[encoded_handle.value]);
}

void SerializedHandleVector::Swap(std::vector<mojo::Handle>* other) {
  handles_.swap(*other);
}

SerializationContext::SerializationContext() {}

SerializationContext::SerializationContext(
    scoped_refptr<AssociatedGroupController> in_group_controller)
    : group_controller(std::move(in_group_controller)) {}

SerializationContext::~SerializationContext() {
  DCHECK(!custom_contexts || custom_contexts->empty());
}

}  // namespace internal
}  // namespace mojo
