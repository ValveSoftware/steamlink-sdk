// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/logging.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id_allocator.h"

namespace cc {

SurfaceManager::FrameSinkSourceMapping::FrameSinkSourceMapping()
    : client(nullptr), source(nullptr) {}

SurfaceManager::FrameSinkSourceMapping::FrameSinkSourceMapping(
    const FrameSinkSourceMapping& other) = default;

SurfaceManager::FrameSinkSourceMapping::~FrameSinkSourceMapping() {
  DCHECK(is_empty()) << "client: " << client
                     << ", children: " << children.size();
}

SurfaceManager::SurfaceManager()
    : kRootSurfaceId(FrameSinkId(0u, 0u),
                     LocalFrameId(0u, base::UnguessableToken::Create())) {
  thread_checker_.DetachFromThread();
}

SurfaceManager::~SurfaceManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (SurfaceDestroyList::iterator it = surfaces_to_destroy_.begin();
       it != surfaces_to_destroy_.end();
       ++it) {
    DeregisterSurface((*it)->surface_id());
  }
  surfaces_to_destroy_.clear();

  // All hierarchies, sources, and surface factory clients should be
  // unregistered prior to SurfaceManager destruction.
  DCHECK_EQ(frame_sink_source_map_.size(), 0u);
  DCHECK_EQ(registered_sources_.size(), 0u);
}

void SurfaceManager::RegisterSurface(Surface* surface) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(surface);
  DCHECK(!surface_map_.count(surface->surface_id()));
  surface_map_[surface->surface_id()] = surface;
}

void SurfaceManager::DeregisterSurface(const SurfaceId& surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  DCHECK(it != surface_map_.end());
  surface_map_.erase(it);
  child_to_parent_refs_.erase(surface_id);
  parent_to_child_refs_.erase(surface_id);
}

void SurfaceManager::Destroy(std::unique_ptr<Surface> surface) {
  DCHECK(thread_checker_.CalledOnValidThread());
  surface->set_destroyed(true);
  surfaces_to_destroy_.push_back(std::move(surface));
  GarbageCollectSurfaces();
}

void SurfaceManager::DidSatisfySequences(const FrameSinkId& frame_sink_id,
                                         std::vector<uint32_t>* sequence) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (uint32_t value : *sequence)
    satisfied_sequences_.insert(SurfaceSequence(frame_sink_id, value));
  sequence->clear();
  GarbageCollectSurfaces();
}

void SurfaceManager::RegisterFrameSinkId(const FrameSinkId& frame_sink_id) {
  bool inserted = valid_frame_sink_ids_.insert(frame_sink_id).second;
  DCHECK(inserted);
}

void SurfaceManager::InvalidateFrameSinkId(const FrameSinkId& frame_sink_id) {
  valid_frame_sink_ids_.erase(frame_sink_id);
  GarbageCollectSurfaces();
}

void SurfaceManager::AddSurfaceReference(const SurfaceId& parent_id,
                                         const SurfaceId& child_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Check some conditions that should never happen. We don't want to crash on
  // bad input from a compromised client so just return early.
  if (parent_id == child_id) {
    LOG(ERROR) << "Cannot add self reference for " << parent_id.ToString();
    return;
  }
  if (parent_id != kRootSurfaceId && surface_map_.count(parent_id) == 0) {
    LOG(ERROR) << "No surface in map for " << parent_id.ToString();
    return;
  }
  if (surface_map_.count(child_id) == 0) {
    LOG(ERROR) << "No surface in map for " << child_id.ToString();
    return;
  }

  parent_to_child_refs_[parent_id].insert(child_id);
  child_to_parent_refs_[child_id].insert(parent_id);
}

void SurfaceManager::RemoveSurfaceReference(const SurfaceId& parent_id,
                                            const SurfaceId& child_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Check if we have the reference that is requested to be removed. We don't
  // want to crash on bad input from a compromised client so just return early.
  if (parent_to_child_refs_.count(parent_id) == 0 ||
      parent_to_child_refs_[parent_id].count(child_id) == 0) {
    LOG(ERROR) << "No reference from " << parent_id.ToString() << " to "
               << child_id.ToString();
    return;
  }

  RemoveSurfaceReferenceImpl(parent_id, child_id);
  GarbageCollectSurfaces();
}

size_t SurfaceManager::GetSurfaceReferenceCount(
    const SurfaceId& surface_id) const {
  auto iter = child_to_parent_refs_.find(surface_id);
  if (iter == child_to_parent_refs_.end())
    return 0;
  return iter->second.size();
}

size_t SurfaceManager::GetReferencedSurfaceCount(
    const SurfaceId& surface_id) const {
  auto iter = parent_to_child_refs_.find(surface_id);
  if (iter == parent_to_child_refs_.end())
    return 0;
  return iter->second.size();
}

void SurfaceManager::GarbageCollectSurfaces() {
  // Simple mark and sweep GC.
  // TODO(jbauman): Reduce the amount of work when nothing needs to be
  // destroyed.
  std::vector<SurfaceId> live_surfaces;
  std::unordered_set<SurfaceId, SurfaceIdHash> live_surfaces_set;

  // GC roots are surfaces that have not been destroyed, or have not had all
  // their destruction dependencies satisfied.
  for (auto& map_entry : surface_map_) {
    const SurfaceId& surface_id = map_entry.first;
    Surface* surface = map_entry.second;
    surface->SatisfyDestructionDependencies(&satisfied_sequences_,
                                            &valid_frame_sink_ids_);

    // Never use both sequences and references for the same Surface.
    DCHECK(surface->GetDestructionDependencyCount() == 0 ||
           GetSurfaceReferenceCount(surface_id) == 0);

    if (!surface->destroyed() || surface->GetDestructionDependencyCount() > 0 ||
        GetSurfaceReferenceCount(surface_id) > 0) {
      live_surfaces_set.insert(surface_id);
      live_surfaces.push_back(surface_id);
    }
  }

  // Mark all surfaces reachable from live surfaces by adding them to
  // live_surfaces and live_surfaces_set.
  for (size_t i = 0; i < live_surfaces.size(); i++) {
    Surface* surf = surface_map_[live_surfaces[i]];
    DCHECK(surf);

    for (const SurfaceId& id : surf->referenced_surfaces()) {
      if (live_surfaces_set.count(id))
        continue;

      Surface* surf2 = GetSurfaceForId(id);
      if (surf2) {
        live_surfaces.push_back(id);
        live_surfaces_set.insert(id);
      }
    }
  }

  std::vector<std::unique_ptr<Surface>> to_destroy;

  // Destroy all remaining unreachable surfaces.
  for (auto iter = surfaces_to_destroy_.begin();
       iter != surfaces_to_destroy_.end();) {
    SurfaceId surface_id = (*iter)->surface_id();
    if (!live_surfaces_set.count(surface_id)) {
      DeregisterSurface(surface_id);
      to_destroy.push_back(std::move(*iter));
      iter = surfaces_to_destroy_.erase(iter);
    } else {
      ++iter;
    }
  }

  to_destroy.clear();
}

void SurfaceManager::RemoveSurfaceReferenceImpl(const SurfaceId& parent_id,
                                                const SurfaceId& child_id) {
  // Remove the reference from parent to child. This doesn't change anything
  // about the validity of the parent.
  parent_to_child_refs_[parent_id].erase(child_id);

  // Remove the reference from child to parent. This might drop the number of
  // references to the child to zero.
  DCHECK_EQ(child_to_parent_refs_.count(child_id), 1u);
  SurfaceIdSet& child_refs = child_to_parent_refs_[child_id];
  DCHECK_EQ(child_refs.count(parent_id), 1u);
  child_refs.erase(parent_id);

  if (!child_refs.empty())
    return;

  // Remove any references the child holds before it gets garbage collected.
  // Copy SurfaceIdSet to avoid iterator invalidation problems.
  SurfaceIdSet child_child_refs = parent_to_child_refs_[child_id];
  for (auto& child_child_id : child_child_refs)
    RemoveSurfaceReferenceImpl(child_id, child_child_id);
  DCHECK_EQ(GetReferencedSurfaceCount(child_id), 0u);
}

void SurfaceManager::RegisterSurfaceFactoryClient(
    const FrameSinkId& frame_sink_id,
    SurfaceFactoryClient* client) {
  DCHECK(client);
  DCHECK_EQ(valid_frame_sink_ids_.count(frame_sink_id), 1u);

  // Will create a new FrameSinkSourceMapping for |frame_sink_id| if necessary.
  FrameSinkSourceMapping& frame_sink_source =
      frame_sink_source_map_[frame_sink_id];
  DCHECK(!frame_sink_source.client);
  frame_sink_source.client = client;

  // Propagate any previously set sources to the new client.
  if (frame_sink_source.source)
    client->SetBeginFrameSource(frame_sink_source.source);
}

void SurfaceManager::UnregisterSurfaceFactoryClient(
    const FrameSinkId& frame_sink_id) {
  DCHECK_EQ(valid_frame_sink_ids_.count(frame_sink_id), 1u);
  DCHECK_EQ(frame_sink_source_map_.count(frame_sink_id), 1u);

  auto iter = frame_sink_source_map_.find(frame_sink_id);
  if (iter->second.source)
    iter->second.client->SetBeginFrameSource(nullptr);
  iter->second.client = nullptr;

  // The SurfaceFactoryClient and hierarchy can be registered/unregistered
  // in either order, so empty namespace_client_map entries need to be
  // checked when removing either clients or relationships.
  if (iter->second.is_empty())
    frame_sink_source_map_.erase(iter);
}

void SurfaceManager::RegisterBeginFrameSource(
    BeginFrameSource* source,
    const FrameSinkId& frame_sink_id) {
  DCHECK(source);
  DCHECK_EQ(registered_sources_.count(source), 0u);
  DCHECK_EQ(valid_frame_sink_ids_.count(frame_sink_id), 1u);

  registered_sources_[source] = frame_sink_id;
  RecursivelyAttachBeginFrameSource(frame_sink_id, source);
}

void SurfaceManager::UnregisterBeginFrameSource(BeginFrameSource* source) {
  DCHECK(source);
  DCHECK_EQ(registered_sources_.count(source), 1u);

  FrameSinkId frame_sink_id = registered_sources_[source];
  registered_sources_.erase(source);

  if (frame_sink_source_map_.count(frame_sink_id) == 0u)
    return;

  // TODO(enne): these walks could be done in one step.
  // Remove this begin frame source from its subtree.
  RecursivelyDetachBeginFrameSource(frame_sink_id, source);
  // Then flush every remaining registered source to fix any sources that
  // became null because of the previous step but that have an alternative.
  for (auto source_iter : registered_sources_)
    RecursivelyAttachBeginFrameSource(source_iter.second, source_iter.first);
}

void SurfaceManager::RecursivelyAttachBeginFrameSource(
    const FrameSinkId& frame_sink_id,
    BeginFrameSource* source) {
  FrameSinkSourceMapping& mapping = frame_sink_source_map_[frame_sink_id];
  if (!mapping.source) {
    mapping.source = source;
    if (mapping.client)
      mapping.client->SetBeginFrameSource(source);
  }
  for (size_t i = 0; i < mapping.children.size(); ++i)
    RecursivelyAttachBeginFrameSource(mapping.children[i], source);
}

void SurfaceManager::RecursivelyDetachBeginFrameSource(
    const FrameSinkId& frame_sink_id,
    BeginFrameSource* source) {
  auto iter = frame_sink_source_map_.find(frame_sink_id);
  if (iter == frame_sink_source_map_.end())
    return;
  if (iter->second.source == source) {
    iter->second.source = nullptr;
    if (iter->second.client)
      iter->second.client->SetBeginFrameSource(nullptr);
  }

  if (iter->second.is_empty()) {
    frame_sink_source_map_.erase(iter);
    return;
  }

  std::vector<FrameSinkId>& children = iter->second.children;
  for (size_t i = 0; i < children.size(); ++i) {
    RecursivelyDetachBeginFrameSource(children[i], source);
  }
}

bool SurfaceManager::ChildContains(
    const FrameSinkId& child_frame_sink_id,
    const FrameSinkId& search_frame_sink_id) const {
  auto iter = frame_sink_source_map_.find(child_frame_sink_id);
  if (iter == frame_sink_source_map_.end())
    return false;

  const std::vector<FrameSinkId>& children = iter->second.children;
  for (size_t i = 0; i < children.size(); ++i) {
    if (children[i] == search_frame_sink_id)
      return true;
    if (ChildContains(children[i], search_frame_sink_id))
      return true;
  }
  return false;
}

void SurfaceManager::RegisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  DCHECK_EQ(valid_frame_sink_ids_.count(parent_frame_sink_id), 1u);
  DCHECK_EQ(valid_frame_sink_ids_.count(child_frame_sink_id), 1u);

  // If it's possible to reach the parent through the child's descendant chain,
  // then this will create an infinite loop.  Might as well just crash here.
  CHECK(!ChildContains(child_frame_sink_id, parent_frame_sink_id));

  std::vector<FrameSinkId>& children =
      frame_sink_source_map_[parent_frame_sink_id].children;
  for (size_t i = 0; i < children.size(); ++i)
    DCHECK(children[i] != child_frame_sink_id);
  children.push_back(child_frame_sink_id);

  // If the parent has no source, then attaching it to this child will
  // not change any downstream sources.
  BeginFrameSource* parent_source =
      frame_sink_source_map_[parent_frame_sink_id].source;
  if (!parent_source)
    return;

  DCHECK_EQ(registered_sources_.count(parent_source), 1u);
  RecursivelyAttachBeginFrameSource(child_frame_sink_id, parent_source);
}

void SurfaceManager::UnregisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  // Deliberately do not check validity of either parent or child namespace
  // here.  They were valid during the registration, so were valid at some
  // point in time.  This makes it possible to invalidate parent and child
  // namespaces independently of each other and not have an ordering dependency
  // of unregistering the hierarchy first before either of them.
  DCHECK_EQ(frame_sink_source_map_.count(parent_frame_sink_id), 1u);

  auto iter = frame_sink_source_map_.find(parent_frame_sink_id);

  std::vector<FrameSinkId>& children = iter->second.children;
  bool found_child = false;
  for (size_t i = 0; i < children.size(); ++i) {
    if (children[i] == child_frame_sink_id) {
      found_child = true;
      children[i] = children.back();
      children.resize(children.size() - 1);
      break;
    }
  }
  DCHECK(found_child);

  // The SurfaceFactoryClient and hierarchy can be registered/unregistered
  // in either order, so empty namespace_client_map entries need to be
  // checked when removing either clients or relationships.
  if (iter->second.is_empty()) {
    frame_sink_source_map_.erase(iter);
    return;
  }

  // If the parent does not have a begin frame source, then disconnecting it
  // will not change any of its children.
  BeginFrameSource* parent_source = iter->second.source;
  if (!parent_source)
    return;

  // TODO(enne): these walks could be done in one step.
  RecursivelyDetachBeginFrameSource(child_frame_sink_id, parent_source);
  for (auto source_iter : registered_sources_)
    RecursivelyAttachBeginFrameSource(source_iter.second, source_iter.first);
}

Surface* SurfaceManager::GetSurfaceForId(const SurfaceId& surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  if (it == surface_map_.end())
    return nullptr;
  return it->second;
}

bool SurfaceManager::SurfaceModified(const SurfaceId& surface_id) {
  CHECK(thread_checker_.CalledOnValidThread());
  bool changed = false;
  for (auto& observer : observer_list_)
    observer.OnSurfaceDamaged(surface_id, &changed);
  return changed;
}

void SurfaceManager::SurfaceCreated(const SurfaceId& surface_id,
                                    const gfx::Size& frame_size,
                                    float device_scale_factor) {
  CHECK(thread_checker_.CalledOnValidThread());
  for (auto& observer : observer_list_)
    observer.OnSurfaceCreated(surface_id, frame_size, device_scale_factor);
}

}  // namespace cc
