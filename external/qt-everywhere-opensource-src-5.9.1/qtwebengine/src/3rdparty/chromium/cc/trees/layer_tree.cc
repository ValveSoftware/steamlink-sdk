// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree.h"

#include "base/auto_reset.h"
#include "base/time/time.h"
#include "cc/input/page_scale_animation.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/layer_tree.pb.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/property_tree_builder.h"

namespace cc {

LayerTree::Inputs::Inputs()
    : top_controls_height(0.f),
      top_controls_shown_ratio(0.f),
      browser_controls_shrink_blink_size(false),
      bottom_controls_height(0.f),
      device_scale_factor(1.f),
      painted_device_scale_factor(1.f),
      page_scale_factor(1.f),
      min_page_scale_factor(1.f),
      max_page_scale_factor(1.f),
      content_source_id_(0),
      background_color(SK_ColorWHITE),
      has_transparent_background(false),
      have_scroll_event_handlers(false),
      event_listener_properties() {}

LayerTree::Inputs::~Inputs() = default;

LayerTree::LayerTree(MutatorHost* mutator_host, LayerTreeHost* layer_tree_host)
    : needs_full_tree_sync_(true),
      needs_meta_info_recomputation_(true),
      in_paint_layer_contents_(false),
      mutator_host_(mutator_host),
      layer_tree_host_(layer_tree_host) {
  DCHECK(mutator_host_);
  DCHECK(layer_tree_host_);
  mutator_host_->SetMutatorHostClient(this);
}

LayerTree::~LayerTree() {
  mutator_host_->SetMutatorHostClient(nullptr);

  // We must clear any pointers into the layer tree prior to destroying it.
  RegisterViewportLayers(nullptr, nullptr, nullptr, nullptr);

  if (inputs_.root_layer) {
    inputs_.root_layer->SetLayerTreeHost(nullptr);

    // The root layer must be destroyed before the layer tree. We've made a
    // contract with our animation controllers that the animation_host will
    // outlive them, and we must make good.
    inputs_.root_layer = nullptr;
  }
}

void LayerTree::SetRootLayer(scoped_refptr<Layer> root_layer) {
  if (inputs_.root_layer.get() == root_layer.get())
    return;

  if (inputs_.root_layer.get())
    inputs_.root_layer->SetLayerTreeHost(nullptr);
  inputs_.root_layer = root_layer;
  if (inputs_.root_layer.get()) {
    DCHECK(!inputs_.root_layer->parent());
    inputs_.root_layer->SetLayerTreeHost(layer_tree_host_);
  }

  if (hud_layer_.get())
    hud_layer_->RemoveFromParent();

  // Reset gpu rasterization tracking.
  // This flag is sticky until a new tree comes along.
  layer_tree_host_->ResetGpuRasterizationTracking();

  SetNeedsFullTreeSync();
}

void LayerTree::RegisterViewportLayers(
    scoped_refptr<Layer> overscroll_elasticity_layer,
    scoped_refptr<Layer> page_scale_layer,
    scoped_refptr<Layer> inner_viewport_scroll_layer,
    scoped_refptr<Layer> outer_viewport_scroll_layer) {
  DCHECK(!inner_viewport_scroll_layer ||
         inner_viewport_scroll_layer != outer_viewport_scroll_layer);
  inputs_.overscroll_elasticity_layer = overscroll_elasticity_layer;
  inputs_.page_scale_layer = page_scale_layer;
  inputs_.inner_viewport_scroll_layer = inner_viewport_scroll_layer;
  inputs_.outer_viewport_scroll_layer = outer_viewport_scroll_layer;
}

void LayerTree::RegisterSelection(const LayerSelection& selection) {
  if (inputs_.selection == selection)
    return;

  inputs_.selection = selection;
  SetNeedsCommit();
}

void LayerTree::SetHaveScrollEventHandlers(bool have_event_handlers) {
  if (inputs_.have_scroll_event_handlers == have_event_handlers)
    return;

  inputs_.have_scroll_event_handlers = have_event_handlers;
  SetNeedsCommit();
}

void LayerTree::SetEventListenerProperties(EventListenerClass event_class,
                                           EventListenerProperties properties) {
  const size_t index = static_cast<size_t>(event_class);
  if (inputs_.event_listener_properties[index] == properties)
    return;

  inputs_.event_listener_properties[index] = properties;
  SetNeedsCommit();
}

void LayerTree::SetViewportSize(const gfx::Size& device_viewport_size) {
  if (inputs_.device_viewport_size == device_viewport_size)
    return;

  inputs_.device_viewport_size = device_viewport_size;

  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void LayerTree::SetBrowserControlsHeight(float height, bool shrink) {
  if (inputs_.top_controls_height == height &&
      inputs_.browser_controls_shrink_blink_size == shrink)
    return;

  inputs_.top_controls_height = height;
  inputs_.browser_controls_shrink_blink_size = shrink;
  SetNeedsCommit();
}

void LayerTree::SetBrowserControlsShownRatio(float ratio) {
  if (inputs_.top_controls_shown_ratio == ratio)
    return;

  inputs_.top_controls_shown_ratio = ratio;
  SetNeedsCommit();
}

void LayerTree::SetBottomControlsHeight(float height) {
  if (inputs_.bottom_controls_height == height)
    return;

  inputs_.bottom_controls_height = height;
  SetNeedsCommit();
}

void LayerTree::SetPageScaleFactorAndLimits(float page_scale_factor,
                                            float min_page_scale_factor,
                                            float max_page_scale_factor) {
  if (inputs_.page_scale_factor == page_scale_factor &&
      inputs_.min_page_scale_factor == min_page_scale_factor &&
      inputs_.max_page_scale_factor == max_page_scale_factor)
    return;

  inputs_.page_scale_factor = page_scale_factor;
  inputs_.min_page_scale_factor = min_page_scale_factor;
  inputs_.max_page_scale_factor = max_page_scale_factor;
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void LayerTree::StartPageScaleAnimation(const gfx::Vector2d& target_offset,
                                        bool use_anchor,
                                        float scale,
                                        base::TimeDelta duration) {
  inputs_.pending_page_scale_animation.reset(new PendingPageScaleAnimation(
      target_offset, use_anchor, scale, duration));

  SetNeedsCommit();
}

bool LayerTree::HasPendingPageScaleAnimation() const {
  return !!inputs_.pending_page_scale_animation.get();
}

void LayerTree::SetDeviceScaleFactor(float device_scale_factor) {
  if (inputs_.device_scale_factor == device_scale_factor)
    return;
  inputs_.device_scale_factor = device_scale_factor;

  property_trees_.needs_rebuild = true;
  SetNeedsCommit();
}

void LayerTree::SetPaintedDeviceScaleFactor(float painted_device_scale_factor) {
  if (inputs_.painted_device_scale_factor == painted_device_scale_factor)
    return;
  inputs_.painted_device_scale_factor = painted_device_scale_factor;

  SetNeedsCommit();
}

void LayerTree::SetDeviceColorSpace(const gfx::ColorSpace& device_color_space) {
  if (inputs_.device_color_space == device_color_space)
    return;
  inputs_.device_color_space = device_color_space;
  LayerTreeHostCommon::CallFunctionForEveryLayer(
      this, [](Layer* layer) { layer->SetNeedsDisplay(); });
}

void LayerTree::SetContentSourceId(uint32_t id) {
  if (inputs_.content_source_id_ == id)
    return;
  inputs_.content_source_id_ = id;
  SetNeedsCommit();
}

void LayerTree::RegisterLayer(Layer* layer) {
  DCHECK(!LayerById(layer->id()));
  DCHECK(!in_paint_layer_contents_);
  layer_id_map_[layer->id()] = layer;
  if (layer->element_id()) {
    mutator_host_->RegisterElement(layer->element_id(),
                                   ElementListType::ACTIVE);
  }
}

void LayerTree::UnregisterLayer(Layer* layer) {
  DCHECK(LayerById(layer->id()));
  DCHECK(!in_paint_layer_contents_);
  if (layer->element_id()) {
    mutator_host_->UnregisterElement(layer->element_id(),
                                     ElementListType::ACTIVE);
  }
  RemoveLayerShouldPushProperties(layer);
  layer_id_map_.erase(layer->id());
}

Layer* LayerTree::LayerById(int id) const {
  LayerIdMap::const_iterator iter = layer_id_map_.find(id);
  return iter != layer_id_map_.end() ? iter->second : nullptr;
}

bool LayerTree::UpdateLayers(const LayerList& update_layer_list,
                             bool* content_is_suitable_for_gpu) {
  base::AutoReset<bool> painting(&in_paint_layer_contents_, true);
  bool did_paint_content = false;
  for (const auto& layer : update_layer_list) {
    did_paint_content |= layer->Update();
    *content_is_suitable_for_gpu &= layer->IsSuitableForGpuRasterization();
  }
  return did_paint_content;
}

void LayerTree::AddLayerShouldPushProperties(Layer* layer) {
  layers_that_should_push_properties_.insert(layer);
}

void LayerTree::RemoveLayerShouldPushProperties(Layer* layer) {
  layers_that_should_push_properties_.erase(layer);
}

std::unordered_set<Layer*>& LayerTree::LayersThatShouldPushProperties() {
  return layers_that_should_push_properties_;
}

bool LayerTree::LayerNeedsPushPropertiesForTesting(Layer* layer) const {
  return layers_that_should_push_properties_.find(layer) !=
         layers_that_should_push_properties_.end();
}

void LayerTree::SetNeedsMetaInfoRecomputation(bool needs_recomputation) {
  needs_meta_info_recomputation_ = needs_recomputation;
}

void LayerTree::SetPageScaleFromImplSide(float page_scale) {
  DCHECK(layer_tree_host_->CommitRequested());
  inputs_.page_scale_factor = page_scale;
  SetPropertyTreesNeedRebuild();
}

void LayerTree::SetElasticOverscrollFromImplSide(
    gfx::Vector2dF elastic_overscroll) {
  DCHECK(layer_tree_host_->CommitRequested());
  elastic_overscroll_ = elastic_overscroll;
}

void LayerTree::UpdateHudLayer(bool show_hud_info) {
  if (show_hud_info) {
    if (!hud_layer_.get()) {
      hud_layer_ = HeadsUpDisplayLayer::Create();
    }

    if (inputs_.root_layer.get() && !hud_layer_->parent())
      inputs_.root_layer->AddChild(hud_layer_);
  } else if (hud_layer_.get()) {
    hud_layer_->RemoveFromParent();
    hud_layer_ = nullptr;
  }
}

void LayerTree::SetNeedsFullTreeSync() {
  needs_full_tree_sync_ = true;
  needs_meta_info_recomputation_ = true;

  property_trees_.needs_rebuild = true;
  SetNeedsCommit();
}

void LayerTree::SetNeedsCommit() {
  layer_tree_host_->SetNeedsCommit();
}

const LayerTreeSettings& LayerTree::GetSettings() const {
  return layer_tree_host_->GetSettings();
}

void LayerTree::SetPropertyTreesNeedRebuild() {
  property_trees_.needs_rebuild = true;
  layer_tree_host_->SetNeedsUpdateLayers();
}

void LayerTree::PushPropertiesTo(LayerTreeImpl* tree_impl,
                                 float unapplied_page_scale_delta) {
  tree_impl->set_needs_full_tree_sync(needs_full_tree_sync_);
  needs_full_tree_sync_ = false;

  if (hud_layer_.get()) {
    LayerImpl* hud_impl = tree_impl->LayerById(hud_layer_->id());
    tree_impl->set_hud_layer(static_cast<HeadsUpDisplayLayerImpl*>(hud_impl));
  } else {
    tree_impl->set_hud_layer(nullptr);
  }

  tree_impl->set_background_color(inputs_.background_color);
  tree_impl->set_has_transparent_background(inputs_.has_transparent_background);
  tree_impl->set_have_scroll_event_handlers(inputs_.have_scroll_event_handlers);
  tree_impl->set_event_listener_properties(
      EventListenerClass::kTouchStartOrMove,
      event_listener_properties(EventListenerClass::kTouchStartOrMove));
  tree_impl->set_event_listener_properties(
      EventListenerClass::kMouseWheel,
      event_listener_properties(EventListenerClass::kMouseWheel));
  tree_impl->set_event_listener_properties(
      EventListenerClass::kTouchEndOrCancel,
      event_listener_properties(EventListenerClass::kTouchEndOrCancel));

  if (inputs_.page_scale_layer && inputs_.inner_viewport_scroll_layer) {
    tree_impl->SetViewportLayersFromIds(
        inputs_.overscroll_elasticity_layer
            ? inputs_.overscroll_elasticity_layer->id()
            : Layer::INVALID_ID,
        inputs_.page_scale_layer->id(),
        inputs_.inner_viewport_scroll_layer->id(),
        inputs_.outer_viewport_scroll_layer
            ? inputs_.outer_viewport_scroll_layer->id()
            : Layer::INVALID_ID);
    DCHECK(inputs_.inner_viewport_scroll_layer
               ->IsContainerForFixedPositionLayers());
  } else {
    tree_impl->ClearViewportLayers();
  }

  tree_impl->RegisterSelection(inputs_.selection);

  bool property_trees_changed_on_active_tree =
      tree_impl->IsActiveTree() && tree_impl->property_trees()->changed;
  // Property trees may store damage status. We preserve the sync tree damage
  // status by pushing the damage status from sync tree property trees to main
  // thread property trees or by moving it onto the layers.
  if (inputs_.root_layer && property_trees_changed_on_active_tree) {
    if (property_trees_.sequence_number ==
        tree_impl->property_trees()->sequence_number)
      tree_impl->property_trees()->PushChangeTrackingTo(&property_trees_);
    else
      tree_impl->MoveChangeTrackingToLayers();
  }
  // Setting property trees must happen before pushing the page scale.
  tree_impl->SetPropertyTrees(&property_trees_);

  tree_impl->PushPageScaleFromMainThread(
      inputs_.page_scale_factor * unapplied_page_scale_delta,
      inputs_.min_page_scale_factor, inputs_.max_page_scale_factor);

  tree_impl->set_browser_controls_shrink_blink_size(
      inputs_.browser_controls_shrink_blink_size);
  tree_impl->set_top_controls_height(inputs_.top_controls_height);
  tree_impl->set_bottom_controls_height(inputs_.bottom_controls_height);
  tree_impl->PushBrowserControlsFromMainThread(
      inputs_.top_controls_shown_ratio);
  tree_impl->elastic_overscroll()->PushFromMainThread(elastic_overscroll_);
  if (tree_impl->IsActiveTree())
    tree_impl->elastic_overscroll()->PushPendingToActive();

  tree_impl->set_painted_device_scale_factor(
      inputs_.painted_device_scale_factor);

  tree_impl->SetDeviceColorSpace(inputs_.device_color_space);

  tree_impl->set_content_source_id(inputs_.content_source_id_);

  if (inputs_.pending_page_scale_animation) {
    tree_impl->SetPendingPageScaleAnimation(
        std::move(inputs_.pending_page_scale_animation));
  }

  DCHECK(!tree_impl->ViewportSizeInvalid());

  tree_impl->set_has_ever_been_drawn(false);
}

void LayerTree::ToProtobuf(proto::LayerTree* proto) {
  TRACE_EVENT0("cc.remote", "LayerProtoConverter::SerializeLayerHierarchy");

  // TODO(khushalsagar): Why walk the tree twice? Why not serialize properties
  // for dirty layers as you serialize the hierarchy?
  if (inputs_.root_layer)
    inputs_.root_layer->ToLayerNodeProto(proto->mutable_root_layer());

  // Viewport layers.
  proto->set_overscroll_elasticity_layer_id(
      inputs_.overscroll_elasticity_layer
          ? inputs_.overscroll_elasticity_layer->id()
          : Layer::INVALID_ID);
  proto->set_page_scale_layer_id(inputs_.page_scale_layer
                                     ? inputs_.page_scale_layer->id()
                                     : Layer::INVALID_ID);
  proto->set_inner_viewport_scroll_layer_id(
      inputs_.inner_viewport_scroll_layer
          ? inputs_.inner_viewport_scroll_layer->id()
          : Layer::INVALID_ID);
  proto->set_outer_viewport_scroll_layer_id(
      inputs_.outer_viewport_scroll_layer
          ? inputs_.outer_viewport_scroll_layer->id()
          : Layer::INVALID_ID);

  // Browser Controls ignored. They are not supported.
  DCHECK(!inputs_.browser_controls_shrink_blink_size);

  proto->set_device_scale_factor(inputs_.device_scale_factor);
  proto->set_painted_device_scale_factor(inputs_.painted_device_scale_factor);
  proto->set_page_scale_factor(inputs_.page_scale_factor);
  proto->set_min_page_scale_factor(inputs_.min_page_scale_factor);
  proto->set_max_page_scale_factor(inputs_.max_page_scale_factor);

  proto->set_background_color(inputs_.background_color);
  proto->set_has_transparent_background(inputs_.has_transparent_background);

  LayerSelectionToProtobuf(inputs_.selection, proto->mutable_selection());
  SizeToProto(inputs_.device_viewport_size,
              proto->mutable_device_viewport_size());

  proto->set_have_scroll_event_handlers(inputs_.have_scroll_event_handlers);
  proto->set_wheel_event_listener_properties(static_cast<uint32_t>(
      event_listener_properties(EventListenerClass::kMouseWheel)));
  proto->set_touch_start_or_move_event_listener_properties(
      static_cast<uint32_t>(
          event_listener_properties(EventListenerClass::kTouchStartOrMove)));
  proto->set_touch_end_or_cancel_event_listener_properties(
      static_cast<uint32_t>(
          event_listener_properties(EventListenerClass::kTouchEndOrCancel)));
}

Layer* LayerTree::LayerByElementId(ElementId element_id) const {
  ElementLayersMap::const_iterator iter = element_layers_map_.find(element_id);
  return iter != element_layers_map_.end() ? iter->second : nullptr;
}

void LayerTree::RegisterElement(ElementId element_id,
                                ElementListType list_type,
                                Layer* layer) {
  if (layer->element_id()) {
    element_layers_map_[layer->element_id()] = layer;
  }

  mutator_host_->RegisterElement(element_id, list_type);
}

void LayerTree::UnregisterElement(ElementId element_id,
                                  ElementListType list_type,
                                  Layer* layer) {
  mutator_host_->UnregisterElement(element_id, list_type);

  if (layer->element_id()) {
    element_layers_map_.erase(layer->element_id());
  }
}

static void SetElementIdForTesting(Layer* layer) {
  layer->SetElementId(LayerIdToElementIdForTesting(layer->id()));
}

void LayerTree::SetElementIdsForTesting() {
  LayerTreeHostCommon::CallFunctionForEveryLayer(this, SetElementIdForTesting);
}

void LayerTree::BuildPropertyTreesForTesting() {
  PropertyTreeBuilder::PreCalculateMetaInformation(root_layer());
  gfx::Transform identity_transform;
  PropertyTreeBuilder::BuildPropertyTrees(
      root_layer(), page_scale_layer(), inner_viewport_scroll_layer(),
      outer_viewport_scroll_layer(), overscroll_elasticity_layer(),
      elastic_overscroll(), page_scale_factor(), device_scale_factor(),
      gfx::Rect(device_viewport_size()), identity_transform, property_trees());
}

bool LayerTree::IsElementInList(ElementId element_id,
                                ElementListType list_type) const {
  return list_type == ElementListType::ACTIVE && LayerByElementId(element_id);
}

void LayerTree::SetMutatorsNeedCommit() {
  layer_tree_host_->SetNeedsCommit();
}

void LayerTree::SetMutatorsNeedRebuildPropertyTrees() {
  property_trees_.needs_rebuild = true;
}

void LayerTree::SetElementFilterMutated(ElementId element_id,
                                        ElementListType list_type,
                                        const FilterOperations& filters) {
  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnFilterAnimated(filters);
}

void LayerTree::SetElementOpacityMutated(ElementId element_id,
                                         ElementListType list_type,
                                         float opacity) {
  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnOpacityAnimated(opacity);
}

void LayerTree::SetElementTransformMutated(ElementId element_id,
                                           ElementListType list_type,
                                           const gfx::Transform& transform) {
  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnTransformAnimated(transform);
}

void LayerTree::SetElementScrollOffsetMutated(
    ElementId element_id,
    ElementListType list_type,
    const gfx::ScrollOffset& scroll_offset) {
  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnScrollOffsetAnimated(scroll_offset);
}

void LayerTree::ElementIsAnimatingChanged(ElementId element_id,
                                          ElementListType list_type,
                                          const PropertyAnimationState& mask,
                                          const PropertyAnimationState& state) {
  Layer* layer = LayerByElementId(element_id);
  if (layer)
    layer->OnIsAnimatingChanged(mask, state);
}

gfx::ScrollOffset LayerTree::GetScrollOffsetForAnimation(
    ElementId element_id) const {
  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  return layer->ScrollOffsetForAnimation();
}

LayerListIterator<Layer> LayerTree::begin() const {
  return LayerListIterator<Layer>(inputs_.root_layer.get());
}

LayerListIterator<Layer> LayerTree::end() const {
  return LayerListIterator<Layer>(nullptr);
}

LayerListReverseIterator<Layer> LayerTree::rbegin() {
  return LayerListReverseIterator<Layer>(inputs_.root_layer.get());
}

LayerListReverseIterator<Layer> LayerTree::rend() {
  return LayerListReverseIterator<Layer>(nullptr);
}

void LayerTree::SetNeedsDisplayOnAllLayers() {
  for (auto* layer : *this)
    layer->SetNeedsDisplay();
}

}  // namespace cc
