// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <set>

#include "base/containers/adapters.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/animation/animation_host.h"
#include "cc/base/histograms.h"
#include "cc/base/math_util.h"
#include "cc/base/synced_property.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "cc/input/page_scale_animation.h"
#include "cc/input/scrollbar_animation_controller.h"
#include "cc/input/scrollbar_animation_controller_linear_fade.h"
#include "cc/input/scrollbar_animation_controller_thinning.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_iterator.h"
#include "cc/layers/layer_list_iterator.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/resources/ui_resource_request.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/occlusion_tracker.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/property_tree_builder.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {

LayerTreeImpl::LayerTreeImpl(
    LayerTreeHostImpl* layer_tree_host_impl,
    scoped_refptr<SyncedProperty<ScaleGroup>> page_scale_factor,
    scoped_refptr<SyncedTopControls> top_controls_shown_ratio,
    scoped_refptr<SyncedElasticOverscroll> elastic_overscroll)
    : layer_tree_host_impl_(layer_tree_host_impl),
      source_frame_number_(-1),
      is_first_frame_after_commit_tracker_(-1),
      root_layer_for_testing_(nullptr),
      hud_layer_(nullptr),
      background_color_(0),
      has_transparent_background_(false),
      last_scrolled_layer_id_(Layer::INVALID_ID),
      overscroll_elasticity_layer_id_(Layer::INVALID_ID),
      page_scale_layer_id_(Layer::INVALID_ID),
      inner_viewport_scroll_layer_id_(Layer::INVALID_ID),
      outer_viewport_scroll_layer_id_(Layer::INVALID_ID),
      page_scale_factor_(page_scale_factor),
      min_page_scale_factor_(0),
      max_page_scale_factor_(0),
      device_scale_factor_(1.f),
      painted_device_scale_factor_(1.f),
      elastic_overscroll_(elastic_overscroll),
      layers_(new OwnedLayerImplList),
      viewport_size_invalid_(false),
      needs_update_draw_properties_(true),
      needs_full_tree_sync_(true),
      next_activation_forces_redraw_(false),
      has_ever_been_drawn_(false),
      have_scroll_event_handlers_(false),
      event_listener_properties_(),
      top_controls_shrink_blink_size_(false),
      top_controls_height_(0),
      top_controls_shown_ratio_(top_controls_shown_ratio) {
  property_trees()->is_main_thread = false;
}

LayerTreeImpl::~LayerTreeImpl() {
  BreakSwapPromises(IsActiveTree() ? SwapPromise::SWAP_FAILS
                                   : SwapPromise::ACTIVATION_FAILS);

  // Need to explicitly clear the tree prior to destroying this so that
  // the LayerTreeImpl pointer is still valid in the LayerImpl dtor.
  DCHECK(LayerListIsEmpty());
  DCHECK(layers_->empty());
}

void LayerTreeImpl::Shutdown() {
  DetachLayers();
  DCHECK(LayerListIsEmpty());
}

void LayerTreeImpl::ReleaseResources() {
  if (!LayerListIsEmpty()) {
    LayerTreeHostCommon::CallFunctionForEveryLayer(
        this, [](LayerImpl* layer) { layer->ReleaseResources(); });
  }
}

void LayerTreeImpl::RecreateResources() {
  if (!LayerListIsEmpty()) {
    LayerTreeHostCommon::CallFunctionForEveryLayer(
        this, [](LayerImpl* layer) { layer->RecreateResources(); });
  }
}

bool LayerTreeImpl::IsViewportLayerId(int id) const {
  if (id == inner_viewport_scroll_layer_id_ ||
      id == outer_viewport_scroll_layer_id_)
    return true;
  if (InnerViewportContainerLayer() &&
      id == InnerViewportContainerLayer()->id())
    return true;
  if (OuterViewportContainerLayer() &&
      id == OuterViewportContainerLayer()->id())
    return true;

  return false;
}

// TODO(sunxd): when we have a layer_id to property_tree index map in property
// trees, use the transform_id parameter instead of looking for indices from
// LayerImpls.
void LayerTreeImpl::DidUpdateScrollOffset(int layer_id, int transform_id) {
  DidUpdateScrollState(layer_id);
  TransformTree& transform_tree = property_trees()->transform_tree;
  ScrollTree& scroll_tree = property_trees()->scroll_tree;

  // If pending tree topology changed and we still want to notify the pending
  // tree about scroll offset in the active tree, we may not find the
  // corresponding pending layer.
  if (LayerById(layer_id)) {
    transform_id = LayerById(layer_id)->transform_tree_index();
  } else {
    DCHECK(!IsActiveTree());
    return;
  }

  if (transform_id != -1) {
    TransformNode* node = transform_tree.Node(transform_id);
    if (node->data.scroll_offset !=
        scroll_tree.current_scroll_offset(layer_id)) {
      node->data.scroll_offset = scroll_tree.current_scroll_offset(layer_id);
      node->data.needs_local_transform_update = true;
      transform_tree.set_needs_update(true);
    }
    node->data.transform_changed = true;
    property_trees()->changed = true;
    set_needs_update_draw_properties();
  }

  if (IsActiveTree() && layer_tree_host_impl_->pending_tree())
    layer_tree_host_impl_->pending_tree()->DidUpdateScrollOffset(layer_id,
                                                                 transform_id);
}

void LayerTreeImpl::DidUpdateScrollState(int layer_id) {
  if (!IsActiveTree())
    return;

  if (layer_id == Layer::INVALID_ID)
    return;

  int scroll_layer_id, clip_layer_id;
  if (IsViewportLayerId(layer_id)) {
    if (!InnerViewportContainerLayer())
      return;

    // For scrollbar purposes, a change to any of the four viewport layers
    // should affect the scrollbars tied to the outermost layers, which express
    // the sum of the entire viewport.
    scroll_layer_id = outer_viewport_scroll_layer_id_;
    clip_layer_id = InnerViewportContainerLayer()->id();
  } else {
    // If the clip layer id was passed in, then look up the scroll layer, or
    // vice versa.
    auto i = clip_scroll_map_.find(layer_id);
    if (i != clip_scroll_map_.end()) {
      scroll_layer_id = i->second;
      clip_layer_id = layer_id;
    } else {
      scroll_layer_id = layer_id;
      clip_layer_id = LayerById(scroll_layer_id)->scroll_clip_layer_id();
    }
  }
  UpdateScrollbars(scroll_layer_id, clip_layer_id);
}

void LayerTreeImpl::UpdateScrollbars(int scroll_layer_id, int clip_layer_id) {
  DCHECK(IsActiveTree());

  LayerImpl* clip_layer = LayerById(clip_layer_id);
  LayerImpl* scroll_layer = LayerById(scroll_layer_id);

  if (!clip_layer || !scroll_layer)
    return;

  gfx::SizeF clip_size(clip_layer->BoundsForScrolling());
  gfx::SizeF scroll_size(scroll_layer->BoundsForScrolling());

  if (scroll_size.IsEmpty())
    return;

  gfx::ScrollOffset current_offset = scroll_layer->CurrentScrollOffset();
  if (IsViewportLayerId(scroll_layer_id)) {
    current_offset += InnerViewportScrollLayer()->CurrentScrollOffset();
    if (OuterViewportContainerLayer())
      clip_size.SetToMin(OuterViewportContainerLayer()->BoundsForScrolling());
    clip_size.Scale(1 / current_page_scale_factor());
  }

  bool scrollbar_needs_animation = false;
  bool scroll_layer_size_did_change = false;
  bool y_offset_did_change = false;
  for (ScrollbarLayerImplBase* scrollbar : ScrollbarsFor(scroll_layer_id)) {
    if (scrollbar->orientation() == HORIZONTAL) {
      scrollbar_needs_animation |= scrollbar->SetCurrentPos(current_offset.x());
      scrollbar_needs_animation |=
          scrollbar->SetClipLayerLength(clip_size.width());
      scrollbar_needs_animation |= scroll_layer_size_did_change |=
          scrollbar->SetScrollLayerLength(scroll_size.width());
    } else {
      scrollbar_needs_animation |= y_offset_did_change |=
          scrollbar->SetCurrentPos(current_offset.y());
      scrollbar_needs_animation |=
          scrollbar->SetClipLayerLength(clip_size.height());
      scrollbar_needs_animation |= scroll_layer_size_did_change |=
          scrollbar->SetScrollLayerLength(scroll_size.height());
    }
    scrollbar_needs_animation |=
        scrollbar->SetVerticalAdjust(clip_layer->bounds_delta().y());
  }

  if (y_offset_did_change && IsViewportLayerId(scroll_layer_id))
    TRACE_COUNTER_ID1("cc", "scroll_offset_y", scroll_layer->id(),
                      current_offset.y());

  if (scrollbar_needs_animation) {
    ScrollbarAnimationController* controller =
        layer_tree_host_impl_->ScrollbarAnimationControllerForId(
            scroll_layer_id);
    if (controller)
      controller->DidScrollUpdate(scroll_layer_size_did_change);
  }
}

RenderSurfaceImpl* LayerTreeImpl::RootRenderSurface() const {
  return layer_list_.empty() ? nullptr : layer_list_[0]->render_surface();
}

bool LayerTreeImpl::LayerListIsEmpty() const {
  return layer_list_.empty();
}

void LayerTreeImpl::SetRootLayerForTesting(std::unique_ptr<LayerImpl> layer) {
  if (root_layer_for_testing_ && layer.get() != root_layer_for_testing_)
    RemoveLayer(root_layer_for_testing_->id());
  root_layer_for_testing_ = layer.get();
  ClearLayerList();
  if (layer) {
    AddLayer(std::move(layer));
    BuildLayerListForTesting();
  }
  layer_tree_host_impl_->OnCanDrawStateChangedForTree();
}

void LayerTreeImpl::SetRootLayerFromLayerListForTesting() {
  root_layer_for_testing_ = layer_list_.empty() ? nullptr : layer_list_[0];
}

void LayerTreeImpl::OnCanDrawStateChangedForTree() {
  layer_tree_host_impl_->OnCanDrawStateChangedForTree();
}

void LayerTreeImpl::AddToLayerList(LayerImpl* layer) {
  layer_list_.push_back(layer);
}

void LayerTreeImpl::ClearLayerList() {
  layer_list_.clear();
}

void LayerTreeImpl::BuildLayerListForTesting() {
  ClearLayerList();
  LayerListIterator<LayerImpl> it(root_layer_for_testing_);
  for (; it != LayerListIterator<LayerImpl>(nullptr); ++it) {
    AddToLayerList(*it);
  }
}

bool LayerTreeImpl::IsRootLayer(const LayerImpl* layer) const {
  return layer_list_.empty() ? false : layer_list_[0] == layer;
}

LayerImpl* LayerTreeImpl::InnerViewportScrollLayer() const {
  return LayerById(inner_viewport_scroll_layer_id_);
}

LayerImpl* LayerTreeImpl::OuterViewportScrollLayer() const {
  return LayerById(outer_viewport_scroll_layer_id_);
}

gfx::ScrollOffset LayerTreeImpl::TotalScrollOffset() const {
  gfx::ScrollOffset offset;

  if (InnerViewportScrollLayer())
    offset += InnerViewportScrollLayer()->CurrentScrollOffset();

  if (OuterViewportScrollLayer())
    offset += OuterViewportScrollLayer()->CurrentScrollOffset();

  return offset;
}

gfx::ScrollOffset LayerTreeImpl::TotalMaxScrollOffset() const {
  gfx::ScrollOffset offset;

  if (InnerViewportScrollLayer())
    offset += InnerViewportScrollLayer()->MaxScrollOffset();

  if (OuterViewportScrollLayer())
    offset += OuterViewportScrollLayer()->MaxScrollOffset();

  return offset;
}

std::unique_ptr<OwnedLayerImplList> LayerTreeImpl::DetachLayers() {
  root_layer_for_testing_ = nullptr;
  layer_list_.clear();
  render_surface_layer_list_.clear();
  set_needs_update_draw_properties();
  std::unique_ptr<OwnedLayerImplList> ret = std::move(layers_);
  layers_.reset(new OwnedLayerImplList);
  return ret;
}

static void UpdateClipTreeForBoundsDeltaOnLayer(LayerImpl* layer,
                                                ClipTree* clip_tree) {
  if (layer && layer->masks_to_bounds()) {
    ClipNode* clip_node = clip_tree->Node(layer->clip_tree_index());
    if (clip_node) {
      DCHECK_EQ(layer->id(), clip_node->owner_id);
      gfx::SizeF bounds = gfx::SizeF(layer->bounds());
      if (clip_node->data.clip.size() != bounds) {
        clip_node->data.clip.set_size(bounds);
        clip_tree->set_needs_update(true);
      }
    }
  }
}

void LayerTreeImpl::SetPropertyTrees(PropertyTrees* property_trees) {
  property_trees_ = *property_trees;
  property_trees->effect_tree.PushCopyRequestsTo(&property_trees_.effect_tree);
  property_trees_.is_main_thread = false;
  property_trees_.is_active = IsActiveTree();
  property_trees_.transform_tree.set_source_to_parent_updates_allowed(false);
  // The value of some effect node properties (like is_drawn) depends on
  // whether we are on the active tree or not. So, we need to update the
  // effect tree.
  if (IsActiveTree())
    property_trees_.effect_tree.set_needs_update(true);
}

void LayerTreeImpl::UpdatePropertyTreesForBoundsDelta() {
  DCHECK(IsActiveTree());
  LayerImpl* inner_container = InnerViewportContainerLayer();
  LayerImpl* outer_container = OuterViewportContainerLayer();
  LayerImpl* inner_scroll = InnerViewportScrollLayer();

  UpdateClipTreeForBoundsDeltaOnLayer(inner_container,
                                      &property_trees_.clip_tree);
  UpdateClipTreeForBoundsDeltaOnLayer(InnerViewportScrollLayer(),
                                      &property_trees_.clip_tree);
  UpdateClipTreeForBoundsDeltaOnLayer(outer_container,
                                      &property_trees_.clip_tree);

  if (inner_container)
    property_trees_.SetInnerViewportContainerBoundsDelta(
        inner_container->bounds_delta());
  if (outer_container)
    property_trees_.SetOuterViewportContainerBoundsDelta(
        outer_container->bounds_delta());
  if (inner_scroll)
    property_trees_.SetInnerViewportScrollBoundsDelta(
        inner_scroll->bounds_delta());
}

void LayerTreeImpl::PushPropertiesTo(LayerTreeImpl* target_tree) {
  // The request queue should have been processed and does not require a push.
  DCHECK_EQ(ui_resource_request_queue_.size(), 0u);

  LayerImpl* layer = target_tree->CurrentlyScrollingLayer();
  target_tree->SetPropertyTrees(&property_trees_);
  target_tree->SetCurrentlyScrollingLayer(layer);
  target_tree->UpdatePropertyTreeScrollOffset(&property_trees_);

  if (next_activation_forces_redraw_) {
    target_tree->ForceRedrawNextActivation();
    next_activation_forces_redraw_ = false;
  }

  target_tree->PassSwapPromises(&swap_promise_list_);

  target_tree->set_top_controls_shrink_blink_size(
      top_controls_shrink_blink_size_);
  target_tree->set_top_controls_height(top_controls_height_);
  target_tree->PushTopControls(nullptr);

  // Active tree already shares the page_scale_factor object with pending
  // tree so only the limits need to be provided.
  target_tree->PushPageScaleFactorAndLimits(nullptr, min_page_scale_factor(),
                                            max_page_scale_factor());
  target_tree->SetDeviceScaleFactor(device_scale_factor());
  target_tree->set_painted_device_scale_factor(painted_device_scale_factor());
  target_tree->elastic_overscroll()->PushPendingToActive();

  target_tree->pending_page_scale_animation_ =
      std::move(pending_page_scale_animation_);

  target_tree->SetViewportLayersFromIds(
      overscroll_elasticity_layer_id_, page_scale_layer_id_,
      inner_viewport_scroll_layer_id_, outer_viewport_scroll_layer_id_);

  target_tree->RegisterSelection(selection_);

  // This should match the property synchronization in
  // LayerTreeHost::finishCommitOnImplThread().
  target_tree->set_source_frame_number(source_frame_number());
  target_tree->set_background_color(background_color());
  target_tree->set_has_transparent_background(has_transparent_background());
  target_tree->set_have_scroll_event_handlers(have_scroll_event_handlers());
  target_tree->set_event_listener_properties(
      EventListenerClass::kTouchStartOrMove,
      event_listener_properties(EventListenerClass::kTouchStartOrMove));
  target_tree->set_event_listener_properties(
      EventListenerClass::kMouseWheel,
      event_listener_properties(EventListenerClass::kMouseWheel));
  target_tree->set_event_listener_properties(
      EventListenerClass::kTouchEndOrCancel,
      event_listener_properties(EventListenerClass::kTouchEndOrCancel));

  if (ViewportSizeInvalid())
    target_tree->SetViewportSizeInvalid();
  else
    target_tree->ResetViewportSizeInvalid();

  if (hud_layer())
    target_tree->set_hud_layer(static_cast<HeadsUpDisplayLayerImpl*>(
        target_tree->LayerById(hud_layer()->id())));
  else
    target_tree->set_hud_layer(NULL);

  target_tree->has_ever_been_drawn_ = false;
}

void LayerTreeImpl::MoveChangeTrackingToLayers() {
  // We need to update the change tracking on property trees before we move it
  // onto the layers.
  property_trees_.UpdateChangeTracking();
  for (auto* layer : *this) {
    if (layer->LayerPropertyChanged())
      layer->NoteLayerPropertyChanged();
  }
}

LayerImplList::const_iterator LayerTreeImpl::begin() const {
  return layer_list_.cbegin();
}

LayerImplList::const_iterator LayerTreeImpl::end() const {
  return layer_list_.cend();
}

LayerImplList::reverse_iterator LayerTreeImpl::rbegin() {
  return layer_list_.rbegin();
}

LayerImplList::reverse_iterator LayerTreeImpl::rend() {
  return layer_list_.rend();
}

LayerImpl* LayerTreeImpl::LayerByElementId(ElementId element_id) const {
  auto iter = element_layers_map_.find(element_id);
  if (iter == element_layers_map_.end())
    return nullptr;

  return iter->second;
}

void LayerTreeImpl::AddToElementMap(LayerImpl* layer) {
  if (!layer->element_id())
    return;

  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "LayerTreeImpl::AddToElementMap", "element",
               layer->element_id().AsValue().release(), "layer_id",
               layer->id());

  element_layers_map_[layer->element_id()] = layer;

  layer_tree_host_impl_->animation_host()->RegisterElement(
      layer->element_id(),
      IsActiveTree() ? ElementListType::ACTIVE : ElementListType::PENDING);
}

void LayerTreeImpl::RemoveFromElementMap(LayerImpl* layer) {
  if (!layer->element_id())
    return;

  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "LayerTreeImpl::RemoveFromElementMap", "element",
               layer->element_id().AsValue().release(), "layer_id",
               layer->id());

  layer_tree_host_impl_->animation_host()->UnregisterElement(
      layer->element_id(),
      IsActiveTree() ? ElementListType::ACTIVE : ElementListType::PENDING);

  element_layers_map_.erase(layer->element_id());
}

void LayerTreeImpl::AddToOpacityAnimationsMap(int id, float opacity) {
  opacity_animations_map_[id] = opacity;
}

void LayerTreeImpl::AddToTransformAnimationsMap(int id,
                                                gfx::Transform transform) {
  transform_animations_map_[id] = transform;
}

LayerImpl* LayerTreeImpl::InnerViewportContainerLayer() const {
  return InnerViewportScrollLayer()
             ? InnerViewportScrollLayer()->scroll_clip_layer()
             : NULL;
}

LayerImpl* LayerTreeImpl::OuterViewportContainerLayer() const {
  return OuterViewportScrollLayer()
             ? OuterViewportScrollLayer()->scroll_clip_layer()
             : NULL;
}

LayerImpl* LayerTreeImpl::CurrentlyScrollingLayer() const {
  DCHECK(IsActiveTree());
  const ScrollNode* scroll_node =
      property_trees_.scroll_tree.CurrentlyScrollingNode();
  return LayerById(scroll_node ? scroll_node->owner_id : Layer::INVALID_ID);
}

int LayerTreeImpl::LastScrolledLayerId() const {
  return last_scrolled_layer_id_;
}

void LayerTreeImpl::SetCurrentlyScrollingLayer(LayerImpl* layer) {
  ScrollTree& scroll_tree = property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.CurrentlyScrollingNode();
  int old_id = scroll_node ? scroll_node->owner_id : Layer::INVALID_ID;
  int new_id = layer ? layer->id() : Layer::INVALID_ID;
  int new_scroll_node_id = layer ? layer->scroll_tree_index() : -1;
  if (layer)
    last_scrolled_layer_id_ = new_id;

  if (old_id == new_id)
    return;

  ScrollbarAnimationController* old_animation_controller =
      layer_tree_host_impl_->ScrollbarAnimationControllerForId(old_id);
  ScrollbarAnimationController* new_animation_controller =
      layer_tree_host_impl_->ScrollbarAnimationControllerForId(new_id);

  if (old_animation_controller)
    old_animation_controller->DidScrollEnd();
  scroll_tree.set_currently_scrolling_node(new_scroll_node_id);
  if (new_animation_controller)
    new_animation_controller->DidScrollBegin();
}

void LayerTreeImpl::ClearCurrentlyScrollingLayer() {
  SetCurrentlyScrollingLayer(NULL);
}

float LayerTreeImpl::ClampPageScaleFactorToLimits(
    float page_scale_factor) const {
  if (min_page_scale_factor_ && page_scale_factor < min_page_scale_factor_)
    page_scale_factor = min_page_scale_factor_;
  else if (max_page_scale_factor_ && page_scale_factor > max_page_scale_factor_)
    page_scale_factor = max_page_scale_factor_;
  return page_scale_factor;
}

void LayerTreeImpl::UpdatePropertyTreeScrollingAndAnimationFromMainThread() {
  // TODO(enne): This should get replaced by pulling out scrolling and
  // animations into their own trees.  Then scrolls and animations would have
  // their own ways of synchronizing across commits.  This occurs to push
  // updates from scrolling deltas on the compositor thread that have occurred
  // after begin frame and updates from animations that have ticked since begin
  // frame to a newly-committed property tree.
  if (layer_list_.empty())
    return;
  std::vector<int> layer_ids_to_remove;
  for (auto& layer_id_to_opacity : opacity_animations_map_) {
    const int id = layer_id_to_opacity.first;
    if (property_trees_.IsInIdToIndexMap(PropertyTrees::TreeType::EFFECT, id)) {
      EffectNode* node = property_trees_.effect_tree.Node(
          property_trees_.effect_id_to_index_map[id]);
      if (!node->data.is_currently_animating_opacity ||
          node->data.opacity == layer_id_to_opacity.second) {
        layer_ids_to_remove.push_back(id);
        continue;
      }
      node->data.opacity = layer_id_to_opacity.second;
      property_trees_.effect_tree.set_needs_update(true);
    }
  }
  for (auto id : layer_ids_to_remove)
    opacity_animations_map_.erase(id);
  layer_ids_to_remove.clear();

  for (auto& layer_id_to_transform : transform_animations_map_) {
    const int id = layer_id_to_transform.first;
    if (property_trees_.IsInIdToIndexMap(PropertyTrees::TreeType::TRANSFORM,
                                         id)) {
      TransformNode* node = property_trees_.transform_tree.Node(
          property_trees_.transform_id_to_index_map[id]);
      if (!node->data.is_currently_animating ||
          node->data.local == layer_id_to_transform.second) {
        layer_ids_to_remove.push_back(id);
        continue;
      }
      node->data.local = layer_id_to_transform.second;
      node->data.needs_local_transform_update = true;
      property_trees_.transform_tree.set_needs_update(true);
    }
  }
  for (auto id : layer_ids_to_remove)
    transform_animations_map_.erase(id);

  LayerTreeHostCommon::CallFunctionForEveryLayer(this, [](LayerImpl* layer) {
    layer->UpdatePropertyTreeForScrollingAndAnimationIfNeeded();
  });
}

void LayerTreeImpl::SetPageScaleOnActiveTree(float active_page_scale) {
  DCHECK(IsActiveTree());
  if (page_scale_factor()->SetCurrent(
          ClampPageScaleFactorToLimits(active_page_scale))) {
    DidUpdatePageScale();
    if (PageScaleLayer()) {
      draw_property_utils::UpdatePageScaleFactor(
          property_trees(), PageScaleLayer(), current_page_scale_factor(),
          device_scale_factor(), layer_tree_host_impl_->DrawTransform());
    } else {
      DCHECK(layer_list_.empty() || active_page_scale == 1);
    }
  }
}

void LayerTreeImpl::PushPageScaleFromMainThread(float page_scale_factor,
                                                float min_page_scale_factor,
                                                float max_page_scale_factor) {
  PushPageScaleFactorAndLimits(&page_scale_factor, min_page_scale_factor,
                               max_page_scale_factor);
}

void LayerTreeImpl::PushPageScaleFactorAndLimits(const float* page_scale_factor,
                                                 float min_page_scale_factor,
                                                 float max_page_scale_factor) {
  DCHECK(page_scale_factor || IsActiveTree());
  bool changed_page_scale = false;

  changed_page_scale |=
      SetPageScaleFactorLimits(min_page_scale_factor, max_page_scale_factor);

  if (page_scale_factor) {
    DCHECK(!IsActiveTree() || !layer_tree_host_impl_->pending_tree());
    changed_page_scale |= page_scale_factor_->Delta() != 1.f;
    // TODO(enne): Once CDP goes away, ignore this call below.  The only time
    // the property trees will differ is if there's been a page scale on the
    // compositor thread after the begin frame, which is the delta check above.
    changed_page_scale |=
        page_scale_factor_->PushFromMainThread(*page_scale_factor);
  }

  if (IsActiveTree()) {
    // TODO(enne): Pushing from pending to active should never require
    // DidUpdatePageScale.  The values should already be set by the fully
    // computed property trees being synced from one tree to another.  Remove
    // this once CDP goes away.
    changed_page_scale |= page_scale_factor_->PushPendingToActive();
  }

  if (changed_page_scale)
    DidUpdatePageScale();

  if (page_scale_factor) {
    if (PageScaleLayer()) {
      draw_property_utils::UpdatePageScaleFactor(
          property_trees(), PageScaleLayer(), current_page_scale_factor(),
          device_scale_factor(), layer_tree_host_impl_->DrawTransform());
    } else {
      DCHECK(layer_list_.empty() || *page_scale_factor == 1);
    }
  }
}

void LayerTreeImpl::set_top_controls_shrink_blink_size(bool shrink) {
  if (top_controls_shrink_blink_size_ == shrink)
    return;

  top_controls_shrink_blink_size_ = shrink;
  if (IsActiveTree())
    layer_tree_host_impl_->UpdateViewportContainerSizes();
}

void LayerTreeImpl::set_top_controls_height(float top_controls_height) {
  if (top_controls_height_ == top_controls_height)
    return;

  top_controls_height_ = top_controls_height;
  if (IsActiveTree())
    layer_tree_host_impl_->UpdateViewportContainerSizes();
}

bool LayerTreeImpl::ClampTopControlsShownRatio() {
  float ratio = top_controls_shown_ratio_->Current(true);
  ratio = std::max(ratio, 0.f);
  ratio = std::min(ratio, 1.f);
  return top_controls_shown_ratio_->SetCurrent(ratio);
}

bool LayerTreeImpl::SetCurrentTopControlsShownRatio(float ratio) {
  bool changed = top_controls_shown_ratio_->SetCurrent(ratio);
  changed |= ClampTopControlsShownRatio();
  return changed;
}

void LayerTreeImpl::PushTopControlsFromMainThread(
    float top_controls_shown_ratio) {
  PushTopControls(&top_controls_shown_ratio);
}

void LayerTreeImpl::PushTopControls(const float* top_controls_shown_ratio) {
  DCHECK(top_controls_shown_ratio || IsActiveTree());

  if (top_controls_shown_ratio) {
    DCHECK(!IsActiveTree() || !layer_tree_host_impl_->pending_tree());
    top_controls_shown_ratio_->PushFromMainThread(*top_controls_shown_ratio);
  }
  if (IsActiveTree()) {
    bool changed_active = top_controls_shown_ratio_->PushPendingToActive();
    changed_active |= ClampTopControlsShownRatio();
    if (changed_active)
      layer_tree_host_impl_->DidChangeTopControlsPosition();
  }
}

bool LayerTreeImpl::SetPageScaleFactorLimits(float min_page_scale_factor,
                                             float max_page_scale_factor) {
  if (min_page_scale_factor == min_page_scale_factor_ &&
      max_page_scale_factor == max_page_scale_factor_)
    return false;

  min_page_scale_factor_ = min_page_scale_factor;
  max_page_scale_factor_ = max_page_scale_factor;

  return true;
}

void LayerTreeImpl::DidUpdatePageScale() {
  if (IsActiveTree())
    page_scale_factor()->SetCurrent(
        ClampPageScaleFactorToLimits(current_page_scale_factor()));

  set_needs_update_draw_properties();
  DidUpdateScrollState(inner_viewport_scroll_layer_id_);
}

void LayerTreeImpl::SetDeviceScaleFactor(float device_scale_factor) {
  if (device_scale_factor == device_scale_factor_)
    return;
  device_scale_factor_ = device_scale_factor;

  set_needs_update_draw_properties();
  if (IsActiveTree())
    layer_tree_host_impl_->SetFullRootLayerDamage();
}

SyncedProperty<ScaleGroup>* LayerTreeImpl::page_scale_factor() {
  return page_scale_factor_.get();
}

const SyncedProperty<ScaleGroup>* LayerTreeImpl::page_scale_factor() const {
  return page_scale_factor_.get();
}

gfx::SizeF LayerTreeImpl::ScrollableViewportSize() const {
  if (!InnerViewportContainerLayer())
    return gfx::SizeF();

  return gfx::ScaleSize(InnerViewportContainerLayer()->BoundsForScrolling(),
                        1.0f / current_page_scale_factor());
}

gfx::Rect LayerTreeImpl::RootScrollLayerDeviceViewportBounds() const {
  LayerImpl* root_scroll_layer = OuterViewportScrollLayer()
                                     ? OuterViewportScrollLayer()
                                     : InnerViewportScrollLayer();
  if (!root_scroll_layer)
    return gfx::Rect();
  return MathUtil::MapEnclosingClippedRect(
      root_scroll_layer->ScreenSpaceTransform(),
      gfx::Rect(root_scroll_layer->bounds()));
}

void LayerTreeImpl::ApplySentScrollAndScaleDeltasFromAbortedCommit() {
  DCHECK(IsActiveTree());

  page_scale_factor()->AbortCommit();
  top_controls_shown_ratio()->AbortCommit();
  elastic_overscroll()->AbortCommit();

  if (layer_list_.empty())
    return;

  property_trees()->scroll_tree.ApplySentScrollDeltasFromAbortedCommit();
}

void LayerTreeImpl::SetViewportLayersFromIds(
    int overscroll_elasticity_layer_id,
    int page_scale_layer_id,
    int inner_viewport_scroll_layer_id,
    int outer_viewport_scroll_layer_id) {
  overscroll_elasticity_layer_id_ = overscroll_elasticity_layer_id;
  page_scale_layer_id_ = page_scale_layer_id;
  inner_viewport_scroll_layer_id_ = inner_viewport_scroll_layer_id;
  outer_viewport_scroll_layer_id_ = outer_viewport_scroll_layer_id;
}

void LayerTreeImpl::ClearViewportLayers() {
  overscroll_elasticity_layer_id_ = Layer::INVALID_ID;
  page_scale_layer_id_ = Layer::INVALID_ID;
  inner_viewport_scroll_layer_id_ = Layer::INVALID_ID;
  outer_viewport_scroll_layer_id_ = Layer::INVALID_ID;
}

// For unit tests, we use the layer's id as its element id.
static void SetElementIdForTesting(LayerImpl* layer) {
  layer->SetElementId(LayerIdToElementIdForTesting(layer->id()));
}

void LayerTreeImpl::SetElementIdsForTesting() {
  LayerListIterator<LayerImpl> it(root_layer_for_testing_);
  for (; it != LayerListIterator<LayerImpl>(nullptr); ++it) {
    SetElementIdForTesting(*it);
  }
}

bool LayerTreeImpl::UpdateDrawProperties(bool update_lcd_text) {
  if (!needs_update_draw_properties_)
    return true;

  // Calling UpdateDrawProperties must clear this flag, so there can be no
  // early outs before this.
  needs_update_draw_properties_ = false;

  // For max_texture_size.  When the renderer is re-created in
  // CreateAndSetRenderer, the needs update draw properties flag is set
  // again.
  if (!layer_tree_host_impl_->renderer())
    return false;

  // Clear this after the renderer early out, as it should still be
  // possible to hit test even without a renderer.
  render_surface_layer_list_.clear();

  if (layer_list_.empty())
    return false;

  {
    base::ElapsedTimer timer;
    TRACE_EVENT2(
        "cc", "LayerTreeImpl::UpdateDrawProperties::CalculateDrawProperties",
        "IsActive", IsActiveTree(), "SourceFrameNumber", source_frame_number_);
    bool can_render_to_separate_surface =
        (!is_in_resourceless_software_draw_mode());

    LayerTreeHostCommon::CalcDrawPropsImplInputs inputs(
        layer_list_[0], DrawViewportSize(),
        layer_tree_host_impl_->DrawTransform(), device_scale_factor(),
        current_page_scale_factor(), PageScaleLayer(),
        InnerViewportScrollLayer(), OuterViewportScrollLayer(),
        elastic_overscroll()->Current(IsActiveTree()),
        OverscrollElasticityLayer(), resource_provider()->max_texture_size(),
        can_render_to_separate_surface,
        settings().layer_transforms_should_scale_layer_contents,
        settings().verify_clip_tree_calculations, &render_surface_layer_list_,
        &property_trees_);
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
    if (const char* client_name = GetClientNameForMetrics()) {
      UMA_HISTOGRAM_COUNTS(
          base::StringPrintf(
              "Compositing.%s.LayerTreeImpl.CalculateDrawPropertiesUs",
              client_name),
          timer.Elapsed().InMicroseconds());
      UMA_HISTOGRAM_COUNTS_100(
          base::StringPrintf("Compositing.%s.NumRenderSurfaces", client_name),
          base::saturated_cast<int>(render_surface_layer_list_.size()));
    }
  }

  {
    TRACE_EVENT2("cc", "LayerTreeImpl::UpdateDrawProperties::Occlusion",
                 "IsActive", IsActiveTree(), "SourceFrameNumber",
                 source_frame_number_);
    OcclusionTracker occlusion_tracker(
        layer_list_[0]->render_surface()->content_rect());
    occlusion_tracker.set_minimum_tracking_size(
        settings().minimum_occlusion_tracking_size);

    // LayerIterator is used here instead of CallFunctionForEveryLayer to only
    // UpdateTilePriorities on layers that will be visible (and thus have valid
    // draw properties) and not because any ordering is required.
    LayerIterator end = LayerIterator::End(&render_surface_layer_list_);
    for (LayerIterator it = LayerIterator::Begin(&render_surface_layer_list_);
         it != end; ++it) {
      occlusion_tracker.EnterLayer(it);

      bool inside_replica = it->InsideReplica();

      // Don't use occlusion if a layer will appear in a replica, since the
      // tile raster code does not know how to look for the replica and would
      // consider it occluded even though the replica is visible.
      // Since occlusion is only used for browser compositor (i.e.
      // use_occlusion_for_tile_prioritization) and it won't use replicas,
      // this should matter not.

      if (it.represents_itself()) {
        Occlusion occlusion =
            inside_replica ? Occlusion()
                           : occlusion_tracker.GetCurrentOcclusionForLayer(
                                 it->DrawTransform());
        it->draw_properties().occlusion_in_content_space = occlusion;
      }

      if (it.represents_contributing_render_surface()) {
        // Surfaces aren't used by the tile raster code, so they can have
        // occlusion regardless of replicas.
        Occlusion occlusion =
            occlusion_tracker.GetCurrentOcclusionForContributingSurface(
                it->render_surface()->draw_transform());
        it->render_surface()->set_occlusion_in_content_space(occlusion);
        // Masks are used to draw the contributing surface, so should have
        // the same occlusion as the surface (nothing inside the surface
        // occludes them).
        if (LayerImpl* mask = it->render_surface()->MaskLayer()) {
          Occlusion mask_occlusion =
              inside_replica
                  ? Occlusion()
                  : occlusion_tracker.GetCurrentOcclusionForContributingSurface(
                        it->render_surface()->draw_transform() *
                        it->DrawTransform());
          mask->draw_properties().occlusion_in_content_space = mask_occlusion;
        }
        if (LayerImpl* replica_mask =
                it->render_surface()->ReplicaMaskLayer()) {
          replica_mask->draw_properties().occlusion_in_content_space =
              Occlusion();
        }
      }

      occlusion_tracker.LeaveLayer(it);
    }

    unoccluded_screen_space_region_ =
        occlusion_tracker.ComputeVisibleRegionInScreen(this);
  }

  // It'd be ideal if this could be done earlier, but when the raster source
  // is updated from the main thread during push properties, update draw
  // properties has not occurred yet and so it's not clear whether or not the
  // layer can or cannot use lcd text.  So, this is the cleanup pass to
  // determine if the raster source needs to be replaced with a non-lcd
  // raster source due to draw properties.
  if (update_lcd_text) {
    // TODO(enne): Make LTHI::sync_tree return this value.
    LayerTreeImpl* sync_tree = layer_tree_host_impl_->CommitToActiveTree()
                                   ? layer_tree_host_impl_->active_tree()
                                   : layer_tree_host_impl_->pending_tree();
    // If this is not the sync tree, then it is not safe to update lcd text
    // as it causes invalidations and the tiles may be in use.
    DCHECK_EQ(this, sync_tree);
    for (const auto& layer : picture_layers_)
      layer->UpdateCanUseLCDTextAfterCommit();
  }

  // Resourceless draw do not need tiles and should not affect existing tile
  // priorities.
  if (!is_in_resourceless_software_draw_mode()) {
    TRACE_EVENT_BEGIN2("cc", "LayerTreeImpl::UpdateDrawProperties::UpdateTiles",
                       "IsActive", IsActiveTree(), "SourceFrameNumber",
                       source_frame_number_);
    size_t layers_updated_count = 0;
    bool tile_priorities_updated = false;
    for (PictureLayerImpl* layer : picture_layers_) {
      if (!layer->is_drawn_render_surface_layer_list_member())
        continue;
      ++layers_updated_count;
      tile_priorities_updated |= layer->UpdateTiles();
    }

    if (tile_priorities_updated)
      DidModifyTilePriorities();

    TRACE_EVENT_END1("cc", "LayerTreeImpl::UpdateDrawProperties::UpdateTiles",
                     "layers_updated_count", layers_updated_count);
  }

  DCHECK(!needs_update_draw_properties_)
      << "CalcDrawProperties should not set_needs_update_draw_properties()";
  return true;
}

void LayerTreeImpl::BuildLayerListAndPropertyTreesForTesting() {
  BuildLayerListForTesting();
  BuildPropertyTreesForTesting();
}

void LayerTreeImpl::BuildPropertyTreesForTesting() {
  PropertyTreeBuilder::PreCalculateMetaInformationForTesting(layer_list_[0]);
  property_trees_.needs_rebuild = true;
  property_trees_.transform_tree.set_source_to_parent_updates_allowed(true);
  PropertyTreeBuilder::BuildPropertyTrees(
      layer_list_[0], PageScaleLayer(), InnerViewportScrollLayer(),
      OuterViewportScrollLayer(), OverscrollElasticityLayer(),
      elastic_overscroll()->Current(IsActiveTree()),
      current_page_scale_factor(), device_scale_factor(),
      gfx::Rect(DrawViewportSize()), layer_tree_host_impl_->DrawTransform(),
      &property_trees_);
  property_trees_.transform_tree.set_source_to_parent_updates_allowed(false);
}

const LayerImplList& LayerTreeImpl::RenderSurfaceLayerList() const {
  // If this assert triggers, then the list is dirty.
  DCHECK(!needs_update_draw_properties_);
  return render_surface_layer_list_;
}

const Region& LayerTreeImpl::UnoccludedScreenSpaceRegion() const {
  // If this assert triggers, then the render_surface_layer_list_ is dirty, so
  // the unoccluded_screen_space_region_ is not valid anymore.
  DCHECK(!needs_update_draw_properties_);
  return unoccluded_screen_space_region_;
}

gfx::SizeF LayerTreeImpl::ScrollableSize() const {
  LayerImpl* root_scroll_layer = OuterViewportScrollLayer()
                                     ? OuterViewportScrollLayer()
                                     : InnerViewportScrollLayer();
  if (!root_scroll_layer)
    return gfx::SizeF();

  gfx::SizeF content_size = root_scroll_layer->BoundsForScrolling();
  gfx::SizeF viewport_size =
      root_scroll_layer->scroll_clip_layer()->BoundsForScrolling();

  content_size.SetToMax(viewport_size);
  return content_size;
}

LayerImpl* LayerTreeImpl::LayerById(int id) const {
  LayerImplMap::const_iterator iter = layer_id_map_.find(id);
  return iter != layer_id_map_.end() ? iter->second : nullptr;
}

void LayerTreeImpl::AddLayerShouldPushProperties(LayerImpl* layer) {
  layers_that_should_push_properties_.insert(layer);
}

void LayerTreeImpl::RemoveLayerShouldPushProperties(LayerImpl* layer) {
  layers_that_should_push_properties_.erase(layer);
}

std::unordered_set<LayerImpl*>&
LayerTreeImpl::LayersThatShouldPushProperties() {
  return layers_that_should_push_properties_;
}

bool LayerTreeImpl::LayerNeedsPushPropertiesForTesting(LayerImpl* layer) {
  return layers_that_should_push_properties_.find(layer) !=
         layers_that_should_push_properties_.end();
}

void LayerTreeImpl::RegisterLayer(LayerImpl* layer) {
  DCHECK(!LayerById(layer->id()));
  layer_id_map_[layer->id()] = layer;
}

void LayerTreeImpl::UnregisterLayer(LayerImpl* layer) {
  DCHECK(LayerById(layer->id()));
  layers_that_should_push_properties_.erase(layer);
  transform_animations_map_.erase(layer->id());
  opacity_animations_map_.erase(layer->id());
  layer_id_map_.erase(layer->id());
}

// These manage ownership of the LayerImpl.
void LayerTreeImpl::AddLayer(std::unique_ptr<LayerImpl> layer) {
  DCHECK(std::find(layers_->begin(), layers_->end(), layer) == layers_->end());
  layers_->push_back(std::move(layer));
  set_needs_update_draw_properties();
}

std::unique_ptr<LayerImpl> LayerTreeImpl::RemoveLayer(int id) {
  for (auto it = layers_->begin(); it != layers_->end(); ++it) {
    if ((*it) && (*it)->id() != id)
      continue;
    std::unique_ptr<LayerImpl> ret = std::move(*it);
    set_needs_update_draw_properties();
    layers_->erase(it);
    return ret;
  }
  return nullptr;
}

size_t LayerTreeImpl::NumLayers() {
  return layer_id_map_.size();
}

void LayerTreeImpl::DidBecomeActive() {
  if (next_activation_forces_redraw_) {
    layer_tree_host_impl_->SetFullRootLayerDamage();
    next_activation_forces_redraw_ = false;
  }

  // Always reset this flag on activation, as we would only have activated
  // if we were in a good state.
  layer_tree_host_impl_->ResetRequiresHighResToDraw();

  if (!layer_list_.empty()) {
    LayerTreeHostCommon::CallFunctionForEveryLayer(
        this, [](LayerImpl* layer) { layer->DidBecomeActive(); });
  }

  for (const auto& swap_promise : swap_promise_list_)
    swap_promise->DidActivate();
  devtools_instrumentation::DidActivateLayerTree(layer_tree_host_impl_->id(),
                                                 source_frame_number_);
}

bool LayerTreeImpl::RequiresHighResToDraw() const {
  return layer_tree_host_impl_->RequiresHighResToDraw();
}

bool LayerTreeImpl::ViewportSizeInvalid() const {
  return viewport_size_invalid_;
}

void LayerTreeImpl::SetViewportSizeInvalid() {
  viewport_size_invalid_ = true;
  layer_tree_host_impl_->OnCanDrawStateChangedForTree();
}

void LayerTreeImpl::ResetViewportSizeInvalid() {
  viewport_size_invalid_ = false;
  layer_tree_host_impl_->OnCanDrawStateChangedForTree();
}

TaskRunnerProvider* LayerTreeImpl::task_runner_provider() const {
  return layer_tree_host_impl_->task_runner_provider();
}

const LayerTreeSettings& LayerTreeImpl::settings() const {
  return layer_tree_host_impl_->settings();
}

const LayerTreeDebugState& LayerTreeImpl::debug_state() const {
  return layer_tree_host_impl_->debug_state();
}

const RendererCapabilitiesImpl& LayerTreeImpl::GetRendererCapabilities() const {
  return layer_tree_host_impl_->GetRendererCapabilities();
}

ContextProvider* LayerTreeImpl::context_provider() const {
  return output_surface()->context_provider();
}

OutputSurface* LayerTreeImpl::output_surface() const {
  return layer_tree_host_impl_->output_surface();
}

ResourceProvider* LayerTreeImpl::resource_provider() const {
  return layer_tree_host_impl_->resource_provider();
}

TileManager* LayerTreeImpl::tile_manager() const {
  return layer_tree_host_impl_->tile_manager();
}

ImageDecodeController* LayerTreeImpl::image_decode_controller() const {
  return layer_tree_host_impl_->image_decode_controller();
}

FrameRateCounter* LayerTreeImpl::frame_rate_counter() const {
  return layer_tree_host_impl_->fps_counter();
}

MemoryHistory* LayerTreeImpl::memory_history() const {
  return layer_tree_host_impl_->memory_history();
}

gfx::Size LayerTreeImpl::device_viewport_size() const {
  return layer_tree_host_impl_->device_viewport_size();
}

DebugRectHistory* LayerTreeImpl::debug_rect_history() const {
  return layer_tree_host_impl_->debug_rect_history();
}

bool LayerTreeImpl::IsActiveTree() const {
  return layer_tree_host_impl_->active_tree() == this;
}

bool LayerTreeImpl::IsPendingTree() const {
  return layer_tree_host_impl_->pending_tree() == this;
}

bool LayerTreeImpl::IsRecycleTree() const {
  return layer_tree_host_impl_->recycle_tree() == this;
}

bool LayerTreeImpl::IsSyncTree() const {
  return layer_tree_host_impl_->sync_tree() == this;
}

LayerImpl* LayerTreeImpl::FindActiveTreeLayerById(int id) {
  LayerTreeImpl* tree = layer_tree_host_impl_->active_tree();
  if (!tree)
    return NULL;
  return tree->LayerById(id);
}

LayerImpl* LayerTreeImpl::FindPendingTreeLayerById(int id) {
  LayerTreeImpl* tree = layer_tree_host_impl_->pending_tree();
  if (!tree)
    return NULL;
  return tree->LayerById(id);
}

bool LayerTreeImpl::PinchGestureActive() const {
  return layer_tree_host_impl_->pinch_gesture_active();
}

BeginFrameArgs LayerTreeImpl::CurrentBeginFrameArgs() const {
  return layer_tree_host_impl_->CurrentBeginFrameArgs();
}

base::TimeDelta LayerTreeImpl::CurrentBeginFrameInterval() const {
  return layer_tree_host_impl_->CurrentBeginFrameInterval();
}

gfx::Rect LayerTreeImpl::DeviceViewport() const {
  return layer_tree_host_impl_->DeviceViewport();
}

gfx::Size LayerTreeImpl::DrawViewportSize() const {
  return layer_tree_host_impl_->DrawViewportSize();
}

const gfx::Rect LayerTreeImpl::ViewportRectForTilePriority() const {
  return layer_tree_host_impl_->ViewportRectForTilePriority();
}

std::unique_ptr<ScrollbarAnimationController>
LayerTreeImpl::CreateScrollbarAnimationController(int scroll_layer_id) {
  DCHECK(settings().scrollbar_fade_delay_ms);
  DCHECK(settings().scrollbar_fade_duration_ms);
  base::TimeDelta delay =
      base::TimeDelta::FromMilliseconds(settings().scrollbar_fade_delay_ms);
  base::TimeDelta resize_delay = base::TimeDelta::FromMilliseconds(
      settings().scrollbar_fade_resize_delay_ms);
  base::TimeDelta duration =
      base::TimeDelta::FromMilliseconds(settings().scrollbar_fade_duration_ms);
  switch (settings().scrollbar_animator) {
    case LayerTreeSettings::LINEAR_FADE: {
      return ScrollbarAnimationControllerLinearFade::Create(
          scroll_layer_id, layer_tree_host_impl_, delay, resize_delay,
          duration);
    }
    case LayerTreeSettings::THINNING: {
      return ScrollbarAnimationControllerThinning::Create(
          scroll_layer_id, layer_tree_host_impl_, delay, resize_delay,
          duration);
    }
    case LayerTreeSettings::NO_ANIMATOR:
      NOTREACHED();
      break;
  }
  return nullptr;
}

void LayerTreeImpl::DidAnimateScrollOffset() {
  layer_tree_host_impl_->DidAnimateScrollOffset();
}

bool LayerTreeImpl::use_gpu_rasterization() const {
  return layer_tree_host_impl_->use_gpu_rasterization();
}

GpuRasterizationStatus LayerTreeImpl::GetGpuRasterizationStatus() const {
  return layer_tree_host_impl_->gpu_rasterization_status();
}

bool LayerTreeImpl::create_low_res_tiling() const {
  return layer_tree_host_impl_->create_low_res_tiling();
}

void LayerTreeImpl::SetNeedsRedraw() {
  layer_tree_host_impl_->SetNeedsRedraw();
}

void LayerTreeImpl::GetAllPrioritizedTilesForTracing(
    std::vector<PrioritizedTile>* prioritized_tiles) const {
  LayerIterator end = LayerIterator::End(&render_surface_layer_list_);
  for (LayerIterator it = LayerIterator::Begin(&render_surface_layer_list_);
       it != end; ++it) {
    if (!it.represents_itself())
      continue;
    LayerImpl* layer_impl = *it;
    layer_impl->GetAllPrioritizedTilesForTracing(prioritized_tiles);
  }
}

void LayerTreeImpl::AsValueInto(base::trace_event::TracedValue* state) const {
  TracedValue::MakeDictIntoImplicitSnapshot(state, "cc::LayerTreeImpl", this);
  state->SetInteger("source_frame_number", source_frame_number_);

  state->BeginArray("render_surface_layer_list");
  LayerIterator end = LayerIterator::End(&render_surface_layer_list_);
  for (LayerIterator it = LayerIterator::Begin(&render_surface_layer_list_);
       it != end; ++it) {
    if (!it.represents_itself())
      continue;
    TracedValue::AppendIDRef(*it, state);
  }
  state->EndArray();

  state->BeginArray("swap_promise_trace_ids");
  for (const auto& swap_promise : swap_promise_list_)
    state->AppendDouble(swap_promise->TraceId());
  state->EndArray();

  state->BeginArray("pinned_swap_promise_trace_ids");
  for (const auto& swap_promise : pinned_swap_promise_list_)
    state->AppendDouble(swap_promise->TraceId());
  state->EndArray();

  state->BeginArray("layers");
  for (auto* layer : *this) {
    state->BeginDictionary();
    layer->AsValueInto(state);
    state->EndDictionary();
  }
  state->EndArray();
}

bool LayerTreeImpl::DistributeRootScrollOffset(
    const gfx::ScrollOffset& root_offset) {
  if (!InnerViewportScrollLayer())
    return false;

  DCHECK(OuterViewportScrollLayer());

  // If we get here, we have both inner/outer viewports, and need to distribute
  // the scroll offset between them.
  gfx::ScrollOffset inner_viewport_offset =
      InnerViewportScrollLayer()->CurrentScrollOffset();
  gfx::ScrollOffset outer_viewport_offset =
      OuterViewportScrollLayer()->CurrentScrollOffset();

  // It may be nothing has changed.
  DCHECK(inner_viewport_offset + outer_viewport_offset == TotalScrollOffset());
  if (inner_viewport_offset + outer_viewport_offset == root_offset)
    return false;

  gfx::ScrollOffset max_outer_viewport_scroll_offset =
      OuterViewportScrollLayer()->MaxScrollOffset();

  outer_viewport_offset = root_offset - inner_viewport_offset;
  outer_viewport_offset.SetToMin(max_outer_viewport_scroll_offset);
  outer_viewport_offset.SetToMax(gfx::ScrollOffset());

  OuterViewportScrollLayer()->SetCurrentScrollOffset(outer_viewport_offset);
  inner_viewport_offset = root_offset - outer_viewport_offset;
  InnerViewportScrollLayer()->SetCurrentScrollOffset(inner_viewport_offset);
  return true;
}

void LayerTreeImpl::QueueSwapPromise(
    std::unique_ptr<SwapPromise> swap_promise) {
  DCHECK(swap_promise);
  swap_promise_list_.push_back(std::move(swap_promise));
}

void LayerTreeImpl::QueuePinnedSwapPromise(
    std::unique_ptr<SwapPromise> swap_promise) {
  DCHECK(IsActiveTree());
  DCHECK(swap_promise);
  pinned_swap_promise_list_.push_back(std::move(swap_promise));
}

void LayerTreeImpl::PassSwapPromises(
    std::vector<std::unique_ptr<SwapPromise>>* new_swap_promise) {
  for (const auto& swap_promise : swap_promise_list_)
    swap_promise->DidNotSwap(SwapPromise::SWAP_FAILS);
  swap_promise_list_.clear();
  swap_promise_list_.swap(*new_swap_promise);
}

void LayerTreeImpl::FinishSwapPromises(CompositorFrameMetadata* metadata) {
  for (const auto& swap_promise : swap_promise_list_)
    swap_promise->DidSwap(metadata);
  swap_promise_list_.clear();
  for (const auto& swap_promise : pinned_swap_promise_list_)
    swap_promise->DidSwap(metadata);
  pinned_swap_promise_list_.clear();
}

void LayerTreeImpl::BreakSwapPromises(SwapPromise::DidNotSwapReason reason) {
  for (const auto& swap_promise : swap_promise_list_)
    swap_promise->DidNotSwap(reason);
  swap_promise_list_.clear();
  for (const auto& swap_promise : pinned_swap_promise_list_)
    swap_promise->DidNotSwap(reason);
  pinned_swap_promise_list_.clear();
}

void LayerTreeImpl::DidModifyTilePriorities() {
  layer_tree_host_impl_->DidModifyTilePriorities();
}

void LayerTreeImpl::set_ui_resource_request_queue(
    const UIResourceRequestQueue& queue) {
  ui_resource_request_queue_ = queue;
}

ResourceId LayerTreeImpl::ResourceIdForUIResource(UIResourceId uid) const {
  return layer_tree_host_impl_->ResourceIdForUIResource(uid);
}

bool LayerTreeImpl::IsUIResourceOpaque(UIResourceId uid) const {
  return layer_tree_host_impl_->IsUIResourceOpaque(uid);
}

void LayerTreeImpl::ProcessUIResourceRequestQueue() {
  for (const auto& req : ui_resource_request_queue_) {
    switch (req.GetType()) {
      case UIResourceRequest::UI_RESOURCE_CREATE:
        layer_tree_host_impl_->CreateUIResource(req.GetId(), req.GetBitmap());
        break;
      case UIResourceRequest::UI_RESOURCE_DELETE:
        layer_tree_host_impl_->DeleteUIResource(req.GetId());
        break;
      case UIResourceRequest::UI_RESOURCE_INVALID_REQUEST:
        NOTREACHED();
        break;
    }
  }
  ui_resource_request_queue_.clear();

  // If all UI resource evictions were not recreated by processing this queue,
  // then another commit is required.
  if (layer_tree_host_impl_->EvictedUIResourcesExist())
    layer_tree_host_impl_->SetNeedsCommit();
}

void LayerTreeImpl::RegisterPictureLayerImpl(PictureLayerImpl* layer) {
  DCHECK(std::find(picture_layers_.begin(), picture_layers_.end(), layer) ==
         picture_layers_.end());
  picture_layers_.push_back(layer);
}

void LayerTreeImpl::UnregisterPictureLayerImpl(PictureLayerImpl* layer) {
  std::vector<PictureLayerImpl*>::iterator it =
      std::find(picture_layers_.begin(), picture_layers_.end(), layer);
  DCHECK(it != picture_layers_.end());
  picture_layers_.erase(it);
}

void LayerTreeImpl::RegisterScrollbar(ScrollbarLayerImplBase* scrollbar_layer) {
  if (scrollbar_layer->ScrollLayerId() == Layer::INVALID_ID)
    return;

  scrollbar_map_.insert(std::pair<int, int>(scrollbar_layer->ScrollLayerId(),
                                            scrollbar_layer->id()));
  if (IsActiveTree() && scrollbar_layer->is_overlay_scrollbar())
    layer_tree_host_impl_->RegisterScrollbarAnimationController(
        scrollbar_layer->ScrollLayerId());

  DidUpdateScrollState(scrollbar_layer->ScrollLayerId());
}

void LayerTreeImpl::UnregisterScrollbar(
    ScrollbarLayerImplBase* scrollbar_layer) {
  int scroll_layer_id = scrollbar_layer->ScrollLayerId();
  if (scroll_layer_id == Layer::INVALID_ID)
    return;

  auto scrollbar_range = scrollbar_map_.equal_range(scroll_layer_id);
  for (auto i = scrollbar_range.first; i != scrollbar_range.second; ++i)
    if (i->second == scrollbar_layer->id()) {
      scrollbar_map_.erase(i);
      break;
    }

  if (IsActiveTree() && scrollbar_map_.count(scroll_layer_id) == 0)
    layer_tree_host_impl_->UnregisterScrollbarAnimationController(
        scroll_layer_id);
}

ScrollbarSet LayerTreeImpl::ScrollbarsFor(int scroll_layer_id) const {
  ScrollbarSet scrollbars;
  auto scrollbar_range = scrollbar_map_.equal_range(scroll_layer_id);
  for (auto i = scrollbar_range.first; i != scrollbar_range.second; ++i)
    scrollbars.insert(LayerById(i->second)->ToScrollbarLayer());
  return scrollbars;
}

void LayerTreeImpl::RegisterScrollLayer(LayerImpl* layer) {
  if (layer->scroll_clip_layer_id() == Layer::INVALID_ID)
    return;

  clip_scroll_map_.insert(
      std::pair<int, int>(layer->scroll_clip_layer_id(), layer->id()));

  DidUpdateScrollState(layer->id());
}

void LayerTreeImpl::UnregisterScrollLayer(LayerImpl* layer) {
  if (layer->scroll_clip_layer_id() == Layer::INVALID_ID)
    return;

  clip_scroll_map_.erase(layer->scroll_clip_layer_id());
}

void LayerTreeImpl::AddSurfaceLayer(LayerImpl* layer) {
  DCHECK(std::find(surface_layers_.begin(), surface_layers_.end(), layer) ==
         surface_layers_.end());
  surface_layers_.push_back(layer);
}

void LayerTreeImpl::RemoveSurfaceLayer(LayerImpl* layer) {
  LayerImplList::iterator it =
      std::find(surface_layers_.begin(), surface_layers_.end(), layer);
  DCHECK(it != surface_layers_.end());
  surface_layers_.erase(it);
}

template <typename LayerType>
static inline bool LayerClipsSubtree(LayerType* layer) {
  return layer->masks_to_bounds() || layer->mask_layer();
}

static bool PointHitsRect(
    const gfx::PointF& screen_space_point,
    const gfx::Transform& local_space_to_screen_space_transform,
    const gfx::Rect& local_space_rect,
    float* distance_to_camera) {
  // If the transform is not invertible, then assume that this point doesn't hit
  // this rect.
  gfx::Transform inverse_local_space_to_screen_space(
      gfx::Transform::kSkipInitialization);
  if (!local_space_to_screen_space_transform.GetInverse(
          &inverse_local_space_to_screen_space))
    return false;

  // Transform the hit test point from screen space to the local space of the
  // given rect.
  bool clipped = false;
  gfx::Point3F planar_point = MathUtil::ProjectPoint3D(
      inverse_local_space_to_screen_space, screen_space_point, &clipped);
  gfx::PointF hit_test_point_in_local_space =
      gfx::PointF(planar_point.x(), planar_point.y());

  // If ProjectPoint could not project to a valid value, then we assume that
  // this point doesn't hit this rect.
  if (clipped)
    return false;

  if (!gfx::RectF(local_space_rect).Contains(hit_test_point_in_local_space))
    return false;

  if (distance_to_camera) {
    // To compute the distance to the camera, we have to take the planar point
    // and pull it back to world space and compute the displacement along the
    // z-axis.
    gfx::Point3F planar_point_in_screen_space(planar_point);
    local_space_to_screen_space_transform.TransformPoint(
        &planar_point_in_screen_space);
    *distance_to_camera = planar_point_in_screen_space.z();
  }

  return true;
}

static bool PointHitsRegion(const gfx::PointF& screen_space_point,
                            const gfx::Transform& screen_space_transform,
                            const Region& layer_space_region) {
  // If the transform is not invertible, then assume that this point doesn't hit
  // this region.
  gfx::Transform inverse_screen_space_transform(
      gfx::Transform::kSkipInitialization);
  if (!screen_space_transform.GetInverse(&inverse_screen_space_transform))
    return false;

  // Transform the hit test point from screen space to the local space of the
  // given region.
  bool clipped = false;
  gfx::PointF hit_test_point_in_layer_space = MathUtil::ProjectPoint(
      inverse_screen_space_transform, screen_space_point, &clipped);

  // If ProjectPoint could not project to a valid value, then we assume that
  // this point doesn't hit this region.
  if (clipped)
    return false;

  return layer_space_region.Contains(
      gfx::ToRoundedPoint(hit_test_point_in_layer_space));
}

static const gfx::Transform SurfaceScreenSpaceTransform(
    const LayerImpl* layer,
    const TransformTree& transform_tree) {
  DCHECK(layer->render_surface());
  return layer->is_drawn_render_surface_layer_list_member()
             ? layer->render_surface()->screen_space_transform()
             : transform_tree.ToScreenSpaceTransformWithoutSublayerScale(
                   layer->render_surface()->TransformTreeIndex());
}

static bool PointIsClippedByAncestorClipNode(
    const gfx::PointF& screen_space_point,
    const LayerImpl* layer,
    const ClipTree& clip_tree,
    const TransformTree& transform_tree) {
  // We need to visit all ancestor clip nodes to check this. Checking with just
  // the combined clip stored at a clip node is not enough because parent
  // combined clip can sometimes be smaller than current combined clip. This can
  // happen when we have transforms like rotation that inflate the combined
  // clip's bounds. Also, the point can be clipped by the content rect of an
  // ancestor render surface.

  // We first check if the point is clipped by viewport.
  const ClipNode* clip_node = clip_tree.Node(1);
  gfx::Rect combined_clip_in_target_space =
      gfx::ToEnclosingRect(clip_node->data.combined_clip_in_target_space);
  if (!PointHitsRect(screen_space_point, gfx::Transform(),
                     combined_clip_in_target_space, NULL))
    return true;

  for (const ClipNode* clip_node = clip_tree.Node(layer->clip_tree_index());
       clip_node->id > 1; clip_node = clip_tree.parent(clip_node)) {
    if (clip_node->data.applies_local_clip) {
      const TransformNode* transform_node =
          transform_tree.Node(clip_node->data.target_id);
      gfx::Rect combined_clip_in_target_space =
          gfx::ToEnclosingRect(clip_node->data.combined_clip_in_target_space);

      const LayerImpl* target_layer =
          layer->layer_tree_impl()->LayerById(transform_node->owner_id);
      DCHECK(transform_node->id == 0 || target_layer->render_surface() ||
             layer->layer_tree_impl()->is_in_resourceless_software_draw_mode());
      gfx::Transform surface_screen_space_transform =
          transform_node->id == 0 ||
                  (layer->layer_tree_impl()
                       ->is_in_resourceless_software_draw_mode())
              ? gfx::Transform()
              : SurfaceScreenSpaceTransform(target_layer, transform_tree);
      if (!PointHitsRect(screen_space_point, surface_screen_space_transform,
                         combined_clip_in_target_space, NULL)) {
        return true;
      }
    }
    const LayerImpl* clip_node_owner =
        layer->layer_tree_impl()->LayerById(clip_node->owner_id);
    if (clip_node_owner->render_surface() &&
        !PointHitsRect(
            screen_space_point,
            SurfaceScreenSpaceTransform(clip_node_owner, transform_tree),
            clip_node_owner->render_surface()->content_rect(), NULL)) {
      return true;
    }
  }
  return false;
}

static bool PointIsClippedBySurfaceOrClipRect(
    const gfx::PointF& screen_space_point,
    const LayerImpl* layer,
    const TransformTree& transform_tree,
    const ClipTree& clip_tree) {
  // Walk up the layer tree and hit-test any render_surfaces and any layer
  // clip rects that are active.
  return PointIsClippedByAncestorClipNode(screen_space_point, layer, clip_tree,
                                          transform_tree);
}

static bool PointHitsLayer(const LayerImpl* layer,
                           const gfx::PointF& screen_space_point,
                           float* distance_to_intersection,
                           const TransformTree& transform_tree,
                           const ClipTree& clip_tree) {
  gfx::Rect content_rect(layer->bounds());
  if (!PointHitsRect(screen_space_point, layer->ScreenSpaceTransform(),
                     content_rect, distance_to_intersection)) {
    return false;
  }

  // At this point, we think the point does hit the layer, but we need to walk
  // up the parents to ensure that the layer was not clipped in such a way
  // that the hit point actually should not hit the layer.
  if (PointIsClippedBySurfaceOrClipRect(screen_space_point, layer,
                                        transform_tree, clip_tree))
    return false;

  // Skip the HUD layer.
  if (layer == layer->layer_tree_impl()->hud_layer())
    return false;

  return true;
}

struct FindClosestMatchingLayerState {
  FindClosestMatchingLayerState()
      : closest_match(NULL),
        closest_distance(-std::numeric_limits<float>::infinity()) {}
  LayerImpl* closest_match;
  // Note that the positive z-axis points towards the camera, so bigger means
  // closer in this case, counterintuitively.
  float closest_distance;
};

template <typename Functor>
static void FindClosestMatchingLayer(const gfx::PointF& screen_space_point,
                                     LayerImpl* root_layer,
                                     const Functor& func,
                                     const TransformTree& transform_tree,
                                     const ClipTree& clip_tree,
                                     FindClosestMatchingLayerState* state) {
  // We want to iterate from front to back when hit testing.
  for (auto* layer : base::Reversed(*root_layer->layer_tree_impl())) {
    if (!func(layer))
      continue;

    float distance_to_intersection = 0.f;
    bool hit = false;
    if (layer->Is3dSorted())
      hit = PointHitsLayer(layer, screen_space_point, &distance_to_intersection,
                           transform_tree, clip_tree);
    else
      hit = PointHitsLayer(layer, screen_space_point, nullptr, transform_tree,
                           clip_tree);

    if (!hit)
      continue;

    bool in_front_of_previous_candidate =
        state->closest_match &&
        layer->sorting_context_id() ==
            state->closest_match->sorting_context_id() &&
        distance_to_intersection >
            state->closest_distance + std::numeric_limits<float>::epsilon();

    if (!state->closest_match || in_front_of_previous_candidate) {
      state->closest_distance = distance_to_intersection;
      state->closest_match = layer;
    }
  }
}

static bool ScrollsOrScrollbarAnyDrawnRenderSurfaceLayerListMember(
    LayerImpl* layer) {
  return layer->scrolls_drawn_descendant() ||
         (layer->ToScrollbarLayer() &&
          layer->is_drawn_render_surface_layer_list_member());
}

struct FindScrollingLayerOrScrollbarLayerFunctor {
  bool operator()(LayerImpl* layer) const {
    return ScrollsOrScrollbarAnyDrawnRenderSurfaceLayerListMember(layer);
  }
};

LayerImpl*
LayerTreeImpl::FindFirstScrollingLayerOrScrollbarLayerThatIsHitByPoint(
    const gfx::PointF& screen_space_point) {
  FindClosestMatchingLayerState state;
  LayerImpl* root_layer = layer_list_.empty() ? nullptr : layer_list_[0];
  FindClosestMatchingLayer(screen_space_point, root_layer,
                           FindScrollingLayerOrScrollbarLayerFunctor(),
                           property_trees_.transform_tree,
                           property_trees_.clip_tree, &state);
  return state.closest_match;
}

struct HitTestVisibleScrollableOrTouchableFunctor {
  bool operator()(LayerImpl* layer) const {
    return layer->is_drawn_render_surface_layer_list_member() ||
           ScrollsOrScrollbarAnyDrawnRenderSurfaceLayerListMember(layer) ||
           !layer->touch_event_handler_region().IsEmpty();
  }
};

LayerImpl* LayerTreeImpl::FindLayerThatIsHitByPoint(
    const gfx::PointF& screen_space_point) {
  if (layer_list_.empty())
    return NULL;
  bool update_lcd_text = false;
  if (!UpdateDrawProperties(update_lcd_text))
    return NULL;
  FindClosestMatchingLayerState state;
  FindClosestMatchingLayer(screen_space_point, layer_list_[0],
                           HitTestVisibleScrollableOrTouchableFunctor(),
                           property_trees_.transform_tree,
                           property_trees_.clip_tree, &state);
  return state.closest_match;
}

static bool LayerHasTouchEventHandlersAt(const gfx::PointF& screen_space_point,
                                         LayerImpl* layer_impl,
                                         const TransformTree& transform_tree,
                                         const ClipTree& clip_tree) {
  if (layer_impl->touch_event_handler_region().IsEmpty())
    return false;

  if (!PointHitsRegion(screen_space_point, layer_impl->ScreenSpaceTransform(),
                       layer_impl->touch_event_handler_region()))
    return false;

  // At this point, we think the point does hit the touch event handler region
  // on the layer, but we need to walk up the parents to ensure that the layer
  // was not clipped in such a way that the hit point actually should not hit
  // the layer.
  if (PointIsClippedBySurfaceOrClipRect(screen_space_point, layer_impl,
                                        transform_tree, clip_tree))
    return false;

  return true;
}

struct FindTouchEventLayerFunctor {
  bool operator()(LayerImpl* layer) const {
    return LayerHasTouchEventHandlersAt(screen_space_point, layer,
                                        transform_tree, clip_tree);
  }
  const gfx::PointF screen_space_point;
  const TransformTree& transform_tree;
  const ClipTree& clip_tree;
};

LayerImpl* LayerTreeImpl::FindLayerThatIsHitByPointInTouchHandlerRegion(
    const gfx::PointF& screen_space_point) {
  if (layer_list_.empty())
    return NULL;
  bool update_lcd_text = false;
  if (!UpdateDrawProperties(update_lcd_text))
    return NULL;
  FindTouchEventLayerFunctor func = {screen_space_point,
                                     property_trees_.transform_tree,
                                     property_trees_.clip_tree};
  FindClosestMatchingLayerState state;
  FindClosestMatchingLayer(screen_space_point, layer_list_[0], func,
                           property_trees_.transform_tree,
                           property_trees_.clip_tree, &state);
  return state.closest_match;
}

void LayerTreeImpl::RegisterSelection(const LayerSelection& selection) {
  selection_ = selection;
}

static gfx::SelectionBound ComputeViewportSelectionBound(
    const LayerSelectionBound& layer_bound,
    LayerImpl* layer,
    float device_scale_factor,
    const TransformTree& transform_tree,
    const ClipTree& clip_tree) {
  gfx::SelectionBound viewport_bound;
  viewport_bound.set_type(layer_bound.type);

  if (!layer || layer_bound.type == gfx::SelectionBound::EMPTY)
    return viewport_bound;

  auto layer_top = gfx::PointF(layer_bound.edge_top);
  auto layer_bottom = gfx::PointF(layer_bound.edge_bottom);
  gfx::Transform screen_space_transform = layer->ScreenSpaceTransform();

  bool clipped = false;
  gfx::PointF screen_top =
      MathUtil::MapPoint(screen_space_transform, layer_top, &clipped);
  gfx::PointF screen_bottom =
      MathUtil::MapPoint(screen_space_transform, layer_bottom, &clipped);

  // MapPoint can produce points with NaN components (even when no inputs are
  // NaN). Since consumers of gfx::SelectionBounds may round |edge_top| or
  // |edge_bottom| (and since rounding will crash on NaN), we return an empty
  // bound instead.
  if (std::isnan(screen_top.x()) || std::isnan(screen_top.y()) ||
      std::isnan(screen_bottom.x()) || std::isnan(screen_bottom.y()))
    return gfx::SelectionBound();

  const float inv_scale = 1.f / device_scale_factor;
  viewport_bound.SetEdgeTop(gfx::ScalePoint(screen_top, inv_scale));
  viewport_bound.SetEdgeBottom(gfx::ScalePoint(screen_bottom, inv_scale));

  // The bottom edge point is used for visibility testing as it is the logical
  // focal point for bound selection handles (this may change in the future).
  // Shifting the visibility point fractionally inward ensures that neighboring
  // or logically coincident layers aligned to integral DPI coordinates will not
  // spuriously occlude the bound.
  gfx::Vector2dF visibility_offset = layer_top - layer_bottom;
  visibility_offset.Scale(device_scale_factor / visibility_offset.Length());
  gfx::PointF visibility_point = layer_bottom + visibility_offset;
  if (visibility_point.x() <= 0)
    visibility_point.set_x(visibility_point.x() + device_scale_factor);
  visibility_point =
      MathUtil::MapPoint(screen_space_transform, visibility_point, &clipped);

  float intersect_distance = 0.f;
  viewport_bound.set_visible(PointHitsLayer(
      layer, visibility_point, &intersect_distance, transform_tree, clip_tree));

  return viewport_bound;
}

void LayerTreeImpl::GetViewportSelection(
    Selection<gfx::SelectionBound>* selection) {
  DCHECK(selection);

  selection->start = ComputeViewportSelectionBound(
      selection_.start,
      selection_.start.layer_id ? LayerById(selection_.start.layer_id) : NULL,
      device_scale_factor(), property_trees_.transform_tree,
      property_trees_.clip_tree);
  selection->is_editable = selection_.is_editable;
  selection->is_empty_text_form_control = selection_.is_empty_text_form_control;
  if (selection->start.type() == gfx::SelectionBound::CENTER ||
      selection->start.type() == gfx::SelectionBound::EMPTY) {
    selection->end = selection->start;
  } else {
    selection->end = ComputeViewportSelectionBound(
        selection_.end,
        selection_.end.layer_id ? LayerById(selection_.end.layer_id) : NULL,
        device_scale_factor(), property_trees_.transform_tree,
        property_trees_.clip_tree);
  }
}

bool LayerTreeImpl::SmoothnessTakesPriority() const {
  return layer_tree_host_impl_->GetTreePriority() == SMOOTHNESS_TAKES_PRIORITY;
}

VideoFrameControllerClient* LayerTreeImpl::GetVideoFrameControllerClient()
    const {
  return layer_tree_host_impl_;
}

void LayerTreeImpl::SetPendingPageScaleAnimation(
    std::unique_ptr<PendingPageScaleAnimation> pending_animation) {
  pending_page_scale_animation_ = std::move(pending_animation);
}

std::unique_ptr<PendingPageScaleAnimation>
LayerTreeImpl::TakePendingPageScaleAnimation() {
  return std::move(pending_page_scale_animation_);
}

bool LayerTreeImpl::IsAnimatingFilterProperty(const LayerImpl* layer) const {
  ElementListType list_type =
      IsActiveTree() ? ElementListType::ACTIVE : ElementListType::PENDING;
  return layer_tree_host_impl_->animation_host()->IsAnimatingFilterProperty(
      layer->element_id(), list_type);
}

bool LayerTreeImpl::IsAnimatingOpacityProperty(const LayerImpl* layer) const {
  ElementListType list_type =
      IsActiveTree() ? ElementListType::ACTIVE : ElementListType::PENDING;
  return layer_tree_host_impl_->animation_host()->IsAnimatingOpacityProperty(
      layer->element_id(), list_type);
}

bool LayerTreeImpl::IsAnimatingTransformProperty(const LayerImpl* layer) const {
  ElementListType list_type =
      IsActiveTree() ? ElementListType::ACTIVE : ElementListType::PENDING;
  return layer_tree_host_impl_->animation_host()->IsAnimatingTransformProperty(
      layer->element_id(), list_type);
}

bool LayerTreeImpl::HasPotentiallyRunningFilterAnimation(
    const LayerImpl* layer) const {
  ElementListType list_type =
      IsActiveTree() ? ElementListType::ACTIVE : ElementListType::PENDING;
  return layer_tree_host_impl_->animation_host()
      ->HasPotentiallyRunningFilterAnimation(layer->element_id(), list_type);
}

bool LayerTreeImpl::HasPotentiallyRunningOpacityAnimation(
    const LayerImpl* layer) const {
  ElementListType list_type =
      IsActiveTree() ? ElementListType::ACTIVE : ElementListType::PENDING;
  return layer_tree_host_impl_->animation_host()
      ->HasPotentiallyRunningOpacityAnimation(layer->element_id(), list_type);
}

bool LayerTreeImpl::HasPotentiallyRunningTransformAnimation(
    const LayerImpl* layer) const {
  ElementListType list_type =
      IsActiveTree() ? ElementListType::ACTIVE : ElementListType::PENDING;
  return layer_tree_host_impl_->animation_host()
      ->HasPotentiallyRunningTransformAnimation(layer->element_id(), list_type);
}

bool LayerTreeImpl::HasAnyAnimationTargetingProperty(
    const LayerImpl* layer,
    TargetProperty::Type property) const {
  return layer_tree_host_impl_->animation_host()
      ->HasAnyAnimationTargetingProperty(layer->element_id(), property);
}

bool LayerTreeImpl::AnimationsPreserveAxisAlignment(
    const LayerImpl* layer) const {
  return layer_tree_host_impl_->animation_host()
      ->AnimationsPreserveAxisAlignment(layer->element_id());
}

bool LayerTreeImpl::HasOnlyTranslationTransforms(const LayerImpl* layer) const {
  ElementListType list_type =
      IsActiveTree() ? ElementListType::ACTIVE : ElementListType::PENDING;
  return layer_tree_host_impl_->animation_host()->HasOnlyTranslationTransforms(
      layer->element_id(), list_type);
}

bool LayerTreeImpl::MaximumTargetScale(const LayerImpl* layer,
                                       float* max_scale) const {
  *max_scale = 0.f;
  ElementListType list_type =
      IsActiveTree() ? ElementListType::ACTIVE : ElementListType::PENDING;
  return layer_tree_host_impl_->animation_host()->MaximumTargetScale(
      layer->element_id(), list_type, max_scale);
}

bool LayerTreeImpl::AnimationStartScale(const LayerImpl* layer,
                                        float* start_scale) const {
  *start_scale = 0.f;
  ElementListType list_type =
      IsActiveTree() ? ElementListType::ACTIVE : ElementListType::PENDING;
  return layer_tree_host_impl_->animation_host()->AnimationStartScale(
      layer->element_id(), list_type, start_scale);
}

bool LayerTreeImpl::HasFilterAnimationThatInflatesBounds(
    const LayerImpl* layer) const {
  return layer_tree_host_impl_->animation_host()
      ->HasFilterAnimationThatInflatesBounds(layer->element_id());
}

bool LayerTreeImpl::HasTransformAnimationThatInflatesBounds(
    const LayerImpl* layer) const {
  return layer_tree_host_impl_->animation_host()
      ->HasTransformAnimationThatInflatesBounds(layer->element_id());
}

bool LayerTreeImpl::HasAnimationThatInflatesBounds(
    const LayerImpl* layer) const {
  return layer_tree_host_impl_->animation_host()
      ->HasAnimationThatInflatesBounds(layer->element_id());
}

bool LayerTreeImpl::FilterAnimationBoundsForBox(const LayerImpl* layer,
                                                const gfx::BoxF& box,
                                                gfx::BoxF* bounds) const {
  return layer_tree_host_impl_->animation_host()->FilterAnimationBoundsForBox(
      layer->element_id(), box, bounds);
}

bool LayerTreeImpl::TransformAnimationBoundsForBox(const LayerImpl* layer,
                                                   const gfx::BoxF& box,
                                                   gfx::BoxF* bounds) const {
  *bounds = gfx::BoxF();
  return layer_tree_host_impl_->animation_host()
      ->TransformAnimationBoundsForBox(layer->element_id(), box, bounds);
}

void LayerTreeImpl::ScrollAnimationAbort(bool needs_completion) {
  layer_tree_host_impl_->animation_host()->ScrollAnimationAbort(
      needs_completion);
}

void LayerTreeImpl::ResetAllChangeTracking() {
  layers_that_should_push_properties_.clear();
  // Iterate over all layers, including masks and replicas.
  for (auto& layer : *layers_)
    layer->ResetChangeTracking();
  property_trees_.ResetAllChangeTracking();
}

}  // namespace cc
