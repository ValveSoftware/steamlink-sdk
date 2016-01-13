// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_STREAM_TEXTURE_ANDROID_H_
#define CONTENT_COMMON_GPU_STREAM_TEXTURE_ANDROID_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "ipc/ipc_listener.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_image.h"

namespace gfx {
class Size;
}

namespace content {

class StreamTexture : public gfx::GLImage,
                      public IPC::Listener,
                      public GpuCommandBufferStub::DestructionObserver {
 public:
  static bool Create(GpuCommandBufferStub* owner_stub,
                     uint32 client_texture_id,
                     int stream_id);

 private:
  StreamTexture(GpuCommandBufferStub* owner_stub,
                int32 route_id,
                uint32 texture_id);
  virtual ~StreamTexture();

  // gfx::GLImage implementation:
  virtual void Destroy() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual bool BindTexImage(unsigned target) OVERRIDE;
  virtual void ReleaseTexImage(unsigned target) OVERRIDE;
  virtual void WillUseTexImage() OVERRIDE;
  virtual void DidUseTexImage() OVERRIDE {}
  virtual void WillModifyTexImage() OVERRIDE {}
  virtual void DidModifyTexImage() OVERRIDE {}

  // GpuCommandBufferStub::DestructionObserver implementation.
  virtual void OnWillDestroyStub() OVERRIDE;

  // Called when a new frame is available for the SurfaceTexture.
  void OnFrameAvailable();

  // IPC::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // IPC message handlers:
  void OnStartListening();
  void OnEstablishPeer(int32 primary_id, int32 secondary_id);
  void OnSetSize(const gfx::Size& size) { size_ = size; }

  scoped_refptr<gfx::SurfaceTexture> surface_texture_;

  // Current transform matrix of the surface texture.
  float current_matrix_[16];

  // Current size of the surface texture.
  gfx::Size size_;

  // Whether we ever bound a valid frame.
  bool has_valid_frame_;

  // Whether a new frame is available that we should update to.
  bool has_pending_frame_;

  GpuCommandBufferStub* owner_stub_;
  int32 route_id_;
  bool has_listener_;

  base::WeakPtrFactory<StreamTexture> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(StreamTexture);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_STREAM_TEXTURE_ANDROID_H_
