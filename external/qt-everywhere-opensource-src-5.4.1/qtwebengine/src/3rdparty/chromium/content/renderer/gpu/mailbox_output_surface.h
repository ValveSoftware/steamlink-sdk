// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_MAILBOX_OUTPUT_SURFACE_H_
#define CONTENT_RENDERER_GPU_MAILBOX_OUTPUT_SURFACE_H_

#include <queue>

#include "cc/resources/resource_format.h"
#include "cc/resources/transferable_resource.h"
#include "content/renderer/gpu/compositor_output_surface.h"
#include "ui/gfx/size.h"

namespace cc {
class CompositorFrameAck;
}

namespace content {

// Implementation of CompositorOutputSurface that renders to textures which
// are sent to the browser through the mailbox extension.
// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread when bindToClient is called.
class MailboxOutputSurface : public CompositorOutputSurface {
 public:
  MailboxOutputSurface(
      int32 routing_id,
      uint32 output_surface_id,
      const scoped_refptr<ContextProviderCommandBuffer>& context_provider,
      scoped_ptr<cc::SoftwareOutputDevice> software_device,
      cc::ResourceFormat format);
  virtual ~MailboxOutputSurface();

  // cc::OutputSurface implementation.
  virtual void EnsureBackbuffer() OVERRIDE;
  virtual void DiscardBackbuffer() OVERRIDE;
  virtual void Reshape(const gfx::Size& size, float scale_factor) OVERRIDE;
  virtual void BindFramebuffer() OVERRIDE;
  virtual void SwapBuffers(cc::CompositorFrame* frame) OVERRIDE;

 private:
  // CompositorOutputSurface overrides.
  virtual void OnSwapAck(uint32 output_surface_id,
                         const cc::CompositorFrameAck& ack) OVERRIDE;

  size_t GetNumAcksPending();

  struct TransferableFrame {
    TransferableFrame() : texture_id(0), sync_point(0) {}

    TransferableFrame(uint32 texture_id,
                      const gpu::Mailbox& mailbox,
                      const gfx::Size size)
        : texture_id(texture_id), mailbox(mailbox), size(size), sync_point(0) {}

    uint32 texture_id;
    gpu::Mailbox mailbox;
    gfx::Size size;
    uint32 sync_point;
  };

  TransferableFrame current_backing_;
  std::deque<TransferableFrame> pending_textures_;
  std::queue<TransferableFrame> returned_textures_;

  uint32 fbo_;
  bool is_backbuffer_discarded_;
  cc::ResourceFormat format_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_MAILBOX_OUTPUT_SURFACE_H_
