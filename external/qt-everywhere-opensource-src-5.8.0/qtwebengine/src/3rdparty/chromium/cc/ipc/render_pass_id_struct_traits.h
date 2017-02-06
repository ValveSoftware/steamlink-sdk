// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_RENDER_PASS_ID_STRUCT_TRAITS_H_
#define CC_IPC_RENDER_PASS_ID_STRUCT_TRAITS_H_

#include "cc/ipc/render_pass_id.mojom.h"
#include "cc/quads/render_pass_id.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::RenderPassId, cc::RenderPassId> {
  static int layer_id(const cc::RenderPassId& id) { return id.layer_id; }

  static uint32_t index(const cc::RenderPassId& id) { return id.index; }

  static bool Read(cc::mojom::RenderPassIdDataView data,
                   cc::RenderPassId* out) {
    out->layer_id = data.layer_id();
    out->index = data.index();
    return true;
  }
};

}  // namespace mojo

#endif  // CC_IPC_RENDER_PASS_ID_STRUCT_TRAITS_H_
