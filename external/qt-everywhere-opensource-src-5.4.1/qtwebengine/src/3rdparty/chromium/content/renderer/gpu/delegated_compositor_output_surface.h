// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_DELEGATED_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_RENDERER_GPU_DELEGATED_COMPOSITOR_OUTPUT_SURFACE_H_

#include "content/renderer/gpu/compositor_output_surface.h"

namespace content {

class DelegatedCompositorOutputSurface : public CompositorOutputSurface {
 public:
  DelegatedCompositorOutputSurface(
      int32 routing_id,
      uint32 output_surface_id,
      const scoped_refptr<ContextProviderCommandBuffer>& context_provider);
  virtual ~DelegatedCompositorOutputSurface() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_DELEGATED_COMPOSITOR_OUTPUT_SURFACE_H_
