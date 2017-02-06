// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_SURFACE_SEQUENCE_STRUCT_TRAITS_H_
#define CC_IPC_SURFACE_SEQUENCE_STRUCT_TRAITS_H_

#include "cc/surfaces/surface_sequence.h"

namespace mojo {

// This template is fully specialized as cc::mojom::SurfaceSequence and
// as cc::mojom::blink::SurfaceSequence, in generated .mojom.h and
// .mojom-blink.h respectively.
template <typename T>
struct StructTraits<T, cc::SurfaceSequence> {
  static uint32_t id_namespace(const cc::SurfaceSequence& id) {
    return id.id_namespace;
  }

  static uint32_t sequence(const cc::SurfaceSequence& id) {
    return id.sequence;
  }

  static bool Read(typename T::DataView data, cc::SurfaceSequence* out) {
    *out = cc::SurfaceSequence(data.id_namespace(), data.sequence());
    return true;
  }
};

}  // namespace mojo

#endif  // CC_IPC_SURFACE_SEQUENCE_STRUCT_TRAITS_H_
