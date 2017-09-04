// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/native_struct_data.h"

#include "mojo/public/cpp/bindings/lib/buffer.h"
#include "mojo/public/cpp/bindings/lib/validation_context.h"

namespace mojo {
namespace internal {

// static
bool NativeStruct_Data::Validate(const void* data,
                                 ValidationContext* validation_context) {
  const ContainerValidateParams data_validate_params(0, false, nullptr);
  return Array_Data<uint8_t>::Validate(data, validation_context,
                                       &data_validate_params);
}

}  // namespace internal
}  // namespace mojo
