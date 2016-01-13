// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/prioritized_resource_manager.h"

#include <algorithm>

#include "base/debug/trace_event.h"
#include "base/stl_util.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/priority_calculator.h"
#include "cc/trees/proxy.h"

namespace cc {

PrioritizedResourceManager::PrioritizedResourceManager(const Proxy* proxy)
    : max_memory_limit_bytes_(DefaultMemoryAllocationLimit()),
      external_priority_cutoff_(PriorityCalculator::AllowEverythingCutoff()),
      memory_use_bytes_(0),
      memory_above_cutoff_bytes_(0),
      max_memory_needed_bytes_(0),
      memory_available_bytes_(0),
      proxy_(proxy),
      backings_tail_not_sorted_(false),
      memory_visible_bytes_(0),
      memory_visible_and_nearby_bytes_(0),
      memory_visible_last_pushed_bytes_(0),
      memory_visible_and_nearby_last_pushed_bytes_(0) {}

PrioritizedResourceManager::~PrioritizedResourceManager() {
  while (textures_.size() > 0)
    UnregisterTexture(*textures_.begin());

  UnlinkAndClearEvictedBackings();
  DCHECK(evicted_backings_.empty());

  // Each remaining backing is a leaked opengl texture. There should be none.
  DCHECK(backings_.empty());
}

size_t PrioritizedResourceManager::MemoryVisibleBytes() const {
  DCHECK(proxy_->IsImplThread());
  return memory_visible_last_pushed_bytes_;
}

size_t PrioritizedResourceManager::MemoryVisibleAndNearbyBytes() const {
  DCHECK(proxy_->IsImplThread());
  return memory_visible_and_nearby_last_pushed_bytes_;
}

void PrioritizedResourceManager::PrioritizeTextures() {
  TRACE_EVENT0("cc", "PrioritizedResourceManager::PrioritizeTextures");
  DCHECK(proxy_->IsMainThread());

  // Sorting textures in this function could be replaced by a slightly
  // modified O(n) quick-select to partition textures rather than
  // sort them (if performance of the sort becomes an issue).

  TextureVector& sorted_textures = temp_texture_vector_;
  sorted_textures.clear();

  // Copy all textures into a vector, sort them, and collect memory requirements
  // statistics.
  memory_visible_bytes_ = 0;
  memory_visible_and_nearby_bytes_ = 0;
  for (TextureSet::iterator it = textures_.begin(); it != textures_.end();
       ++it) {
    PrioritizedResource* texture = (*it);
    sorted_textures.push_back(texture);
    if (PriorityCalculator::priority_is_higher(
            texture->request_priority(),
            PriorityCalculator::AllowVisibleOnlyCutoff()))
      memory_visible_bytes_ += texture->bytes();
    if (PriorityCalculator::priority_is_higher(
            texture->request_priority(),
            PriorityCalculator::AllowVisibleAndNearbyCutoff()))
      memory_visible_and_nearby_bytes_ += texture->bytes();
  }
  std::sort(sorted_textures.begin(), sorted_textures.end(), CompareTextures);

  // Compute a priority cutoff based on memory pressure
  memory_available_bytes_ = max_memory_limit_bytes_;
  priority_cutoff_ = external_priority_cutoff_;
  size_t memory_bytes = 0;
  for (TextureVector::iterator it = sorted_textures.begin();
       it != sorted_textures.end();
       ++it) {
    if ((*it)->is_self_managed()) {
      // Account for self-managed memory immediately by reducing the memory
      // available (since it never gets acquired).
      size_t new_memory_bytes = memory_bytes + (*it)->bytes();
      if (new_memory_bytes > memory_available_bytes_) {
        priority_cutoff_ = (*it)->request_priority();
        memory_available_bytes_ = memory_bytes;
        break;
      }
      memory_available_bytes_ -= (*it)->bytes();
    } else {
      size_t new_memory_bytes = memory_bytes + (*it)->bytes();
      if (new_memory_bytes > memory_available_bytes_) {
        priority_cutoff_ = (*it)->request_priority();
        break;
      }
      memory_bytes = new_memory_bytes;
    }
  }

  // Disallow any textures with priority below the external cutoff to have
  // backings.
  for (TextureVector::iterator it = sorted_textures.begin();
       it != sorted_textures.end();
       ++it) {
    PrioritizedResource* texture = (*it);
    if (!PriorityCalculator::priority_is_higher(texture->request_priority(),
                                                external_priority_cutoff_) &&
        texture->have_backing_texture())
      texture->Unlink();
  }

  // Only allow textures if they are higher than the cutoff. All textures
  // of the same priority are accepted or rejected together, rather than
  // being partially allowed randomly.
  max_memory_needed_bytes_ = 0;
  memory_above_cutoff_bytes_ = 0;
  for (TextureVector::iterator it = sorted_textures.begin();
       it != sorted_textures.end();
       ++it) {
    PrioritizedResource* resource = *it;
    bool is_above_priority_cutoff = PriorityCalculator::priority_is_higher(
        resource->request_priority(), priority_cutoff_);
    resource->set_above_priority_cutoff(is_above_priority_cutoff);
    if (!resource->is_self_managed()) {
      max_memory_needed_bytes_ += resource->bytes();
      if (is_above_priority_cutoff)
        memory_above_cutoff_bytes_ += resource->bytes();
    }
  }
  sorted_textures.clear();

  DCHECK_LE(memory_above_cutoff_bytes_, memory_available_bytes_);
  DCHECK_LE(MemoryAboveCutoffBytes(), MaxMemoryLimitBytes());
}

void PrioritizedResourceManager::PushTexturePrioritiesToBackings() {
  TRACE_EVENT0("cc",
               "PrioritizedResourceManager::PushTexturePrioritiesToBackings");
  DCHECK(proxy_->IsImplThread() && proxy_->IsMainThreadBlocked());

  AssertInvariants();
  for (BackingList::iterator it = backings_.begin(); it != backings_.end();
       ++it)
    (*it)->UpdatePriority();
  SortBackings();
  AssertInvariants();

  // Push memory requirements to the impl thread structure.
  memory_visible_last_pushed_bytes_ = memory_visible_bytes_;
  memory_visible_and_nearby_last_pushed_bytes_ =
      memory_visible_and_nearby_bytes_;
}

void PrioritizedResourceManager::UpdateBackingsState(
    ResourceProvider* resource_provider) {
  TRACE_EVENT0("cc",
               "PrioritizedResourceManager::UpdateBackingsInDrawingImplTree");
  DCHECK(proxy_->IsImplThread() && proxy_->IsMainThreadBlocked());

  AssertInvariants();
  for (BackingList::iterator it = backings_.begin(); it != backings_.end();
       ++it) {
    PrioritizedResource::Backing* backing = (*it);
    backing->UpdateState(resource_provider);
  }
  SortBackings();
  AssertInvariants();
}

void PrioritizedResourceManager::SortBackings() {
  TRACE_EVENT0("cc", "PrioritizedResourceManager::SortBackings");
  DCHECK(proxy_->IsImplThread());

  // Put backings in eviction/recycling order.
  backings_.sort(CompareBackings);
  backings_tail_not_sorted_ = false;
}

void PrioritizedResourceManager::ClearPriorities() {
  DCHECK(proxy_->IsMainThread());
  for (TextureSet::iterator it = textures_.begin(); it != textures_.end();
       ++it) {
    // TODO(reveman): We should remove this and just set all priorities to
    // PriorityCalculator::lowestPriority() once we have priorities for all
    // textures (we can't currently calculate distances for off-screen
    // textures).
    (*it)->set_request_priority(
        PriorityCalculator::LingeringPriority((*it)->request_priority()));
  }
}

bool PrioritizedResourceManager::RequestLate(PrioritizedResource* texture) {
  DCHECK(proxy_->IsMainThread());

  // This is already above cutoff, so don't double count it's memory below.
  if (texture->is_above_priority_cutoff())
    return true;

  // Allow textures that have priority equal to the cutoff, but not strictly
  // lower.
  if (PriorityCalculator::priority_is_lower(texture->request_priority(),
                                            priority_cutoff_))
    return false;

  // Disallow textures that do not have a priority strictly higher than the
  // external cutoff.
  if (!PriorityCalculator::priority_is_higher(texture->request_priority(),
                                              external_priority_cutoff_))
    return false;

  size_t new_memory_bytes = memory_above_cutoff_bytes_ + texture->bytes();
  if (new_memory_bytes > memory_available_bytes_)
    return false;

  memory_above_cutoff_bytes_ = new_memory_bytes;
  texture->set_above_priority_cutoff(true);
  return true;
}

void PrioritizedResourceManager::AcquireBackingTextureIfNeeded(
    PrioritizedResource* texture,
    ResourceProvider* resource_provider) {
  DCHECK(proxy_->IsImplThread() && proxy_->IsMainThreadBlocked());
  DCHECK(!texture->is_self_managed());
  DCHECK(texture->is_above_priority_cutoff());
  if (texture->backing() || !texture->is_above_priority_cutoff())
    return;

  // Find a backing below, by either recycling or allocating.
  PrioritizedResource::Backing* backing = NULL;

  // First try to recycle
  for (BackingList::iterator it = backings_.begin(); it != backings_.end();
       ++it) {
    if (!(*it)->CanBeRecycledIfNotInExternalUse())
      break;
    if (resource_provider->InUseByConsumer((*it)->id()))
      continue;
    if ((*it)->size() == texture->size() &&
        (*it)->format() == texture->format()) {
      backing = (*it);
      backings_.erase(it);
      break;
    }
  }

  // Otherwise reduce memory and just allocate a new backing texures.
  if (!backing) {
    EvictBackingsToReduceMemory(memory_available_bytes_ - texture->bytes(),
                                PriorityCalculator::AllowEverythingCutoff(),
                                EVICT_ONLY_RECYCLABLE,
                                DO_NOT_UNLINK_BACKINGS,
                                resource_provider);
    backing =
        CreateBacking(texture->size(), texture->format(), resource_provider);
  }

  // Move the used backing to the end of the eviction list, and note that
  // the tail is not sorted.
  if (backing->owner())
    backing->owner()->Unlink();
  texture->Link(backing);
  backings_.push_back(backing);
  backings_tail_not_sorted_ = true;

  // Update the backing's priority from its new owner.
  backing->UpdatePriority();
}

bool PrioritizedResourceManager::EvictBackingsToReduceMemory(
    size_t limit_bytes,
    int priority_cutoff,
    EvictionPolicy eviction_policy,
    UnlinkPolicy unlink_policy,
    ResourceProvider* resource_provider) {
  DCHECK(proxy_->IsImplThread());
  if (unlink_policy == UNLINK_BACKINGS)
    DCHECK(proxy_->IsMainThreadBlocked());
  if (MemoryUseBytes() <= limit_bytes &&
      PriorityCalculator::AllowEverythingCutoff() == priority_cutoff)
    return false;

  // Destroy backings until we are below the limit,
  // or until all backings remaining are above the cutoff.
  bool evicted_anything = false;
  while (backings_.size() > 0) {
    PrioritizedResource::Backing* backing = backings_.front();
    if (MemoryUseBytes() <= limit_bytes &&
        PriorityCalculator::priority_is_higher(
            backing->request_priority_at_last_priority_update(),
            priority_cutoff))
      break;
    if (eviction_policy == EVICT_ONLY_RECYCLABLE &&
        !backing->CanBeRecycledIfNotInExternalUse())
      break;
    if (unlink_policy == UNLINK_BACKINGS && backing->owner())
      backing->owner()->Unlink();
    EvictFirstBackingResource(resource_provider);
    evicted_anything = true;
  }
  return evicted_anything;
}

void PrioritizedResourceManager::ReduceWastedMemory(
    ResourceProvider* resource_provider) {
  // We currently collect backings from deleted textures for later recycling.
  // However, if we do that forever we will always use the max limit even if
  // we really need very little memory. This should probably be solved by
  // reducing the limit externally, but until then this just does some "clean
  // up" of unused backing textures (any more than 10%).
  size_t wasted_memory = 0;
  for (BackingList::iterator it = backings_.begin(); it != backings_.end();
       ++it) {
    if ((*it)->owner())
      break;
    if ((*it)->in_parent_compositor())
      continue;
    wasted_memory += (*it)->bytes();
  }
  size_t wasted_memory_to_allow = memory_available_bytes_ / 10;
  // If the external priority cutoff indicates that unused memory should be
  // freed, then do not allow any memory for texture recycling.
  if (external_priority_cutoff_ != PriorityCalculator::AllowEverythingCutoff())
    wasted_memory_to_allow = 0;
  if (wasted_memory > wasted_memory_to_allow)
    EvictBackingsToReduceMemory(MemoryUseBytes() -
                                (wasted_memory - wasted_memory_to_allow),
                                PriorityCalculator::AllowEverythingCutoff(),
                                EVICT_ONLY_RECYCLABLE,
                                DO_NOT_UNLINK_BACKINGS,
                                resource_provider);
}

void PrioritizedResourceManager::ReduceMemory(
    ResourceProvider* resource_provider) {
  DCHECK(proxy_->IsImplThread() && proxy_->IsMainThreadBlocked());
  EvictBackingsToReduceMemory(memory_available_bytes_,
                              PriorityCalculator::AllowEverythingCutoff(),
                              EVICT_ANYTHING,
                              UNLINK_BACKINGS,
                              resource_provider);
  DCHECK_LE(MemoryUseBytes(), memory_available_bytes_);

  ReduceWastedMemory(resource_provider);
}

void PrioritizedResourceManager::ClearAllMemory(
    ResourceProvider* resource_provider) {
  DCHECK(proxy_->IsImplThread() && proxy_->IsMainThreadBlocked());
  if (!resource_provider) {
    DCHECK(backings_.empty());
    return;
  }
  EvictBackingsToReduceMemory(0,
                              PriorityCalculator::AllowEverythingCutoff(),
                              EVICT_ANYTHING,
                              DO_NOT_UNLINK_BACKINGS,
                              resource_provider);
}

bool PrioritizedResourceManager::ReduceMemoryOnImplThread(
    size_t limit_bytes,
    int priority_cutoff,
    ResourceProvider* resource_provider) {
  DCHECK(proxy_->IsImplThread());
  DCHECK(resource_provider);

  // If we are in the process of uploading a new frame then the backings at the
  // very end of the list are not sorted by priority. Sort them before doing the
  // eviction.
  if (backings_tail_not_sorted_)
    SortBackings();
  return EvictBackingsToReduceMemory(limit_bytes,
                                     priority_cutoff,
                                     EVICT_ANYTHING,
                                     DO_NOT_UNLINK_BACKINGS,
                                     resource_provider);
}

void PrioritizedResourceManager::UnlinkAndClearEvictedBackings() {
  DCHECK(proxy_->IsMainThread());
  base::AutoLock scoped_lock(evicted_backings_lock_);
  for (BackingList::const_iterator it = evicted_backings_.begin();
       it != evicted_backings_.end();
       ++it) {
    PrioritizedResource::Backing* backing = (*it);
    if (backing->owner())
      backing->owner()->Unlink();
    delete backing;
  }
  evicted_backings_.clear();
}

bool PrioritizedResourceManager::LinkedEvictedBackingsExist() const {
  DCHECK(proxy_->IsImplThread() && proxy_->IsMainThreadBlocked());
  base::AutoLock scoped_lock(evicted_backings_lock_);
  for (BackingList::const_iterator it = evicted_backings_.begin();
       it != evicted_backings_.end();
       ++it) {
    if ((*it)->owner())
      return true;
  }
  return false;
}

void PrioritizedResourceManager::RegisterTexture(PrioritizedResource* texture) {
  DCHECK(proxy_->IsMainThread());
  DCHECK(texture);
  DCHECK(!texture->resource_manager());
  DCHECK(!texture->backing());
  DCHECK(!ContainsKey(textures_, texture));

  texture->set_manager_internal(this);
  textures_.insert(texture);
}

void PrioritizedResourceManager::UnregisterTexture(
    PrioritizedResource* texture) {
  DCHECK(proxy_->IsMainThread() ||
         (proxy_->IsImplThread() && proxy_->IsMainThreadBlocked()));
  DCHECK(texture);
  DCHECK(ContainsKey(textures_, texture));

  ReturnBackingTexture(texture);
  texture->set_manager_internal(NULL);
  textures_.erase(texture);
  texture->set_above_priority_cutoff(false);
}

void PrioritizedResourceManager::ReturnBackingTexture(
    PrioritizedResource* texture) {
  DCHECK(proxy_->IsMainThread() ||
         (proxy_->IsImplThread() && proxy_->IsMainThreadBlocked()));
  if (texture->backing())
    texture->Unlink();
}

PrioritizedResource::Backing* PrioritizedResourceManager::CreateBacking(
    const gfx::Size& size,
    ResourceFormat format,
    ResourceProvider* resource_provider) {
  DCHECK(proxy_->IsImplThread() && proxy_->IsMainThreadBlocked());
  DCHECK(resource_provider);
  ResourceProvider::ResourceId resource_id =
      resource_provider->CreateManagedResource(
          size,
          GL_TEXTURE_2D,
          GL_CLAMP_TO_EDGE,
          ResourceProvider::TextureUsageAny,
          format);
  PrioritizedResource::Backing* backing = new PrioritizedResource::Backing(
      resource_id, resource_provider, size, format);
  memory_use_bytes_ += backing->bytes();
  return backing;
}

void PrioritizedResourceManager::EvictFirstBackingResource(
    ResourceProvider* resource_provider) {
  DCHECK(proxy_->IsImplThread());
  DCHECK(resource_provider);
  DCHECK(!backings_.empty());
  PrioritizedResource::Backing* backing = backings_.front();

  // Note that we create a backing and its resource at the same time, but we
  // delete the backing structure and its resource in two steps. This is because
  // we can delete the resource while the main thread is running, but we cannot
  // unlink backings while the main thread is running.
  backing->DeleteResource(resource_provider);
  memory_use_bytes_ -= backing->bytes();
  backings_.pop_front();
  base::AutoLock scoped_lock(evicted_backings_lock_);
  evicted_backings_.push_back(backing);
}

void PrioritizedResourceManager::AssertInvariants() {
#if DCHECK_IS_ON
  DCHECK(proxy_->IsImplThread() && proxy_->IsMainThreadBlocked());

  // If we hit any of these asserts, there is a bug in this class. To see
  // where the bug is, call this function at the beginning and end of
  // every public function.

  // Backings/textures must be doubly-linked and only to other backings/textures
  // in this manager.
  for (BackingList::iterator it = backings_.begin(); it != backings_.end();
       ++it) {
    if ((*it)->owner()) {
      DCHECK(ContainsKey(textures_, (*it)->owner()));
      DCHECK((*it)->owner()->backing() == (*it));
    }
  }
  for (TextureSet::iterator it = textures_.begin(); it != textures_.end();
       ++it) {
    PrioritizedResource* texture = (*it);
    PrioritizedResource::Backing* backing = texture->backing();
    base::AutoLock scoped_lock(evicted_backings_lock_);
    if (backing) {
      if (backing->ResourceHasBeenDeleted()) {
        DCHECK(std::find(backings_.begin(), backings_.end(), backing) ==
               backings_.end());
        DCHECK(std::find(evicted_backings_.begin(),
                         evicted_backings_.end(),
                         backing) != evicted_backings_.end());
      } else {
        DCHECK(std::find(backings_.begin(), backings_.end(), backing) !=
               backings_.end());
        DCHECK(std::find(evicted_backings_.begin(),
                         evicted_backings_.end(),
                         backing) == evicted_backings_.end());
      }
      DCHECK(backing->owner() == texture);
    }
  }

  // At all times, backings that can be evicted must always come before
  // backings that can't be evicted in the backing texture list (otherwise
  // ReduceMemory will not find all textures available for eviction/recycling).
  bool reached_unrecyclable = false;
  PrioritizedResource::Backing* previous_backing = NULL;
  for (BackingList::iterator it = backings_.begin(); it != backings_.end();
       ++it) {
    PrioritizedResource::Backing* backing = *it;
    if (previous_backing &&
        (!backings_tail_not_sorted_ ||
         !backing->was_above_priority_cutoff_at_last_priority_update()))
      DCHECK(CompareBackings(previous_backing, backing));
    if (!backing->CanBeRecycledIfNotInExternalUse())
      reached_unrecyclable = true;
    if (reached_unrecyclable)
      DCHECK(!backing->CanBeRecycledIfNotInExternalUse());
    else
      DCHECK(backing->CanBeRecycledIfNotInExternalUse());
    previous_backing = backing;
  }
#endif  // DCHECK_IS_ON
}

const Proxy* PrioritizedResourceManager::ProxyForDebug() const {
  return proxy_;
}

}  // namespace cc
