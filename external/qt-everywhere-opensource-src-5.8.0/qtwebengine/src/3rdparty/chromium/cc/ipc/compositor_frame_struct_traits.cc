// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/ipc/compositor_frame_struct_traits.h"
#include "cc/ipc/compositor_frame_metadata_struct_traits.h"
#include "cc/ipc/render_pass_struct_traits.h"
#include "cc/ipc/transferable_resource_struct_traits.h"

namespace mojo {

// static
bool StructTraits<cc::mojom::CompositorFrame, cc::CompositorFrame>::Read(
    cc::mojom::CompositorFrameDataView data,
    cc::CompositorFrame* out) {
  if (!data.ReadMetadata(&out->metadata))
    return false;

  out->delegated_frame_data.reset(new cc::DelegatedFrameData());
  return data.ReadResources(&out->delegated_frame_data->resource_list) &&
         data.ReadPasses(&out->delegated_frame_data->render_pass_list);
}

}  // namespace mojo
