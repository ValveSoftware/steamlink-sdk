// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_LAYER_UPDATER_H_
#define CC_RESOURCES_LAYER_UPDATER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/vector2d.h"

namespace cc {

class PrioritizedResource;
class PrioritizedResourceManager;
class ResourceUpdateQueue;
class TextureManager;

class CC_EXPORT LayerUpdater : public base::RefCounted<LayerUpdater> {
 public:
  // Allows updaters to store per-resource update properties.
  class CC_EXPORT Resource {
   public:
    virtual ~Resource();

    PrioritizedResource* texture() { return texture_.get(); }
    // TODO(reveman): partial_update should be a property of this class
    // instead of an argument passed to Update().
    virtual void Update(ResourceUpdateQueue* queue,
                        const gfx::Rect& source_rect,
                        const gfx::Vector2d& dest_offset,
                        bool partial_update) = 0;

   protected:
    explicit Resource(scoped_ptr<PrioritizedResource> texture);

   private:
    scoped_ptr<PrioritizedResource> texture_;

    DISALLOW_COPY_AND_ASSIGN(Resource);
  };

  LayerUpdater() {}

  virtual scoped_ptr<Resource> CreateResource(
      PrioritizedResourceManager* manager) = 0;
  // The |resulting_opaque_rect| gives back a region of the layer that was
  // painted opaque. If the layer is marked opaque in the updater, then this
  // region should be ignored in preference for the entire layer's area.
  virtual void PrepareToUpdate(const gfx::Rect& content_rect,
                               const gfx::Size& tile_size,
                               float contents_width_scale,
                               float contents_height_scale,
                               gfx::Rect* resulting_opaque_rect) {}
  virtual void ReduceMemoryUsage() {}

  // Set true by the layer when it is known that the entire output is going to
  // be opaque.
  virtual void SetOpaque(bool opaque) {}
  // Set true by the layer when it is known that the entire output bounds will
  // be rasterized.
  virtual void SetFillsBoundsCompletely(bool fills_bounds) {}

 protected:
  virtual ~LayerUpdater() {}

 private:
  friend class base::RefCounted<LayerUpdater>;

  DISALLOW_COPY_AND_ASSIGN(LayerUpdater);
};

}  // namespace cc

#endif  // CC_RESOURCES_LAYER_UPDATER_H_
