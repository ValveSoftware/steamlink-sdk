// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/compositor/blimp_output_surface.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "blimp/client/feature/compositor/blimp_context_provider.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface_client.h"
#include "gpu/command_buffer/client/context_support.h"

namespace blimp {
namespace client {

BlimpOutputSurface::BlimpOutputSurface(
    scoped_refptr<BlimpContextProvider> context_provider)
    : cc::OutputSurface(std::move(context_provider), nullptr, nullptr) {}

BlimpOutputSurface::~BlimpOutputSurface() {}

void BlimpOutputSurface::SwapBuffers(cc::CompositorFrame frame) {
  // See cc::OutputSurface::SwapBuffers() comment for details.
  context_provider_->ContextSupport()->Swap();
  client_->DidSwapBuffers();
  cc::OutputSurface::PostSwapBuffersComplete();
}

uint32_t BlimpOutputSurface::GetFramebufferCopyTextureFormat() {
  auto* gl = static_cast<BlimpContextProvider*>(context_provider());
  return gl->GetCopyTextureInternalFormat();
}

}  // namespace client
}  // namespace blimp
