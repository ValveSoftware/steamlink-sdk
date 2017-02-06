// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DISPLAY_COMPOSITOR_BUFFER_QUEUE_H_
#define COMPONENTS_DISPLAY_COMPOSITOR_BUFFER_QUEUE_H_

#include <stddef.h>

#include <deque>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/display_compositor/display_compositor_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class GpuMemoryBuffer;
}

namespace gpu {
class GpuMemoryBufferManager;

namespace gles2 {
class GLES2Interface;
}
}

namespace display_compositor {

class GLHelper;

// Provides a surface that manages its own buffers, backed by GpuMemoryBuffers
// created using CHROMIUM_gpu_memory_buffer_image. Double/triple buffering is
// implemented internally. Doublebuffering occurs if PageFlipComplete is called
// before the next BindFramebuffer call, otherwise it creates extra buffers.
class DISPLAY_COMPOSITOR_EXPORT BufferQueue {
 public:
  BufferQueue(gpu::gles2::GLES2Interface* gl,
              uint32_t texture_target,
              uint32_t internal_format,
              GLHelper* gl_helper,
              gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
              gpu::SurfaceHandle surface_handle);
  virtual ~BufferQueue();

  void Initialize();

  void BindFramebuffer();
  void SwapBuffers(const gfx::Rect& damage);
  void PageFlipComplete();
  void Reshape(const gfx::Size& size, float scale_factor);

  void RecreateBuffers();

  uint32_t current_texture_id() const {
    return current_surface_ ? current_surface_->texture : 0;
  }
  uint32_t fbo() const { return fbo_; }
  uint32_t internal_format() const { return internal_format_; }

 private:
  friend class BufferQueueTest;
  friend class AllocatedSurface;

  struct DISPLAY_COMPOSITOR_EXPORT AllocatedSurface {
    AllocatedSurface(BufferQueue* buffer_queue,
                     std::unique_ptr<gfx::GpuMemoryBuffer> buffer,
                     uint32_t texture,
                     uint32_t image,
                     const gfx::Rect& rect);
    ~AllocatedSurface();
    BufferQueue* const buffer_queue;
    std::unique_ptr<gfx::GpuMemoryBuffer> buffer;
    const uint32_t texture;
    const uint32_t image;
    gfx::Rect damage;  // This is the damage for this frame from the previous.
  };

  void FreeAllSurfaces();

  void FreeSurfaceResources(AllocatedSurface* surface);

  // Copy everything that is in |copy_rect|, except for what is in
  // |exclude_rect| from |source_texture| to |texture|.
  virtual void CopyBufferDamage(int texture,
                                int source_texture,
                                const gfx::Rect& new_damage,
                                const gfx::Rect& old_damage);

  void UpdateBufferDamage(const gfx::Rect& damage);

  // Return a surface, available to be drawn into.
  std::unique_ptr<AllocatedSurface> GetNextSurface();

  std::unique_ptr<AllocatedSurface> RecreateBuffer(
      std::unique_ptr<AllocatedSurface> surface);

  gpu::gles2::GLES2Interface* const gl_;
  gfx::Size size_;
  uint32_t fbo_;
  size_t allocated_count_;
  uint32_t texture_target_;
  uint32_t internal_format_;
  // This surface is currently bound. This may be nullptr if no surface has
  // been bound, or if allocation failed at bind.
  std::unique_ptr<AllocatedSurface> current_surface_;
  // The surface currently on the screen, if any.
  std::unique_ptr<AllocatedSurface> displayed_surface_;
  // These are free for use, and are not nullptr.
  std::vector<std::unique_ptr<AllocatedSurface>> available_surfaces_;
  // These have been swapped but are not displayed yet. Entries of this deque
  // may be nullptr, if they represent frames that have been destroyed.
  std::deque<std::unique_ptr<AllocatedSurface>> in_flight_surfaces_;
  GLHelper* gl_helper_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  gpu::SurfaceHandle surface_handle_;

  DISALLOW_COPY_AND_ASSIGN(BufferQueue);
};

}  // namespace display_compositor

#endif  // COMPONENTS_DISPLAY_COMPOSITOR_BUFFER_QUEUE_H_
