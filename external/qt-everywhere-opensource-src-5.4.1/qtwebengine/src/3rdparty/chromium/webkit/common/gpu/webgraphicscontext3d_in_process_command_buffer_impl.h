// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMMON_GPU_WEBGRAPHICSCONTEXT3D_IN_PROCESS_COMMAND_BUFFER_IMPL_H_
#define WEBKIT_COMMON_GPU_WEBGRAPHICSCONTEXT3D_IN_PROCESS_COMMAND_BUFFER_IMPL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "ui/gfx/native_widget_types.h"
#include "webkit/common/gpu/webgraphicscontext3d_impl.h"
#include "webkit/common/gpu/webkit_gpu_export.h"

namespace gpu {
class ContextSupport;

namespace gles2 {
class GLES2Interface;
class GLES2Implementation;
}
}

namespace gpu {
class GLInProcessContext;
struct GLInProcessContextAttribs;
}

namespace webkit {
namespace gpu {

class WEBKIT_GPU_EXPORT WebGraphicsContext3DInProcessCommandBufferImpl
    : public WebGraphicsContext3DImpl {
 public:
  static scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>
      CreateViewContext(
          const blink::WebGraphicsContext3D::Attributes& attributes,
          bool lose_context_when_out_of_memory,
          gfx::AcceleratedWidget window);

  static scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>
      CreateOffscreenContext(
          const blink::WebGraphicsContext3D::Attributes& attributes,
          bool lose_context_when_out_of_memory);

  static scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>
      WrapContext(
          scoped_ptr< ::gpu::GLInProcessContext> context,
          const blink::WebGraphicsContext3D::Attributes& attributes);

  virtual ~WebGraphicsContext3DInProcessCommandBufferImpl();

  // Convert WebGL context creation attributes into GLInProcessContext / EGL
  // size requests.
  static void ConvertAttributes(
      const blink::WebGraphicsContext3D::Attributes& attributes,
      ::gpu::GLInProcessContextAttribs* output_attribs);

  //----------------------------------------------------------------------
  // WebGraphicsContext3D methods
  virtual bool makeContextCurrent();

  virtual bool isContextLost();

  virtual WGC3Denum getGraphicsResetStatusARB();

  ::gpu::ContextSupport* GetContextSupport();

  ::gpu::gles2::GLES2Implementation* GetImplementation() {
    return real_gl_;
  }

 private:
  WebGraphicsContext3DInProcessCommandBufferImpl(
      scoped_ptr< ::gpu::GLInProcessContext> context,
      const blink::WebGraphicsContext3D::Attributes& attributes,
      bool lose_context_when_out_of_memory,
      bool is_offscreen,
      gfx::AcceleratedWidget window);

  void OnContextLost();

  bool MaybeInitializeGL();

  // Used to try to find bugs in code that calls gl directly through the gl api
  // instead of going through WebGraphicsContext3D.
  void ClearContext();

  ::gpu::GLInProcessContextAttribs attribs_;
  bool share_resources_;
  bool webgl_context_;

  bool is_offscreen_;
  // Only used when not offscreen.
  gfx::AcceleratedWidget window_;

  // The context we use for OpenGL rendering.
  scoped_ptr< ::gpu::GLInProcessContext> context_;
  // The GLES2Implementation we use for OpenGL rendering.
  ::gpu::gles2::GLES2Implementation* real_gl_;
};

}  // namespace gpu
}  // namespace webkit

#endif  // WEBKIT_COMMON_GPU_WEBGRAPHICSCONTEXT3D_IN_PROCESS_COMMAND_BUFFER_IMPL_H_
