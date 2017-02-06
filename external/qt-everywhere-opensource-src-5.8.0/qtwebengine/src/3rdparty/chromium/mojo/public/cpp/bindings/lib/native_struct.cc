// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/native_struct.h"

namespace mojo {

// static
NativeStructPtr NativeStruct::New() {
  NativeStructPtr rv;
  internal::StructHelper<NativeStruct>::Initialize(&rv);
  return rv;
}

NativeStruct::NativeStruct() : data(nullptr) {}

NativeStruct::~NativeStruct() {}

NativeStructPtr NativeStruct::Clone() const {
  NativeStructPtr rv(New());
  rv->data = data.Clone();
  return rv;
}

bool NativeStruct::Equals(const NativeStruct& other) const {
  return data.Equals(other.data);
}

}  // namespace mojo
