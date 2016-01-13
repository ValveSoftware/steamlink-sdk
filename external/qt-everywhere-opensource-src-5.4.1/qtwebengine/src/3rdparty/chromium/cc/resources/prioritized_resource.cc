// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/prioritized_resource.h"

#include <algorithm>

#include "cc/resources/platform_color.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/priority_calculator.h"
#include "cc/trees/proxy.h"

namespace cc {

PrioritizedResource::PrioritizedResource(PrioritizedResourceManager* manager,
                                         const gfx::Size& size,
                                         ResourceFormat format)
    : size_(size),
      format_(format),
      bytes_(0),
      contents_swizzled_(false),
      priority_(PriorityCalculator::LowestPriority()),
      is_above_priority_cutoff_(false),
      is_self_managed_(false),
      backing_(NULL),
      manager_(NULL) {
  bytes_ = Resource::MemorySizeBytes(size, format);
  if (manager)
    manager->RegisterTexture(this);
}

PrioritizedResource::~PrioritizedResource() {
  if (manager_)
    manager_->UnregisterTexture(this);
}

void PrioritizedResource::SetTextureManager(
    PrioritizedResourceManager* manager) {
  if (manager_ == manager)
    return;
  if (manager_)
    manager_->UnregisterTexture(this);
  if (manager)
    manager->RegisterTexture(this);
}

void PrioritizedResource::SetDimensions(const gfx::Size& size,
                                        ResourceFormat format) {
  if (format_ != format || size_ != size) {
    is_above_priority_cutoff_ = false;
    format_ = format;
    size_ = size;
    bytes_ = Resource::MemorySizeBytes(size, format);
    DCHECK(manager_ || !backing_);
    if (manager_)
      manager_->ReturnBackingTexture(this);
  }
}

bool PrioritizedResource::RequestLate() {
  if (!manager_)
    return false;
  return manager_->RequestLate(this);
}

bool PrioritizedResource::BackingResourceWasEvicted() const {
  return backing_ ? backing_->ResourceHasBeenDeleted() : false;
}

void PrioritizedResource::AcquireBackingTexture(
    ResourceProvider* resource_provider) {
  DCHECK(is_above_priority_cutoff_);
  if (is_above_priority_cutoff_)
    manager_->AcquireBackingTextureIfNeeded(this, resource_provider);
}

void PrioritizedResource::SetPixels(ResourceProvider* resource_provider,
                                    const uint8_t* image,
                                    const gfx::Rect& image_rect,
                                    const gfx::Rect& source_rect,
                                    const gfx::Vector2d& dest_offset) {
  DCHECK(is_above_priority_cutoff_);
  if (is_above_priority_cutoff_)
    AcquireBackingTexture(resource_provider);
  DCHECK(backing_);
  resource_provider->SetPixels(
      resource_id(), image, image_rect, source_rect, dest_offset);

  // The component order may be bgra if we uploaded bgra pixels to rgba
  // texture. Mark contents as swizzled if image component order is
  // different than texture format.
  contents_swizzled_ = !PlatformColor::SameComponentOrder(format_);
}

void PrioritizedResource::Link(Backing* backing) {
  DCHECK(backing);
  DCHECK(!backing->owner_);
  DCHECK(!backing_);

  backing_ = backing;
  backing_->owner_ = this;
}

void PrioritizedResource::Unlink() {
  DCHECK(backing_);
  DCHECK(backing_->owner_ == this);

  backing_->owner_ = NULL;
  backing_ = NULL;
}

void PrioritizedResource::SetToSelfManagedMemoryPlaceholder(size_t bytes) {
  SetDimensions(gfx::Size(), RGBA_8888);
  set_is_self_managed(true);
  bytes_ = bytes;
}

PrioritizedResource::Backing::Backing(unsigned id,
                                      ResourceProvider* resource_provider,
                                      const gfx::Size& size,
                                      ResourceFormat format)
    : Resource(id, size, format),
      owner_(NULL),
      priority_at_last_priority_update_(PriorityCalculator::LowestPriority()),
      was_above_priority_cutoff_at_last_priority_update_(false),
      in_drawing_impl_tree_(false),
      in_parent_compositor_(false),
#if !DCHECK_IS_ON
      resource_has_been_deleted_(false) {
#else
      resource_has_been_deleted_(false),
      resource_provider_(resource_provider) {
#endif
}

PrioritizedResource::Backing::~Backing() {
  DCHECK(!owner_);
  DCHECK(resource_has_been_deleted_);
}

void PrioritizedResource::Backing::DeleteResource(
    ResourceProvider* resource_provider) {
  DCHECK(!proxy() || proxy()->IsImplThread());
  DCHECK(!resource_has_been_deleted_);
#if DCHECK_IS_ON
  DCHECK(resource_provider == resource_provider_);
#endif

  resource_provider->DeleteResource(id());
  set_id(0);
  resource_has_been_deleted_ = true;
}

bool PrioritizedResource::Backing::ResourceHasBeenDeleted() const {
  DCHECK(!proxy() || proxy()->IsImplThread());
  return resource_has_been_deleted_;
}

bool PrioritizedResource::Backing::CanBeRecycledIfNotInExternalUse() const {
  DCHECK(!proxy() || proxy()->IsImplThread());
  return !was_above_priority_cutoff_at_last_priority_update_ &&
         !in_drawing_impl_tree_;
}

void PrioritizedResource::Backing::UpdatePriority() {
  DCHECK(!proxy() ||
         (proxy()->IsImplThread() && proxy()->IsMainThreadBlocked()));
  if (owner_) {
    priority_at_last_priority_update_ = owner_->request_priority();
    was_above_priority_cutoff_at_last_priority_update_ =
        owner_->is_above_priority_cutoff();
  } else {
    priority_at_last_priority_update_ = PriorityCalculator::LowestPriority();
    was_above_priority_cutoff_at_last_priority_update_ = false;
  }
}

void PrioritizedResource::Backing::UpdateState(
    ResourceProvider* resource_provider) {
  DCHECK(!proxy() ||
         (proxy()->IsImplThread() && proxy()->IsMainThreadBlocked()));
  in_drawing_impl_tree_ = !!owner();
  in_parent_compositor_ = resource_provider->InUseByConsumer(id());
  if (!in_drawing_impl_tree_) {
    DCHECK_EQ(priority_at_last_priority_update_,
              PriorityCalculator::LowestPriority());
  }
}

void PrioritizedResource::ReturnBackingTexture() {
  DCHECK(manager_ || !backing_);
  if (manager_)
    manager_->ReturnBackingTexture(this);
}

const Proxy* PrioritizedResource::Backing::proxy() const {
  if (!owner_ || !owner_->resource_manager())
    return NULL;
  return owner_->resource_manager()->ProxyForDebug();
}

}  // namespace cc
