// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_SKIA_BINDINGS_GRCONTEXT_FOR_GLES2_INTERFACE_H_
#define GPU_SKIA_BINDINGS_GRCONTEXT_FOR_GLES2_INTERFACE_H_

#include "base/macros.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class GrContext;

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace skia_bindings {

// This class binds an offscreen GrContext to an offscreen context3d. The
// context3d is used by the GrContext so must be valid as long as this class
// is alive.
class GrContextForGLES2Interface {
 public:
  explicit GrContextForGLES2Interface(gpu::gles2::GLES2Interface* gl);
  virtual ~GrContextForGLES2Interface();

  GrContext* get() { return gr_context_.get(); }

  void OnLostContext();
  void ResetContext(uint32_t state);
  void FreeGpuResources();

 private:
  sk_sp<class GrContext> gr_context_;

  DISALLOW_COPY_AND_ASSIGN(GrContextForGLES2Interface);
};

}  // namespace skia_bindings

#endif  // GPU_SKIA_BINDINGS_GRCONTEXT_FOR_GLES2_INTERFACE_H_
