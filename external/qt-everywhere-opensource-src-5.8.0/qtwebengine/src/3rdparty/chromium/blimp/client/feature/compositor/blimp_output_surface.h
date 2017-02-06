// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_OUTPUT_SURFACE_H_
#define BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_OUTPUT_SURFACE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/output/output_surface.h"

namespace blimp {
namespace client {
class BlimpContextProvider;

// Minimal implementation of cc::OutputSurface.
class BlimpOutputSurface : public cc::OutputSurface {
 public:
  explicit BlimpOutputSurface(
      scoped_refptr<BlimpContextProvider> context_provider);
  ~BlimpOutputSurface() override;

  // OutputSurface implementation.
  void SwapBuffers(cc::CompositorFrame frame) override;
  uint32_t GetFramebufferCopyTextureFormat() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpOutputSurface);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_OUTPUT_SURFACE_H_
