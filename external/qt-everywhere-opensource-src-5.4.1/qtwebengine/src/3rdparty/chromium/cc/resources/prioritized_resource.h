// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PRIORITIZED_RESOURCE_H_
#define CC_RESOURCES_PRIORITIZED_RESOURCE_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/resources/priority_calculator.h"
#include "cc/resources/resource.h"
#include "cc/resources/resource_provider.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/vector2d.h"

namespace cc {

class PrioritizedResourceManager;
class Proxy;

class CC_EXPORT PrioritizedResource {
 public:
  static scoped_ptr<PrioritizedResource> Create(
      PrioritizedResourceManager* manager,
      const gfx::Size& size,
      ResourceFormat format) {
    return make_scoped_ptr(new PrioritizedResource(manager, size, format));
  }
  static scoped_ptr<PrioritizedResource> Create(
      PrioritizedResourceManager* manager) {
    return make_scoped_ptr(
        new PrioritizedResource(manager, gfx::Size(), RGBA_8888));
  }
  ~PrioritizedResource();

  // Texture properties. Changing these causes the backing texture to be lost.
  // Setting these to the same value is a no-op.
  void SetTextureManager(PrioritizedResourceManager* manager);
  PrioritizedResourceManager* resource_manager() { return manager_; }
  void SetDimensions(const gfx::Size& size, ResourceFormat format);
  ResourceFormat format() const { return format_; }
  gfx::Size size() const { return size_; }
  size_t bytes() const { return bytes_; }
  bool contents_swizzled() const { return contents_swizzled_; }

  // Set priority for the requested texture.
  void set_request_priority(int priority) { priority_ = priority; }
  int request_priority() const { return priority_; }

  // After PrioritizedResource::PrioritizeTextures() is called, this returns
  // if the the request succeeded and this texture can be acquired for use.
  bool can_acquire_backing_texture() const { return is_above_priority_cutoff_; }

  // This returns whether we still have a backing texture. This can continue
  // to be true even after CanAcquireBackingTexture() becomes false. In this
  // case the texture can be used but shouldn't be updated since it will get
  // taken away "soon".
  bool have_backing_texture() const { return !!backing(); }

  bool BackingResourceWasEvicted() const;

  // If CanAcquireBackingTexture() is true AcquireBackingTexture() will acquire
  // a backing texture for use. Call this whenever the texture is actually
  // needed.
  void AcquireBackingTexture(ResourceProvider* resource_provider);

  // TODO(epenner): Request late is really a hack for when we are totally out of
  // memory (all textures are visible) but we can still squeeze into the limit
  // by not painting occluded textures. In this case the manager refuses all
  // visible textures and RequestLate() will enable CanAcquireBackingTexture()
  // on a call-order basis. We might want to just remove this in the future
  // (carefully) and just make sure we don't regress OOMs situations.
  bool RequestLate();

  // Update pixels of backing resource from image. This functions will aquire
  // the backing if needed.
  void SetPixels(ResourceProvider* resource_provider,
                 const uint8_t* image,
                 const gfx::Rect& image_rect,
                 const gfx::Rect& source_rect,
                 const gfx::Vector2d& dest_offset);

  ResourceProvider::ResourceId resource_id() const {
    return backing_ ? backing_->id() : 0;
  }

  // Self-managed textures are accounted for when prioritizing other textures,
  // but they are not allocated/recycled/deleted, so this needs to be done
  // externally. CanAcquireBackingTexture() indicates if the texture would have
  // been allowed given its priority.
  void set_is_self_managed(bool is_self_managed) {
    is_self_managed_ = is_self_managed;
  }
  bool is_self_managed() { return is_self_managed_; }
  void SetToSelfManagedMemoryPlaceholder(size_t bytes);

  void ReturnBackingTexture();

 private:
  friend class PrioritizedResourceManager;
  friend class PrioritizedResourceTest;

  class Backing : public Resource {
   public:
    Backing(unsigned id,
            ResourceProvider* resource_provider,
            const gfx::Size& size,
            ResourceFormat format);
    ~Backing();
    void UpdatePriority();
    void UpdateState(ResourceProvider* resource_provider);

    PrioritizedResource* owner() { return owner_; }
    bool CanBeRecycledIfNotInExternalUse() const;
    int request_priority_at_last_priority_update() const {
      return priority_at_last_priority_update_;
    }
    bool was_above_priority_cutoff_at_last_priority_update() const {
      return was_above_priority_cutoff_at_last_priority_update_;
    }
    bool in_drawing_impl_tree() const { return in_drawing_impl_tree_; }
    bool in_parent_compositor() const { return in_parent_compositor_; }

    void DeleteResource(ResourceProvider* resource_provider);
    bool ResourceHasBeenDeleted() const;

   private:
    const Proxy* proxy() const;

    friend class PrioritizedResource;
    friend class PrioritizedResourceManager;
    PrioritizedResource* owner_;
    int priority_at_last_priority_update_;
    bool was_above_priority_cutoff_at_last_priority_update_;

    // Set if this is currently-drawing impl tree.
    bool in_drawing_impl_tree_;
    // Set if this is in the parent compositor.
    bool in_parent_compositor_;

    bool resource_has_been_deleted_;

#if DCHECK_IS_ON
    ResourceProvider* resource_provider_;
#endif
    DISALLOW_COPY_AND_ASSIGN(Backing);
  };

  PrioritizedResource(PrioritizedResourceManager* resource_manager,
                      const gfx::Size& size,
                      ResourceFormat format);

  bool is_above_priority_cutoff() { return is_above_priority_cutoff_; }
  void set_above_priority_cutoff(bool is_above_priority_cutoff) {
    is_above_priority_cutoff_ = is_above_priority_cutoff;
  }
  void set_manager_internal(PrioritizedResourceManager* manager) {
    manager_ = manager;
  }

  Backing* backing() const { return backing_; }
  void Link(Backing* backing);
  void Unlink();

  gfx::Size size_;
  ResourceFormat format_;
  size_t bytes_;
  bool contents_swizzled_;

  int priority_;
  bool is_above_priority_cutoff_;
  bool is_self_managed_;

  Backing* backing_;
  PrioritizedResourceManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(PrioritizedResource);
};

}  // namespace cc

#endif  // CC_RESOURCES_PRIORITIZED_RESOURCE_H_
