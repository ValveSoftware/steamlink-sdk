// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bitmap_uploader/bitmap_uploader.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "cc/ipc/compositor_frame.mojom.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "components/mus/public/cpp/gles2_context.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_surface.h"

namespace bitmap_uploader {
namespace {

const uint32_t g_transparent_color = 0x00000000;

}  // namespace

const char kBitmapUploaderForAcceleratedWidget[] =
    "__BITMAP_UPLOADER_ACCELERATED_WIDGET__";

BitmapUploader::BitmapUploader(mus::Window* window)
    : window_(window),
      color_(g_transparent_color),
      width_(0),
      height_(0),
      format_(BGRA),
      next_resource_id_(1u),
      id_namespace_(0u) {}

BitmapUploader::~BitmapUploader() {
}

void BitmapUploader::Init(shell::Connector* connector) {
  surface_ = window_->RequestSurface(mus::mojom::SurfaceType::DEFAULT);
  surface_->BindToThread();
  surface_->set_client(this);

  gles2_context_ = mus::GLES2Context::CreateOffscreenContext(
      std::vector<int32_t>(), connector);
  // CreateOffscreenContext() may return null.
}

// Sets the color which is RGBA.
void BitmapUploader::SetColor(uint32_t color) {
  if (color_ == color)
    return;
  color_ = color;
  if (surface_)
    Upload();
}

// Sets a bitmap.
void BitmapUploader::SetBitmap(int width,
                               int height,
                               std::unique_ptr<std::vector<unsigned char>> data,
                               Format format) {
  width_ = width;
  height_ = height;
  bitmap_ = std::move(data);
  format_ = format;
  if (surface_)
    Upload();
}

void BitmapUploader::Upload() {
  if (!gles2_context_)
    return;

  const gfx::Rect bounds(window_->bounds().size());

  cc::CompositorFrame frame;
  // TODO(rjkroege): Support device scale factors other than 1.
  frame.metadata.device_scale_factor = 1.0f;
  frame.delegated_frame_data.reset(new cc::DelegatedFrameData());
  frame.delegated_frame_data->resource_list.resize(0u);

  const cc::RenderPassId render_pass_id(1, 1);
  std::unique_ptr<cc::RenderPass> pass = cc::RenderPass::Create();
  pass->SetAll(render_pass_id, bounds, bounds, gfx::Transform(),
               true /* has_transparent_background */);

  // The SharedQuadState is owned by the SharedQuadStateList
  // shared_quad_state_list.
  cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(gfx::Transform(), bounds.size(), bounds, bounds,
              false /* is_clipped */, 1.f /* opacity */, SkXfermode::kSrc_Mode,
              0 /* sorting_context_id */);

  if (bitmap_.get()) {
    gpu::gles2::GLES2Interface* gl = gles2_context_->interface();
    gfx::Size bitmap_size(width_, height_);
    GLuint texture_id = BindTextureForSize(bitmap_size);
    gl->TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, bitmap_size.width(),
                      bitmap_size.height(), TextureFormat(), GL_UNSIGNED_BYTE,
                      &((*bitmap_)[0]));

    gpu::Mailbox mailbox;
    gl->GenMailboxCHROMIUM(mailbox.name);
    gl->ProduceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);

    const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
    gl->ShallowFlushCHROMIUM();

    gpu::SyncToken sync_token;
    gl->GenSyncTokenCHROMIUM(fence_sync, sync_token.GetData());

    cc::TransferableResource resource;
    resource.id = next_resource_id_++;
    resource_to_texture_id_map_[resource.id] = texture_id;
    resource.format = cc::ResourceFormat::RGBA_8888;
    resource.filter = GL_LINEAR;
    resource.size = bitmap_size;
    resource.mailbox_holder =
        gpu::MailboxHolder(mailbox, sync_token, GL_TEXTURE_2D);
    resource.read_lock_fences_enabled = false;
    resource.is_software = false;
    resource.is_overlay_candidate = false;
    frame.delegated_frame_data->resource_list.push_back(std::move(resource));

    cc::TextureDrawQuad* quad =
        pass->CreateAndAppendDrawQuad<cc::TextureDrawQuad>();

    gfx::Size rect_size;
    if (width_ <= bounds.width() && height_ <= bounds.height()) {
      rect_size.SetSize(width_, height_);
    } else {
      // The source bitmap is larger than the viewport. Resize it while
      // maintaining the aspect ratio.
      float width_ratio = static_cast<float>(width_) / bounds.width();
      float height_ratio = static_cast<float>(height_) / bounds.height();
      if (width_ratio > height_ratio) {
        rect_size.SetSize(bounds.width(), height_ / width_ratio);
      } else {
        rect_size.SetSize(width_ / height_ratio, bounds.height());
      }
    }
    gfx::Rect rect(rect_size);
    const bool needs_blending = true;
    const bool premultiplied_alpha = true;
    const gfx::PointF uv_top_left(0.f, 0.f);
    const gfx::PointF uv_bottom_right(1.f, 1.f);
    float vertex_opacity[4] = {1.f, 1.f, 1.f, 1.f};
    const bool y_flipped = false;
    const bool nearest_neighbor = false;
    quad->SetAll(sqs, rect, rect, rect, needs_blending, resource.id,
                 gfx::Size(), premultiplied_alpha, uv_top_left, uv_bottom_right,
                 g_transparent_color, vertex_opacity, y_flipped,
                 nearest_neighbor, false);
  }

  if (color_ != g_transparent_color) {
    cc::SolidColorDrawQuad* quad =
        pass->CreateAndAppendDrawQuad<cc::SolidColorDrawQuad>();
    const bool force_antialiasing_off = false;
    const gfx::Rect opaque_rect(0, 0, 0, 0);
    const bool needs_blending = true;
    quad->SetAll(sqs, bounds, opaque_rect, bounds, needs_blending, color_,
                 force_antialiasing_off);
  }

  frame.delegated_frame_data->render_pass_list.push_back(std::move(pass));

  // TODO(rjkroege, fsamuel): We should throttle frames.
  surface_->SubmitCompositorFrame(std::move(frame), base::Closure());
}

uint32_t BitmapUploader::BindTextureForSize(const gfx::Size& size) {
  gpu::gles2::GLES2Interface* gl = gles2_context_->interface();
  // TODO(jamesr): Recycle textures.
  GLuint texture = 0u;
  gl->GenTextures(1, &texture);
  gl->BindTexture(GL_TEXTURE_2D, texture);
  gl->TexImage2D(GL_TEXTURE_2D, 0, TextureFormat(), size.width(), size.height(),
                 0, TextureFormat(), GL_UNSIGNED_BYTE, 0);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  return texture;
}

void BitmapUploader::SetIdNamespace(uint32_t id_namespace) {
  id_namespace_ = id_namespace;
  if (color_ != g_transparent_color || bitmap_.get())
    Upload();
}

void BitmapUploader::OnResourcesReturned(
    mus::WindowSurface* surface,
    mojo::Array<cc::ReturnedResource> resources) {
  gpu::gles2::GLES2Interface* gl = gles2_context_->interface();
  // TODO(jamesr): Recycle.
  for (size_t i = 0; i < resources.size(); ++i) {
    cc::ReturnedResource resource = std::move(resources[i]);
    DCHECK_EQ(1, resource.count);
    gl->WaitSyncTokenCHROMIUM(resource.sync_token.GetConstData());
    uint32_t texture_id = resource_to_texture_id_map_[resource.id];
    DCHECK_NE(0u, texture_id);
    resource_to_texture_id_map_.erase(resource.id);
    gl->DeleteTextures(1, &texture_id);
  }
}

}  // namespace bitmap_uploader
