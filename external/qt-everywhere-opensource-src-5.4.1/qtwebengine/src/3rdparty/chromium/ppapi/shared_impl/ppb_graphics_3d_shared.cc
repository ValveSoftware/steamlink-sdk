// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_graphics_3d_shared.h"

#include "base/logging.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "ppapi/c/pp_errors.h"

namespace ppapi {

PPB_Graphics3D_Shared::PPB_Graphics3D_Shared(PP_Instance instance)
    : Resource(OBJECT_IS_IMPL, instance) {}

PPB_Graphics3D_Shared::PPB_Graphics3D_Shared(const HostResource& host_resource)
    : Resource(OBJECT_IS_PROXY, host_resource) {}

PPB_Graphics3D_Shared::~PPB_Graphics3D_Shared() {
  // Make sure that GLES2 implementation has already been destroyed.
  DCHECK(!gles2_helper_.get());
  DCHECK(!transfer_buffer_.get());
  DCHECK(!gles2_impl_.get());
}

thunk::PPB_Graphics3D_API* PPB_Graphics3D_Shared::AsPPB_Graphics3D_API() {
  return this;
}

int32_t PPB_Graphics3D_Shared::GetAttribs(int32_t attrib_list[]) {
  // TODO(alokp): Implement me.
  return PP_ERROR_FAILED;
}

int32_t PPB_Graphics3D_Shared::SetAttribs(const int32_t attrib_list[]) {
  // TODO(alokp): Implement me.
  return PP_ERROR_FAILED;
}

int32_t PPB_Graphics3D_Shared::GetError() {
  // TODO(alokp): Implement me.
  return PP_ERROR_FAILED;
}

int32_t PPB_Graphics3D_Shared::ResizeBuffers(int32_t width, int32_t height) {
  if ((width < 0) || (height < 0))
    return PP_ERROR_BADARGUMENT;

  gles2_impl()->ResizeCHROMIUM(width, height, 1.f);
  // TODO(alokp): Check if resize succeeded and return appropriate error code.
  return PP_OK;
}

int32_t PPB_Graphics3D_Shared::SwapBuffers(
    scoped_refptr<TrackedCallback> callback) {
  if (HasPendingSwap()) {
    Log(PP_LOGLEVEL_ERROR,
        "PPB_Graphics3D.SwapBuffers: Plugin attempted swap "
        "with previous swap still pending.");
    // Already a pending SwapBuffers that hasn't returned yet.
    return PP_ERROR_INPROGRESS;
  }

  swap_callback_ = callback;
  return DoSwapBuffers();
}

int32_t PPB_Graphics3D_Shared::GetAttribMaxValue(int32_t attribute,
                                                 int32_t* value) {
  // TODO(alokp): Implement me.
  return PP_ERROR_FAILED;
}

void* PPB_Graphics3D_Shared::MapTexSubImage2DCHROMIUM(GLenum target,
                                                      GLint level,
                                                      GLint xoffset,
                                                      GLint yoffset,
                                                      GLsizei width,
                                                      GLsizei height,
                                                      GLenum format,
                                                      GLenum type,
                                                      GLenum access) {
  return gles2_impl_->MapTexSubImage2DCHROMIUM(
      target, level, xoffset, yoffset, width, height, format, type, access);
}

void PPB_Graphics3D_Shared::UnmapTexSubImage2DCHROMIUM(const void* mem) {
  gles2_impl_->UnmapTexSubImage2DCHROMIUM(mem);
}

void PPB_Graphics3D_Shared::SwapBuffersACK(int32_t pp_error) {
  DCHECK(HasPendingSwap());
  swap_callback_->Run(pp_error);
}

bool PPB_Graphics3D_Shared::HasPendingSwap() const {
  return TrackedCallback::IsPending(swap_callback_);
}

bool PPB_Graphics3D_Shared::CreateGLES2Impl(
    int32 command_buffer_size,
    int32 transfer_buffer_size,
    gpu::gles2::GLES2Implementation* share_gles2) {
  gpu::CommandBuffer* command_buffer = GetCommandBuffer();
  DCHECK(command_buffer);

  // Create the GLES2 helper, which writes the command buffer protocol.
  gles2_helper_.reset(new gpu::gles2::GLES2CmdHelper(command_buffer));
  if (!gles2_helper_->Initialize(command_buffer_size))
    return false;

  // Create a transfer buffer used to copy resources between the renderer
  // process and the GPU process.
  const int32 kMinTransferBufferSize = 256 * 1024;
  const int32 kMaxTransferBufferSize = 16 * 1024 * 1024;
  transfer_buffer_.reset(new gpu::TransferBuffer(gles2_helper_.get()));

  const bool bind_creates_resources = true;
  const bool lose_context_when_out_of_memory = false;

  // Create the object exposing the OpenGL API.
  gles2_impl_.reset(new gpu::gles2::GLES2Implementation(
      gles2_helper_.get(),
      share_gles2 ? share_gles2->share_group() : NULL,
      transfer_buffer_.get(),
      bind_creates_resources,
      lose_context_when_out_of_memory,
      GetGpuControl()));

  if (!gles2_impl_->Initialize(
           transfer_buffer_size,
           kMinTransferBufferSize,
           std::max(kMaxTransferBufferSize, transfer_buffer_size),
           gpu::gles2::GLES2Implementation::kNoLimit)) {
    return false;
  }

  gles2_impl_->PushGroupMarkerEXT(0, "PPAPIContext");

  return true;
}

void PPB_Graphics3D_Shared::DestroyGLES2Impl() {
  gles2_impl_.reset();
  transfer_buffer_.reset();
  gles2_helper_.reset();
}

}  // namespace ppapi
