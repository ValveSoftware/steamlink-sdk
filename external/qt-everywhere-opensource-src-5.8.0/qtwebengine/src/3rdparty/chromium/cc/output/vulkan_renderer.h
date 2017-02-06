// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_VULKAN_RENDERER_H_
#define CC_OUTPUT_VULKAN_RENDERER_H_

#include "cc/base/cc_export.h"
#include "cc/output/direct_renderer.h"
#include "cc/output/renderer.h"

namespace cc {

class TextureMailboxDeleter;

class CC_EXPORT VulkanRenderer : public DirectRenderer {
 public:
  static std::unique_ptr<VulkanRenderer> Create(
      RendererClient* client,
      const RendererSettings* settings,
      OutputSurface* output_surface,
      ResourceProvider* resource_provider,
      TextureMailboxDeleter* texture_mailbox_deleter,
      int highp_threshold_min);

  ~VulkanRenderer() override;

  // Implementation of public Renderer functions.
  const RendererCapabilitiesImpl& Capabilities() const override;
  void Finish() override;
  void SwapBuffers(const CompositorFrameMetadata& metadata) override;
  void ReceiveSwapBuffersAck(const CompositorFrameAck& ack) override;

 protected:
  VulkanRenderer(RendererClient* client,
                 const RendererSettings* settings,
                 OutputSurface* output_surface,
                 ResourceProvider* resource_provider,
                 TextureMailboxDeleter* texture_mailbox_deleter,
                 int highp_threshold_min);

  // Implementations of protected Renderer functions.
  void DidChangeVisibility() override;

  // Implementations of protected DirectRenderer functions.
  void BindFramebufferToOutputSurface(DrawingFrame* frame) override;
  bool BindFramebufferToTexture(DrawingFrame* frame,
                                const ScopedResource* resource) override;
  void SetScissorTestRect(const gfx::Rect& scissor_rect) override;
  void PrepareSurfaceForPass(DrawingFrame* frame,
                             SurfaceInitializationMode initialization_mode,
                             const gfx::Rect& render_pass_scissor) override;
  void DoDrawQuad(DrawingFrame* frame,
                  const DrawQuad* quad,
                  const gfx::QuadF* clip_region) override;
  void BeginDrawingFrame(DrawingFrame* frame) override;
  void FinishDrawingFrame(DrawingFrame* frame) override;
  void FinishDrawingQuadList() override;
  bool FlippedFramebuffer(const DrawingFrame* frame) const override;
  void EnsureScissorTestEnabled() override;
  void EnsureScissorTestDisabled() override;
  void DiscardBackbuffer() override;
  void EnsureBackbuffer() override;
  void CopyCurrentRenderPassToBitmap(
      DrawingFrame* frame,
      std::unique_ptr<CopyOutputRequest> request) override;

 private:
  RendererCapabilitiesImpl capabilities_;

  DISALLOW_COPY_AND_ASSIGN(VulkanRenderer);
};

}  // namespace cc

#endif  // CC_OUTPUT_VULKAN_RENDERER_H_
