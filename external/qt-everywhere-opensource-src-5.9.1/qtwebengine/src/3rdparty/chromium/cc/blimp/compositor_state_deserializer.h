// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLIMP_COMPOSITOR_STATE_DESERIALIZER_H_
#define CC_BLIMP_COMPOSITOR_STATE_DESERIALIZER_H_

#include <unordered_map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/blimp/synced_property_remote.h"
#include "ui/gfx/geometry/scroll_offset.h"

namespace cc {
namespace proto {
class ClientStateUpdate;
class LayerNode;
class LayerProperties;
class LayerTree;
class LayerTreeHost;
}  // namespace proto

class ClientPictureCache;
class DeserializedContentLayerClient;
class Layer;
class LayerFactory;
class LayerTreeHostInProcess;

class CC_EXPORT CompositorStateDeserializerClient {
 public:
  virtual ~CompositorStateDeserializerClient() {}

  // Used to inform the client that the local state received from the engine was
  // modified on the impl thread and a ClientStateUpdate needs to be scheduled
  // to synchronize it with the main thread on the engine.
  virtual void DidUpdateLocalState() = 0;
};

// Deserializes the compositor updates into the LayerTreeHost.
class CC_EXPORT CompositorStateDeserializer {
 public:
  CompositorStateDeserializer(
      LayerTreeHostInProcess* layer_tree_host,
      std::unique_ptr<ClientPictureCache> client_picture_cache,
      CompositorStateDeserializerClient* client);
  ~CompositorStateDeserializer();

  // Returns the local layer on the client for the given |engine_layer_id|.
  Layer* GetLayerForEngineId(int engine_layer_id) const;

  // Deserializes the |layer_tree_host_proto| into the LayerTreeHost.
  void DeserializeCompositorUpdate(
      const proto::LayerTreeHost& layer_tree_host_proto);

  // Allows tests to inject the LayerFactory.
  void SetLayerFactoryForTesting(std::unique_ptr<LayerFactory> layer_factory);

  // Updates any viewport related deltas that have been reported to the main
  // thread from the impl thread.
  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta);

  // Pulls a state update for changes reported to the main thread, that need to
  // be synchronized with the associated state on the engine main thread.
  void PullClientStateUpdate(proto::ClientStateUpdate* client_state_update);

  // Informs that the client state update pulled was applied to the main thread
  // on the engine.
  // Note that this assumes that the state update provided to the engine was
  // reflected back by the engine. If the application of this update resulted in
  // any changes to the main thread state on the engine, these must be
  // de-serialized and applied to the LayerTreeHost before a frame is committed
  // to the impl thread.
  void DidApplyStateUpdatesOnEngine();

  // Sends any deltas that have been received on the main thread, but have not
  // yet been applied to the main thread state back to the impl thread. This
  // must be called for every main frame sent to the impl thread.
  void SendUnappliedDeltasToLayerTreeHost();

 private:
  using SyncedRemoteScrollOffset =
      SyncedPropertyRemote<AdditionGroup<gfx::ScrollOffset>>;

  // A holder for the Layer and any data tied to it.
  struct LayerData {
    LayerData();
    LayerData(LayerData&& other);
    ~LayerData();

    LayerData& operator=(LayerData&& other);

    scoped_refptr<Layer> layer;

    // Set only for PictureLayers.
    std::unique_ptr<DeserializedContentLayerClient> content_layer_client;

    SyncedRemoteScrollOffset synced_scroll_offset;

   private:
    DISALLOW_COPY_AND_ASSIGN(LayerData);
  };

  using EngineIdToLayerMap = std::unordered_map<int, LayerData>;
  // Map of the scrollbar layer id to the corresponding scroll layer id. Both
  // ids refer to the engine layer id.
  using ScrollbarLayerToScrollLayerId = std::unordered_map<int, int>;

  void SychronizeLayerTreeState(const proto::LayerTree& layer_tree_proto);
  void SynchronizeLayerState(
      const proto::LayerProperties& layer_properties_proto);
  void SynchronizeLayerHierarchyRecursive(
      Layer* layer,
      const proto::LayerNode& layer_node,
      EngineIdToLayerMap* new_layer_map,
      ScrollbarLayerToScrollLayerId* scrollbar_layer_to_scroll_layer);
  scoped_refptr<Layer> GetLayerAndAddToNewMap(
      const proto::LayerNode& layer_node,
      EngineIdToLayerMap* new_layer_map,
      ScrollbarLayerToScrollLayerId* scrollbar_layer_to_scroll_layer);

  void LayerScrolled(int engine_layer_id);

  scoped_refptr<Layer> GetLayer(int engine_layer_id) const;
  DeserializedContentLayerClient* GetContentLayerClient(
      int engine_layer_id) const;
  int GetClientIdFromEngineId(int engine_layer_id) const;
  LayerData* GetLayerData(int engine_layer_id);

  std::unique_ptr<LayerFactory> layer_factory_;

  LayerTreeHostInProcess* layer_tree_host_;
  std::unique_ptr<ClientPictureCache> client_picture_cache_;
  CompositorStateDeserializerClient* client_;

  EngineIdToLayerMap engine_id_to_layer_;
  SyncedPropertyRemote<ScaleGroup> synced_page_scale_;

  base::WeakPtrFactory<CompositorStateDeserializer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CompositorStateDeserializer);
};

}  // namespace cc

#endif  // CC_BLIMP_COMPOSITOR_STATE_DESERIALIZER_H_
