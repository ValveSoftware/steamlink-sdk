// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/mus_context_factory.h"

#include "base/memory/ptr_util.h"
#include "services/ui/public/cpp/context_provider.h"
#include "ui/aura/mus/gpu_service.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/reflector.h"
#include "ui/gl/gl_bindings.h"

namespace aura {
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

MusContextFactory::MusContextFactory(GpuService* gpu_service)
    : next_sink_id_(1u), gpu_service_(gpu_service) {}

MusContextFactory::~MusContextFactory() {}

void MusContextFactory::CreateCompositorFrameSink(
    base::WeakPtr<ui::Compositor> compositor) {
  WindowTreeHost* host =
      WindowTreeHost::GetForAcceleratedWidget(compositor->widget());
  WindowPortMus* window_port = WindowPortMus::Get(host->window());
  DCHECK(window_port);
  auto compositor_frame_sink = window_port->RequestCompositorFrameSink(
      ui::mojom::CompositorFrameSinkType::DEFAULT,
      make_scoped_refptr(
          new ui::ContextProvider(gpu_service_->EstablishGpuChannelSync())),
      gpu_service_->gpu_memory_buffer_manager());
  compositor->SetCompositorFrameSink(std::move(compositor_frame_sink));
}

std::unique_ptr<ui::Reflector> MusContextFactory::CreateReflector(
    ui::Compositor* mirroed_compositor,
    ui::Layer* mirroring_layer) {
  // NOTIMPLEMENTED();
  return base::WrapUnique(new FakeReflector);
}

void MusContextFactory::RemoveReflector(ui::Reflector* reflector) {
  // NOTIMPLEMENTED();
}

scoped_refptr<cc::ContextProvider>
MusContextFactory::SharedMainThreadContextProvider() {
  // NOTIMPLEMENTED();
  return nullptr;
}

void MusContextFactory::RemoveCompositor(ui::Compositor* compositor) {
  // NOTIMPLEMENTED();
}

bool MusContextFactory::DoesCreateTestContexts() {
  return false;
}

uint32_t MusContextFactory::GetImageTextureTarget(gfx::BufferFormat format,
                                                  gfx::BufferUsage usage) {
  // No GpuMemoryBuffer support, so just return GL_TEXTURE_2D.
  return GL_TEXTURE_2D;
}

gpu::GpuMemoryBufferManager* MusContextFactory::GetGpuMemoryBufferManager() {
  return gpu_service_->gpu_memory_buffer_manager();
}

cc::TaskGraphRunner* MusContextFactory::GetTaskGraphRunner() {
  return raster_thread_helper_.task_graph_runner();
}

cc::FrameSinkId MusContextFactory::AllocateFrameSinkId() {
  return cc::FrameSinkId(0, next_sink_id_++);
}

cc::SurfaceManager* MusContextFactory::GetSurfaceManager() {
  return &surface_manager_;
}

void MusContextFactory::SetDisplayVisible(ui::Compositor* compositor,
                                          bool visible) {
  // TODO(fsamuel): display[compositor]->SetVisible(visible);
}

void MusContextFactory::ResizeDisplay(ui::Compositor* compositor,
                                      const gfx::Size& size) {
  // TODO(fsamuel): display[compositor]->Resize(size);
}

}  // namespace aura
