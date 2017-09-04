// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/surface_context_factory.h"

#include "base/memory/ptr_util.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "services/ui/public/cpp/context_provider.h"
#include "services/ui/public/cpp/gpu_service.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_compositor_frame_sink.h"
#include "ui/compositor/reflector.h"
#include "ui/gl/gl_bindings.h"
#include "ui/views/mus/native_widget_mus.h"

namespace views {
namespace {

class FakeReflector : public ui::Reflector {
 public:
  FakeReflector() {}
  ~FakeReflector() override {}
  void OnMirroringCompositorResized() override {}
  void AddMirroringLayer(ui::Layer* layer) override {}
  void RemoveMirroringLayer(ui::Layer* layer) override {}
};

}  // namespace

SurfaceContextFactory::SurfaceContextFactory(ui::GpuService* gpu_service)
    : next_sink_id_(1u), gpu_service_(gpu_service) {}

SurfaceContextFactory::~SurfaceContextFactory() {}

void SurfaceContextFactory::CreateCompositorFrameSink(
    base::WeakPtr<ui::Compositor> compositor) {
  ui::Window* window = compositor->window();
  NativeWidgetMus* native_widget = NativeWidgetMus::GetForWindow(window);
  ui::mojom::CompositorFrameSinkType compositor_frame_sink_type =
      native_widget->compositor_frame_sink_type();
  auto compositor_frame_sink = window->RequestCompositorFrameSink(
      compositor_frame_sink_type, make_scoped_refptr(new ui::ContextProvider(
                                      gpu_service_->EstablishGpuChannelSync())),
      gpu_service_->gpu_memory_buffer_manager());
  compositor->SetCompositorFrameSink(std::move(compositor_frame_sink));
}

std::unique_ptr<ui::Reflector> SurfaceContextFactory::CreateReflector(
    ui::Compositor* mirroed_compositor,
    ui::Layer* mirroring_layer) {
  // NOTIMPLEMENTED();
  return base::WrapUnique(new FakeReflector);
}

void SurfaceContextFactory::RemoveReflector(ui::Reflector* reflector) {
  // NOTIMPLEMENTED();
}

scoped_refptr<cc::ContextProvider>
SurfaceContextFactory::SharedMainThreadContextProvider() {
  // NOTIMPLEMENTED();
  return nullptr;
}

void SurfaceContextFactory::RemoveCompositor(ui::Compositor* compositor) {
  // NOTIMPLEMENTED();
}

bool SurfaceContextFactory::DoesCreateTestContexts() {
  return false;
}

uint32_t SurfaceContextFactory::GetImageTextureTarget(gfx::BufferFormat format,
                                                      gfx::BufferUsage usage) {
  // No GpuMemoryBuffer support, so just return GL_TEXTURE_2D.
  return GL_TEXTURE_2D;
}

gpu::GpuMemoryBufferManager*
SurfaceContextFactory::GetGpuMemoryBufferManager() {
  return gpu_service_->gpu_memory_buffer_manager();
}

cc::TaskGraphRunner* SurfaceContextFactory::GetTaskGraphRunner() {
  return raster_thread_helper_.task_graph_runner();
}

cc::FrameSinkId SurfaceContextFactory::AllocateFrameSinkId() {
  return cc::FrameSinkId(0, next_sink_id_++);
}

cc::SurfaceManager* SurfaceContextFactory::GetSurfaceManager() {
  return &surface_manager_;
}

void SurfaceContextFactory::SetDisplayVisible(ui::Compositor* compositor,
                                              bool visible) {
  // TODO(fsamuel): display[compositor]->SetVisible(visible);
}

void SurfaceContextFactory::ResizeDisplay(ui::Compositor* compositor,
                                          const gfx::Size& size) {
  // TODO(fsamuel): display[compositor]->Resize(size);
}

}  // namespace views
