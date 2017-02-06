// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"

#include <stddef.h>
#include <string.h>
#include <utility>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/skia_bindings/gl_bindings_skia_cmd_buffer.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

namespace skia_bindings {

GrContextForGLES2Interface::GrContextForGLES2Interface(
    gpu::gles2::GLES2Interface* gl) {
  sk_sp<GrGLInterface> interface(
      skia_bindings::CreateGLES2InterfaceBindings(gl));
  gr_context_ = sk_sp<GrContext>(
      GrContext::Create(kOpenGL_GrBackend,
                        // GrContext takes ownership of |interface|.
                        reinterpret_cast<GrBackendContext>(interface.get())));
  if (gr_context_) {
    // The limit of the number of GPU resources we hold in the GrContext's
    // GPU cache.
    static const int kMaxGaneshResourceCacheCount = 8196;
    // The limit of the bytes allocated toward GPU resources in the GrContext's
    // GPU cache.
    static const size_t kMaxGaneshResourceCacheBytes = 96 * 1024 * 1024;

    gr_context_->setResourceCacheLimits(kMaxGaneshResourceCacheCount,
                                        kMaxGaneshResourceCacheBytes);
  }
}

GrContextForGLES2Interface::~GrContextForGLES2Interface() {
  // At this point the GLES2Interface is going to be destroyed, so have
  // the GrContext clean up and not try to use it anymore.
  if (gr_context_)
    gr_context_->releaseResourcesAndAbandonContext();
}

void GrContextForGLES2Interface::OnLostContext() {
  if (gr_context_)
    gr_context_->abandonContext();
}

void GrContextForGLES2Interface::ResetContext(uint32_t state) {
  if (gr_context_)
    gr_context_->resetContext(state);
}

void GrContextForGLES2Interface::FreeGpuResources() {
  if (gr_context_) {
    TRACE_EVENT_INSTANT0("gpu", "GrContext::freeGpuResources",
                         TRACE_EVENT_SCOPE_THREAD);
    gr_context_->freeGpuResources();
  }
}

}  // namespace skia_bindings
