// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/pepper_container_app/graphics_3d_resource.h"

#include "base/logging.h"
#include "mojo/examples/pepper_container_app/mojo_ppapi_globals.h"
#include "mojo/examples/pepper_container_app/plugin_instance.h"
#include "mojo/public/c/gles2/gles2.h"
#include "ppapi/c/pp_errors.h"

namespace mojo {
namespace examples {

namespace {

gpu::CommandBuffer::State GetErrorState() {
  gpu::CommandBuffer::State error_state;
  error_state.error = gpu::error::kGenericError;
  return error_state;
}

}  // namespace

Graphics3DResource::Graphics3DResource(PP_Instance instance)
    : Resource(ppapi::OBJECT_IS_IMPL, instance) {
  ScopedMessagePipeHandle pipe = MojoPpapiGlobals::Get()->CreateGLES2Context();
  context_ = MojoGLES2CreateContext(pipe.release().value(),
                                    &ContextLostThunk,
                                    &DrawAnimationFrameThunk,
                                    this);
}

bool Graphics3DResource::IsBoundGraphics() const {
  PluginInstance* plugin_instance =
      MojoPpapiGlobals::Get()->GetInstance(pp_instance());
  return plugin_instance && plugin_instance->IsBoundGraphics(pp_resource());
}

void Graphics3DResource::BindGraphics() {
  MojoGLES2MakeCurrent(context_);
}

ppapi::thunk::PPB_Graphics3D_API* Graphics3DResource::AsPPB_Graphics3D_API() {
  return this;
}

int32_t Graphics3DResource::GetAttribs(int32_t attrib_list[]) {
  NOTIMPLEMENTED();
  return PP_ERROR_FAILED;
}

int32_t Graphics3DResource::SetAttribs(const int32_t attrib_list[]) {
  NOTIMPLEMENTED();
  return PP_ERROR_FAILED;
}

int32_t Graphics3DResource::GetError() {
  NOTIMPLEMENTED();
  return PP_ERROR_FAILED;
}

int32_t Graphics3DResource::ResizeBuffers(int32_t width, int32_t height) {
  NOTIMPLEMENTED();
  return PP_ERROR_FAILED;
}

int32_t Graphics3DResource::SwapBuffers(
    scoped_refptr<ppapi::TrackedCallback> callback) {
  if (!IsBoundGraphics())
    return PP_ERROR_FAILED;

  MojoGLES2SwapBuffers();
  return PP_OK;
}

int32_t Graphics3DResource::GetAttribMaxValue(int32_t attribute,
                                              int32_t* value) {
  NOTIMPLEMENTED();
  return PP_ERROR_FAILED;
}

PP_Bool Graphics3DResource::SetGetBuffer(int32_t shm_id) {
  NOTIMPLEMENTED();
  return PP_FALSE;
}

scoped_refptr<gpu::Buffer> Graphics3DResource::CreateTransferBuffer(
    uint32_t size,
    int32* id) {
  NOTIMPLEMENTED();
  return NULL;
}

PP_Bool Graphics3DResource::DestroyTransferBuffer(int32_t id) {
  NOTIMPLEMENTED();
  return PP_FALSE;
}

PP_Bool Graphics3DResource::Flush(int32_t put_offset) {
  NOTIMPLEMENTED();
  return PP_FALSE;
}

gpu::CommandBuffer::State Graphics3DResource::WaitForTokenInRange(int32_t start,
                                                                  int32_t end) {
  NOTIMPLEMENTED();
  return GetErrorState();
}

gpu::CommandBuffer::State Graphics3DResource::WaitForGetOffsetInRange(
    int32_t start, int32_t end) {
  NOTIMPLEMENTED();
  return GetErrorState();
}

void* Graphics3DResource::MapTexSubImage2DCHROMIUM(GLenum target,
                                                   GLint level,
                                                   GLint xoffset,
                                                   GLint yoffset,
                                                   GLsizei width,
                                                   GLsizei height,
                                                   GLenum format,
                                                   GLenum type,
                                                   GLenum access) {
  NOTIMPLEMENTED();
  return NULL;
}

void Graphics3DResource::UnmapTexSubImage2DCHROMIUM(const void* mem) {
  NOTIMPLEMENTED();
}

uint32_t Graphics3DResource::InsertSyncPoint() {
  NOTIMPLEMENTED();
  return 0;
}

Graphics3DResource::~Graphics3DResource() {
  MojoGLES2DestroyContext(context_);
}

void Graphics3DResource::ContextLostThunk(void* closure) {
  static_cast<Graphics3DResource*>(closure)->ContextLost();
}

void Graphics3DResource::DrawAnimationFrameThunk(void* closure) {
  // TODO(yzshen): Use this notification to drive the SwapBuffers() callback.
}

void Graphics3DResource::ContextLost() {
  PluginInstance* plugin_instance =
      MojoPpapiGlobals::Get()->GetInstance(pp_instance());
  if (plugin_instance)
    plugin_instance->Graphics3DContextLost();
}

}  // namespace examples
}  // namespace mojo
