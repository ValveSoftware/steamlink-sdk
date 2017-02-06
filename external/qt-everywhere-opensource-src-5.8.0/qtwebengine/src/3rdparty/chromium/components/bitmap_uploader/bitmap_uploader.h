// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BITMAP_UPLOADER_BITMAP_UPLOADER_H_
#define COMPONENTS_BITMAP_UPLOADER_BITMAP_UPLOADER_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "components/bitmap_uploader/bitmap_uploader_export.h"
#include "components/mus/public/cpp/window_surface.h"
#include "components/mus/public/cpp/window_surface_client.h"
#include "components/mus/public/interfaces/surface.mojom.h"
#include "gpu/GLES2/gl2chromium.h"
#include "gpu/GLES2/gl2extchromium.h"

namespace mus {
class GLES2Context;
}

namespace shell {
class Connector;
}

namespace bitmap_uploader {

BITMAP_UPLOADER_EXPORT extern const char kBitmapUploaderForAcceleratedWidget[];

// BitmapUploader is useful if you want to draw a bitmap or color in a
// mus::Window.
class BITMAP_UPLOADER_EXPORT BitmapUploader
    : public NON_EXPORTED_BASE(mus::WindowSurfaceClient) {
 public:
  explicit BitmapUploader(mus::Window* window);
  ~BitmapUploader() override;

  void Init(shell::Connector* connector);

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

  uint32_t BindTextureForSize(const gfx::Size& size);

  uint32_t TextureFormat() const {
    return format_ == BGRA ? GL_BGRA_EXT : GL_RGBA;
  }

  void SetIdNamespace(uint32_t id_namespace);

  // WindowSurfaceClient implementation.
  void OnResourcesReturned(
      mus::WindowSurface* surface,
      mojo::Array<cc::ReturnedResource> resources) override;

  mus::Window* window_;
  std::unique_ptr<mus::WindowSurface> surface_;
  // This may be null if there is an error contacting mus/initializing. We
  // assume we'll be shutting down soon and do nothing in this case.
  std::unique_ptr<mus::GLES2Context> gles2_context_;

  uint32_t color_;
  int width_;
  int height_;
  Format format_;
  std::unique_ptr<std::vector<unsigned char>> bitmap_;
  uint32_t next_resource_id_;
  uint32_t id_namespace_;
  base::hash_map<uint32_t, uint32_t> resource_to_texture_id_map_;

  DISALLOW_COPY_AND_ASSIGN(BitmapUploader);
};

}  // namespace bitmap_uploader

#endif  // COMPONENTS_BITMAP_UPLOADER_BITMAP_UPLAODER_H_
