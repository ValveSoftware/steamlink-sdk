// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_DEMO_BITMAP_UPLOADER_H_
#define SERVICES_UI_DEMO_BITMAP_UPLOADER_H_

#include <stdint.h>

#include <memory>
#include <unordered_map>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/output/compositor_frame_sink_client.h"
#include "gpu/GLES2/gl2chromium.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "services/ui/public/cpp/window_compositor_frame_sink.h"

namespace gpu {
class GpuChannelHost;
}

namespace ui {

class GpuService;
class Window;

extern const char kBitmapUploaderForAcceleratedWidget[];

// BitmapUploader is useful if you want to draw a bitmap or color in a
// Window.
class BitmapUploader : public cc::CompositorFrameSinkClient {
 public:
  explicit BitmapUploader(Window* window);
  ~BitmapUploader() override;

  void Init(GpuService* gpu_service);
  // Sets the color which is RGBA.
  void SetColor(uint32_t color);

  enum Format {
    RGBA,  // Pixel layout on Android.
    BGRA,  // Pixel layout everywhere else.
  };

  // Sets a bitmap.
  void SetBitmap(int width,
                 int height,
                 std::unique_ptr<std::vector<unsigned char>> data,
                 Format format);

 private:
  void Upload();

  void OnGpuChannelEstablished(
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      scoped_refptr<gpu::GpuChannelHost> gpu_channel);

  uint32_t BindTextureForSize(const gfx::Size& size);

  uint32_t TextureFormat() const {
    return format_ == BGRA ? GL_BGRA_EXT : GL_RGBA;
  }

  void SetIdNamespace(uint32_t id_namespace);

  // cc::CompositorFrameSinkClient implementation.
  void SetBeginFrameSource(cc::BeginFrameSource* source) override;
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override;
  void SetTreeActivationCallback(const base::Closure& callback) override;
  void DidReceiveCompositorFrameAck() override;
  void DidLoseCompositorFrameSink() override;
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              bool resourceless_software_draw) override;
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override;
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) override;

  Window* window_;
  std::unique_ptr<WindowCompositorFrameSink> compositor_frame_sink_;

  uint32_t color_;
  int width_;
  int height_;
  Format format_;
  std::unique_ptr<std::vector<unsigned char>> bitmap_;
  uint32_t next_resource_id_;
  std::unordered_map<uint32_t, uint32_t> resource_to_texture_id_map_;

  base::WeakPtrFactory<BitmapUploader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BitmapUploader);
};

}  // namespace ui

#endif  // SERVICES_UI_DEMO_BITMAP_UPLOADER_H_
