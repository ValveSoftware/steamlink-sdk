// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/image_layer_updater.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/resource_update_queue.h"

namespace cc {

ImageLayerUpdater::Resource::Resource(ImageLayerUpdater* updater,
                                      scoped_ptr<PrioritizedResource> texture)
    : LayerUpdater::Resource(texture.Pass()), updater_(updater) {}

ImageLayerUpdater::Resource::~Resource() {}

void ImageLayerUpdater::Resource::Update(ResourceUpdateQueue* queue,
                                         const gfx::Rect& source_rect,
                                         const gfx::Vector2d& dest_offset,
                                         bool partial_update) {
  updater_->UpdateTexture(
      queue, texture(), source_rect, dest_offset, partial_update);
}

// static
scoped_refptr<ImageLayerUpdater> ImageLayerUpdater::Create() {
  return make_scoped_refptr(new ImageLayerUpdater());
}

scoped_ptr<LayerUpdater::Resource> ImageLayerUpdater::CreateResource(
    PrioritizedResourceManager* manager) {
  return scoped_ptr<LayerUpdater::Resource>(
      new Resource(this, PrioritizedResource::Create(manager)));
}

void ImageLayerUpdater::UpdateTexture(ResourceUpdateQueue* queue,
                                      PrioritizedResource* texture,
                                      const gfx::Rect& source_rect,
                                      const gfx::Vector2d& dest_offset,
                                      bool partial_update) {
  // Source rect should never go outside the image pixels, even if this
  // is requested because the texture extends outside the image.
  gfx::Rect clipped_source_rect = source_rect;
  gfx::Rect image_rect = gfx::Rect(0, 0, bitmap_.width(), bitmap_.height());
  clipped_source_rect.Intersect(image_rect);

  gfx::Vector2d clipped_dest_offset =
      dest_offset +
      gfx::Vector2d(clipped_source_rect.origin() - source_rect.origin());

  ResourceUpdate upload = ResourceUpdate::Create(
      texture, &bitmap_, image_rect, clipped_source_rect, clipped_dest_offset);
  if (partial_update)
    queue->AppendPartialUpload(upload);
  else
    queue->AppendFullUpload(upload);
}

void ImageLayerUpdater::SetBitmap(const SkBitmap& bitmap) {
  DCHECK(bitmap.pixelRef());
  bitmap_ = bitmap;
}

bool ImageLayerUpdater::UsingBitmap(const SkBitmap& bitmap) const {
  return bitmap.pixelRef() == bitmap_.pixelRef();
}

}  // namespace cc
