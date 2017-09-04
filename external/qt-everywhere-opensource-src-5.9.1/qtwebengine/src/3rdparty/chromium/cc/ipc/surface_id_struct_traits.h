// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_SURFACE_ID_STRUCT_TRAITS_H_
#define CC_IPC_SURFACE_ID_STRUCT_TRAITS_H_

#include "cc/ipc/frame_sink_id_struct_traits.h"
#include "cc/ipc/local_frame_id_struct_traits.h"
#include "cc/ipc/surface_id.mojom-shared.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/surface_id.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::SurfaceIdDataView, cc::SurfaceId> {
  static const cc::FrameSinkId& frame_sink_id(const cc::SurfaceId& id) {
    return id.frame_sink_id();
  }

  static const cc::LocalFrameId& local_frame_id(const cc::SurfaceId& id) {
    return id.local_frame_id();
  }

  static bool Read(cc::mojom::SurfaceIdDataView data, cc::SurfaceId* out) {
    cc::FrameSinkId frame_sink_id;
    if (!data.ReadFrameSinkId(&frame_sink_id))
      return false;

    cc::LocalFrameId local_frame_id;
    if (!data.ReadLocalFrameId(&local_frame_id))
      return false;

    *out = cc::SurfaceId(frame_sink_id, local_frame_id);
    return true;
  }
};

}  // namespace mojo

#endif  // CC_IPC_SURFACE_ID_STRUCT_TRAITS_H_
