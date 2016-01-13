// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PRIORITIZED_RESOURCE_MANAGER_H_
#define CC_RESOURCES_PRIORITIZED_RESOURCE_MANAGER_H_

#include <list>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "cc/base/cc_export.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/priority_calculator.h"
#include "cc/resources/resource.h"
#include "cc/trees/proxy.h"
#include "ui/gfx/size.h"

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template <> struct hash<cc::PrioritizedResource*> {
  size_t operator()(cc::PrioritizedResource* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
}  // namespace BASE_HASH_NAMESPACE
#endif  // COMPILER

namespace cc {

class PriorityCalculator;
class Proxy;

class CC_EXPORT PrioritizedResourceManager {
 public:
  static scoped_ptr<PrioritizedResourceManager> Create(const Proxy* proxy) {
    return make_scoped_ptr(new PrioritizedResourceManager(proxy));
  }
  scoped_ptr<PrioritizedResource> CreateTexture(
      const gfx::Size& size, ResourceFormat format) {
    return make_scoped_ptr(new PrioritizedResource(this, size, format));
  }
  ~PrioritizedResourceManager();

  typedef std::list<PrioritizedResource::Backing*> BackingList;

  // TODO(epenner): (http://crbug.com/137094) This 64MB default is a straggler
  // from the old texture manager and is just to give us a default memory
  // allocation before we get a callback from the GPU memory manager. We
  // should probaby either:
  // - wait for the callback before rendering anything instead
  // - push this into the GPU memory manager somehow.
  static size_t DefaultMemoryAllocationLimit() { return 64 * 1024 * 1024; }

  // MemoryUseBytes() describes the number of bytes used by existing allocated
  // textures.
  size_t MemoryUseBytes() const { return memory_use_bytes_; }
  // MemoryAboveCutoffBytes() describes the number of bytes that
  // would be used if all textures that are above the cutoff were allocated.
  // MemoryUseBytes() <= MemoryAboveCutoffBytes() should always be true.
  size_t MemoryAboveCutoffBytes() const { return memory_above_cutoff_bytes_; }
  // MaxMemoryNeededBytes() describes the number of bytes that would be used
  // by textures if there were no limit on memory usage.
  size_t MaxMemoryNeededBytes() const { return max_memory_needed_bytes_; }
  size_t MemoryForSelfManagedTextures() const {
    return max_memory_limit_bytes_ - memory_available_bytes_;
  }

  void SetMaxMemoryLimitBytes(size_t bytes) { max_memory_limit_bytes_ = bytes; }
  size_t MaxMemoryLimitBytes() const { return max_memory_limit_bytes_; }

  // Sepecify a external priority cutoff. Only textures that have a strictly
  // higher priority than this cutoff will be allowed.
  void SetExternalPriorityCutoff(int priority_cutoff) {
    external_priority_cutoff_ = priority_cutoff;
  }
  int ExternalPriorityCutoff() const {
    return external_priority_cutoff_;
  }

  // Return the amount of texture memory required at particular cutoffs.
  size_t MemoryVisibleBytes() const;
  size_t MemoryVisibleAndNearbyBytes() const;

  void PrioritizeTextures();
  void ClearPriorities();

  // Delete contents textures' backing resources until they use only
  // limit_bytes bytes. This may be called on the impl thread while the main
  // thread is running. Returns true if resources are indeed evicted as a
  // result of this call.
  bool ReduceMemoryOnImplThread(size_t limit_bytes,
                                int priority_cutoff,
                                ResourceProvider* resource_provider);

  // Returns true if there exist any textures that are linked to backings that
  // have had their resources evicted. Only when we commit a tree that has no
  // textures linked to evicted backings may we allow drawing. After an
  // eviction, this will not become true until unlinkAndClearEvictedBackings
  // is called.
  bool LinkedEvictedBackingsExist() const;

  // Unlink the list of contents textures' backings from their owning textures
  // and delete the evicted backings' structures. This is called just before
  // updating layers, and is only ever called on the main thread.
  void UnlinkAndClearEvictedBackings();

  bool RequestLate(PrioritizedResource* texture);

  void ReduceWastedMemory(ResourceProvider* resource_provider);
  void ReduceMemory(ResourceProvider* resource_provider);
  void ClearAllMemory(ResourceProvider* resource_provider);

  void AcquireBackingTextureIfNeeded(PrioritizedResource* texture,
                                     ResourceProvider* resource_provider);

  void RegisterTexture(PrioritizedResource* texture);
  void UnregisterTexture(PrioritizedResource* texture);
  void ReturnBackingTexture(PrioritizedResource* texture);

  // Update all backings' priorities from their owning texture.
  void PushTexturePrioritiesToBackings();

  // Mark all textures' backings as being in the drawing impl tree.
  void UpdateBackingsState(ResourceProvider* resource_provider);

  const Proxy* ProxyForDebug() const;

 private:
  friend class PrioritizedResourceTest;

  enum EvictionPolicy {
    EVICT_ONLY_RECYCLABLE,
    EVICT_ANYTHING,
  };
  enum UnlinkPolicy {
    DO_NOT_UNLINK_BACKINGS,
    UNLINK_BACKINGS,
  };

  // Compare textures. Highest priority first.
  static inline bool CompareTextures(PrioritizedResource* a,
                                     PrioritizedResource* b) {
    if (a->request_priority() == b->request_priority())
      return a < b;
    return PriorityCalculator::priority_is_higher(a->request_priority(),
                                                  b->request_priority());
  }
  // Compare backings. Lowest priority first.
  static inline bool CompareBackings(PrioritizedResource::Backing* a,
                                     PrioritizedResource::Backing* b) {
    // Make textures that can be recycled appear first.
    if (a->CanBeRecycledIfNotInExternalUse() !=
        b->CanBeRecycledIfNotInExternalUse())
      return (a->CanBeRecycledIfNotInExternalUse() >
              b->CanBeRecycledIfNotInExternalUse());
    // Then sort by being above or below the priority cutoff.
    if (a->was_above_priority_cutoff_at_last_priority_update() !=
        b->was_above_priority_cutoff_at_last_priority_update())
      return (a->was_above_priority_cutoff_at_last_priority_update() <
              b->was_above_priority_cutoff_at_last_priority_update());
    // Then sort by priority (note that backings that no longer have owners will
    // always have the lowest priority).
    if (a->request_priority_at_last_priority_update() !=
        b->request_priority_at_last_priority_update())
      return PriorityCalculator::priority_is_lower(
          a->request_priority_at_last_priority_update(),
          b->request_priority_at_last_priority_update());
    // Then sort by being in the impl tree versus being completely
    // unreferenced.
    if (a->in_drawing_impl_tree() != b->in_drawing_impl_tree())
      return (a->in_drawing_impl_tree() < b->in_drawing_impl_tree());
    // Finally, prefer to evict textures in the parent compositor because
    // they will otherwise take another roundtrip to the parent compositor
    // before they are evicted.
    if (a->in_parent_compositor() != b->in_parent_compositor())
      return (a->in_parent_compositor() > b->in_parent_compositor());
    return a < b;
  }

  explicit PrioritizedResourceManager(const Proxy* proxy);

  bool EvictBackingsToReduceMemory(size_t limit_bytes,
                                   int priority_cutoff,
                                   EvictionPolicy eviction_policy,
                                   UnlinkPolicy unlink_policy,
                                   ResourceProvider* resource_provider);
  PrioritizedResource::Backing* CreateBacking(
      const gfx::Size& size,
      ResourceFormat format,
      ResourceProvider* resource_provider);
  void EvictFirstBackingResource(ResourceProvider* resource_provider);
  void SortBackings();

  void AssertInvariants();

  size_t max_memory_limit_bytes_;
  // The priority cutoff based on memory pressure. This is not a strict
  // cutoff -- RequestLate allows textures with priority equal to this
  // cutoff to be allowed.
  int priority_cutoff_;
  // The priority cutoff based on external memory policy. This is a strict
  // cutoff -- no textures with priority equal to this cutoff will be allowed.
  int external_priority_cutoff_;
  size_t memory_use_bytes_;
  size_t memory_above_cutoff_bytes_;
  size_t max_memory_needed_bytes_;
  size_t memory_available_bytes_;

  typedef base::hash_set<PrioritizedResource*> TextureSet;
  typedef std::vector<PrioritizedResource*> TextureVector;

  const Proxy* proxy_;

  TextureSet textures_;
  // This list is always sorted in eviction order, with the exception the
  // newly-allocated or recycled textures at the very end of the tail that
  // are not sorted by priority.
  BackingList backings_;
  bool backings_tail_not_sorted_;

  // The list of backings that have been evicted, but may still be linked
  // to textures. This can be accessed concurrently by the main and impl
  // threads, and may only be accessed while holding evicted_backings_lock_.
  mutable base::Lock evicted_backings_lock_;
  BackingList evicted_backings_;

  TextureVector temp_texture_vector_;

  // Statistics about memory usage at priority cutoffs, computed at
  // PrioritizeTextures.
  size_t memory_visible_bytes_;
  size_t memory_visible_and_nearby_bytes_;

  // Statistics copied at the time of PushTexturePrioritiesToBackings.
  size_t memory_visible_last_pushed_bytes_;
  size_t memory_visible_and_nearby_last_pushed_bytes_;

  DISALLOW_COPY_AND_ASSIGN(PrioritizedResourceManager);
};

}  // namespace cc

#endif  // CC_RESOURCES_PRIORITIZED_RESOURCE_MANAGER_H_
