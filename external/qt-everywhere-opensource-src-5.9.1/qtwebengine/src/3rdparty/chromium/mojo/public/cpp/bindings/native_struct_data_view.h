// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_NATIVE_STRUCT_DATA_VIEW_H_
#define MOJO_PUBLIC_CPP_BINDINGS_NATIVE_STRUCT_DATA_VIEW_H_

#include "mojo/public/cpp/bindings/lib/native_struct_data.h"
#include "mojo/public/cpp/bindings/lib/serialization_context.h"

namespace mojo {

class NativeStructDataView {
 public:
  using Data_ = internal::NativeStruct_Data;

  NativeStructDataView() {}

  NativeStructDataView(Data_* data, internal::SerializationContext* context)
      : data_(data) {}

  bool is_null() const { return !data_; }

  size_t size() const { return data_->data.size(); }

  uint8_t operator[](size_t index) const { return data_->data.at(index); }

  const uint8_t* data() const { return data_->data.storage(); }

 private:
  Data_* data_ = nullptr;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_NATIVE_STRUCT_DATA_VIEW_H_
