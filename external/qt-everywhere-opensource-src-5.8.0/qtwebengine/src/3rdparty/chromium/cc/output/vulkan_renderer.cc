// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/vulkan_renderer.h"

namespace cc {

std::unique_ptr<VulkanRenderer> VulkanRenderer::Create(
    RendererClient* client,
    const RendererSettings* settings,
    OutputSurface* output_surface,
    ResourceProvider* resource_provider,
    TextureMailboxDeleter* texture_mailbox_deleter,
    int highp_threshold_min) {
  return std::unique_ptr<VulkanRenderer>(
      new VulkanRenderer(client, settings, output_surface, resource_provider,
                         texture_mailbox_deleter, highp_threshold_min));
}

VulkanRenderer::~VulkanRenderer() {}

const RendererCapabilitiesImpl& VulkanRenderer::Capabilities() const {
  return capabilities_;
}

void VulkanRenderer::Finish() {
  NOTIMPLEMENTED();
}

void VulkanRenderer::SwapBuffers(const CompositorFrameMetadata& metadata) {
  CompositorFrame* compositor_frame = nullptr;
  output_surface_->SwapBuffers(compositor_frame);
}

void VulkanRenderer::ReceiveSwapBuffersAck(const CompositorFrameAck& ack) {
  NOTIMPLEMENTED();
}

VulkanRenderer::VulkanRenderer(RendererClient* client,
                               const RendererSettings* settings,
                               OutputSurface* output_surface,
                               ResourceProvider* resource_provider,
                               TextureMailboxDeleter* texture_mailbox_deleter,
                               int highp_threshold_min)
    : DirectRenderer(client, settings, output_surface, resource_provider) {}

void VulkanRenderer::DidChangeVisibility() {
  NOTIMPLEMENTED();
}

void VulkanRenderer::BindFramebufferToOutputSurface(DrawingFrame* frame) {
  NOTIMPLEMENTED();
}

bool VulkanRenderer::BindFramebufferToTexture(DrawingFrame* frame,
                                              const ScopedResource* resource) {
  NOTIMPLEMENTED();
  return false;
}

void VulkanRenderer::SetScissorTestRect(const gfx::Rect& scissor_rect) {
  NOTIMPLEMENTED();
}

void VulkanRenderer::PrepareSurfaceForPass(
    DrawingFrame* frame,
    SurfaceInitializationMode initialization_mode,
    const gfx::Rect& render_pass_scissor) {
  NOTIMPLEMENTED();
}

void VulkanRenderer::DoDrawQuad(DrawingFrame* frame,
                                const DrawQuad* quad,
                                const gfx::QuadF* clip_region) {
  NOTIMPLEMENTED();
}

void VulkanRenderer::BeginDrawingFrame(DrawingFrame* frame) {
  NOTIMPLEMENTED();
}

void VulkanRenderer::FinishDrawingFrame(DrawingFrame* frame) {
  NOTIMPLEMENTED();
}

void VulkanRenderer::FinishDrawingQuadList() {
  NOTIMPLEMENTED();
}

bool VulkanRenderer::FlippedFramebuffer(const DrawingFrame* frame) const {
  NOTIMPLEMENTED();
  return false;
}

void VulkanRenderer::EnsureScissorTestEnabled() {
  NOTIMPLEMENTED();
}

void VulkanRenderer::EnsureScissorTestDisabled() {
  NOTIMPLEMENTED();
}

void VulkanRenderer::DiscardBackbuffer() {
  NOTIMPLEMENTED();
}

void VulkanRenderer::EnsureBackbuffer() {
  NOTIMPLEMENTED();
}

void VulkanRenderer::CopyCurrentRenderPassToBitmap(
    DrawingFrame* frame,
    std::unique_ptr<CopyOutputRequest> request) {
  NOTIMPLEMENTED();
}

}  // namespace cc
