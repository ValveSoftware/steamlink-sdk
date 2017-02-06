// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/surface_context_factory.h"

#include "base/memory/ptr_util.h"
#include "cc/output/output_surface.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "components/mus/public/cpp/window.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "ui/compositor/reflector.h"
#include "ui/gl/gl_bindings.h"

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

SurfaceContextFactory::SurfaceContextFactory(
    shell::Connector* connector,
    mus::Window* window,
    mus::mojom::SurfaceType surface_type)
    : surface_binding_(connector, window, surface_type),
      next_surface_id_namespace_(1u) {}

SurfaceContextFactory::~SurfaceContextFactory() {}

void SurfaceContextFactory::CreateOutputSurface(
    base::WeakPtr<ui::Compositor> compositor) {
  // NOTIMPLEMENTED();
  std::unique_ptr<cc::OutputSurface> surface =
      surface_binding_.CreateOutputSurface();
  if (surface)
    compositor->SetOutputSurface(std::move(surface));
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

cc::SharedBitmapManager* SurfaceContextFactory::GetSharedBitmapManager() {
  // NOTIMPLEMENTED();
  return nullptr;
}

gpu::GpuMemoryBufferManager*
SurfaceContextFactory::GetGpuMemoryBufferManager() {
  return &gpu_memory_buffer_manager_;
}

cc::TaskGraphRunner* SurfaceContextFactory::GetTaskGraphRunner() {
  return raster_thread_helper_.task_graph_runner();
}

std::unique_ptr<cc::SurfaceIdAllocator>
SurfaceContextFactory::CreateSurfaceIdAllocator() {
  return base::WrapUnique(
      new cc::SurfaceIdAllocator(next_surface_id_namespace_++));
}

cc::SurfaceManager* SurfaceContextFactory::GetSurfaceManager() {
  // NOTIMPLEMENTED();
  return nullptr;
}

void SurfaceContextFactory::ResizeDisplay(ui::Compositor* compositor,
                                          const gfx::Size& size) {
  // NOTIMPLEMENTED();
}

}  // namespace views
