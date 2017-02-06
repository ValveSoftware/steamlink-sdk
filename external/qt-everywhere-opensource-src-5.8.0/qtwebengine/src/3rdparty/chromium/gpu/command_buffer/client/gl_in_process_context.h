// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GL_IN_PROCESS_CONTEXT_H_
#define GPU_COMMAND_BUFFER_CLIENT_GL_IN_PROCESS_CONTEXT_H_

#include <stddef.h>
#include <stdint.h>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "gl_in_process_context_export.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/in_process_command_buffer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_preference.h"

namespace gfx {
class Size;
}

#if defined(OS_ANDROID)
namespace gl {
class SurfaceTexture;
}
#endif

namespace gpu {
struct SharedMemoryLimits;

namespace gles2 {
class GLES2Implementation;
}

class GL_IN_PROCESS_CONTEXT_EXPORT GLInProcessContext {
 public:
  virtual ~GLInProcessContext() {}

  // Create a GLInProcessContext, if |is_offscreen| is true, renders to an
  // offscreen context. |attrib_list| must be NULL or a NONE-terminated list
  // of attribute/value pairs.
  // If |surface| is not NULL, then it must match |is_offscreen|,
  // |window| must be gfx::kNullAcceleratedWidget, and the command buffer
  // service must run on the same thread as this client because GLSurface is
  // not thread safe. If |surface| is NULL, then the other parameters are used
  // to correctly create a surface.
  static GLInProcessContext* Create(
      scoped_refptr<gpu::InProcessCommandBuffer::Service> service,
      scoped_refptr<gl::GLSurface> surface,
      bool is_offscreen,
      gfx::AcceleratedWidget window,
      GLInProcessContext* share_context,
      const gpu::gles2::ContextCreationAttribHelper& attribs,
      const SharedMemoryLimits& memory_limits,
      GpuMemoryBufferManager* gpu_memory_buffer_manager,
      ImageFactory* image_factory);

  // Allows direct access to the GLES2 implementation so a GLInProcessContext
  // can be used without making it current.
  virtual gles2::GLES2Implementation* GetImplementation() = 0;

  virtual void SetLock(base::Lock* lock) = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_GL_IN_PROCESS_CONTEXT_H_
