// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_MANAGER_H_
#define CC_SURFACES_SURFACE_MANAGER_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "cc/surfaces/surface_damage_observer.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_sequence.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {
class BeginFrameSource;
class CompositorFrame;
class Surface;
class SurfaceFactoryClient;

class CC_SURFACES_EXPORT SurfaceManager {
 public:
  SurfaceManager();
  ~SurfaceManager();

  void RegisterSurface(Surface* surface);
  void DeregisterSurface(SurfaceId surface_id);

  // Destroy the Surface once a set of sequence numbers has been satisfied.
  void Destroy(std::unique_ptr<Surface> surface);

  Surface* GetSurfaceForId(SurfaceId surface_id);

  void AddObserver(SurfaceDamageObserver* obs) {
    observer_list_.AddObserver(obs);
  }

  void RemoveObserver(SurfaceDamageObserver* obs) {
    observer_list_.RemoveObserver(obs);
  }

  bool SurfaceModified(SurfaceId surface_id);

  // A frame for a surface satisfies a set of sequence numbers in a particular
  // id namespace.
  void DidSatisfySequences(uint32_t id_namespace,
                           std::vector<uint32_t>* sequence);

  void RegisterSurfaceIdNamespace(uint32_t id_namespace);

  // Invalidate a namespace that might still have associated sequences,
  // possibly because a renderer process has crashed.
  void InvalidateSurfaceIdNamespace(uint32_t id_namespace);

  // SurfaceFactoryClient, hierarchy, and BeginFrameSource can be registered
  // and unregistered in any order with respect to each other.
  //
  // This happens in practice, e.g. the relationship to between ui::Compositor /
  // DelegatedFrameHost is known before ui::Compositor has a surface/client).
  // However, DelegatedFrameHost can register itself as a client before its
  // relationship with the ui::Compositor is known.

  // Associates a SurfaceFactoryClient with the surface id namespace it uses.
  // SurfaceFactoryClient and surface namespaces/allocators have a 1:1 mapping.
  // Caller guarantees the client is alive between register/unregister.
  // Reregistering the same namespace when a previous client is active is not
  // valid.
  void RegisterSurfaceFactoryClient(uint32_t id_namespace,
                                    SurfaceFactoryClient* client);
  void UnregisterSurfaceFactoryClient(uint32_t id_namespace);

  // Associates a |source| with a particular namespace.  That namespace and
  // any children of that namespace with valid clients can potentially use
  // that |source|.
  void RegisterBeginFrameSource(BeginFrameSource* source,
                                uint32_t id_namespace);
  void UnregisterBeginFrameSource(BeginFrameSource* source);

  // Register a relationship between two namespaces.  This relationship means
  // that surfaces from the child namespace will be displayed in the parent.
  // Children are allowed to use any begin frame source that their parent can
  // use.
  void RegisterSurfaceNamespaceHierarchy(uint32_t parent_namespace,
                                         uint32_t child_namespace);
  void UnregisterSurfaceNamespaceHierarchy(uint32_t parent_namespace,
                                           uint32_t child_namespace);

 private:
  void RecursivelyAttachBeginFrameSource(uint32_t id_namespace,
                                         BeginFrameSource* source);
  void RecursivelyDetachBeginFrameSource(uint32_t id_namespace,
                                         BeginFrameSource* source);
  // Returns true if |child namespace| is or has |search_namespace| as a child.
  bool ChildContains(uint32_t child_namespace, uint32_t search_namespace) const;

  void GarbageCollectSurfaces();

  using SurfaceMap = std::unordered_map<SurfaceId, Surface*, SurfaceIdHash>;
  SurfaceMap surface_map_;
  base::ObserverList<SurfaceDamageObserver> observer_list_;
  base::ThreadChecker thread_checker_;

  // List of surfaces to be destroyed, along with what sequences they're still
  // waiting on.
  using SurfaceDestroyList = std::list<std::unique_ptr<Surface>>;
  SurfaceDestroyList surfaces_to_destroy_;

  // Set of SurfaceSequences that have been satisfied by a frame but not yet
  // waited on.
  std::unordered_set<SurfaceSequence, SurfaceSequenceHash> satisfied_sequences_;

  // Set of valid surface ID namespaces. When a namespace is removed from
  // this set, any remaining sequences with that namespace are considered
  // satisfied.
  std::unordered_set<uint32_t> valid_surface_id_namespaces_;

  // Begin frame source routing. Both BeginFrameSource and SurfaceFactoryClient
  // pointers guaranteed alive by callers until unregistered.
  struct ClientSourceMapping {
    ClientSourceMapping();
    ClientSourceMapping(const ClientSourceMapping& other);
    ~ClientSourceMapping();
    bool is_empty() const { return !client && !children.size(); }
    // The client that's responsible for creating this namespace.  Never null.
    SurfaceFactoryClient* client;
    // The currently assigned begin frame source for this client.
    BeginFrameSource* source;
    // This represents a dag of parent -> children mapping.
    std::vector<uint32_t> children;
  };
  std::unordered_map<uint32_t, ClientSourceMapping> namespace_client_map_;
  // Set of which sources are registered to which namespace.  Any child
  // that is implicitly using this namespace must be reachable by the
  // parent in the dag.
  std::unordered_map<BeginFrameSource*, uint32_t> registered_sources_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceManager);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_MANAGER_H_
