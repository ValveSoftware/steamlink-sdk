// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_PEPPER_CONTAINER_APP_GRAPHICS_3D_RESOURCE_H_
#define MOJO_EXAMPLES_PEPPER_CONTAINER_APP_GRAPHICS_3D_RESOURCE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "mojo/public/c/gles2/gles2_types.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_graphics_3d_api.h"

namespace mojo {
namespace examples {

class Graphics3DResource : public ppapi::Resource,
                           public ppapi::thunk::PPB_Graphics3D_API {
 public:
  explicit Graphics3DResource(PP_Instance instance);

  bool IsBoundGraphics() const;
  void BindGraphics();

  // ppapi::Resource overrides.
  virtual ppapi::thunk::PPB_Graphics3D_API* AsPPB_Graphics3D_API() OVERRIDE;

  // ppapi::thunk::PPB_Graphics3D_API implementation.
  virtual int32_t GetAttribs(int32_t attrib_list[]) OVERRIDE;
  virtual int32_t SetAttribs(const int32_t attrib_list[]) OVERRIDE;
  virtual int32_t GetError() OVERRIDE;
  virtual int32_t ResizeBuffers(int32_t width, int32_t height) OVERRIDE;
  virtual int32_t SwapBuffers(
      scoped_refptr<ppapi::TrackedCallback> callback) OVERRIDE;
  virtual int32_t GetAttribMaxValue(int32_t attribute, int32_t* value) OVERRIDE;
  virtual PP_Bool SetGetBuffer(int32_t shm_id) OVERRIDE;
  virtual scoped_refptr<gpu::Buffer> CreateTransferBuffer(uint32_t size,
                                                          int32* id) OVERRIDE;
  virtual PP_Bool DestroyTransferBuffer(int32_t id) OVERRIDE;
  virtual PP_Bool Flush(int32_t put_offset) OVERRIDE;
  virtual gpu::CommandBuffer::State WaitForTokenInRange(int32_t start,
                                                        int32_t end) OVERRIDE;
  virtual gpu::CommandBuffer::State WaitForGetOffsetInRange(
      int32_t start, int32_t end) OVERRIDE;
  virtual void* MapTexSubImage2DCHROMIUM(GLenum target,
                                         GLint level,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLsizei width,
                                         GLsizei height,
                                         GLenum format,
                                         GLenum type,
                                         GLenum access) OVERRIDE;
  virtual void UnmapTexSubImage2DCHROMIUM(const void* mem) OVERRIDE;
  virtual uint32_t InsertSyncPoint() OVERRIDE;

 private:
  virtual ~Graphics3DResource();

  static void ContextLostThunk(void* closure);
  static void DrawAnimationFrameThunk(void* closure);
  void ContextLost();

  MojoGLES2Context context_;
  DISALLOW_COPY_AND_ASSIGN(Graphics3DResource);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_PEPPER_CONTAINER_APP_GRAPHICS_3D_RESOURCE_H_
