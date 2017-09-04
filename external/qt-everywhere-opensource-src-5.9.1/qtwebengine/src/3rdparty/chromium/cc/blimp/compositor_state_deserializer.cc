// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blimp/compositor_state_deserializer.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "cc/blimp/client_picture_cache.h"
#include "cc/blimp/deserialized_content_layer_client.h"
#include "cc/blimp/layer_factory.h"
#include "cc/blimp/picture_data_conversions.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/solid_color_scrollbar_layer.h"
#include "cc/proto/cc_conversions.h"
#include "cc/proto/client_state_update.pb.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/layer_tree_host.pb.h"
#include "cc/proto/skia_conversions.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_host_in_process.h"

namespace cc {
namespace {

class DefaultLayerFactory : public LayerFactory {
 public:
  DefaultLayerFactory() = default;
  ~DefaultLayerFactory() override = default;

  // LayerFactory implementation.
  scoped_refptr<Layer> CreateLayer(int engine_layer_id) override {
    return Layer::Create();
  }
  scoped_refptr<PictureLayer> CreatePictureLayer(
      int engine_layer_id,
      ContentLayerClient* content_layer_client) override {
    return PictureLayer::Create(content_layer_client);
  }
  scoped_refptr<SolidColorScrollbarLayer> CreateSolidColorScrollbarLayer(
      int engine_layer_id,
      ScrollbarOrientation orientation,
      int thumb_thickness,
      int track_start,
      bool is_left_side_vertical_scrollbar,
      int scroll_layer_id) override {
    return SolidColorScrollbarLayer::Create(
        orientation, thumb_thickness, track_start,
        is_left_side_vertical_scrollbar, scroll_layer_id);
  }
  scoped_refptr<PictureLayer> CreateFakePictureLayer(
      int engine_layer_id,
      ContentLayerClient* content_layer_client) override {
    // We should never create fake layers in production code.
    NOTREACHED();
    return PictureLayer::Create(content_layer_client);
  }
  scoped_refptr<Layer> CreatePushPropertiesCountingLayer(
      int engine_layer_id) override {
    // We should never create fake layers in production code.
    NOTREACHED();
    return Layer::Create();
  }
};

}  // namespace

CompositorStateDeserializer::LayerData::LayerData() = default;

CompositorStateDeserializer::LayerData::~LayerData() = default;

CompositorStateDeserializer::LayerData::LayerData(LayerData&& other) = default;

CompositorStateDeserializer::LayerData& CompositorStateDeserializer::LayerData::
operator=(LayerData&& other) = default;

CompositorStateDeserializer::CompositorStateDeserializer(
    LayerTreeHostInProcess* layer_tree_host,
    std::unique_ptr<ClientPictureCache> client_picture_cache,
    CompositorStateDeserializerClient* client)
    : layer_factory_(base::MakeUnique<DefaultLayerFactory>()),
      layer_tree_host_(layer_tree_host),
      client_picture_cache_(std::move(client_picture_cache)),
      client_(client),
      weak_factory_(this) {
  DCHECK(layer_tree_host_);
  DCHECK(client_);
}

CompositorStateDeserializer::~CompositorStateDeserializer() = default;

Layer* CompositorStateDeserializer::GetLayerForEngineId(
    int engine_layer_id) const {
  EngineIdToLayerMap::const_iterator layer_it =
      engine_id_to_layer_.find(engine_layer_id);
  return layer_it != engine_id_to_layer_.end() ? layer_it->second.layer.get()
                                               : nullptr;
}

void CompositorStateDeserializer::DeserializeCompositorUpdate(
    const proto::LayerTreeHost& layer_tree_host_proto) {
  SychronizeLayerTreeState(layer_tree_host_proto.layer_tree());

  // Ensure ClientPictureCache contains all the necessary SkPictures before
  // deserializing the properties.
  proto::SkPictures proto_pictures = layer_tree_host_proto.pictures();
  std::vector<PictureData> pictures =
      SkPicturesProtoToPictureDataVector(proto_pictures);
  client_picture_cache_->ApplyCacheUpdate(pictures);

  const proto::LayerUpdate& layer_updates =
      layer_tree_host_proto.layer_updates();
  for (int i = 0; i < layer_updates.layers_size(); ++i) {
    SynchronizeLayerState(layer_updates.layers(i));
  }

  // The deserialization is finished, so now clear the cache.
  client_picture_cache_->Flush();
}

void CompositorStateDeserializer::SetLayerFactoryForTesting(
    std::unique_ptr<LayerFactory> layer_factory) {
  layer_factory_ = std::move(layer_factory);
}

void CompositorStateDeserializer::ApplyViewportDeltas(
    const gfx::Vector2dF& inner_delta,
    const gfx::Vector2dF& outer_delta,
    const gfx::Vector2dF& elastic_overscroll_delta,
    float page_scale,
    float top_controls_delta) {
  DCHECK_EQ(top_controls_delta, 0.0f);
  DCHECK(elastic_overscroll_delta == gfx::Vector2dF());
  DCHECK(outer_delta == gfx::Vector2dF());

  // The inner_delta can be ignored here, since we receive that in the scroll
  // callback on the layer itself.
  if (page_scale != 1.0f) {
    LayerTree* layer_tree = layer_tree_host_->GetLayerTree();
    synced_page_scale_.UpdateDeltaFromImplThread(
        layer_tree->page_scale_factor());
    layer_tree->SetPageScaleFactorAndLimits(
        synced_page_scale_.EngineMain(), layer_tree->min_page_scale_factor(),
        layer_tree->max_page_scale_factor());
    client_->DidUpdateLocalState();
  }
}

void CompositorStateDeserializer::PullClientStateUpdate(
    proto::ClientStateUpdate* client_state_update) {
  for (auto& layer_it : engine_id_to_layer_) {
    int engine_layer_id = layer_it.first;
    auto& synced_scroll_offset = layer_it.second.synced_scroll_offset;
    gfx::ScrollOffset scroll_offset_delta =
        synced_scroll_offset.PullDeltaForEngineUpdate();
    gfx::Vector2dF scroll_delta_vector =
        gfx::ScrollOffsetToVector2dF(scroll_offset_delta);

    if (scroll_delta_vector.IsZero()) {
      continue;
    }

    proto::ScrollUpdate* scroll_update =
        client_state_update->add_scroll_updates();
    scroll_update->set_layer_id(engine_layer_id);
    Vector2dFToProto(scroll_delta_vector,
                     scroll_update->mutable_scroll_delta());
  }

  float page_scale_delta = synced_page_scale_.PullDeltaForEngineUpdate();
  if (page_scale_delta != 1.0f) {
    client_state_update->set_page_scale_delta(page_scale_delta);
  }
}

void CompositorStateDeserializer::DidApplyStateUpdatesOnEngine() {
  for (auto& layer_it : engine_id_to_layer_) {
    Layer* layer = layer_it.second.layer.get();
    auto& synced_scroll_offset = layer_it.second.synced_scroll_offset;

    synced_scroll_offset.DidApplySentDeltaOnEngine();
    layer->SetScrollOffset(synced_scroll_offset.EngineMain());
  }

  synced_page_scale_.DidApplySentDeltaOnEngine();
  LayerTree* layer_tree = layer_tree_host_->GetLayerTree();
  layer_tree->SetPageScaleFactorAndLimits(synced_page_scale_.EngineMain(),
                                          layer_tree->min_page_scale_factor(),
                                          layer_tree->max_page_scale_factor());
}

void CompositorStateDeserializer::SendUnappliedDeltasToLayerTreeHost() {
  std::unique_ptr<ReflectedMainFrameState> reflected_main_frame_state =
      base::MakeUnique<ReflectedMainFrameState>();

  for (auto& layer_it : engine_id_to_layer_) {
    Layer* layer = layer_it.second.layer.get();
    auto& synced_scroll_offset = layer_it.second.synced_scroll_offset;

    gfx::ScrollOffset scroll_offset_delta =
        synced_scroll_offset.DeltaNotAppliedOnEngine();
    gfx::Vector2dF scroll_delta_vector =
        gfx::ScrollOffsetToVector2dF(scroll_offset_delta);
    if (scroll_delta_vector.IsZero())
      continue;

    ReflectedMainFrameState::ScrollUpdate scroll_update;
    scroll_update.layer_id = layer->id();
    scroll_update.scroll_delta = scroll_delta_vector;
    reflected_main_frame_state->scrolls.push_back(scroll_update);
  }

  reflected_main_frame_state->page_scale_delta =
      synced_page_scale_.DeltaNotAppliedOnEngine();
  layer_tree_host_->SetReflectedMainFrameState(
      std::move(reflected_main_frame_state));
}

void CompositorStateDeserializer::SychronizeLayerTreeState(
    const proto::LayerTree& layer_tree_proto) {
  LayerTree* layer_tree = layer_tree_host_->GetLayerTree();

  // Synchronize the tree hierarchy first.
  // TODO(khushalsagar): Don't do this if the hierarchy didn't change. See
  // crbug.com/605170.
  EngineIdToLayerMap new_engine_id_to_layer;
  ScrollbarLayerToScrollLayerId scrollbar_layer_to_scroll_layer;
  if (layer_tree_proto.has_root_layer()) {
    const proto::LayerNode& root_layer_node = layer_tree_proto.root_layer();
    layer_tree->SetRootLayer(
        GetLayerAndAddToNewMap(root_layer_node, &new_engine_id_to_layer,
                               &scrollbar_layer_to_scroll_layer));
    SynchronizeLayerHierarchyRecursive(layer_tree->root_layer(),
                                       root_layer_node, &new_engine_id_to_layer,
                                       &scrollbar_layer_to_scroll_layer);
  } else {
    layer_tree->SetRootLayer(nullptr);
  }
  engine_id_to_layer_.swap(new_engine_id_to_layer);

  // Now that the tree has been synced, we can set up the scroll layers, since
  // the corresponding engine layers have been created.
  for (const auto& scrollbar : scrollbar_layer_to_scroll_layer) {
    // This corresponds to the id of the Scrollbar Layer.
    int scrollbar_layer_id = scrollbar.first;

    // This corresponds to the id of the scroll layer for this scrollbar.
    int scroll_layer_id = scrollbar.second;

    SolidColorScrollbarLayer* scrollbar_layer =
        static_cast<SolidColorScrollbarLayer*>(
            GetLayerForEngineId(scrollbar_layer_id));

    scrollbar_layer->SetScrollLayer(GetClientIdFromEngineId(scroll_layer_id));
  }

  // Synchronize rest of the tree state.
  layer_tree->RegisterViewportLayers(
      GetLayer(layer_tree_proto.overscroll_elasticity_layer_id()),
      GetLayer(layer_tree_proto.page_scale_layer_id()),
      GetLayer(layer_tree_proto.inner_viewport_scroll_layer_id()),
      GetLayer(layer_tree_proto.outer_viewport_scroll_layer_id()));

  layer_tree->SetDeviceScaleFactor(layer_tree_proto.device_scale_factor());
  layer_tree->SetPaintedDeviceScaleFactor(
      layer_tree_proto.painted_device_scale_factor());

  float min_page_scale_factor = layer_tree_proto.min_page_scale_factor();
  float max_page_scale_factor = layer_tree_proto.max_page_scale_factor();
  float page_scale_factor = layer_tree_proto.page_scale_factor();
  synced_page_scale_.PushFromEngineMainThread(page_scale_factor);
  layer_tree->SetPageScaleFactorAndLimits(synced_page_scale_.EngineMain(),
                                          min_page_scale_factor,
                                          max_page_scale_factor);

  layer_tree->set_background_color(layer_tree_proto.background_color());
  layer_tree->set_has_transparent_background(
      layer_tree_proto.has_transparent_background());

  LayerSelection selection;
  LayerSelectionFromProtobuf(&selection, layer_tree_proto.selection());
  layer_tree->RegisterSelection(selection);
  layer_tree->SetViewportSize(
      ProtoToSize(layer_tree_proto.device_viewport_size()));

  layer_tree->SetHaveScrollEventHandlers(
      layer_tree_proto.have_scroll_event_handlers());
  layer_tree->SetEventListenerProperties(
      EventListenerClass::kMouseWheel,
      static_cast<EventListenerProperties>(
          layer_tree_proto.wheel_event_listener_properties()));
  layer_tree->SetEventListenerProperties(
      EventListenerClass::kTouchStartOrMove,
      static_cast<EventListenerProperties>(
          layer_tree_proto.touch_start_or_move_event_listener_properties()));
  layer_tree->SetEventListenerProperties(
      EventListenerClass::kTouchEndOrCancel,
      static_cast<EventListenerProperties>(
          layer_tree_proto.touch_end_or_cancel_event_listener_properties()));
}

void CompositorStateDeserializer::SynchronizeLayerState(
    const proto::LayerProperties& layer_properties_proto) {
  int engine_layer_id = layer_properties_proto.id();
  Layer* layer = GetLayerForEngineId(engine_layer_id);

  // Layer Inputs -----------------------------------------------------
  const proto::BaseLayerProperties& base = layer_properties_proto.base();
  layer->SetNeedsDisplayRect(ProtoToRect(base.update_rect()));
  layer->SetBounds(ProtoToSize(base.bounds()));
  layer->SetMasksToBounds(base.masks_to_bounds());
  layer->SetOpacity(base.opacity());
  layer->SetBlendMode(SkXfermodeModeFromProto(base.blend_mode()));
  layer->SetIsRootForIsolatedGroup(base.is_root_for_isolated_group());
  layer->SetContentsOpaque(base.contents_opaque());
  layer->SetPosition(ProtoToPointF(base.position()));
  layer->SetTransform(ProtoToTransform(base.transform()));
  layer->SetTransformOrigin(ProtoToPoint3F(base.transform_origin()));
  layer->SetIsDrawable(base.is_drawable());
  layer->SetDoubleSided(base.double_sided());
  layer->SetShouldFlattenTransform(base.should_flatten_transform());
  layer->Set3dSortingContextId(base.sorting_context_id());
  layer->SetUseParentBackfaceVisibility(base.use_parent_backface_visibility());
  layer->SetBackgroundColor(base.background_color());

  gfx::ScrollOffset engine_scroll_offset =
      ProtoToScrollOffset(base.scroll_offset());
  SyncedRemoteScrollOffset& synced_scroll_offset =
      GetLayerData(engine_layer_id)->synced_scroll_offset;
  synced_scroll_offset.PushFromEngineMainThread(engine_scroll_offset);
  layer->SetScrollOffset(synced_scroll_offset.EngineMain());

  layer->SetScrollClipLayerId(
      GetClientIdFromEngineId(base.scroll_clip_layer_id()));
  layer->SetUserScrollable(base.user_scrollable_horizontal(),
                           base.user_scrollable_vertical());

  if (layer->main_thread_scrolling_reasons()) {
    layer->ClearMainThreadScrollingReasons(
        layer->main_thread_scrolling_reasons());
  }
  if (base.main_thread_scrolling_reasons()) {
    layer->AddMainThreadScrollingReasons(base.main_thread_scrolling_reasons());
  }

  layer->SetNonFastScrollableRegion(
      RegionFromProto(base.non_fast_scrollable_region()));
  layer->SetTouchEventHandlerRegion(
      RegionFromProto(base.touch_event_handler_region()));

  layer->SetIsContainerForFixedPositionLayers(
      base.is_container_for_fixed_position_layers());

  LayerPositionConstraint position_constraint;
  position_constraint.FromProtobuf(base.position_constraint());
  layer->SetPositionConstraint(position_constraint);

  LayerStickyPositionConstraint sticky_position_constraint;
  sticky_position_constraint.FromProtobuf(base.sticky_position_constraint());
  layer->SetStickyPositionConstraint(sticky_position_constraint);
  layer->SetScrollParent(GetLayerForEngineId(base.scroll_parent_id()));
  layer->SetClipParent(GetLayerForEngineId(base.clip_parent_id()));

  layer->SetHasWillChangeTransformHint(base.has_will_change_transform_hint());
  layer->SetHideLayerAndSubtree(base.hide_layer_and_subtree());

  // ------------------------------------------------------------------

  // PictureLayer Properties deserialization.
  if (layer_properties_proto.has_picture()) {
    const proto::PictureLayerProperties& picture_properties =
        layer_properties_proto.picture();

    // Only PictureLayers set picture.
    PictureLayer* picture_layer =
        static_cast<PictureLayer*>(GetLayerForEngineId(engine_layer_id));
    picture_layer->SetNearestNeighbor(picture_properties.nearest_neighbor());

    gfx::Rect recorded_viewport =
        ProtoToRect(picture_properties.recorded_viewport());
    scoped_refptr<DisplayItemList> display_list;
    std::vector<uint32_t> used_engine_picture_ids;
    if (picture_properties.has_display_list()) {
      display_list = DisplayItemList::CreateFromProto(
          picture_properties.display_list(), client_picture_cache_.get(),
          &used_engine_picture_ids);
    } else {
      display_list = nullptr;
    }

    // TODO(khushalsagar): The caching here is sub-optimal. If a layer does not
    // PushProperties, its pictures won't get counted here even if the layer
    // is drawn in the current frame.
    for (uint32_t engine_picture_id : used_engine_picture_ids)
      client_picture_cache_->MarkUsed(engine_picture_id);

    GetContentLayerClient(engine_layer_id)
        ->UpdateDisplayListAndRecordedViewport(display_list, recorded_viewport);
  }
}

void CompositorStateDeserializer::SynchronizeLayerHierarchyRecursive(
    Layer* layer,
    const proto::LayerNode& layer_node,
    EngineIdToLayerMap* new_layer_map,
    ScrollbarLayerToScrollLayerId* scrollbar_layer_to_scroll_layer) {
  layer->RemoveAllChildren();

  // Children.
  for (int i = 0; i < layer_node.children_size(); i++) {
    const proto::LayerNode& child_layer_node = layer_node.children(i);
    scoped_refptr<Layer> child_layer = GetLayerAndAddToNewMap(
        child_layer_node, new_layer_map, scrollbar_layer_to_scroll_layer);
    layer->AddChild(child_layer);
    SynchronizeLayerHierarchyRecursive(child_layer.get(), child_layer_node,
                                       new_layer_map,
                                       scrollbar_layer_to_scroll_layer);
  }

  // Mask Layer.
  if (layer_node.has_mask_layer()) {
    const proto::LayerNode& mask_layer_node = layer_node.mask_layer();
    scoped_refptr<Layer> mask_layer = GetLayerAndAddToNewMap(
        mask_layer_node, new_layer_map, scrollbar_layer_to_scroll_layer);
    layer->SetMaskLayer(mask_layer.get());
    SynchronizeLayerHierarchyRecursive(mask_layer.get(), mask_layer_node,
                                       new_layer_map,
                                       scrollbar_layer_to_scroll_layer);
  } else {
    layer->SetMaskLayer(nullptr);
  }

  // Scroll callback.
  layer->set_did_scroll_callback(
      base::Bind(&CompositorStateDeserializer::LayerScrolled,
                 weak_factory_.GetWeakPtr(), layer_node.id()));
}

scoped_refptr<Layer> CompositorStateDeserializer::GetLayerAndAddToNewMap(
    const proto::LayerNode& layer_node,
    EngineIdToLayerMap* new_layer_map,
    ScrollbarLayerToScrollLayerId* scrollbar_layer_to_scroll_layer) {
  DCHECK(new_layer_map->find(layer_node.id()) == new_layer_map->end())
      << "A LayerNode should have been de-serialized only once";

  scoped_refptr<Layer> layer;
  EngineIdToLayerMap::iterator layer_map_it =
      engine_id_to_layer_.find(layer_node.id());

  if (layer_map_it != engine_id_to_layer_.end()) {
    // We can re-use the old layer.
    layer = layer_map_it->second.layer;
    (*new_layer_map)[layer_node.id()] = std::move(layer_map_it->second);
    engine_id_to_layer_.erase(layer_map_it);
    return layer;
  }

  // We need to create a new layer.
  auto& layer_data = (*new_layer_map)[layer_node.id()];
  switch (layer_node.type()) {
    case proto::LayerNode::UNKNOWN:
      NOTREACHED() << "Unknown Layer type";
    case proto::LayerNode::LAYER:
      layer_data.layer = layer_factory_->CreateLayer(layer_node.id());
      break;
    case proto::LayerNode::PICTURE_LAYER:
      layer_data.content_layer_client =
          base::MakeUnique<DeserializedContentLayerClient>();
      layer_data.layer = layer_factory_->CreatePictureLayer(
          layer_node.id(), layer_data.content_layer_client.get());
      break;
    case proto::LayerNode::FAKE_PICTURE_LAYER:
      // FAKE_PICTURE_LAYER is for testing only.
      layer_data.content_layer_client =
          base::MakeUnique<DeserializedContentLayerClient>();
      layer_data.layer = layer_factory_->CreateFakePictureLayer(
          layer_node.id(), layer_data.content_layer_client.get());
      break;
    case proto::LayerNode::SOLID_COLOR_SCROLLBAR_LAYER: {
      // SolidColorScrollbarLayers attach their properties in the LayerNode
      // itself.
      const proto::SolidColorScrollbarLayerProperties& scrollbar =
          layer_node.solid_scrollbar();

      DCHECK(scrollbar_layer_to_scroll_layer->find(layer_node.id()) ==
             scrollbar_layer_to_scroll_layer->end());
      int scroll_layer_id = scrollbar.scroll_layer_id();
      (*scrollbar_layer_to_scroll_layer)[layer_node.id()] = scroll_layer_id;

      int thumb_thickness = scrollbar.thumb_thickness();
      int track_start = scrollbar.track_start();
      bool is_left_side_vertical_scrollbar =
          scrollbar.is_left_side_vertical_scrollbar();
      ScrollbarOrientation orientation =
          ScrollbarOrientationFromProto(scrollbar.orientation());

      // We use the invalid id for the |scroll_layer_id| because the
      // corresponding layer on the client may not have been created yet.
      layer_data.layer = layer_factory_->CreateSolidColorScrollbarLayer(
          layer_node.id(), orientation, thumb_thickness, track_start,
          is_left_side_vertical_scrollbar, Layer::LayerIdLabels::INVALID_ID);
    } break;
    case proto::LayerNode::PUSH_PROPERTIES_COUNTING_LAYER:
      // PUSH_PROPERTIES_COUNTING_LAYER is for testing only.
      layer_data.layer =
          layer_factory_->CreatePushPropertiesCountingLayer(layer_node.id());
      break;
  }

  layer = layer_data.layer;
  return layer;
}

void CompositorStateDeserializer::LayerScrolled(int engine_layer_id) {
  LayerData* layer_data = GetLayerData(engine_layer_id);
  Layer* layer = layer_data->layer.get();
  SyncedRemoteScrollOffset& synced_scroll_offset =
      layer_data->synced_scroll_offset;
  synced_scroll_offset.UpdateDeltaFromImplThread(layer->scroll_offset());
  layer->SetScrollOffset(synced_scroll_offset.EngineMain());
  client_->DidUpdateLocalState();
}

int CompositorStateDeserializer::GetClientIdFromEngineId(
    int engine_layer_id) const {
  Layer* layer = GetLayerForEngineId(engine_layer_id);
  return layer ? layer->id() : Layer::LayerIdLabels::INVALID_ID;
}

scoped_refptr<Layer> CompositorStateDeserializer::GetLayer(
    int engine_layer_id) const {
  EngineIdToLayerMap::const_iterator layer_it =
      engine_id_to_layer_.find(engine_layer_id);
  return layer_it != engine_id_to_layer_.end() ? layer_it->second.layer
                                               : nullptr;
}

DeserializedContentLayerClient*
CompositorStateDeserializer::GetContentLayerClient(int engine_layer_id) const {
  EngineIdToLayerMap::const_iterator layer_it =
      engine_id_to_layer_.find(engine_layer_id);
  return layer_it != engine_id_to_layer_.end()
             ? layer_it->second.content_layer_client.get()
             : nullptr;
}

CompositorStateDeserializer::LayerData*
CompositorStateDeserializer::GetLayerData(int engine_layer_id) {
  EngineIdToLayerMap::iterator layer_it =
      engine_id_to_layer_.find(engine_layer_id);
  return layer_it != engine_id_to_layer_.end() ? &layer_it->second : nullptr;
}

}  // namespace cc
