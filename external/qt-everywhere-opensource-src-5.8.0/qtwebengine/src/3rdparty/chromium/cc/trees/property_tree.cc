// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <set>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/base/math_util.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/scroll_state.h"
#include "cc/layers/layer_impl.h"
#include "cc/output/copy_output_request.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/property_tree.pb.h"
#include "cc/proto/scroll_offset.pb.h"
#include "cc/proto/synced_property_conversions.h"
#include "cc/proto/transform.pb.h"
#include "cc/proto/vector2df.pb.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {

template <typename T>
bool TreeNode<T>::operator==(const TreeNode<T>& other) const {
  return id == other.id && parent_id == other.parent_id &&
         owner_id == other.owner_id && data == other.data;
}

template <typename T>
void TreeNode<T>::ToProtobuf(proto::TreeNode* proto) const {
  proto->set_id(id);
  proto->set_parent_id(parent_id);
  proto->set_owner_id(owner_id);
  data.ToProtobuf(proto);
}

template <typename T>
void TreeNode<T>::FromProtobuf(const proto::TreeNode& proto) {
  id = proto.id();
  parent_id = proto.parent_id();
  owner_id = proto.owner_id();
  data.FromProtobuf(proto);
}

template <typename T>
void TreeNode<T>::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  value->SetInteger("owner_id", owner_id);
  data.AsValueInto(value);
}

template struct TreeNode<TransformNodeData>;
template struct TreeNode<ClipNodeData>;
template struct TreeNode<EffectNodeData>;
template struct TreeNode<ScrollNodeData>;

template <typename T>
PropertyTree<T>::PropertyTree()
    : needs_update_(false) {
  nodes_.push_back(T());
  back()->id = 0;
  back()->parent_id = -1;
}

template <typename T>
PropertyTree<T>::~PropertyTree() {
}

TransformTree::TransformTree()
    : source_to_parent_updates_allowed_(true),
      page_scale_factor_(1.f),
      device_scale_factor_(1.f),
      device_transform_scale_factor_(1.f) {
  cached_data_.push_back(TransformCachedNodeData());
}

TransformTree::~TransformTree() {
}

template <typename T>
int PropertyTree<T>::Insert(const T& tree_node, int parent_id) {
  DCHECK_GT(nodes_.size(), 0u);
  nodes_.push_back(tree_node);
  T& node = nodes_.back();
  node.parent_id = parent_id;
  node.id = static_cast<int>(nodes_.size()) - 1;
  return node.id;
}

template <typename T>
void PropertyTree<T>::clear() {
  nodes_.clear();
  nodes_.push_back(T());
  back()->id = 0;
  back()->parent_id = -1;
}

template <typename T>
bool PropertyTree<T>::operator==(const PropertyTree<T>& other) const {
  return nodes_ == other.nodes() && needs_update_ == other.needs_update();
}

template <typename T>
void PropertyTree<T>::ToProtobuf(proto::PropertyTree* proto) const {
  DCHECK_EQ(0, proto->nodes_size());
  for (const auto& node : nodes_)
    node.ToProtobuf(proto->add_nodes());
  proto->set_needs_update(needs_update_);
}

template <typename T>
void PropertyTree<T>::FromProtobuf(
    const proto::PropertyTree& proto,
    std::unordered_map<int, int>* node_id_to_index_map) {
  // Verify that the property tree is empty.
  DCHECK_EQ(static_cast<int>(nodes_.size()), 1);
  DCHECK_EQ(back()->id, 0);
  DCHECK_EQ(back()->parent_id, -1);

  // Add the first node.
  DCHECK_GT(proto.nodes_size(), 0);
  nodes_.back().FromProtobuf(proto.nodes(0));

  DCHECK(!node_id_to_index_map || (*node_id_to_index_map).empty());
  for (int i = 1; i < proto.nodes_size(); ++i) {
    nodes_.push_back(T());
    nodes_.back().FromProtobuf(proto.nodes(i));
    (*node_id_to_index_map)[nodes_.back().owner_id] = nodes_.back().id;
  }

  needs_update_ = proto.needs_update();
}

template <typename T>
void PropertyTree<T>::AsValueInto(base::trace_event::TracedValue* value) const {
  value->BeginArray("nodes");
  for (const auto& node : nodes_) {
    value->BeginDictionary();
    node.AsValueInto(value);
    value->EndDictionary();
  }
  value->EndArray();
}

template class PropertyTree<TransformNode>;
template class PropertyTree<ClipNode>;
template class PropertyTree<EffectNode>;
template class PropertyTree<ScrollNode>;

TransformNodeData::TransformNodeData()
    : source_node_id(-1),
      sorting_context_id(0),
      needs_local_transform_update(true),
      node_and_ancestors_are_animated_or_invertible(true),
      is_invertible(true),
      ancestors_are_invertible(true),
      has_potential_animation(false),
      is_currently_animating(false),
      to_screen_is_potentially_animated(false),
      has_only_translation_animations(true),
      flattens_inherited_transform(false),
      node_and_ancestors_are_flat(true),
      node_and_ancestors_have_only_integer_translation(true),
      scrolls(false),
      needs_sublayer_scale(false),
      affected_by_inner_viewport_bounds_delta_x(false),
      affected_by_inner_viewport_bounds_delta_y(false),
      affected_by_outer_viewport_bounds_delta_x(false),
      affected_by_outer_viewport_bounds_delta_y(false),
      in_subtree_of_page_scale_layer(false),
      transform_changed(false),
      post_local_scale_factor(1.0f) {}

TransformNodeData::TransformNodeData(const TransformNodeData& other) = default;

TransformNodeData::~TransformNodeData() {
}

bool TransformNodeData::operator==(const TransformNodeData& other) const {
  return pre_local == other.pre_local && local == other.local &&
         post_local == other.post_local && to_parent == other.to_parent &&
         source_node_id == other.source_node_id &&
         sorting_context_id == other.sorting_context_id &&
         needs_local_transform_update == other.needs_local_transform_update &&
         node_and_ancestors_are_animated_or_invertible ==
             other.node_and_ancestors_are_animated_or_invertible &&
         is_invertible == other.is_invertible &&
         ancestors_are_invertible == other.ancestors_are_invertible &&
         has_potential_animation == other.has_potential_animation &&
         is_currently_animating == other.is_currently_animating &&
         to_screen_is_potentially_animated ==
             other.to_screen_is_potentially_animated &&
         has_only_translation_animations ==
             other.has_only_translation_animations &&
         flattens_inherited_transform == other.flattens_inherited_transform &&
         node_and_ancestors_are_flat == other.node_and_ancestors_are_flat &&
         node_and_ancestors_have_only_integer_translation ==
             other.node_and_ancestors_have_only_integer_translation &&
         scrolls == other.scrolls &&
         needs_sublayer_scale == other.needs_sublayer_scale &&
         affected_by_inner_viewport_bounds_delta_x ==
             other.affected_by_inner_viewport_bounds_delta_x &&
         affected_by_inner_viewport_bounds_delta_y ==
             other.affected_by_inner_viewport_bounds_delta_y &&
         affected_by_outer_viewport_bounds_delta_x ==
             other.affected_by_outer_viewport_bounds_delta_x &&
         affected_by_outer_viewport_bounds_delta_y ==
             other.affected_by_outer_viewport_bounds_delta_y &&
         in_subtree_of_page_scale_layer ==
             other.in_subtree_of_page_scale_layer &&
         transform_changed == other.transform_changed &&
         post_local_scale_factor == other.post_local_scale_factor &&
         sublayer_scale == other.sublayer_scale &&
         scroll_offset == other.scroll_offset &&
         scroll_snap == other.scroll_snap &&
         source_offset == other.source_offset &&
         source_to_parent == other.source_to_parent;
}

void TransformNodeData::update_pre_local_transform(
    const gfx::Point3F& transform_origin) {
  pre_local.MakeIdentity();
  pre_local.Translate3d(-transform_origin.x(), -transform_origin.y(),
                        -transform_origin.z());
}

void TransformNodeData::update_post_local_transform(
    const gfx::PointF& position,
    const gfx::Point3F& transform_origin) {
  post_local.MakeIdentity();
  post_local.Scale(post_local_scale_factor, post_local_scale_factor);
  post_local.Translate3d(
      position.x() + source_offset.x() + transform_origin.x(),
      position.y() + source_offset.y() + transform_origin.y(),
      transform_origin.z());
}

void TransformNodeData::ToProtobuf(proto::TreeNode* proto) const {
  DCHECK(!proto->has_transform_node_data());
  proto::TranformNodeData* data = proto->mutable_transform_node_data();

  TransformToProto(pre_local, data->mutable_pre_local());
  TransformToProto(local, data->mutable_local());
  TransformToProto(post_local, data->mutable_post_local());

  TransformToProto(to_parent, data->mutable_to_parent());

  data->set_source_node_id(source_node_id);
  data->set_sorting_context_id(sorting_context_id);

  data->set_needs_local_transform_update(needs_local_transform_update);

  data->set_node_and_ancestors_are_animated_or_invertible(
      node_and_ancestors_are_animated_or_invertible);

  data->set_is_invertible(is_invertible);
  data->set_ancestors_are_invertible(ancestors_are_invertible);

  data->set_has_potential_animation(has_potential_animation);
  data->set_is_currently_animating(is_currently_animating);
  data->set_to_screen_is_potentially_animated(
      to_screen_is_potentially_animated);
  data->set_has_only_translation_animations(has_only_translation_animations);

  data->set_flattens_inherited_transform(flattens_inherited_transform);
  data->set_node_and_ancestors_are_flat(node_and_ancestors_are_flat);

  data->set_node_and_ancestors_have_only_integer_translation(
      node_and_ancestors_have_only_integer_translation);
  data->set_scrolls(scrolls);
  data->set_needs_sublayer_scale(needs_sublayer_scale);

  data->set_affected_by_inner_viewport_bounds_delta_x(
      affected_by_inner_viewport_bounds_delta_x);
  data->set_affected_by_inner_viewport_bounds_delta_y(
      affected_by_inner_viewport_bounds_delta_y);
  data->set_affected_by_outer_viewport_bounds_delta_x(
      affected_by_outer_viewport_bounds_delta_x);
  data->set_affected_by_outer_viewport_bounds_delta_y(
      affected_by_outer_viewport_bounds_delta_y);

  data->set_in_subtree_of_page_scale_layer(in_subtree_of_page_scale_layer);
  data->set_transform_changed(transform_changed);
  data->set_post_local_scale_factor(post_local_scale_factor);

  Vector2dFToProto(sublayer_scale, data->mutable_sublayer_scale());
  ScrollOffsetToProto(scroll_offset, data->mutable_scroll_offset());
  Vector2dFToProto(scroll_snap, data->mutable_scroll_snap());
  Vector2dFToProto(source_offset, data->mutable_source_offset());
  Vector2dFToProto(source_to_parent, data->mutable_source_to_parent());
}

void TransformNodeData::FromProtobuf(const proto::TreeNode& proto) {
  DCHECK(proto.has_transform_node_data());
  const proto::TranformNodeData& data = proto.transform_node_data();

  pre_local = ProtoToTransform(data.pre_local());
  local = ProtoToTransform(data.local());
  post_local = ProtoToTransform(data.post_local());

  to_parent = ProtoToTransform(data.to_parent());

  source_node_id = data.source_node_id();
  sorting_context_id = data.sorting_context_id();

  needs_local_transform_update = data.needs_local_transform_update();

  node_and_ancestors_are_animated_or_invertible =
      data.node_and_ancestors_are_animated_or_invertible();

  is_invertible = data.is_invertible();
  ancestors_are_invertible = data.ancestors_are_invertible();

  has_potential_animation = data.has_potential_animation();
  is_currently_animating = data.is_currently_animating();
  to_screen_is_potentially_animated = data.to_screen_is_potentially_animated();
  has_only_translation_animations = data.has_only_translation_animations();

  flattens_inherited_transform = data.flattens_inherited_transform();
  node_and_ancestors_are_flat = data.node_and_ancestors_are_flat();

  node_and_ancestors_have_only_integer_translation =
      data.node_and_ancestors_have_only_integer_translation();
  scrolls = data.scrolls();
  needs_sublayer_scale = data.needs_sublayer_scale();

  affected_by_inner_viewport_bounds_delta_x =
      data.affected_by_inner_viewport_bounds_delta_x();
  affected_by_inner_viewport_bounds_delta_y =
      data.affected_by_inner_viewport_bounds_delta_y();
  affected_by_outer_viewport_bounds_delta_x =
      data.affected_by_outer_viewport_bounds_delta_x();
  affected_by_outer_viewport_bounds_delta_y =
      data.affected_by_outer_viewport_bounds_delta_y();

  in_subtree_of_page_scale_layer = data.in_subtree_of_page_scale_layer();
  transform_changed = data.transform_changed();
  post_local_scale_factor = data.post_local_scale_factor();

  sublayer_scale = ProtoToVector2dF(data.sublayer_scale());
  scroll_offset = ProtoToScrollOffset(data.scroll_offset());
  scroll_snap = ProtoToVector2dF(data.scroll_snap());
  source_offset = ProtoToVector2dF(data.source_offset());
  source_to_parent = ProtoToVector2dF(data.source_to_parent());
}

void TransformNodeData::AsValueInto(
    base::trace_event::TracedValue* value) const {
  MathUtil::AddToTracedValue("pre_local", pre_local, value);
  MathUtil::AddToTracedValue("local", local, value);
  MathUtil::AddToTracedValue("post_local", post_local, value);
  // TODO(sunxd): make frameviewer work without target_id
  value->SetInteger("target_id", 0);
  value->SetInteger("content_target_id", 0);
  value->SetInteger("source_node_id", source_node_id);
  value->SetInteger("sorting_context_id", sorting_context_id);
}

TransformCachedNodeData::TransformCachedNodeData()
    : target_id(-1), content_target_id(-1) {}

TransformCachedNodeData::TransformCachedNodeData(
    const TransformCachedNodeData& other) = default;

TransformCachedNodeData::~TransformCachedNodeData() {}

bool TransformCachedNodeData::operator==(
    const TransformCachedNodeData& other) const {
  return from_target == other.from_target && to_target == other.to_target &&
         from_screen == other.from_screen && to_screen == other.to_screen &&
         target_id == other.target_id &&
         content_target_id == other.content_target_id;
}

void TransformCachedNodeData::ToProtobuf(
    proto::TransformCachedNodeData* proto) const {
  TransformToProto(from_target, proto->mutable_from_target());
  TransformToProto(to_target, proto->mutable_to_target());
  TransformToProto(from_screen, proto->mutable_from_screen());
  TransformToProto(to_screen, proto->mutable_to_screen());
  proto->set_target_id(target_id);
  proto->set_content_target_id(content_target_id);
}

void TransformCachedNodeData::FromProtobuf(
    const proto::TransformCachedNodeData& proto) {
  from_target = ProtoToTransform(proto.from_target());
  to_target = ProtoToTransform(proto.to_target());
  from_screen = ProtoToTransform(proto.from_screen());
  to_screen = ProtoToTransform(proto.to_screen());
  target_id = proto.target_id();
  content_target_id = proto.content_target_id();
}

ClipNodeData::ClipNodeData()
    : transform_id(-1),
      target_id(-1),
      applies_local_clip(true),
      layer_clipping_uses_only_local_clip(false),
      target_is_clipped(false),
      layers_are_clipped(false),
      layers_are_clipped_when_surfaces_disabled(false),
      resets_clip(false) {}

ClipNodeData::ClipNodeData(const ClipNodeData& other) = default;

bool ClipNodeData::operator==(const ClipNodeData& other) const {
  return clip == other.clip &&
         combined_clip_in_target_space == other.combined_clip_in_target_space &&
         clip_in_target_space == other.clip_in_target_space &&
         transform_id == other.transform_id && target_id == other.target_id &&
         applies_local_clip == other.applies_local_clip &&
         layer_clipping_uses_only_local_clip ==
             other.layer_clipping_uses_only_local_clip &&
         target_is_clipped == other.target_is_clipped &&
         layers_are_clipped == other.layers_are_clipped &&
         layers_are_clipped_when_surfaces_disabled ==
             other.layers_are_clipped_when_surfaces_disabled &&
         resets_clip == other.resets_clip;
}

void ClipNodeData::ToProtobuf(proto::TreeNode* proto) const {
  DCHECK(!proto->has_clip_node_data());
  proto::ClipNodeData* data = proto->mutable_clip_node_data();

  RectFToProto(clip, data->mutable_clip());
  RectFToProto(combined_clip_in_target_space,
               data->mutable_combined_clip_in_target_space());
  RectFToProto(clip_in_target_space, data->mutable_clip_in_target_space());

  data->set_transform_id(transform_id);
  data->set_target_id(target_id);
  data->set_applies_local_clip(applies_local_clip);
  data->set_layer_clipping_uses_only_local_clip(
      layer_clipping_uses_only_local_clip);
  data->set_target_is_clipped(target_is_clipped);
  data->set_layers_are_clipped(layers_are_clipped);
  data->set_layers_are_clipped_when_surfaces_disabled(
      layers_are_clipped_when_surfaces_disabled);
  data->set_resets_clip(resets_clip);
}

void ClipNodeData::FromProtobuf(const proto::TreeNode& proto) {
  DCHECK(proto.has_clip_node_data());
  const proto::ClipNodeData& data = proto.clip_node_data();

  clip = ProtoToRectF(data.clip());
  combined_clip_in_target_space =
      ProtoToRectF(data.combined_clip_in_target_space());
  clip_in_target_space = ProtoToRectF(data.clip_in_target_space());

  transform_id = data.transform_id();
  target_id = data.target_id();
  applies_local_clip = data.applies_local_clip();
  layer_clipping_uses_only_local_clip =
      data.layer_clipping_uses_only_local_clip();
  target_is_clipped = data.target_is_clipped();
  layers_are_clipped = data.layers_are_clipped();
  layers_are_clipped_when_surfaces_disabled =
      data.layers_are_clipped_when_surfaces_disabled();
  resets_clip = data.resets_clip();
}

void ClipNodeData::AsValueInto(base::trace_event::TracedValue* value) const {
  MathUtil::AddToTracedValue("clip", clip, value);
  value->SetInteger("transform_id", transform_id);
  value->SetInteger("target_id", target_id);
  value->SetBoolean("applies_local_clip", applies_local_clip);
  value->SetBoolean("layer_clipping_uses_only_local_clip",
                    layer_clipping_uses_only_local_clip);
  value->SetBoolean("target_is_clipped", target_is_clipped);
  value->SetBoolean("layers_are_clipped", layers_are_clipped);
  value->SetBoolean("layers_are_clipped_when_surfaces_disabled",
                    layers_are_clipped_when_surfaces_disabled);
  value->SetBoolean("resets_clip", resets_clip);
}

EffectNodeData::EffectNodeData()
    : opacity(1.f),
      screen_space_opacity(1.f),
      has_render_surface(false),
      render_surface(nullptr),
      has_copy_request(false),
      hidden_by_backface_visibility(false),
      double_sided(false),
      is_drawn(true),
      subtree_hidden(false),
      has_potential_opacity_animation(false),
      is_currently_animating_opacity(false),
      effect_changed(false),
      num_copy_requests_in_subtree(0),
      has_unclipped_descendants(false),
      transform_id(0),
      clip_id(0),
      target_id(0),
      mask_layer_id(-1),
      replica_layer_id(-1),
      replica_mask_layer_id(-1) {}

EffectNodeData::EffectNodeData(const EffectNodeData& other) = default;

bool EffectNodeData::operator==(const EffectNodeData& other) const {
  return opacity == other.opacity &&
         screen_space_opacity == other.screen_space_opacity &&
         has_render_surface == other.has_render_surface &&
         has_copy_request == other.has_copy_request &&
         background_filters == other.background_filters &&
         hidden_by_backface_visibility == other.hidden_by_backface_visibility &&
         double_sided == other.double_sided && is_drawn == other.is_drawn &&
         subtree_hidden == other.subtree_hidden &&
         has_potential_opacity_animation ==
             other.has_potential_opacity_animation &&
         is_currently_animating_opacity ==
             other.is_currently_animating_opacity &&
         effect_changed == other.effect_changed &&
         num_copy_requests_in_subtree == other.num_copy_requests_in_subtree &&
         transform_id == other.transform_id && clip_id == other.clip_id &&
         target_id == other.target_id && mask_layer_id == other.mask_layer_id &&
         replica_layer_id == other.replica_layer_id &&
         replica_mask_layer_id == other.replica_mask_layer_id;
}

void EffectNodeData::ToProtobuf(proto::TreeNode* proto) const {
  DCHECK(!proto->has_effect_node_data());
  proto::EffectNodeData* data = proto->mutable_effect_node_data();
  data->set_opacity(opacity);
  data->set_screen_space_opacity(screen_space_opacity);
  data->set_has_render_surface(has_render_surface);
  data->set_has_copy_request(has_copy_request);
  data->set_hidden_by_backface_visibility(hidden_by_backface_visibility);
  data->set_double_sided(double_sided);
  data->set_is_drawn(is_drawn);
  data->set_subtree_hidden(subtree_hidden);
  data->set_has_potential_opacity_animation(has_potential_opacity_animation);
  data->set_is_currently_animating_opacity(is_currently_animating_opacity);
  data->set_effect_changed(effect_changed);
  data->set_num_copy_requests_in_subtree(num_copy_requests_in_subtree);
  data->set_transform_id(transform_id);
  data->set_clip_id(clip_id);
  data->set_target_id(target_id);
  data->set_mask_layer_id(mask_layer_id);
  data->set_replica_layer_id(replica_layer_id);
  data->set_replica_mask_layer_id(replica_mask_layer_id);
}

void EffectNodeData::FromProtobuf(const proto::TreeNode& proto) {
  DCHECK(proto.has_effect_node_data());
  const proto::EffectNodeData& data = proto.effect_node_data();

  opacity = data.opacity();
  screen_space_opacity = data.screen_space_opacity();
  has_render_surface = data.has_render_surface();
  has_copy_request = data.has_copy_request();
  hidden_by_backface_visibility = data.hidden_by_backface_visibility();
  double_sided = data.double_sided();
  is_drawn = data.is_drawn();
  subtree_hidden = data.subtree_hidden();
  has_potential_opacity_animation = data.has_potential_opacity_animation();
  is_currently_animating_opacity = data.is_currently_animating_opacity();
  effect_changed = data.effect_changed();
  num_copy_requests_in_subtree = data.num_copy_requests_in_subtree();
  transform_id = data.transform_id();
  clip_id = data.clip_id();
  target_id = data.target_id();
  mask_layer_id = data.mask_layer_id();
  replica_layer_id = data.replica_layer_id();
  replica_mask_layer_id = data.replica_mask_layer_id();
}

void EffectNodeData::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetDouble("opacity", opacity);
  value->SetBoolean("has_render_surface", has_render_surface);
  value->SetBoolean("has_copy_request", has_copy_request);
  value->SetBoolean("double_sided", double_sided);
  value->SetBoolean("is_drawn", is_drawn);
  value->SetBoolean("has_potential_opacity_animation",
                    has_potential_opacity_animation);
  value->SetBoolean("effect_changed", effect_changed);
  value->SetInteger("num_copy_requests_in_subtree",
                    num_copy_requests_in_subtree);
  value->SetInteger("transform_id", transform_id);
  value->SetInteger("clip_id", clip_id);
  value->SetInteger("target_id", target_id);
  value->SetInteger("mask_layer_id", mask_layer_id);
  value->SetInteger("replica_layer_id", replica_layer_id);
  value->SetInteger("replica_mask_layer_id", replica_mask_layer_id);
}

ScrollNodeData::ScrollNodeData()
    : scrollable(false),
      main_thread_scrolling_reasons(
          MainThreadScrollingReason::kNotScrollingOnMain),
      contains_non_fast_scrollable_region(false),
      max_scroll_offset_affected_by_page_scale(false),
      is_inner_viewport_scroll_layer(false),
      is_outer_viewport_scroll_layer(false),
      should_flatten(false),
      user_scrollable_horizontal(false),
      user_scrollable_vertical(false),
      transform_id(0),
      num_drawn_descendants(0) {}

ScrollNodeData::ScrollNodeData(const ScrollNodeData& other) = default;

bool ScrollNodeData::operator==(const ScrollNodeData& other) const {
  return scrollable == other.scrollable &&
         main_thread_scrolling_reasons == other.main_thread_scrolling_reasons &&
         contains_non_fast_scrollable_region ==
             other.contains_non_fast_scrollable_region &&
         scroll_clip_layer_bounds == other.scroll_clip_layer_bounds &&
         bounds == other.bounds &&
         max_scroll_offset_affected_by_page_scale ==
             other.max_scroll_offset_affected_by_page_scale &&
         is_inner_viewport_scroll_layer ==
             other.is_inner_viewport_scroll_layer &&
         is_outer_viewport_scroll_layer ==
             other.is_outer_viewport_scroll_layer &&
         offset_to_transform_parent == other.offset_to_transform_parent &&
         should_flatten == other.should_flatten &&
         user_scrollable_horizontal == other.user_scrollable_horizontal &&
         user_scrollable_vertical == other.user_scrollable_vertical &&
         element_id == other.element_id && transform_id == other.transform_id;
}

void ScrollNodeData::ToProtobuf(proto::TreeNode* proto) const {
  DCHECK(!proto->has_scroll_node_data());
  proto::ScrollNodeData* data = proto->mutable_scroll_node_data();
  data->set_scrollable(scrollable);
  data->set_main_thread_scrolling_reasons(main_thread_scrolling_reasons);
  data->set_contains_non_fast_scrollable_region(
      contains_non_fast_scrollable_region);
  SizeToProto(scroll_clip_layer_bounds,
              data->mutable_scroll_clip_layer_bounds());
  SizeToProto(bounds, data->mutable_bounds());
  data->set_max_scroll_offset_affected_by_page_scale(
      max_scroll_offset_affected_by_page_scale);
  data->set_is_inner_viewport_scroll_layer(is_inner_viewport_scroll_layer);
  data->set_is_outer_viewport_scroll_layer(is_outer_viewport_scroll_layer);
  Vector2dFToProto(offset_to_transform_parent,
                   data->mutable_offset_to_transform_parent());
  data->set_should_flatten(should_flatten);
  data->set_user_scrollable_horizontal(user_scrollable_horizontal);
  data->set_user_scrollable_vertical(user_scrollable_vertical);
  element_id.ToProtobuf(data->mutable_element_id());
  data->set_transform_id(transform_id);
}

void ScrollNodeData::FromProtobuf(const proto::TreeNode& proto) {
  DCHECK(proto.has_scroll_node_data());
  const proto::ScrollNodeData& data = proto.scroll_node_data();

  scrollable = data.scrollable();
  main_thread_scrolling_reasons = data.main_thread_scrolling_reasons();
  contains_non_fast_scrollable_region =
      data.contains_non_fast_scrollable_region();
  scroll_clip_layer_bounds = ProtoToSize(data.scroll_clip_layer_bounds());
  bounds = ProtoToSize(data.bounds());
  max_scroll_offset_affected_by_page_scale =
      data.max_scroll_offset_affected_by_page_scale();
  is_inner_viewport_scroll_layer = data.is_inner_viewport_scroll_layer();
  is_outer_viewport_scroll_layer = data.is_outer_viewport_scroll_layer();
  offset_to_transform_parent =
      ProtoToVector2dF(data.offset_to_transform_parent());
  should_flatten = data.should_flatten();
  user_scrollable_horizontal = data.user_scrollable_horizontal();
  user_scrollable_vertical = data.user_scrollable_vertical();
  element_id.FromProtobuf(data.element_id());
  transform_id = data.transform_id();
}

void ScrollNodeData::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetBoolean("scrollable", scrollable);
  MathUtil::AddToTracedValue("scroll_clip_layer_bounds",
                             scroll_clip_layer_bounds, value);
  MathUtil::AddToTracedValue("bounds", bounds, value);
  MathUtil::AddToTracedValue("offset_to_transform_parent",
                             offset_to_transform_parent, value);
  value->SetBoolean("should_flatten", should_flatten);
  value->SetBoolean("user_scrollable_horizontal", user_scrollable_horizontal);
  value->SetBoolean("user_scrollable_vertical", user_scrollable_vertical);

  element_id.AddToTracedValue(value);
  value->SetInteger("transform_id", transform_id);
}

int TransformTree::Insert(const TransformNode& tree_node, int parent_id) {
  int node_id = PropertyTree<TransformNode>::Insert(tree_node, parent_id);
  DCHECK_EQ(node_id, static_cast<int>(cached_data_.size()));

  cached_data_.push_back(TransformCachedNodeData());
  return node_id;
}

void TransformTree::clear() {
  PropertyTree<TransformNode>::clear();

  nodes_affected_by_inner_viewport_bounds_delta_.clear();
  nodes_affected_by_outer_viewport_bounds_delta_.clear();
  cached_data_.clear();
  cached_data_.push_back(TransformCachedNodeData());
}

bool TransformTree::ComputeTransform(int source_id,
                                     int dest_id,
                                     gfx::Transform* transform) const {
  transform->MakeIdentity();

  if (source_id == dest_id)
    return true;

  if (source_id > dest_id) {
    return CombineTransformsBetween(source_id, dest_id, transform);
  }

  return CombineInversesBetween(source_id, dest_id, transform);
}

bool TransformTree::ComputeTransformWithDestinationSublayerScale(
    int source_id,
    int dest_id,
    gfx::Transform* transform) const {
  bool success = ComputeTransform(source_id, dest_id, transform);

  const TransformNode* dest_node = Node(dest_id);
  if (!dest_node->data.needs_sublayer_scale)
    return success;

  transform->matrix().postScale(dest_node->data.sublayer_scale.x(),
                                dest_node->data.sublayer_scale.y(), 1.f);
  return success;
}

bool TransformTree::ComputeTransformWithSourceSublayerScale(
    int source_id,
    int dest_id,
    gfx::Transform* transform) const {
  bool success = ComputeTransform(source_id, dest_id, transform);

  const TransformNode* source_node = Node(source_id);
  if (!source_node->data.needs_sublayer_scale)
    return success;

  if (source_node->data.sublayer_scale.x() == 0 ||
      source_node->data.sublayer_scale.y() == 0)
    return false;

  transform->Scale(1.f / source_node->data.sublayer_scale.x(),
                   1.f / source_node->data.sublayer_scale.y());
  return success;
}

bool TransformTree::Are2DAxisAligned(int source_id, int dest_id) const {
  gfx::Transform transform;
  return ComputeTransform(source_id, dest_id, &transform) &&
         transform.Preserves2dAxisAlignment();
}

bool TransformTree::NeedsSourceToParentUpdate(TransformNode* node) {
  return (source_to_parent_updates_allowed() &&
          node->parent_id != node->data.source_node_id);
}

void TransformTree::ResetChangeTracking() {
  for (int id = 1; id < static_cast<int>(size()); ++id) {
    TransformNode* node = Node(id);
    node->data.transform_changed = false;
  }
}

void TransformTree::UpdateTransforms(int id) {
  TransformNode* node = Node(id);
  TransformNode* parent_node = parent(node);
  TransformNode* target_node = Node(TargetId(id));
  TransformNode* source_node = Node(node->data.source_node_id);
  property_trees()->UpdateCachedNumber();
  if (node->data.needs_local_transform_update ||
      NeedsSourceToParentUpdate(node))
    UpdateLocalTransform(node);
  else
    UndoSnapping(node);
  UpdateScreenSpaceTransform(node, parent_node, target_node);
  UpdateSublayerScale(node);
  UpdateTargetSpaceTransform(node, target_node);
  UpdateAnimationProperties(node, parent_node);
  UpdateSnapping(node);
  UpdateNodeAndAncestorsHaveIntegerTranslations(node, parent_node);
  UpdateTransformChanged(node, parent_node, source_node);
  UpdateNodeAndAncestorsAreAnimatedOrInvertible(node, parent_node);
}

bool TransformTree::IsDescendant(int desc_id, int source_id) const {
  while (desc_id != source_id) {
    if (desc_id < 0)
      return false;
    desc_id = Node(desc_id)->parent_id;
  }
  return true;
}

bool TransformTree::CombineTransformsBetween(int source_id,
                                             int dest_id,
                                             gfx::Transform* transform) const {
  DCHECK(source_id > dest_id);
  const TransformNode* current = Node(source_id);
  const TransformNode* dest = Node(dest_id);
  // Combine transforms to and from the screen when possible. Since flattening
  // is a non-linear operation, we cannot use this approach when there is
  // non-trivial flattening between the source and destination nodes. For
  // example, consider the tree R->A->B->C, where B flattens its inherited
  // transform, and A has a non-flat transform. Suppose C is the source and A is
  // the destination. The expected result is C * B. But C's to_screen
  // transform is C * B * flattened(A * R), and A's from_screen transform is
  // R^{-1} * A^{-1}. If at least one of A and R isn't flat, the inverse of
  // flattened(A * R) won't be R^{-1} * A{-1}, so multiplying C's to_screen and
  // A's from_screen will not produce the correct result.
  if (!dest || (dest->data.ancestors_are_invertible &&
                dest->data.node_and_ancestors_are_flat)) {
    transform->ConcatTransform(ToScreen(current->id));
    if (dest)
      transform->ConcatTransform(FromScreen(dest->id));
    return true;
  }

  // Flattening is defined in a way that requires it to be applied while
  // traversing downward in the tree. We first identify nodes that are on the
  // path from the source to the destination (this is traversing upward), and
  // then we visit these nodes in reverse order, flattening as needed. We
  // early-out if we get to a node whose target node is the destination, since
  // we can then re-use the target space transform stored at that node. However,
  // we cannot re-use a stored target space transform if the destination has a
  // zero sublayer scale, since stored target space transforms have sublayer
  // scale baked in, but we need to compute an unscaled transform.
  std::vector<int> source_to_destination;
  source_to_destination.push_back(current->id);
  current = parent(current);
  bool destination_has_non_zero_sublayer_scale =
      dest->data.sublayer_scale.x() != 0.f &&
      dest->data.sublayer_scale.y() != 0.f;
  DCHECK(destination_has_non_zero_sublayer_scale ||
         !dest->data.ancestors_are_invertible);
  for (; current && current->id > dest_id; current = parent(current)) {
    if (destination_has_non_zero_sublayer_scale &&
        TargetId(current->id) == dest_id &&
        ContentTargetId(current->id) == dest_id)
      break;
    source_to_destination.push_back(current->id);
  }

  gfx::Transform combined_transform;
  if (current->id > dest_id) {
    combined_transform = ToTarget(current->id);
    // The stored target space transform has sublayer scale baked in, but we
    // need the unscaled transform.
    combined_transform.matrix().postScale(1.0f / dest->data.sublayer_scale.x(),
                                          1.0f / dest->data.sublayer_scale.y(),
                                          1.0f);
  } else if (current->id < dest_id) {
    // We have reached the lowest common ancestor of the source and destination
    // nodes. This case can occur when we are transforming between a node
    // corresponding to a fixed-position layer (or its descendant) and the node
    // corresponding to the layer's render target. For example, consider the
    // layer tree R->T->S->F where F is fixed-position, S owns a render surface,
    // and T has a significant transform. This will yield the following
    // transform tree:
    //    R
    //    |
    //    T
    //   /|
    //  S F
    // In this example, T will have id 2, S will have id 3, and F will have id
    // 4. When walking up the ancestor chain from F, the first node with a
    // smaller id than S will be T, the lowest common ancestor of these nodes.
    // We compute the transform from T to S here, and then from F to T in the
    // loop below.
    DCHECK(IsDescendant(dest_id, current->id));
    CombineInversesBetween(current->id, dest_id, &combined_transform);
    DCHECK(combined_transform.IsApproximatelyIdentityOrTranslation(
        SkDoubleToMScalar(1e-4)));
  }

  size_t source_to_destination_size = source_to_destination.size();
  for (size_t i = 0; i < source_to_destination_size; ++i) {
    size_t index = source_to_destination_size - 1 - i;
    const TransformNode* node = Node(source_to_destination[index]);
    if (node->data.flattens_inherited_transform)
      combined_transform.FlattenTo2d();
    combined_transform.PreconcatTransform(node->data.to_parent);
  }

  transform->ConcatTransform(combined_transform);
  return true;
}

bool TransformTree::CombineInversesBetween(int source_id,
                                           int dest_id,
                                           gfx::Transform* transform) const {
  DCHECK(source_id < dest_id);
  const TransformNode* current = Node(dest_id);
  const TransformNode* dest = Node(source_id);
  // Just as in CombineTransformsBetween, we can use screen space transforms in
  // this computation only when there isn't any non-trivial flattening
  // involved.
  if (current->data.ancestors_are_invertible &&
      current->data.node_and_ancestors_are_flat) {
    transform->PreconcatTransform(FromScreen(current->id));
    if (dest)
      transform->PreconcatTransform(ToScreen(dest->id));
    return true;
  }

  // Inverting a flattening is not equivalent to flattening an inverse. This
  // means we cannot, for example, use the inverse of each node's to_parent
  // transform, flattening where needed. Instead, we must compute the transform
  // from the destination to the source, with flattening, and then invert the
  // result.
  gfx::Transform dest_to_source;
  CombineTransformsBetween(dest_id, source_id, &dest_to_source);
  gfx::Transform source_to_dest;
  bool all_are_invertible = dest_to_source.GetInverse(&source_to_dest);
  transform->PreconcatTransform(source_to_dest);
  return all_are_invertible;
}

void TransformTree::UpdateLocalTransform(TransformNode* node) {
  gfx::Transform transform = node->data.post_local;
  if (NeedsSourceToParentUpdate(node)) {
    gfx::Transform to_parent;
    ComputeTransform(node->data.source_node_id, node->parent_id, &to_parent);
    node->data.source_to_parent = to_parent.To2dTranslation();
  }

  gfx::Vector2dF fixed_position_adjustment;
  gfx::Vector2dF inner_viewport_bounds_delta =
      property_trees()->inner_viewport_container_bounds_delta();
  gfx::Vector2dF outer_viewport_bounds_delta =
      property_trees()->outer_viewport_container_bounds_delta();
  if (node->data.affected_by_inner_viewport_bounds_delta_x)
    fixed_position_adjustment.set_x(inner_viewport_bounds_delta.x());
  else if (node->data.affected_by_outer_viewport_bounds_delta_x)
    fixed_position_adjustment.set_x(outer_viewport_bounds_delta.x());

  if (node->data.affected_by_inner_viewport_bounds_delta_y)
    fixed_position_adjustment.set_y(inner_viewport_bounds_delta.y());
  else if (node->data.affected_by_outer_viewport_bounds_delta_y)
    fixed_position_adjustment.set_y(outer_viewport_bounds_delta.y());

  transform.Translate(
      node->data.source_to_parent.x() - node->data.scroll_offset.x() +
          fixed_position_adjustment.x(),
      node->data.source_to_parent.y() - node->data.scroll_offset.y() +
          fixed_position_adjustment.y());
  transform.PreconcatTransform(node->data.local);
  transform.PreconcatTransform(node->data.pre_local);
  node->data.set_to_parent(transform);
  node->data.needs_local_transform_update = false;
}

void TransformTree::UpdateScreenSpaceTransform(TransformNode* node,
                                               TransformNode* parent_node,
                                               TransformNode* target_node) {
  if (!parent_node) {
    SetToScreen(node->id, node->data.to_parent);
    node->data.ancestors_are_invertible = true;
    node->data.to_screen_is_potentially_animated = false;
    node->data.node_and_ancestors_are_flat = node->data.to_parent.IsFlat();
  } else {
    gfx::Transform to_screen_space_transform = ToScreen(parent_node->id);
    if (node->data.flattens_inherited_transform)
      to_screen_space_transform.FlattenTo2d();
    to_screen_space_transform.PreconcatTransform(node->data.to_parent);
    node->data.ancestors_are_invertible =
        parent_node->data.ancestors_are_invertible;
    node->data.node_and_ancestors_are_flat =
        parent_node->data.node_and_ancestors_are_flat &&
        node->data.to_parent.IsFlat();
    SetToScreen(node->id, to_screen_space_transform);
  }

  gfx::Transform from_screen;
  if (!ToScreen(node->id).GetInverse(&from_screen))
    node->data.ancestors_are_invertible = false;
  SetFromScreen(node->id, from_screen);
}

void TransformTree::UpdateSublayerScale(TransformNode* node) {
  // The sublayer scale depends on the screen space transform, so update it too.
  if (!node->data.needs_sublayer_scale) {
    node->data.sublayer_scale = gfx::Vector2dF(1.0f, 1.0f);
    return;
  }

  float layer_scale_factor =
      device_scale_factor_ * device_transform_scale_factor_;
  if (node->data.in_subtree_of_page_scale_layer)
    layer_scale_factor *= page_scale_factor_;
  node->data.sublayer_scale = MathUtil::ComputeTransform2dScaleComponents(
      ToScreen(node->id), layer_scale_factor);
}

void TransformTree::UpdateTargetSpaceTransform(TransformNode* node,
                                               TransformNode* target_node) {
  gfx::Transform target_space_transform;
  if (node->data.needs_sublayer_scale) {
    target_space_transform.MakeIdentity();
    target_space_transform.Scale(node->data.sublayer_scale.x(),
                                 node->data.sublayer_scale.y());
  } else {
    // In order to include the root transform for the root surface, we walk up
    // to the root of the transform tree in ComputeTransform.
    int target_id = target_node->id;
    ComputeTransformWithDestinationSublayerScale(node->id, target_id,
                                                 &target_space_transform);
  }

  gfx::Transform from_target;
  if (!target_space_transform.GetInverse(&from_target))
    node->data.ancestors_are_invertible = false;
  SetToTarget(node->id, target_space_transform);
  SetFromTarget(node->id, from_target);
}

void TransformTree::UpdateAnimationProperties(TransformNode* node,
                                              TransformNode* parent_node) {
  bool ancestor_is_animating = false;
  if (parent_node)
    ancestor_is_animating = parent_node->data.to_screen_is_potentially_animated;
  node->data.to_screen_is_potentially_animated =
      node->data.has_potential_animation || ancestor_is_animating;
}

void TransformTree::UndoSnapping(TransformNode* node) {
  // to_parent transform has the scroll snap from previous frame baked in.
  // We need to undo it and use the un-snapped transform to compute current
  // target and screen space transforms.
  node->data.to_parent.Translate(-node->data.scroll_snap.x(),
                                 -node->data.scroll_snap.y());
}

void TransformTree::UpdateSnapping(TransformNode* node) {
  if (!node->data.scrolls || node->data.to_screen_is_potentially_animated ||
      !ToScreen(node->id).IsScaleOrTranslation() ||
      !node->data.ancestors_are_invertible) {
    return;
  }

  // Scroll snapping must be done in screen space (the pixels we care about).
  // This means we effectively snap the screen space transform. If ST is the
  // screen space transform and ST' is ST with its translation components
  // rounded, then what we're after is the scroll delta X, where ST * X = ST'.
  // I.e., we want a transform that will realize our scroll snap. It follows
  // that X = ST^-1 * ST'. We cache ST and ST^-1 to make this more efficient.
  gfx::Transform rounded = ToScreen(node->id);
  rounded.RoundTranslationComponents();
  gfx::Transform delta = FromScreen(node->id);
  delta *= rounded;

  DCHECK(delta.IsApproximatelyIdentityOrTranslation(SkDoubleToMScalar(1e-4)))
      << delta.ToString();

  gfx::Vector2dF translation = delta.To2dTranslation();

  // Now that we have our scroll delta, we must apply it to each of our
  // combined, to/from matrices.
  SetToScreen(node->id, rounded);
  node->data.to_parent.Translate(translation.x(), translation.y());
  gfx::Transform from_screen = FromScreen(node->id);
  from_screen.matrix().postTranslate(-translation.x(), -translation.y(), 0);
  SetFromScreen(node->id, from_screen);
  gfx::Transform to_target = ToTarget(node->id);
  to_target.Translate(translation.x(), translation.y());
  SetToTarget(node->id, to_target);
  gfx::Transform from_target = FromTarget(node->id);
  from_target.matrix().postTranslate(-translation.x(), -translation.y(), 0);
  SetFromTarget(node->id, from_target);
  node->data.scroll_snap = translation;
}

void TransformTree::UpdateTransformChanged(TransformNode* node,
                                           TransformNode* parent_node,
                                           TransformNode* source_node) {
  if (parent_node && parent_node->data.transform_changed) {
    node->data.transform_changed = true;
    return;
  }

  if (source_node && source_node->id != parent_node->id &&
      source_to_parent_updates_allowed_ && source_node->data.transform_changed)
    node->data.transform_changed = true;
}

void TransformTree::UpdateNodeAndAncestorsAreAnimatedOrInvertible(
    TransformNode* node,
    TransformNode* parent_node) {
  if (!parent_node) {
    node->data.node_and_ancestors_are_animated_or_invertible =
        node->data.has_potential_animation || node->data.is_invertible;
    return;
  }
  if (!parent_node->data.node_and_ancestors_are_animated_or_invertible) {
    node->data.node_and_ancestors_are_animated_or_invertible = false;
    return;
  }
  bool is_invertible = node->data.is_invertible;
  // Even when the current node's transform and the parent's screen space
  // transform are invertible, the current node's screen space transform can
  // become uninvertible due to floating-point arithmetic.
  if (!node->data.ancestors_are_invertible &&
      parent_node->data.ancestors_are_invertible)
    is_invertible = false;
  node->data.node_and_ancestors_are_animated_or_invertible =
      node->data.has_potential_animation || is_invertible;
}

void TransformTree::SetDeviceTransform(const gfx::Transform& transform,
                                       gfx::PointF root_position) {
  gfx::Transform root_post_local = transform;
  TransformNode* node = Node(1);
  root_post_local.Scale(node->data.post_local_scale_factor,
                        node->data.post_local_scale_factor);
  root_post_local.Translate(root_position.x(), root_position.y());
  if (node->data.post_local == root_post_local)
    return;

  node->data.post_local = root_post_local;
  node->data.needs_local_transform_update = true;
  set_needs_update(true);
}

void TransformTree::SetDeviceTransformScaleFactor(
    const gfx::Transform& transform) {
  gfx::Vector2dF device_transform_scale_components =
      MathUtil::ComputeTransform2dScaleComponents(transform, 1.f);

  // Not handling the rare case of different x and y device scale.
  device_transform_scale_factor_ =
      std::max(device_transform_scale_components.x(),
               device_transform_scale_components.y());
}

void TransformTree::UpdateInnerViewportContainerBoundsDelta() {
  if (nodes_affected_by_inner_viewport_bounds_delta_.empty())
    return;

  set_needs_update(true);
  for (int i : nodes_affected_by_inner_viewport_bounds_delta_)
    Node(i)->data.needs_local_transform_update = true;
}

void TransformTree::UpdateOuterViewportContainerBoundsDelta() {
  if (nodes_affected_by_outer_viewport_bounds_delta_.empty())
    return;

  set_needs_update(true);
  for (int i : nodes_affected_by_outer_viewport_bounds_delta_)
    Node(i)->data.needs_local_transform_update = true;
}

void TransformTree::AddNodeAffectedByInnerViewportBoundsDelta(int node_id) {
  nodes_affected_by_inner_viewport_bounds_delta_.push_back(node_id);
}

void TransformTree::AddNodeAffectedByOuterViewportBoundsDelta(int node_id) {
  nodes_affected_by_outer_viewport_bounds_delta_.push_back(node_id);
}

bool TransformTree::HasNodesAffectedByInnerViewportBoundsDelta() const {
  return !nodes_affected_by_inner_viewport_bounds_delta_.empty();
}

bool TransformTree::HasNodesAffectedByOuterViewportBoundsDelta() const {
  return !nodes_affected_by_outer_viewport_bounds_delta_.empty();
}

const gfx::Transform& TransformTree::FromTarget(int node_id) const {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  return cached_data_[node_id].from_target;
}

void TransformTree::SetFromTarget(int node_id,
                                  const gfx::Transform& transform) {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  cached_data_[node_id].from_target = transform;
}

const gfx::Transform& TransformTree::ToTarget(int node_id) const {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  return cached_data_[node_id].to_target;
}

void TransformTree::SetToTarget(int node_id, const gfx::Transform& transform) {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  cached_data_[node_id].to_target = transform;
}

const gfx::Transform& TransformTree::FromScreen(int node_id) const {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  return cached_data_[node_id].from_screen;
}

void TransformTree::SetFromScreen(int node_id,
                                  const gfx::Transform& transform) {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  cached_data_[node_id].from_screen = transform;
}

const gfx::Transform& TransformTree::ToScreen(int node_id) const {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  return cached_data_[node_id].to_screen;
}

void TransformTree::SetToScreen(int node_id, const gfx::Transform& transform) {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  cached_data_[node_id].to_screen = transform;
}

int TransformTree::TargetId(int node_id) const {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  return cached_data_[node_id].target_id;
}

void TransformTree::SetTargetId(int node_id, int target_id) {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  cached_data_[node_id].target_id = target_id;
}

int TransformTree::ContentTargetId(int node_id) const {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  return cached_data_[node_id].content_target_id;
}

void TransformTree::SetContentTargetId(int node_id, int content_target_id) {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  cached_data_[node_id].content_target_id = content_target_id;
}

gfx::Transform TransformTree::ToScreenSpaceTransformWithoutSublayerScale(
    int id) const {
  DCHECK_GT(id, 0);
  if (id == 1) {
    return gfx::Transform();
  }
  const TransformNode* node = Node(id);
  gfx::Transform screen_space_transform = ToScreen(id);
  if (node->data.sublayer_scale.x() != 0.0 &&
      node->data.sublayer_scale.y() != 0.0)
    screen_space_transform.Scale(1.0 / node->data.sublayer_scale.x(),
                                 1.0 / node->data.sublayer_scale.y());
  return screen_space_transform;
}

bool TransformTree::operator==(const TransformTree& other) const {
  return PropertyTree::operator==(other) &&
         source_to_parent_updates_allowed_ ==
             other.source_to_parent_updates_allowed() &&
         page_scale_factor_ == other.page_scale_factor() &&
         device_scale_factor_ == other.device_scale_factor() &&
         device_transform_scale_factor_ ==
             other.device_transform_scale_factor() &&
         nodes_affected_by_inner_viewport_bounds_delta_ ==
             other.nodes_affected_by_inner_viewport_bounds_delta() &&
         nodes_affected_by_outer_viewport_bounds_delta_ ==
             other.nodes_affected_by_outer_viewport_bounds_delta() &&
         cached_data_ == other.cached_data();
}

void TransformTree::ToProtobuf(proto::PropertyTree* proto) const {
  DCHECK(!proto->has_property_type());
  proto->set_property_type(proto::PropertyTree::Transform);

  PropertyTree::ToProtobuf(proto);
  proto::TransformTreeData* data = proto->mutable_transform_tree_data();

  data->set_source_to_parent_updates_allowed(source_to_parent_updates_allowed_);
  data->set_page_scale_factor(page_scale_factor_);
  data->set_device_scale_factor(device_scale_factor_);
  data->set_device_transform_scale_factor(device_transform_scale_factor_);

  for (auto i : nodes_affected_by_inner_viewport_bounds_delta_)
    data->add_nodes_affected_by_inner_viewport_bounds_delta(i);

  for (auto i : nodes_affected_by_outer_viewport_bounds_delta_)
    data->add_nodes_affected_by_outer_viewport_bounds_delta(i);

  for (int i = 0; i < static_cast<int>(cached_data_.size()); ++i)
    cached_data_[i].ToProtobuf(data->add_cached_data());
}

void TransformTree::FromProtobuf(
    const proto::PropertyTree& proto,
    std::unordered_map<int, int>* node_id_to_index_map) {
  DCHECK(proto.has_property_type());
  DCHECK_EQ(proto.property_type(), proto::PropertyTree::Transform);

  PropertyTree::FromProtobuf(proto, node_id_to_index_map);
  const proto::TransformTreeData& data = proto.transform_tree_data();

  source_to_parent_updates_allowed_ = data.source_to_parent_updates_allowed();
  page_scale_factor_ = data.page_scale_factor();
  device_scale_factor_ = data.device_scale_factor();
  device_transform_scale_factor_ = data.device_transform_scale_factor();

  DCHECK(nodes_affected_by_inner_viewport_bounds_delta_.empty());
  for (int i = 0; i < data.nodes_affected_by_inner_viewport_bounds_delta_size();
       ++i) {
    nodes_affected_by_inner_viewport_bounds_delta_.push_back(
        data.nodes_affected_by_inner_viewport_bounds_delta(i));
  }

  DCHECK(nodes_affected_by_outer_viewport_bounds_delta_.empty());
  for (int i = 0; i < data.nodes_affected_by_outer_viewport_bounds_delta_size();
       ++i) {
    nodes_affected_by_outer_viewport_bounds_delta_.push_back(
        data.nodes_affected_by_outer_viewport_bounds_delta(i));
  }

  DCHECK_EQ(static_cast<int>(cached_data_.size()), 1);
  cached_data_.back().FromProtobuf(data.cached_data(0));
  for (int i = 1; i < data.cached_data_size(); ++i) {
    cached_data_.push_back(TransformCachedNodeData());
    cached_data_.back().FromProtobuf(data.cached_data(i));
  }
}

EffectTree::EffectTree() {}

EffectTree::~EffectTree() {}

void EffectTree::clear() {
  PropertyTree<EffectNode>::clear();
  mask_replica_layer_ids_.clear();
}

float EffectTree::EffectiveOpacity(const EffectNode* node) const {
  return node->data.subtree_hidden ? 0.f : node->data.opacity;
}

void EffectTree::UpdateOpacities(EffectNode* node, EffectNode* parent_node) {
  node->data.screen_space_opacity = EffectiveOpacity(node);

  if (parent_node)
    node->data.screen_space_opacity *= parent_node->data.screen_space_opacity;
}

void EffectTree::UpdateIsDrawn(EffectNode* node, EffectNode* parent_node) {
  // Nodes that have screen space opacity 0 are hidden. So they are not drawn.
  // Exceptions:
  // 1) Nodes that contribute to copy requests, whether hidden or not, must be
  //    drawn.
  // 2) Nodes that have a background filter.
  // 3) Nodes with animating screen space opacity on main thread or pending tree
  //    are drawn if their parent is drawn irrespective of their opacity.
  if (node->data.has_copy_request)
    node->data.is_drawn = true;
  else if (EffectiveOpacity(node) == 0.f &&
           (!node->data.has_potential_opacity_animation ||
            property_trees()->is_active) &&
           node->data.background_filters.IsEmpty())
    node->data.is_drawn = false;
  else if (parent_node)
    node->data.is_drawn = parent_node->data.is_drawn;
  else
    node->data.is_drawn = true;
}

void EffectTree::UpdateEffectChanged(EffectNode* node,
                                     EffectNode* parent_node) {
  if (parent_node && parent_node->data.effect_changed) {
    node->data.effect_changed = true;
  }
}

void EffectTree::UpdateBackfaceVisibility(EffectNode* node,
                                          EffectNode* parent_node) {
  if (!parent_node) {
    node->data.hidden_by_backface_visibility = false;
    return;
  }
  if (parent_node->data.hidden_by_backface_visibility) {
    node->data.hidden_by_backface_visibility = true;
    return;
  }

  TransformTree& transform_tree = property_trees()->transform_tree;
  if (node->data.has_render_surface && !node->data.double_sided) {
    TransformNode* transform_node =
        transform_tree.Node(node->data.transform_id);
    if (transform_node->data.is_invertible &&
        transform_node->data.ancestors_are_invertible) {
      if (transform_node->data.sorting_context_id) {
        const TransformNode* parent_transform_node =
            transform_tree.parent(transform_node);
        if (parent_transform_node &&
            parent_transform_node->data.sorting_context_id ==
                transform_node->data.sorting_context_id) {
          gfx::Transform surface_draw_transform;
          transform_tree.ComputeTransform(
              transform_node->id, transform_tree.TargetId(transform_node->id),
              &surface_draw_transform);
          node->data.hidden_by_backface_visibility =
              surface_draw_transform.IsBackFaceVisible();
        } else {
          node->data.hidden_by_backface_visibility =
              transform_node->data.local.IsBackFaceVisible();
        }
        return;
      }
    }
  }
  node->data.hidden_by_backface_visibility = false;
}

void EffectTree::UpdateEffects(int id) {
  EffectNode* node = Node(id);
  EffectNode* parent_node = parent(node);

  UpdateOpacities(node, parent_node);
  UpdateIsDrawn(node, parent_node);
  UpdateEffectChanged(node, parent_node);
  UpdateBackfaceVisibility(node, parent_node);
}

void EffectTree::AddCopyRequest(int node_id,
                                std::unique_ptr<CopyOutputRequest> request) {
  copy_requests_.insert(std::make_pair(node_id, std::move(request)));
}

void EffectTree::PushCopyRequestsTo(EffectTree* other_tree) {
  // If other_tree still has copy requests, this means there was a commit
  // without a draw. This only happens in some edge cases during lost context or
  // visibility changes, so don't try to handle preserving these output
  // requests.
  if (!other_tree->copy_requests_.empty()) {
    // Destroying these copy requests will abort them.
    other_tree->copy_requests_.clear();
  }

  if (copy_requests_.empty())
    return;

  for (auto& request : copy_requests_) {
    other_tree->copy_requests_.insert(
        std::make_pair(request.first, std::move(request.second)));
  }
  copy_requests_.clear();

  // Property trees need to get rebuilt since effect nodes (and render surfaces)
  // that were created only for the copy requests we just pushed are no longer
  // needed.
  if (property_trees()->is_main_thread)
    property_trees()->needs_rebuild = true;
}

void EffectTree::TakeCopyRequestsAndTransformToSurface(
    int node_id,
    std::vector<std::unique_ptr<CopyOutputRequest>>* requests) {
  EffectNode* effect_node = Node(node_id);
  DCHECK(effect_node->data.has_render_surface);
  DCHECK(effect_node->data.has_copy_request);

  auto range = copy_requests_.equal_range(node_id);
  for (auto it = range.first; it != range.second; ++it)
    requests->push_back(std::move(it->second));
  copy_requests_.erase(range.first, range.second);

  for (auto& it : *requests) {
    if (!it->has_area())
      continue;

    // The area needs to be transformed from the space of content that draws to
    // the surface to the space of the surface itself.
    int destination_id = effect_node->data.transform_id;
    int source_id;
    if (effect_node->parent_id != -1) {
      // For non-root surfaces, transform only by sub-layer scale.
      source_id = destination_id;
    } else {
      // The root surface doesn't have the notion of sub-layer scale, but
      // instead has a similar notion of transforming from the space of the root
      // layer to the space of the screen.
      DCHECK_EQ(0, destination_id);
      source_id = 1;
    }
    gfx::Transform transform;
    property_trees()
        ->transform_tree.ComputeTransformWithDestinationSublayerScale(
            source_id, destination_id, &transform);
    it->set_area(MathUtil::MapEnclosingClippedRect(transform, it->area()));
  }
}

bool EffectTree::HasCopyRequests() const {
  return !copy_requests_.empty();
}

void EffectTree::ClearCopyRequests() {
  for (auto& node : nodes()) {
    node.data.num_copy_requests_in_subtree = 0;
    node.data.has_copy_request = false;
  }

  // Any copy requests that are still left will be aborted (sending an empty
  // result) on destruction.
  copy_requests_.clear();
  set_needs_update(true);
}

int EffectTree::ClosestAncestorWithCopyRequest(int id) const {
  DCHECK_GE(id, 0);
  const EffectNode* node = Node(id);
  while (node->id > 1) {
    if (node->data.has_copy_request)
      return node->id;

    node = parent(node);
  }

  if (node->data.has_copy_request)
    return node->id;
  else
    return -1;
}

void EffectTree::AddMaskOrReplicaLayerId(int id) {
  mask_replica_layer_ids_.push_back(id);
}

bool EffectTree::ContributesToDrawnSurface(int id) {
  // All drawn nodes contribute to drawn surface.
  // Exception : Nodes that are hidden and are drawn only for the sake of
  // copy requests.
  EffectNode* node = Node(id);
  EffectNode* parent_node = parent(node);
  return node->data.is_drawn && (!parent_node || parent_node->data.is_drawn);
}

void EffectTree::ResetChangeTracking() {
  for (int id = 1; id < static_cast<int>(size()); ++id) {
    EffectNode* node = Node(id);
    node->data.effect_changed = false;
  }
}

void TransformTree::UpdateNodeAndAncestorsHaveIntegerTranslations(
    TransformNode* node,
    TransformNode* parent_node) {
  node->data.node_and_ancestors_have_only_integer_translation =
      node->data.to_parent.IsIdentityOrIntegerTranslation();
  if (parent_node)
    node->data.node_and_ancestors_have_only_integer_translation =
        node->data.node_and_ancestors_have_only_integer_translation &&
        parent_node->data.node_and_ancestors_have_only_integer_translation;
}

void ClipTree::SetViewportClip(gfx::RectF viewport_rect) {
  if (size() < 2)
    return;
  ClipNode* node = Node(1);
  if (viewport_rect == node->data.clip)
    return;
  node->data.clip = viewport_rect;
  set_needs_update(true);
}

gfx::RectF ClipTree::ViewportClip() {
  const unsigned long min_size = 1;
  DCHECK_GT(size(), min_size);
  return Node(1)->data.clip;
}

bool ClipTree::operator==(const ClipTree& other) const {
  return PropertyTree::operator==(other);
}

void ClipTree::ToProtobuf(proto::PropertyTree* proto) const {
  DCHECK(!proto->has_property_type());
  proto->set_property_type(proto::PropertyTree::Clip);

  PropertyTree::ToProtobuf(proto);
}

void ClipTree::FromProtobuf(
    const proto::PropertyTree& proto,
    std::unordered_map<int, int>* node_id_to_index_map) {
  DCHECK(proto.has_property_type());
  DCHECK_EQ(proto.property_type(), proto::PropertyTree::Clip);

  PropertyTree::FromProtobuf(proto, node_id_to_index_map);
}

EffectTree& EffectTree::operator=(const EffectTree& from) {
  PropertyTree::operator=(from);
  mask_replica_layer_ids_ = from.mask_replica_layer_ids_;
  // copy_requests_ are omitted here, since these need to be moved rather
  // than copied or assigned.
  return *this;
}

bool EffectTree::operator==(const EffectTree& other) const {
  return PropertyTree::operator==(other) &&
         mask_replica_layer_ids_ == other.mask_replica_layer_ids_;
}

void EffectTree::ToProtobuf(proto::PropertyTree* proto) const {
  DCHECK(!proto->has_property_type());
  proto->set_property_type(proto::PropertyTree::Effect);

  PropertyTree::ToProtobuf(proto);
  proto::EffectTreeData* data = proto->mutable_effect_tree_data();

  for (auto i : mask_replica_layer_ids_)
    data->add_mask_replica_layer_ids(i);
}

void EffectTree::FromProtobuf(
    const proto::PropertyTree& proto,
    std::unordered_map<int, int>* node_id_to_index_map) {
  DCHECK(proto.has_property_type());
  DCHECK_EQ(proto.property_type(), proto::PropertyTree::Effect);

  PropertyTree::FromProtobuf(proto, node_id_to_index_map);
  const proto::EffectTreeData& data = proto.effect_tree_data();

  DCHECK(mask_replica_layer_ids_.empty());
  for (int i = 0; i < data.mask_replica_layer_ids_size(); ++i) {
    mask_replica_layer_ids_.push_back(data.mask_replica_layer_ids(i));
  }
}

ScrollTree::ScrollTree()
    : currently_scrolling_node_id_(-1),
      layer_id_to_scroll_offset_map_(ScrollTree::ScrollOffsetMap()) {}

ScrollTree::~ScrollTree() {}

ScrollTree& ScrollTree::operator=(const ScrollTree& from) {
  PropertyTree::operator=(from);
  currently_scrolling_node_id_ = -1;
  // layer_id_to_scroll_offset_map_ is intentionally omitted in operator=,
  // because we do not want to simply copy the map when property tree is
  // propagating from pending to active.
  // In the main to pending case, we do want to copy it, but this can be done by
  // calling UpdateScrollOffsetMap after the assignment;
  // In the other case, we want pending and active property trees to share the
  // same map.
  return *this;
}

bool ScrollTree::operator==(const ScrollTree& other) const {
  const ScrollTree::ScrollOffsetMap& other_scroll_offset_map =
      other.scroll_offset_map();
  if (layer_id_to_scroll_offset_map_.size() != other_scroll_offset_map.size())
    return false;

  for (auto map_entry : layer_id_to_scroll_offset_map_) {
    int key = map_entry.first;
    if (other_scroll_offset_map.find(key) == other_scroll_offset_map.end() ||
        map_entry.second != layer_id_to_scroll_offset_map_.at(key))
      return false;
  }

  bool is_currently_scrolling_node_equal =
      (currently_scrolling_node_id_ == -1)
          ? (!other.CurrentlyScrollingNode())
          : (other.CurrentlyScrollingNode() &&
             currently_scrolling_node_id_ ==
                 other.CurrentlyScrollingNode()->id);

  return PropertyTree::operator==(other) && is_currently_scrolling_node_equal;
}

void ScrollTree::ToProtobuf(proto::PropertyTree* proto) const {
  DCHECK(!proto->has_property_type());
  proto->set_property_type(proto::PropertyTree::Scroll);

  PropertyTree::ToProtobuf(proto);
  proto::ScrollTreeData* data = proto->mutable_scroll_tree_data();

  data->set_currently_scrolling_node_id(currently_scrolling_node_id_);
  for (auto i : layer_id_to_scroll_offset_map_) {
    data->add_layer_id_to_scroll_offset_map();
    proto::ScrollOffsetMapEntry* entry =
        data->mutable_layer_id_to_scroll_offset_map(
            data->layer_id_to_scroll_offset_map_size() - 1);
    entry->set_layer_id(i.first);
    SyncedScrollOffsetToProto(*i.second.get(), entry->mutable_scroll_offset());
  }
}

void ScrollTree::FromProtobuf(
    const proto::PropertyTree& proto,
    std::unordered_map<int, int>* node_id_to_index_map) {
  DCHECK(proto.has_property_type());
  DCHECK_EQ(proto.property_type(), proto::PropertyTree::Scroll);

  PropertyTree::FromProtobuf(proto, node_id_to_index_map);
  const proto::ScrollTreeData& data = proto.scroll_tree_data();

  currently_scrolling_node_id_ = data.currently_scrolling_node_id();

  // TODO(khushalsagar): This should probably be removed if the copy constructor
  // for ScrollTree copies the |layer_id_to_scroll_offset_map_| as well.
  layer_id_to_scroll_offset_map_.clear();
  for (int i = 0; i < data.layer_id_to_scroll_offset_map_size(); ++i) {
    const proto::ScrollOffsetMapEntry entry =
        data.layer_id_to_scroll_offset_map(i);
    layer_id_to_scroll_offset_map_[entry.layer_id()] = new SyncedScrollOffset();
    ProtoToSyncedScrollOffset(
        entry.scroll_offset(),
        layer_id_to_scroll_offset_map_[entry.layer_id()].get());
  }
}

void ScrollTree::clear() {
  PropertyTree<ScrollNode>::clear();

  if (property_trees()->is_main_thread) {
    currently_scrolling_node_id_ = -1;
    layer_id_to_scroll_offset_map_.clear();
  }
}

gfx::ScrollOffset ScrollTree::MaxScrollOffset(int scroll_node_id) const {
  const ScrollNode* scroll_node = Node(scroll_node_id);
  gfx::SizeF scroll_bounds = gfx::SizeF(scroll_node->data.bounds.width(),
                                        scroll_node->data.bounds.height());

  if (scroll_node->data.is_inner_viewport_scroll_layer) {
    scroll_bounds.Enlarge(
        property_trees()->inner_viewport_scroll_bounds_delta().x(),
        property_trees()->inner_viewport_scroll_bounds_delta().y());
  }

  if (!scroll_node->data.scrollable || scroll_bounds.IsEmpty())
    return gfx::ScrollOffset();

  TransformTree& transform_tree = property_trees()->transform_tree;
  float scale_factor = 1.f;
  if (scroll_node->data.max_scroll_offset_affected_by_page_scale)
    scale_factor = transform_tree.page_scale_factor();

  gfx::SizeF scaled_scroll_bounds = gfx::ScaleSize(scroll_bounds, scale_factor);
  scaled_scroll_bounds.SetSize(std::floor(scaled_scroll_bounds.width()),
                               std::floor(scaled_scroll_bounds.height()));

  gfx::Size clip_layer_bounds = scroll_clip_layer_bounds(scroll_node->id);

  gfx::ScrollOffset max_offset(
      scaled_scroll_bounds.width() - clip_layer_bounds.width(),
      scaled_scroll_bounds.height() - clip_layer_bounds.height());

  max_offset.Scale(1 / scale_factor);
  max_offset.SetToMax(gfx::ScrollOffset());
  return max_offset;
}

gfx::Size ScrollTree::scroll_clip_layer_bounds(int scroll_node_id) const {
  const ScrollNode* scroll_node = Node(scroll_node_id);
  gfx::Size scroll_clip_layer_bounds =
      scroll_node->data.scroll_clip_layer_bounds;

  gfx::Vector2dF scroll_clip_layer_bounds_delta;
  if (scroll_node->data.is_inner_viewport_scroll_layer) {
    scroll_clip_layer_bounds_delta.Add(
        property_trees()->inner_viewport_container_bounds_delta());
  } else if (scroll_node->data.is_outer_viewport_scroll_layer) {
    scroll_clip_layer_bounds_delta.Add(
        property_trees()->outer_viewport_container_bounds_delta());
  }

  gfx::Vector2d delta = gfx::ToCeiledVector2d(scroll_clip_layer_bounds_delta);
  scroll_clip_layer_bounds.SetSize(
      scroll_clip_layer_bounds.width() + delta.x(),
      scroll_clip_layer_bounds.height() + delta.y());

  return scroll_clip_layer_bounds;
}

ScrollNode* ScrollTree::CurrentlyScrollingNode() {
  ScrollNode* scroll_node = Node(currently_scrolling_node_id_);
  return scroll_node;
}

const ScrollNode* ScrollTree::CurrentlyScrollingNode() const {
  const ScrollNode* scroll_node = Node(currently_scrolling_node_id_);
  return scroll_node;
}

void ScrollTree::set_currently_scrolling_node(int scroll_node_id) {
  currently_scrolling_node_id_ = scroll_node_id;
}

gfx::Transform ScrollTree::ScreenSpaceTransform(int scroll_node_id) const {
  const ScrollNode* scroll_node = Node(scroll_node_id);
  const TransformTree& transform_tree = property_trees()->transform_tree;
  const TransformNode* transform_node =
      transform_tree.Node(scroll_node->data.transform_id);
  gfx::Transform screen_space_transform(
      1, 0, 0, 1, scroll_node->data.offset_to_transform_parent.x(),
      scroll_node->data.offset_to_transform_parent.y());
  screen_space_transform.ConcatTransform(
      transform_tree.ToScreen(transform_node->id));
  if (scroll_node->data.should_flatten)
    screen_space_transform.FlattenTo2d();
  return screen_space_transform;
}

SyncedScrollOffset* ScrollTree::synced_scroll_offset(int layer_id) {
  if (layer_id_to_scroll_offset_map_.find(layer_id) ==
      layer_id_to_scroll_offset_map_.end()) {
    layer_id_to_scroll_offset_map_[layer_id] = new SyncedScrollOffset;
  }
  return layer_id_to_scroll_offset_map_[layer_id].get();
}

const SyncedScrollOffset* ScrollTree::synced_scroll_offset(int layer_id) const {
  if (layer_id_to_scroll_offset_map_.find(layer_id) ==
      layer_id_to_scroll_offset_map_.end()) {
    return nullptr;
  }
  return layer_id_to_scroll_offset_map_.at(layer_id).get();
}

const gfx::ScrollOffset ScrollTree::current_scroll_offset(int layer_id) const {
  return synced_scroll_offset(layer_id)
             ? synced_scroll_offset(layer_id)->Current(
                   property_trees()->is_active)
             : gfx::ScrollOffset();
}

gfx::ScrollOffset ScrollTree::PullDeltaForMainThread(
    SyncedScrollOffset* scroll_offset) {
  // TODO(miletus): Remove all this temporary flooring machinery when
  // Blink fully supports fractional scrolls.
  gfx::ScrollOffset current_offset =
      scroll_offset->Current(property_trees()->is_active);
  gfx::ScrollOffset current_delta = property_trees()->is_active
                                        ? scroll_offset->Delta()
                                        : scroll_offset->PendingDelta().get();
  gfx::ScrollOffset floored_delta(floor(current_delta.x()),
                                  floor(current_delta.y()));
  gfx::ScrollOffset diff_delta = floored_delta - current_delta;
  gfx::ScrollOffset tmp_offset = current_offset + diff_delta;
  scroll_offset->SetCurrent(tmp_offset);
  gfx::ScrollOffset delta = scroll_offset->PullDeltaForMainThread();
  scroll_offset->SetCurrent(current_offset);
  return delta;
}

void ScrollTree::CollectScrollDeltas(ScrollAndScaleSet* scroll_info) {
  for (auto map_entry : layer_id_to_scroll_offset_map_) {
    gfx::ScrollOffset scroll_delta =
        PullDeltaForMainThread(map_entry.second.get());

    if (!scroll_delta.IsZero()) {
      LayerTreeHostCommon::ScrollUpdateInfo scroll;
      scroll.layer_id = map_entry.first;
      scroll.scroll_delta = gfx::Vector2d(scroll_delta.x(), scroll_delta.y());
      scroll_info->scrolls.push_back(scroll);
    }
  }
}

void ScrollTree::CollectScrollDeltasForTesting() {
  for (auto map_entry : layer_id_to_scroll_offset_map_) {
    PullDeltaForMainThread(map_entry.second.get());
  }
}

void ScrollTree::UpdateScrollOffsetMapEntry(
    int key,
    ScrollTree::ScrollOffsetMap* new_scroll_offset_map,
    LayerTreeImpl* layer_tree_impl) {
  bool changed = false;
  // If we are pushing scroll offset from main to pending tree, we create a new
  // instance of synced scroll offset; if we are pushing from pending to active,
  // we reuse the pending tree's value in the map.
  if (!property_trees()->is_active) {
    changed = synced_scroll_offset(key)->PushFromMainThread(
        new_scroll_offset_map->at(key)->PendingBase());

    if (new_scroll_offset_map->at(key)->clobber_active_value()) {
      synced_scroll_offset(key)->set_clobber_active_value();
    }
    if (changed) {
      layer_tree_impl->DidUpdateScrollOffset(key, -1);
    }
  } else {
    layer_id_to_scroll_offset_map_[key] = new_scroll_offset_map->at(key);
    changed |= synced_scroll_offset(key)->PushPendingToActive();
    if (changed) {
      layer_tree_impl->DidUpdateScrollOffset(key, -1);
    }
  }
}

void ScrollTree::UpdateScrollOffsetMap(
    ScrollTree::ScrollOffsetMap* new_scroll_offset_map,
    LayerTreeImpl* layer_tree_impl) {
  if (layer_tree_impl && !layer_tree_impl->LayerListIsEmpty()) {
    DCHECK(!property_trees()->is_main_thread);
    for (auto map_entry = layer_id_to_scroll_offset_map_.begin();
         map_entry != layer_id_to_scroll_offset_map_.end();) {
      int key = map_entry->first;
      if (new_scroll_offset_map->find(key) != new_scroll_offset_map->end()) {
        UpdateScrollOffsetMapEntry(key, new_scroll_offset_map, layer_tree_impl);
        ++map_entry;
      } else {
        map_entry = layer_id_to_scroll_offset_map_.erase(map_entry);
      }
    }

    for (auto& map_entry : *new_scroll_offset_map) {
      int key = map_entry.first;
      if (layer_id_to_scroll_offset_map_.find(key) ==
          layer_id_to_scroll_offset_map_.end())
        UpdateScrollOffsetMapEntry(key, new_scroll_offset_map, layer_tree_impl);
    }
  }
}

ScrollTree::ScrollOffsetMap& ScrollTree::scroll_offset_map() {
  return layer_id_to_scroll_offset_map_;
}

const ScrollTree::ScrollOffsetMap& ScrollTree::scroll_offset_map() const {
  return layer_id_to_scroll_offset_map_;
}

void ScrollTree::ApplySentScrollDeltasFromAbortedCommit() {
  DCHECK(property_trees()->is_active);
  for (auto& map_entry : layer_id_to_scroll_offset_map_)
    map_entry.second->AbortCommit();
}

bool ScrollTree::SetBaseScrollOffset(int layer_id,
                                     const gfx::ScrollOffset& scroll_offset) {
  return synced_scroll_offset(layer_id)->PushFromMainThread(scroll_offset);
}

bool ScrollTree::SetScrollOffset(int layer_id,
                                 const gfx::ScrollOffset& scroll_offset) {
  if (property_trees()->is_main_thread)
    return synced_scroll_offset(layer_id)->PushFromMainThread(scroll_offset);
  else if (property_trees()->is_active)
    return synced_scroll_offset(layer_id)->SetCurrent(scroll_offset);
  return false;
}

bool ScrollTree::UpdateScrollOffsetBaseForTesting(
    int layer_id,
    const gfx::ScrollOffset& offset) {
  DCHECK(!property_trees()->is_main_thread);
  bool changed = synced_scroll_offset(layer_id)->PushFromMainThread(offset);
  if (property_trees()->is_active)
    changed |= synced_scroll_offset(layer_id)->PushPendingToActive();
  return changed;
}

bool ScrollTree::SetScrollOffsetDeltaForTesting(int layer_id,
                                                const gfx::Vector2dF& delta) {
  return synced_scroll_offset(layer_id)->SetCurrent(
      synced_scroll_offset(layer_id)->ActiveBase() + gfx::ScrollOffset(delta));
}

const gfx::ScrollOffset ScrollTree::GetScrollOffsetBaseForTesting(
    int layer_id) const {
  DCHECK(!property_trees()->is_main_thread);
  if (synced_scroll_offset(layer_id))
    return property_trees()->is_active
               ? synced_scroll_offset(layer_id)->ActiveBase()
               : synced_scroll_offset(layer_id)->PendingBase();
  else
    return gfx::ScrollOffset();
}

const gfx::ScrollOffset ScrollTree::GetScrollOffsetDeltaForTesting(
    int layer_id) const {
  DCHECK(!property_trees()->is_main_thread);
  if (synced_scroll_offset(layer_id))
    return property_trees()->is_active
               ? synced_scroll_offset(layer_id)->Delta()
               : synced_scroll_offset(layer_id)->PendingDelta().get();
  else
    return gfx::ScrollOffset();
}

void ScrollTree::DistributeScroll(ScrollNode* scroll_node,
                                  ScrollState* scroll_state) {
  DCHECK(scroll_node && scroll_state);
  if (scroll_state->FullyConsumed())
    return;
  scroll_state->DistributeToScrollChainDescendant();

  // If the scroll doesn't propagate, and we're currently scrolling
  // a node other than this one, prevent the scroll from
  // propagating to this node.
  if (!scroll_state->should_propagate() &&
      scroll_state->delta_consumed_for_scroll_sequence() &&
      scroll_state->current_native_scrolling_node()->id != scroll_node->id) {
    return;
  }

  scroll_state->layer_tree_impl()->ApplyScroll(scroll_node, scroll_state);
}

gfx::Vector2dF ScrollTree::ScrollBy(ScrollNode* scroll_node,
                                    const gfx::Vector2dF& scroll,
                                    LayerTreeImpl* layer_tree_impl) {
  gfx::ScrollOffset adjusted_scroll(scroll);
  if (!scroll_node->data.user_scrollable_horizontal)
    adjusted_scroll.set_x(0);
  if (!scroll_node->data.user_scrollable_vertical)
    adjusted_scroll.set_y(0);
  DCHECK(scroll_node->data.scrollable);
  gfx::ScrollOffset old_offset = current_scroll_offset(scroll_node->owner_id);
  gfx::ScrollOffset new_offset =
      ClampScrollOffsetToLimits(old_offset + adjusted_scroll, scroll_node);
  if (SetScrollOffset(scroll_node->owner_id, new_offset))
    layer_tree_impl->DidUpdateScrollOffset(scroll_node->owner_id,
                                           scroll_node->data.transform_id);

  gfx::ScrollOffset unscrolled =
      old_offset + gfx::ScrollOffset(scroll) - new_offset;
  return gfx::Vector2dF(unscrolled.x(), unscrolled.y());
}

gfx::ScrollOffset ScrollTree::ClampScrollOffsetToLimits(
    gfx::ScrollOffset offset,
    ScrollNode* scroll_node) const {
  offset.SetToMin(MaxScrollOffset(scroll_node->id));
  offset.SetToMax(gfx::ScrollOffset());
  return offset;
}

PropertyTreesCachedData::PropertyTreesCachedData()
    : property_tree_update_number(0) {
  animation_scales.clear();
}

PropertyTreesCachedData::~PropertyTreesCachedData() {}

PropertyTrees::PropertyTrees()
    : needs_rebuild(true),
      non_root_surfaces_enabled(true),
      changed(false),
      full_tree_damaged(false),
      sequence_number(0),
      is_main_thread(true),
      is_active(false) {
  transform_tree.SetPropertyTrees(this);
  effect_tree.SetPropertyTrees(this);
  clip_tree.SetPropertyTrees(this);
  scroll_tree.SetPropertyTrees(this);
}

PropertyTrees::~PropertyTrees() {}

bool PropertyTrees::operator==(const PropertyTrees& other) const {
  return transform_tree == other.transform_tree &&
         effect_tree == other.effect_tree && clip_tree == other.clip_tree &&
         scroll_tree == other.scroll_tree &&
         transform_id_to_index_map == other.transform_id_to_index_map &&
         effect_id_to_index_map == other.effect_id_to_index_map &&
         clip_id_to_index_map == other.clip_id_to_index_map &&
         scroll_id_to_index_map == other.scroll_id_to_index_map &&
         always_use_active_tree_opacity_effect_ids ==
             other.always_use_active_tree_opacity_effect_ids &&
         needs_rebuild == other.needs_rebuild && changed == other.changed &&
         full_tree_damaged == other.full_tree_damaged &&
         is_main_thread == other.is_main_thread &&
         is_active == other.is_active &&
         non_root_surfaces_enabled == other.non_root_surfaces_enabled &&
         sequence_number == other.sequence_number;
}

PropertyTrees& PropertyTrees::operator=(const PropertyTrees& from) {
  transform_tree = from.transform_tree;
  effect_tree = from.effect_tree;
  clip_tree = from.clip_tree;
  scroll_tree = from.scroll_tree;
  transform_id_to_index_map = from.transform_id_to_index_map;
  effect_id_to_index_map = from.effect_id_to_index_map;
  always_use_active_tree_opacity_effect_ids =
      from.always_use_active_tree_opacity_effect_ids;
  clip_id_to_index_map = from.clip_id_to_index_map;
  scroll_id_to_index_map = from.scroll_id_to_index_map;
  needs_rebuild = from.needs_rebuild;
  changed = from.changed;
  full_tree_damaged = from.full_tree_damaged;
  non_root_surfaces_enabled = from.non_root_surfaces_enabled;
  sequence_number = from.sequence_number;
  is_main_thread = from.is_main_thread;
  is_active = from.is_active;
  inner_viewport_container_bounds_delta_ =
      from.inner_viewport_container_bounds_delta();
  outer_viewport_container_bounds_delta_ =
      from.outer_viewport_container_bounds_delta();
  inner_viewport_scroll_bounds_delta_ =
      from.inner_viewport_scroll_bounds_delta();
  transform_tree.SetPropertyTrees(this);
  effect_tree.SetPropertyTrees(this);
  clip_tree.SetPropertyTrees(this);
  scroll_tree.SetPropertyTrees(this);
  ResetCachedData();
  return *this;
}

void PropertyTrees::ToProtobuf(proto::PropertyTrees* proto) const {
  // TODO(khushalsagar): Add support for sending diffs when serializaing
  // property trees. See crbug/555370.
  transform_tree.ToProtobuf(proto->mutable_transform_tree());
  effect_tree.ToProtobuf(proto->mutable_effect_tree());
  clip_tree.ToProtobuf(proto->mutable_clip_tree());
  scroll_tree.ToProtobuf(proto->mutable_scroll_tree());
  proto->set_needs_rebuild(needs_rebuild);
  proto->set_changed(changed);
  proto->set_full_tree_damaged(full_tree_damaged);
  proto->set_non_root_surfaces_enabled(non_root_surfaces_enabled);
  proto->set_is_main_thread(is_main_thread);
  proto->set_is_active(is_active);

  // TODO(khushalsagar): Consider using the sequence number to decide if
  // property trees need to be serialized again for a commit. See crbug/555370.
  proto->set_sequence_number(sequence_number);

  for (auto i : always_use_active_tree_opacity_effect_ids)
    proto->add_always_use_active_tree_opacity_effect_ids(i);
}

// static
void PropertyTrees::FromProtobuf(const proto::PropertyTrees& proto) {
  transform_tree.FromProtobuf(proto.transform_tree(),
                              &transform_id_to_index_map);
  effect_tree.FromProtobuf(proto.effect_tree(), &effect_id_to_index_map);
  clip_tree.FromProtobuf(proto.clip_tree(), &clip_id_to_index_map);
  scroll_tree.FromProtobuf(proto.scroll_tree(), &scroll_id_to_index_map);

  needs_rebuild = proto.needs_rebuild();
  changed = proto.changed();
  full_tree_damaged = proto.full_tree_damaged();
  non_root_surfaces_enabled = proto.non_root_surfaces_enabled();
  sequence_number = proto.sequence_number();
  is_main_thread = proto.is_main_thread();
  is_active = proto.is_active();

  transform_tree.SetPropertyTrees(this);
  effect_tree.SetPropertyTrees(this);
  clip_tree.SetPropertyTrees(this);
  scroll_tree.SetPropertyTrees(this);
  for (auto i : proto.always_use_active_tree_opacity_effect_ids())
    always_use_active_tree_opacity_effect_ids.push_back(i);
}

void PropertyTrees::SetInnerViewportContainerBoundsDelta(
    gfx::Vector2dF bounds_delta) {
  if (inner_viewport_container_bounds_delta_ == bounds_delta)
    return;

  inner_viewport_container_bounds_delta_ = bounds_delta;
  transform_tree.UpdateInnerViewportContainerBoundsDelta();
}

void PropertyTrees::SetOuterViewportContainerBoundsDelta(
    gfx::Vector2dF bounds_delta) {
  if (outer_viewport_container_bounds_delta_ == bounds_delta)
    return;

  outer_viewport_container_bounds_delta_ = bounds_delta;
  transform_tree.UpdateOuterViewportContainerBoundsDelta();
}

void PropertyTrees::SetInnerViewportScrollBoundsDelta(
    gfx::Vector2dF bounds_delta) {
  inner_viewport_scroll_bounds_delta_ = bounds_delta;
}

void PropertyTrees::PushOpacityIfNeeded(PropertyTrees* target_tree) {
  for (int id : target_tree->always_use_active_tree_opacity_effect_ids) {
    if (effect_id_to_index_map.find(id) == effect_id_to_index_map.end())
      continue;
    EffectNode* source_effect_node =
        effect_tree.Node(effect_id_to_index_map[id]);
    EffectNode* target_effect_node =
        target_tree->effect_tree.Node(target_tree->effect_id_to_index_map[id]);
    float source_opacity = source_effect_node->data.opacity;
    float target_opacity = target_effect_node->data.opacity;
    if (source_opacity == target_opacity)
      continue;
    target_effect_node->data.opacity = source_opacity;
    target_tree->effect_tree.set_needs_update(true);
  }
}

void PropertyTrees::RemoveIdFromIdToIndexMaps(int id) {
  transform_id_to_index_map.erase(id);
  effect_id_to_index_map.erase(id);
  clip_id_to_index_map.erase(id);
  scroll_id_to_index_map.erase(id);
}

bool PropertyTrees::IsInIdToIndexMap(TreeType tree_type, int id) {
  std::unordered_map<int, int>* id_to_index_map = nullptr;
  switch (tree_type) {
    case TRANSFORM:
      id_to_index_map = &transform_id_to_index_map;
      break;
    case EFFECT:
      id_to_index_map = &effect_id_to_index_map;
      break;
    case CLIP:
      id_to_index_map = &clip_id_to_index_map;
      break;
    case SCROLL:
      id_to_index_map = &scroll_id_to_index_map;
      break;
  }
  return id_to_index_map->find(id) != id_to_index_map->end();
}

void PropertyTrees::UpdateChangeTracking() {
  for (int id = 1; id < static_cast<int>(effect_tree.size()); ++id) {
    EffectNode* node = effect_tree.Node(id);
    EffectNode* parent_node = effect_tree.parent(node);
    effect_tree.UpdateEffectChanged(node, parent_node);
  }
  for (int i = 1; i < static_cast<int>(transform_tree.size()); ++i) {
    TransformNode* node = transform_tree.Node(i);
    TransformNode* parent_node = transform_tree.parent(node);
    TransformNode* source_node = transform_tree.Node(node->data.source_node_id);
    transform_tree.UpdateTransformChanged(node, parent_node, source_node);
  }
}

void PropertyTrees::PushChangeTrackingTo(PropertyTrees* tree) {
  for (int id = 1; id < static_cast<int>(effect_tree.size()); ++id) {
    EffectNode* node = effect_tree.Node(id);
    if (node->data.effect_changed) {
      EffectNode* target_node = tree->effect_tree.Node(node->id);
      target_node->data.effect_changed = true;
    }
  }
  for (int id = 1; id < static_cast<int>(transform_tree.size()); ++id) {
    TransformNode* node = transform_tree.Node(id);
    if (node->data.transform_changed) {
      TransformNode* target_node = tree->transform_tree.Node(node->id);
      target_node->data.transform_changed = true;
    }
  }
  // Ensure that change tracking is updated even if property trees don't have
  // other reasons to get updated.
  tree->UpdateChangeTracking();
  tree->full_tree_damaged = full_tree_damaged;
}

void PropertyTrees::ResetAllChangeTracking() {
  transform_tree.ResetChangeTracking();
  effect_tree.ResetChangeTracking();
  changed = false;
  full_tree_damaged = false;
}

std::unique_ptr<base::trace_event::TracedValue> PropertyTrees::AsTracedValue()
    const {
  auto value = base::WrapUnique(new base::trace_event::TracedValue);

  value->SetInteger("sequence_number", sequence_number);

  value->BeginDictionary("transform_tree");
  transform_tree.AsValueInto(value.get());
  value->EndDictionary();

  value->BeginDictionary("effect_tree");
  effect_tree.AsValueInto(value.get());
  value->EndDictionary();

  value->BeginDictionary("clip_tree");
  clip_tree.AsValueInto(value.get());
  value->EndDictionary();

  value->BeginDictionary("scroll_tree");
  scroll_tree.AsValueInto(value.get());
  value->EndDictionary();

  return value;
}

CombinedAnimationScale PropertyTrees::GetAnimationScales(
    int transform_node_id,
    LayerTreeImpl* layer_tree_impl) {
  if (cached_data_.animation_scales[transform_node_id].update_number !=
      cached_data_.property_tree_update_number) {
    if (!layer_tree_impl->settings()
             .layer_transforms_should_scale_layer_contents) {
      cached_data_.animation_scales[transform_node_id].update_number =
          cached_data_.property_tree_update_number;
      cached_data_.animation_scales[transform_node_id]
          .combined_maximum_animation_target_scale = 0.f;
      cached_data_.animation_scales[transform_node_id]
          .combined_starting_animation_scale = 0.f;
      return CombinedAnimationScale(
          cached_data_.animation_scales[transform_node_id]
              .combined_maximum_animation_target_scale,
          cached_data_.animation_scales[transform_node_id]
              .combined_starting_animation_scale);
    }

    TransformNode* node = transform_tree.Node(transform_node_id);
    TransformNode* parent_node = transform_tree.parent(node);
    bool ancestor_is_animating_scale = false;
    float ancestor_maximum_target_scale = 0.f;
    float ancestor_starting_animation_scale = 0.f;
    if (parent_node) {
      CombinedAnimationScale combined_animation_scale =
          GetAnimationScales(parent_node->id, layer_tree_impl);
      ancestor_maximum_target_scale =
          combined_animation_scale.maximum_animation_scale;
      ancestor_starting_animation_scale =
          combined_animation_scale.starting_animation_scale;
      ancestor_is_animating_scale =
          cached_data_.animation_scales[parent_node->id]
              .to_screen_has_scale_animation;
    }

    cached_data_.animation_scales[transform_node_id]
        .to_screen_has_scale_animation =
        !node->data.has_only_translation_animations ||
        ancestor_is_animating_scale;

    // Once we've failed to compute a maximum animated scale at an ancestor, we
    // continue to fail.
    bool failed_at_ancestor =
        ancestor_is_animating_scale && ancestor_maximum_target_scale == 0.f;

    // Computing maximum animated scale in the presence of non-scale/translation
    // transforms isn't supported.
    bool failed_for_non_scale_or_translation =
        !transform_tree.ToTarget(transform_node_id).IsScaleOrTranslation();

    // We don't attempt to accumulate animation scale from multiple nodes with
    // scale animations, because of the risk of significant overestimation. For
    // example, one node might be increasing scale from 1 to 10 at the same time
    // as another node is decreasing scale from 10 to 1. Naively combining these
    // scales would produce a scale of 100.
    bool failed_for_multiple_scale_animations =
        ancestor_is_animating_scale &&
        !node->data.has_only_translation_animations;

    if (failed_at_ancestor || failed_for_non_scale_or_translation ||
        failed_for_multiple_scale_animations) {
      // This ensures that descendants know we've failed to compute a maximum
      // animated scale.
      cached_data_.animation_scales[transform_node_id]
          .to_screen_has_scale_animation = true;

      cached_data_.animation_scales[transform_node_id]
          .combined_maximum_animation_target_scale = 0.f;
      cached_data_.animation_scales[transform_node_id]
          .combined_starting_animation_scale = 0.f;
    } else if (!cached_data_.animation_scales[transform_node_id]
                    .to_screen_has_scale_animation) {
      cached_data_.animation_scales[transform_node_id]
          .combined_maximum_animation_target_scale = 0.f;
      cached_data_.animation_scales[transform_node_id]
          .combined_starting_animation_scale = 0.f;
    } else if (node->data.has_only_translation_animations) {
      // An ancestor is animating scale.
      gfx::Vector2dF local_scales =
          MathUtil::ComputeTransform2dScaleComponents(node->data.local, 0.f);
      float max_local_scale = std::max(local_scales.x(), local_scales.y());
      cached_data_.animation_scales[transform_node_id]
          .combined_maximum_animation_target_scale =
          max_local_scale * ancestor_maximum_target_scale;
      cached_data_.animation_scales[transform_node_id]
          .combined_starting_animation_scale =
          max_local_scale * ancestor_starting_animation_scale;
    } else {
      // TODO(sunxd): make LayerTreeImpl::MaximumTargetScale take layer id as
      // parameter.
      LayerImpl* layer_impl = layer_tree_impl->LayerById(node->owner_id);
      layer_tree_impl->MaximumTargetScale(
          layer_impl, &cached_data_.animation_scales[transform_node_id]
                           .local_maximum_animation_target_scale);
      layer_tree_impl->AnimationStartScale(
          layer_impl, &cached_data_.animation_scales[transform_node_id]
                           .local_starting_animation_scale);
      gfx::Vector2dF local_scales =
          MathUtil::ComputeTransform2dScaleComponents(node->data.local, 0.f);
      float max_local_scale = std::max(local_scales.x(), local_scales.y());

      if (cached_data_.animation_scales[transform_node_id]
                  .local_starting_animation_scale == 0.f ||
          cached_data_.animation_scales[transform_node_id]
                  .local_maximum_animation_target_scale == 0.f) {
        cached_data_.animation_scales[transform_node_id]
            .combined_maximum_animation_target_scale =
            max_local_scale * ancestor_maximum_target_scale;
        cached_data_.animation_scales[transform_node_id]
            .combined_starting_animation_scale =
            max_local_scale * ancestor_starting_animation_scale;
      } else {
        gfx::Vector2dF ancestor_scales =
            parent_node ? MathUtil::ComputeTransform2dScaleComponents(
                              transform_tree.ToTarget(parent_node->id), 0.f)
                        : gfx::Vector2dF(1.f, 1.f);
        float max_ancestor_scale =
            std::max(ancestor_scales.x(), ancestor_scales.y());
        cached_data_.animation_scales[transform_node_id]
            .combined_maximum_animation_target_scale =
            max_ancestor_scale *
            cached_data_.animation_scales[transform_node_id]
                .local_maximum_animation_target_scale;
        cached_data_.animation_scales[transform_node_id]
            .combined_starting_animation_scale =
            max_ancestor_scale *
            cached_data_.animation_scales[transform_node_id]
                .local_starting_animation_scale;
      }
    }
    cached_data_.animation_scales[transform_node_id].update_number =
        cached_data_.property_tree_update_number;
  }
  return CombinedAnimationScale(cached_data_.animation_scales[transform_node_id]
                                    .combined_maximum_animation_target_scale,
                                cached_data_.animation_scales[transform_node_id]
                                    .combined_starting_animation_scale);
}

void PropertyTrees::SetAnimationScalesForTesting(
    int transform_id,
    float maximum_animation_scale,
    float starting_animation_scale) {
  cached_data_.animation_scales[transform_id]
      .combined_maximum_animation_target_scale = maximum_animation_scale;
  cached_data_.animation_scales[transform_id]
      .combined_starting_animation_scale = starting_animation_scale;
  cached_data_.animation_scales[transform_id].update_number =
      cached_data_.property_tree_update_number;
}

void PropertyTrees::ResetCachedData() {
  cached_data_.property_tree_update_number = 0;
  cached_data_.animation_scales = std::vector<AnimationScaleData>(
      transform_tree.nodes().size(), AnimationScaleData());
}

void PropertyTrees::UpdateCachedNumber() {
  cached_data_.property_tree_update_number++;
}

}  // namespace cc
