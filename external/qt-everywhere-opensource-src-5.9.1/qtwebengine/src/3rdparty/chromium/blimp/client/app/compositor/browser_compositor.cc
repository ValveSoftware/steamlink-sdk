// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/compositor/browser_compositor.h"

#include "blimp/client/public/compositor/compositor_dependencies.h"
#include "blimp/client/support/compositor/blimp_context_provider.h"
#include "cc/output/context_provider.h"

namespace blimp {
namespace client {

BrowserCompositor::BrowserCompositor(
    CompositorDependencies* compositor_dependencies)
    : BlimpEmbedderCompositor(compositor_dependencies) {}

BrowserCompositor::~BrowserCompositor() = default;

void BrowserCompositor::SetAcceleratedWidget(gfx::AcceleratedWidget widget) {
  scoped_refptr<cc::ContextProvider> provider;

  if (widget != gfx::kNullAcceleratedWidget) {
    provider = BlimpContextProvider::Create(
        widget, compositor_dependencies()->GetGpuMemoryBufferManager());
  }

  SetContextProvider(std::move(provider));
}

void BrowserCompositor::DidReceiveCompositorFrameAck() {
  if (!did_complete_swap_buffers_.is_null()) {
    did_complete_swap_buffers_.Run();
  }
}

}  // namespace client
}  // namespace blimp
