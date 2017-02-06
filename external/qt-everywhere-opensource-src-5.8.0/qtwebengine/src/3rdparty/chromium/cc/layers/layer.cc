// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/atomic_sequence_num.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/mutable_properties.h"
#include "cc/base/simple_enclosed_region.h"
#include "cc/debug/frame_viewer_instrumentation.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/layer_client.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/layer_proto_converter.h"
#include "cc/layers/scrollbar_layer_interface.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/proto/cc_conversions.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/layer.pb.h"
#include "cc/proto/skia_conversions.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {

base::StaticAtomicSequenceNumber g_next_layer_id;

scoped_refptr<Layer> Layer::Create() {
  return make_scoped_refptr(new Layer());
}

Layer::Layer()
    :  // Layer IDs start from 1.
      layer_id_(g_next_layer_id.GetNext() + 1),
      ignore_set_needs_commit_(false),
      sorting_context_id_(0),
      parent_(nullptr),
      layer_tree_host_(nullptr),
      scroll_clip_layer_id_(INVALID_ID),
      num_descendants_that_draw_content_(0),
      transform_tree_index_(-1),
      effect_tree_index_(-1),
      clip_tree_index_(-1),
      scroll_tree_index_(-1),
      property_tree_sequence_number_(-1),
      mutable_properties_(MutableProperty::kNone),
      main_thread_scrolling_reasons_(
          MainThreadScrollingReason::kNotScrollingOnMain),
      should_flatten_transform_from_property_tree_(false),
      user_scrollable_horizontal_(true),
      user_scrollable_vertical_(true),
      is_root_for_isolated_group_(false),
      is_container_for_fixed_position_layers_(false),
      is_drawable_(false),
      draws_content_(false),
      hide_layer_and_subtree_(false),
      masks_to_bounds_(false),
      contents_opaque_(false),
      double_sided_(true),
      should_flatten_transform_(true),
      use_parent_backface_visibility_(false),
      use_local_transform_for_backface_visibility_(false),
      should_check_backface_visibility_(false),
      force_render_surface_for_testing_(false),
      subtree_property_changed_(false),
      layer_property_changed_(false),
      has_will_change_transform_hint_(false),
      background_color_(0),
      safe_opaque_background_color_(0),
      opacity_(1.f),
      blend_mode_(SkXfermode::kSrcOver_Mode),
      draw_blend_mode_(SkXfermode::kSrcOver_Mode),
      scroll_parent_(nullptr),
      clip_parent_(nullptr),
      replica_layer_(nullptr),
      client_(nullptr),
      num_unclipped_descendants_(0) {}

Layer::~Layer() {
  // Our parent should be holding a reference to us so there should be no
  // way for us to be destroyed while we still have a parent.
  DCHECK(!parent());
  // Similarly we shouldn't have a layer tree host since it also keeps a
  // reference to us.
  DCHECK(!layer_tree_host());

  RemoveFromScrollTree();
  RemoveFromClipTree();

  // Remove the parent reference from all children and dependents.
  RemoveAllChildren();
  if (mask_layer_.get()) {
    DCHECK_EQ(this, mask_layer_->parent());
    mask_layer_->RemoveFromParent();
  }
  if (replica_layer_.get()) {
    DCHECK_EQ(this, replica_layer_->parent());
    replica_layer_->RemoveFromParent();
  }
}

void Layer::SetLayerTreeHost(LayerTreeHost* host) {
  if (layer_tree_host_ == host)
    return;

  if (layer_tree_host_) {
    layer_tree_host_->property_trees()->RemoveIdFromIdToIndexMaps(id());
    layer_tree_host_->property_trees()->needs_rebuild = true;
    layer_tree_host_->UnregisterLayer(this);
    if (element_id_) {
      layer_tree_host_->animation_host()->UnregisterElement(
          element_id_, ElementListType::ACTIVE);
      layer_tree_host_->RemoveFromElementMap(this);
    }
  }
  if (host) {
    host->property_trees()->needs_rebuild = true;
    host->RegisterLayer(this);
    if (element_id_) {
      host->AddToElementMap(this);
      host->animation_host()->RegisterElement(element_id_,
                                              ElementListType::ACTIVE);
    }
  }

  layer_tree_host_ = host;
  InvalidatePropertyTreesIndices();

  // When changing hosts, the layer needs to commit its properties to the impl
  // side for the new host.
  SetNeedsPushProperties();

  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->SetLayerTreeHost(host);

  if (mask_layer_.get())
    mask_layer_->SetLayerTreeHost(host);
  if (replica_layer_.get())
    replica_layer_->SetLayerTreeHost(host);

  const bool has_any_animation =
      layer_tree_host_ ? layer_tree_host_->HasAnyAnimation(this) : false;

  if (host && has_any_animation)
    host->SetNeedsCommit();
}

void Layer::SetNeedsUpdate() {
  if (layer_tree_host_ && !ignore_set_needs_commit_)
    layer_tree_host_->SetNeedsUpdateLayers();
}

void Layer::SetNeedsCommit() {
  if (!layer_tree_host_)
    return;

  SetNeedsPushProperties();
  layer_tree_host_->property_trees()->needs_rebuild = true;

  if (ignore_set_needs_commit_)
    return;

  layer_tree_host_->SetNeedsCommit();
}

void Layer::SetNeedsCommitNoRebuild() {
  if (!layer_tree_host_)
    return;

  SetNeedsPushProperties();

  if (ignore_set_needs_commit_)
    return;

  layer_tree_host_->SetNeedsCommit();
}

void Layer::SetNeedsFullTreeSync() {
  if (!layer_tree_host_)
    return;

  layer_tree_host_->SetNeedsFullTreeSync();
}

void Layer::SetNextCommitWaitsForActivation() {
  if (!layer_tree_host_)
    return;

  layer_tree_host_->SetNextCommitWaitsForActivation();
}

void Layer::SetNeedsPushProperties() {
  if (layer_tree_host_)
    layer_tree_host_->AddLayerShouldPushProperties(this);
}

void Layer::ResetNeedsPushPropertiesForTesting() {
  layer_tree_host_->RemoveLayerShouldPushProperties(this);
}

bool Layer::IsPropertyChangeAllowed() const {
  if (!layer_tree_host_)
    return true;

  return !layer_tree_host_->in_paint_layer_contents();
}

sk_sp<SkPicture> Layer::GetPicture() const {
  return nullptr;
}

void Layer::SetParent(Layer* layer) {
  DCHECK(!layer || !layer->HasAncestor(this));

  parent_ = layer;
  SetLayerTreeHost(parent_ ? parent_->layer_tree_host() : nullptr);

  if (!layer_tree_host_)
    return;

  layer_tree_host_->property_trees()->needs_rebuild = true;
}

void Layer::AddChild(scoped_refptr<Layer> child) {
  InsertChild(child, children_.size());
}

void Layer::InsertChild(scoped_refptr<Layer> child, size_t index) {
  DCHECK(IsPropertyChangeAllowed());
  child->RemoveFromParent();
  AddDrawableDescendants(child->NumDescendantsThatDrawContent() +
                         (child->DrawsContent() ? 1 : 0));
  child->SetParent(this);
  child->SetSubtreePropertyChanged();

  index = std::min(index, children_.size());
  children_.insert(children_.begin() + index, child);
  SetNeedsFullTreeSync();
}

void Layer::RemoveFromParent() {
  DCHECK(IsPropertyChangeAllowed());
  if (parent_)
    parent_->RemoveChildOrDependent(this);
}

void Layer::RemoveChildOrDependent(Layer* child) {
  if (mask_layer_.get() == child) {
    mask_layer_->SetParent(nullptr);
    mask_layer_ = nullptr;
    SetNeedsFullTreeSync();
    return;
  }
  if (replica_layer_.get() == child) {
    replica_layer_->SetParent(nullptr);
    replica_layer_ = nullptr;
    SetNeedsFullTreeSync();
    return;
  }

  for (LayerList::iterator iter = children_.begin();
       iter != children_.end();
       ++iter) {
    if (iter->get() != child)
      continue;

    child->SetParent(nullptr);
    AddDrawableDescendants(-child->NumDescendantsThatDrawContent() -
                           (child->DrawsContent() ? 1 : 0));
    children_.erase(iter);
    SetNeedsFullTreeSync();
    return;
  }
}

void Layer::ReplaceChild(Layer* reference, scoped_refptr<Layer> new_layer) {
  DCHECK(reference);
  DCHECK_EQ(reference->parent(), this);
  DCHECK(IsPropertyChangeAllowed());

  if (reference == new_layer.get())
    return;

  // Find the index of |reference| in |children_|.
  auto reference_it =
      std::find_if(children_.begin(), children_.end(),
                   [reference](const scoped_refptr<Layer>& layer) {
                     return layer.get() == reference;
                   });
  DCHECK(reference_it != children_.end());
  size_t reference_index = reference_it - children_.begin();
  reference->RemoveFromParent();

  if (new_layer.get()) {
    new_layer->RemoveFromParent();
    InsertChild(new_layer, reference_index);
  }
}

void Layer::SetBounds(const gfx::Size& size) {
  DCHECK(IsPropertyChangeAllowed());
  if (bounds() == size)
    return;
  bounds_ = size;

  if (!layer_tree_host_)
    return;

  if (masks_to_bounds())
    SetSubtreePropertyChanged();
  SetNeedsCommit();
}

Layer* Layer::RootLayer() {
  Layer* layer = this;
  while (layer->parent())
    layer = layer->parent();
  return layer;
}

void Layer::RemoveAllChildren() {
  DCHECK(IsPropertyChangeAllowed());
  while (children_.size()) {
    Layer* layer = children_[0].get();
    DCHECK_EQ(this, layer->parent());
    layer->RemoveFromParent();
  }
}

void Layer::SetChildren(const LayerList& children) {
  DCHECK(IsPropertyChangeAllowed());
  if (children == children_)
    return;

  RemoveAllChildren();
  for (size_t i = 0; i < children.size(); ++i)
    AddChild(children[i]);
}

bool Layer::HasAncestor(const Layer* ancestor) const {
  for (const Layer* layer = parent(); layer; layer = layer->parent()) {
    if (layer == ancestor)
      return true;
  }
  return false;
}

void Layer::RequestCopyOfOutput(std::unique_ptr<CopyOutputRequest> request) {
  DCHECK(IsPropertyChangeAllowed());
  if (void* source = request->source()) {
    auto it =
        std::find_if(copy_requests_.begin(), copy_requests_.end(),
                     [source](const std::unique_ptr<CopyOutputRequest>& x) {
                       return x->source() == source;
                     });
    if (it != copy_requests_.end())
      copy_requests_.erase(it);
  }
  if (request->IsEmpty())
    return;
  copy_requests_.push_back(std::move(request));
  SetSubtreePropertyChanged();
  SetNeedsCommit();
}

void Layer::SetBackgroundColor(SkColor background_color) {
  DCHECK(IsPropertyChangeAllowed());
  if (background_color_ == background_color)
    return;
  background_color_ = background_color;
  SetNeedsCommit();
}

void Layer::SetSafeOpaqueBackgroundColor(SkColor background_color) {
  DCHECK(IsPropertyChangeAllowed());
  if (safe_opaque_background_color_ == background_color)
    return;
  safe_opaque_background_color_ = background_color;
  SetNeedsPushProperties();
}

SkColor Layer::SafeOpaqueBackgroundColor() const {
  if (contents_opaque())
    return safe_opaque_background_color_;
  SkColor color = background_color();
  if (SkColorGetA(color) == 255)
    color = SK_ColorTRANSPARENT;
  return color;
}

void Layer::SetMasksToBounds(bool masks_to_bounds) {
  DCHECK(IsPropertyChangeAllowed());
  if (masks_to_bounds_ == masks_to_bounds)
    return;
  masks_to_bounds_ = masks_to_bounds;
  SetNeedsCommit();
  SetSubtreePropertyChanged();
}

void Layer::SetMaskLayer(Layer* mask_layer) {
  DCHECK(IsPropertyChangeAllowed());
  if (mask_layer_.get() == mask_layer)
    return;
  if (mask_layer_.get()) {
    DCHECK_EQ(this, mask_layer_->parent());
    mask_layer_->RemoveFromParent();
  }
  mask_layer_ = mask_layer;
  if (mask_layer_.get()) {
    mask_layer_->RemoveFromParent();
    DCHECK(!mask_layer_->parent());
    mask_layer_->SetParent(this);
    mask_layer_->SetIsMask(true);
  }
  SetSubtreePropertyChanged();
  SetNeedsFullTreeSync();
}

void Layer::SetReplicaLayer(Layer* layer) {
  DCHECK(IsPropertyChangeAllowed());
  if (replica_layer_.get() == layer)
    return;
  if (replica_layer_.get()) {
    DCHECK_EQ(this, replica_layer_->parent());
    replica_layer_->RemoveFromParent();
  }
  replica_layer_ = layer;
  if (replica_layer_.get()) {
    DCHECK(!replica_layer_->parent());
    replica_layer_->RemoveFromParent();
    replica_layer_->SetParent(this);
  }
  SetSubtreePropertyChanged();
  SetNeedsFullTreeSync();
}

void Layer::SetFilters(const FilterOperations& filters) {
  DCHECK(IsPropertyChangeAllowed());
  if (filters_ == filters)
    return;
  filters_ = filters;
  SetSubtreePropertyChanged();
  SetNeedsCommit();
}

bool Layer::FilterIsAnimating() const {
  DCHECK(layer_tree_host_);
  return layer_tree_host_->IsAnimatingFilterProperty(this);
}

bool Layer::HasPotentiallyRunningFilterAnimation() const {
  return layer_tree_host_->HasPotentiallyRunningFilterAnimation(this);
}

void Layer::SetBackgroundFilters(const FilterOperations& filters) {
  DCHECK(IsPropertyChangeAllowed());
  if (background_filters_ == filters)
    return;
  background_filters_ = filters;
  SetLayerPropertyChanged();
  SetNeedsCommit();
}

void Layer::SetOpacity(float opacity) {
  DCHECK(IsPropertyChangeAllowed());
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);

  if (opacity_ == opacity)
    return;
  // We need to force a property tree rebuild when opacity changes from 1 to a
  // non-1 value or vice-versa as render surfaces can change.
  bool force_rebuild = opacity == 1.f || opacity_ == 1.f;
  opacity_ = opacity;
  SetSubtreePropertyChanged();
  if (layer_tree_host_ && !force_rebuild) {
    PropertyTrees* property_trees = layer_tree_host_->property_trees();
    auto effect_id_to_index = property_trees->effect_id_to_index_map.find(id());
    if (effect_id_to_index != property_trees->effect_id_to_index_map.end()) {
      EffectNode* node =
          property_trees->effect_tree.Node(effect_id_to_index->second);
      node->data.opacity = opacity;
      node->data.effect_changed = true;
      property_trees->effect_tree.set_needs_update(true);
      SetNeedsCommitNoRebuild();
      return;
    }
  }
  SetNeedsCommit();
}

float Layer::EffectiveOpacity() const {
  return hide_layer_and_subtree_ ? 0.f : opacity_;
}

bool Layer::OpacityIsAnimating() const {
  DCHECK(layer_tree_host_);
  return layer_tree_host_->IsAnimatingOpacityProperty(this);
}

bool Layer::HasPotentiallyRunningOpacityAnimation() const {
  return layer_tree_host_->HasPotentiallyRunningOpacityAnimation(this);
}

bool Layer::OpacityCanAnimateOnImplThread() const {
  return false;
}

bool Layer::AlwaysUseActiveTreeOpacity() const {
  return false;
}

void Layer::SetBlendMode(SkXfermode::Mode blend_mode) {
  DCHECK(IsPropertyChangeAllowed());
  if (blend_mode_ == blend_mode)
    return;

  // Allowing only blend modes that are defined in the CSS Compositing standard:
  // http://dev.w3.org/fxtf/compositing-1/#blending
  switch (blend_mode) {
    case SkXfermode::kSrcOver_Mode:
    case SkXfermode::kScreen_Mode:
    case SkXfermode::kOverlay_Mode:
    case SkXfermode::kDarken_Mode:
    case SkXfermode::kLighten_Mode:
    case SkXfermode::kColorDodge_Mode:
    case SkXfermode::kColorBurn_Mode:
    case SkXfermode::kHardLight_Mode:
    case SkXfermode::kSoftLight_Mode:
    case SkXfermode::kDifference_Mode:
    case SkXfermode::kExclusion_Mode:
    case SkXfermode::kMultiply_Mode:
    case SkXfermode::kHue_Mode:
    case SkXfermode::kSaturation_Mode:
    case SkXfermode::kColor_Mode:
    case SkXfermode::kLuminosity_Mode:
      // supported blend modes
      break;
    case SkXfermode::kClear_Mode:
    case SkXfermode::kSrc_Mode:
    case SkXfermode::kDst_Mode:
    case SkXfermode::kDstOver_Mode:
    case SkXfermode::kSrcIn_Mode:
    case SkXfermode::kDstIn_Mode:
    case SkXfermode::kSrcOut_Mode:
    case SkXfermode::kDstOut_Mode:
    case SkXfermode::kSrcATop_Mode:
    case SkXfermode::kDstATop_Mode:
    case SkXfermode::kXor_Mode:
    case SkXfermode::kPlus_Mode:
    case SkXfermode::kModulate_Mode:
      // Porter Duff Compositing Operators are not yet supported
      // http://dev.w3.org/fxtf/compositing-1/#porterduffcompositingoperators
      NOTREACHED();
      return;
  }

  blend_mode_ = blend_mode;
  SetNeedsCommit();
  SetSubtreePropertyChanged();
}

void Layer::SetIsRootForIsolatedGroup(bool root) {
  DCHECK(IsPropertyChangeAllowed());
  if (is_root_for_isolated_group_ == root)
    return;
  is_root_for_isolated_group_ = root;
  SetNeedsCommit();
}

void Layer::SetContentsOpaque(bool opaque) {
  DCHECK(IsPropertyChangeAllowed());
  if (contents_opaque_ == opaque)
    return;
  contents_opaque_ = opaque;
  SetNeedsCommit();
  SetSubtreePropertyChanged();
}

void Layer::SetPosition(const gfx::PointF& position) {
  DCHECK(IsPropertyChangeAllowed());
  if (position_ == position)
    return;
  position_ = position;

  if (!layer_tree_host_)
    return;

  SetSubtreePropertyChanged();
  PropertyTrees* property_trees = layer_tree_host_->property_trees();
  if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::TRANSFORM,
                                       id())) {
    DCHECK_EQ(transform_tree_index(),
              property_trees->transform_id_to_index_map[id()]);
    TransformNode* transform_node =
        property_trees->transform_tree.Node(transform_tree_index());
    transform_node->data.update_post_local_transform(position,
                                                     transform_origin());
    transform_node->data.needs_local_transform_update = true;
    transform_node->data.transform_changed = true;
    layer_tree_host_->property_trees()->transform_tree.set_needs_update(true);
    SetNeedsCommitNoRebuild();
    return;
  }

  SetNeedsCommit();
}

bool Layer::IsContainerForFixedPositionLayers() const {
  if (!transform_.IsIdentityOrTranslation())
    return true;
  if (parent_ && !parent_->transform_.IsIdentityOrTranslation())
    return true;
  return is_container_for_fixed_position_layers_;
}

bool Are2dAxisAligned(const gfx::Transform& a, const gfx::Transform& b) {
  if (a.IsScaleOrTranslation() && b.IsScaleOrTranslation()) {
    return true;
  }

  gfx::Transform inverse(gfx::Transform::kSkipInitialization);
  if (b.GetInverse(&inverse)) {
    inverse *= a;
    return inverse.Preserves2dAxisAlignment();
  } else {
    // TODO(weiliangc): Should return false because b is not invertible.
    return a.Preserves2dAxisAlignment();
  }
}

void Layer::SetTransform(const gfx::Transform& transform) {
  DCHECK(IsPropertyChangeAllowed());
  if (transform_ == transform)
    return;

  SetSubtreePropertyChanged();
  if (layer_tree_host_) {
    PropertyTrees* property_trees = layer_tree_host_->property_trees();
    if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::TRANSFORM,
                                         id())) {
      // We need to trigger a rebuild if we could have affected 2d axis
      // alignment. We'll check to see if transform and transform_ are axis
      // align with respect to one another.
      DCHECK_EQ(transform_tree_index(),
                property_trees->transform_id_to_index_map[id()]);
      TransformNode* transform_node =
          property_trees->transform_tree.Node(transform_tree_index());
      bool preserves_2d_axis_alignment =
          Are2dAxisAligned(transform_, transform);
      transform_node->data.local = transform;
      transform_node->data.needs_local_transform_update = true;
      transform_node->data.transform_changed = true;
      layer_tree_host_->property_trees()->transform_tree.set_needs_update(true);
      if (preserves_2d_axis_alignment)
        SetNeedsCommitNoRebuild();
      else
        SetNeedsCommit();
      transform_ = transform;
      return;
    }
  }

  transform_ = transform;

  SetNeedsCommit();
}

void Layer::SetTransformOrigin(const gfx::Point3F& transform_origin) {
  DCHECK(IsPropertyChangeAllowed());
  if (transform_origin_ == transform_origin)
    return;
  transform_origin_ = transform_origin;

  if (!layer_tree_host_)
    return;

  SetSubtreePropertyChanged();
  PropertyTrees* property_trees = layer_tree_host_->property_trees();
  if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::TRANSFORM,
                                       id())) {
    DCHECK_EQ(transform_tree_index(),
              property_trees->transform_id_to_index_map[id()]);
    TransformNode* transform_node =
        property_trees->transform_tree.Node(transform_tree_index());
    transform_node->data.update_pre_local_transform(transform_origin);
    transform_node->data.update_post_local_transform(position(),
                                                     transform_origin);
    transform_node->data.needs_local_transform_update = true;
    transform_node->data.transform_changed = true;
    layer_tree_host_->property_trees()->transform_tree.set_needs_update(true);
    SetNeedsCommitNoRebuild();
    return;
  }

  SetNeedsCommit();
}

bool Layer::AnimationsPreserveAxisAlignment() const {
  DCHECK(layer_tree_host_);
  return layer_tree_host_->AnimationsPreserveAxisAlignment(this);
}

bool Layer::TransformIsAnimating() const {
  DCHECK(layer_tree_host_);
  return layer_tree_host_->IsAnimatingTransformProperty(this);
}

bool Layer::HasPotentiallyRunningTransformAnimation() const {
  return layer_tree_host_->HasPotentiallyRunningTransformAnimation(this);
}

bool Layer::HasOnlyTranslationTransforms() const {
  return layer_tree_host_->HasOnlyTranslationTransforms(this);
}

bool Layer::MaximumTargetScale(float* max_scale) const {
  return layer_tree_host_->MaximumTargetScale(this, max_scale);
}

bool Layer::AnimationStartScale(float* start_scale) const {
  return layer_tree_host_->AnimationStartScale(this, start_scale);
}

bool Layer::HasAnyAnimationTargetingProperty(
    TargetProperty::Type property) const {
  return layer_tree_host_->HasAnyAnimationTargetingProperty(this, property);
}

bool Layer::ScrollOffsetAnimationWasInterrupted() const {
  DCHECK(layer_tree_host_);
  return layer_tree_host_->ScrollOffsetAnimationWasInterrupted(this);
}

void Layer::SetScrollParent(Layer* parent) {
  DCHECK(IsPropertyChangeAllowed());
  if (scroll_parent_ == parent)
    return;

  if (scroll_parent_)
    scroll_parent_->RemoveScrollChild(this);

  scroll_parent_ = parent;

  if (scroll_parent_)
    scroll_parent_->AddScrollChild(this);

  SetNeedsCommit();
}

void Layer::AddScrollChild(Layer* child) {
  if (!scroll_children_)
    scroll_children_.reset(new std::set<Layer*>);
  scroll_children_->insert(child);
  SetNeedsCommit();
}

void Layer::RemoveScrollChild(Layer* child) {
  scroll_children_->erase(child);
  if (scroll_children_->empty())
    scroll_children_ = nullptr;
  SetNeedsCommit();
}

void Layer::SetClipParent(Layer* ancestor) {
  DCHECK(IsPropertyChangeAllowed());
  if (clip_parent_ == ancestor)
    return;

  if (clip_parent_)
    clip_parent_->RemoveClipChild(this);

  clip_parent_ = ancestor;

  if (clip_parent_)
    clip_parent_->AddClipChild(this);

  SetNeedsCommit();
  if (layer_tree_host_)
    layer_tree_host_->SetNeedsMetaInfoRecomputation(true);
}

void Layer::AddClipChild(Layer* child) {
  if (!clip_children_)
    clip_children_.reset(new std::set<Layer*>);
  clip_children_->insert(child);
  SetNeedsCommit();
}

void Layer::RemoveClipChild(Layer* child) {
  clip_children_->erase(child);
  if (clip_children_->empty())
    clip_children_ = nullptr;
  SetNeedsCommit();
}

void Layer::SetScrollOffset(const gfx::ScrollOffset& scroll_offset) {
  DCHECK(IsPropertyChangeAllowed());

  if (scroll_offset_ == scroll_offset)
    return;
  scroll_offset_ = scroll_offset;

  if (!layer_tree_host_)
    return;

  PropertyTrees* property_trees = layer_tree_host_->property_trees();
  if (scroll_tree_index() != -1 && scrollable())
    property_trees->scroll_tree.SetScrollOffset(id(), scroll_offset);

  if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::TRANSFORM,
                                       id())) {
    DCHECK_EQ(transform_tree_index(),
              property_trees->transform_id_to_index_map[id()]);
    TransformNode* transform_node =
        property_trees->transform_tree.Node(transform_tree_index());
    transform_node->data.scroll_offset = CurrentScrollOffset();
    transform_node->data.needs_local_transform_update = true;
    property_trees->transform_tree.set_needs_update(true);
    SetNeedsCommitNoRebuild();
    return;
  }

  SetNeedsCommit();
}

void Layer::SetScrollOffsetFromImplSide(
    const gfx::ScrollOffset& scroll_offset) {
  DCHECK(IsPropertyChangeAllowed());
  // This function only gets called during a BeginMainFrame, so there
  // is no need to call SetNeedsUpdate here.
  DCHECK(layer_tree_host_ && layer_tree_host_->CommitRequested());
  if (scroll_offset_ == scroll_offset)
    return;
  scroll_offset_ = scroll_offset;
  SetNeedsPushProperties();

  bool needs_rebuild = true;

  PropertyTrees* property_trees = layer_tree_host_->property_trees();
  if (scroll_tree_index() != -1 && scrollable())
    property_trees->scroll_tree.SetScrollOffset(id(), scroll_offset);

  if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::TRANSFORM,
                                       id())) {
    DCHECK_EQ(transform_tree_index(),
              property_trees->transform_id_to_index_map[id()]);
    TransformNode* transform_node =
        property_trees->transform_tree.Node(transform_tree_index());
    transform_node->data.scroll_offset = CurrentScrollOffset();
    transform_node->data.needs_local_transform_update = true;
    property_trees->transform_tree.set_needs_update(true);
    needs_rebuild = false;
  }

  if (needs_rebuild)
    property_trees->needs_rebuild = true;

  if (!did_scroll_callback_.is_null())
    did_scroll_callback_.Run();
  // The callback could potentially change the layer structure:
  // "this" may have been destroyed during the process.
}

void Layer::SetScrollClipLayerId(int clip_layer_id) {
  DCHECK(IsPropertyChangeAllowed());
  if (scroll_clip_layer_id_ == clip_layer_id)
    return;
  scroll_clip_layer_id_ = clip_layer_id;
  SetNeedsCommit();
}

Layer* Layer::scroll_clip_layer() const {
  return layer_tree_host()->LayerById(scroll_clip_layer_id_);
}

void Layer::SetUserScrollable(bool horizontal, bool vertical) {
  DCHECK(IsPropertyChangeAllowed());
  if (user_scrollable_horizontal_ == horizontal &&
      user_scrollable_vertical_ == vertical)
    return;
  user_scrollable_horizontal_ = horizontal;
  user_scrollable_vertical_ = vertical;
  SetNeedsCommit();
}

void Layer::AddMainThreadScrollingReasons(
    uint32_t main_thread_scrolling_reasons) {
  DCHECK(IsPropertyChangeAllowed());
  DCHECK(main_thread_scrolling_reasons);
  uint32_t new_reasons =
      main_thread_scrolling_reasons_ | main_thread_scrolling_reasons;
  if (main_thread_scrolling_reasons_ == new_reasons)
    return;
  main_thread_scrolling_reasons_ = new_reasons;
  SetNeedsCommit();
}

void Layer::ClearMainThreadScrollingReasons(
    uint32_t main_thread_scrolling_reasons_to_clear) {
  DCHECK(IsPropertyChangeAllowed());
  DCHECK(main_thread_scrolling_reasons_to_clear);
  uint32_t new_reasons =
      ~main_thread_scrolling_reasons_to_clear & main_thread_scrolling_reasons_;
  if (new_reasons == main_thread_scrolling_reasons_)
    return;
  main_thread_scrolling_reasons_ = new_reasons;
  SetNeedsCommit();
}

void Layer::SetNonFastScrollableRegion(const Region& region) {
  DCHECK(IsPropertyChangeAllowed());
  if (non_fast_scrollable_region_ == region)
    return;
  non_fast_scrollable_region_ = region;
  SetNeedsCommit();
}

void Layer::SetTouchEventHandlerRegion(const Region& region) {
  DCHECK(IsPropertyChangeAllowed());
  if (touch_event_handler_region_ == region)
    return;

  touch_event_handler_region_ = region;
  SetNeedsCommit();
}

void Layer::SetForceRenderSurfaceForTesting(bool force) {
  DCHECK(IsPropertyChangeAllowed());
  if (force_render_surface_for_testing_ == force)
    return;
  force_render_surface_for_testing_ = force;
  SetNeedsCommit();
}

void Layer::SetDoubleSided(bool double_sided) {
  DCHECK(IsPropertyChangeAllowed());
  if (double_sided_ == double_sided)
    return;
  double_sided_ = double_sided;
  SetNeedsCommit();
  SetSubtreePropertyChanged();
}

void Layer::Set3dSortingContextId(int id) {
  DCHECK(IsPropertyChangeAllowed());
  if (id == sorting_context_id_)
    return;
  sorting_context_id_ = id;
  SetNeedsCommit();
  SetSubtreePropertyChanged();
}

void Layer::SetTransformTreeIndex(int index) {
  DCHECK(IsPropertyChangeAllowed());
  if (transform_tree_index_ == index)
    return;
  transform_tree_index_ = index;
  SetNeedsPushProperties();
}

int Layer::transform_tree_index() const {
  if (!layer_tree_host_ ||
      layer_tree_host_->property_trees()->sequence_number !=
          property_tree_sequence_number_) {
    return -1;
  }
  return transform_tree_index_;
}

void Layer::SetClipTreeIndex(int index) {
  DCHECK(IsPropertyChangeAllowed());
  if (clip_tree_index_ == index)
    return;
  clip_tree_index_ = index;
  SetNeedsPushProperties();
}

int Layer::clip_tree_index() const {
  if (!layer_tree_host_ ||
      layer_tree_host_->property_trees()->sequence_number !=
          property_tree_sequence_number_) {
    return -1;
  }
  return clip_tree_index_;
}

void Layer::SetEffectTreeIndex(int index) {
  DCHECK(IsPropertyChangeAllowed());
  if (effect_tree_index_ == index)
    return;
  effect_tree_index_ = index;
  SetNeedsPushProperties();
}

int Layer::effect_tree_index() const {
  if (!layer_tree_host_ ||
      layer_tree_host_->property_trees()->sequence_number !=
          property_tree_sequence_number_) {
    return -1;
  }
  return effect_tree_index_;
}

void Layer::SetScrollTreeIndex(int index) {
  DCHECK(IsPropertyChangeAllowed());
  if (scroll_tree_index_ == index)
    return;
  scroll_tree_index_ = index;
  SetNeedsPushProperties();
}

int Layer::scroll_tree_index() const {
  if (!layer_tree_host_ ||
      layer_tree_host_->property_trees()->sequence_number !=
          property_tree_sequence_number_) {
    return -1;
  }
  return scroll_tree_index_;
}

void Layer::InvalidatePropertyTreesIndices() {
  int invalid_property_tree_index = -1;
  SetTransformTreeIndex(invalid_property_tree_index);
  SetClipTreeIndex(invalid_property_tree_index);
  SetEffectTreeIndex(invalid_property_tree_index);
}

void Layer::SetShouldFlattenTransform(bool should_flatten) {
  DCHECK(IsPropertyChangeAllowed());
  if (should_flatten_transform_ == should_flatten)
    return;
  should_flatten_transform_ = should_flatten;
  SetNeedsCommit();
  SetSubtreePropertyChanged();
}

void Layer::SetUseParentBackfaceVisibility(bool use) {
  DCHECK(IsPropertyChangeAllowed());
  if (use_parent_backface_visibility_ == use)
    return;
  use_parent_backface_visibility_ = use;
  SetNeedsPushProperties();
}

void Layer::SetUseLocalTransformForBackfaceVisibility(bool use_local) {
  if (use_local_transform_for_backface_visibility_ == use_local)
    return;
  use_local_transform_for_backface_visibility_ = use_local;
  SetNeedsPushProperties();
}

void Layer::SetShouldCheckBackfaceVisibility(
    bool should_check_backface_visibility) {
  if (should_check_backface_visibility_ == should_check_backface_visibility)
    return;
  should_check_backface_visibility_ = should_check_backface_visibility;
  SetNeedsPushProperties();
}

void Layer::SetIsDrawable(bool is_drawable) {
  DCHECK(IsPropertyChangeAllowed());
  if (is_drawable_ == is_drawable)
    return;

  is_drawable_ = is_drawable;
  UpdateDrawsContent(HasDrawableContent());
}

void Layer::SetHideLayerAndSubtree(bool hide) {
  DCHECK(IsPropertyChangeAllowed());
  if (hide_layer_and_subtree_ == hide)
    return;

  hide_layer_and_subtree_ = hide;
  SetNeedsCommit();
  SetSubtreePropertyChanged();
}

void Layer::SetNeedsDisplayRect(const gfx::Rect& dirty_rect) {
  if (dirty_rect.IsEmpty())
    return;

  SetNeedsPushProperties();
  update_rect_.Union(dirty_rect);

  if (DrawsContent())
    SetNeedsUpdate();
}

bool Layer::DescendantIsFixedToContainerLayer() const {
  for (size_t i = 0; i < children_.size(); ++i) {
    if (children_[i]->position_constraint_.is_fixed_position() ||
        children_[i]->DescendantIsFixedToContainerLayer())
      return true;
  }
  return false;
}

void Layer::SetIsContainerForFixedPositionLayers(bool container) {
  if (is_container_for_fixed_position_layers_ == container)
    return;
  is_container_for_fixed_position_layers_ = container;

  if (layer_tree_host_ && layer_tree_host_->CommitRequested())
    return;

  // Only request a commit if we have a fixed positioned descendant.
  if (DescendantIsFixedToContainerLayer())
    SetNeedsCommit();
}

void Layer::SetPositionConstraint(const LayerPositionConstraint& constraint) {
  DCHECK(IsPropertyChangeAllowed());
  if (position_constraint_ == constraint)
    return;
  position_constraint_ = constraint;
  SetNeedsCommit();
}

static void RunCopyCallbackOnMainThread(
    std::unique_ptr<CopyOutputRequest> request,
    std::unique_ptr<CopyOutputResult> result) {
  request->SendResult(std::move(result));
}

static void PostCopyCallbackToMainThread(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    std::unique_ptr<CopyOutputRequest> request,
    std::unique_ptr<CopyOutputResult> result) {
  main_thread_task_runner->PostTask(FROM_HERE,
                                    base::Bind(&RunCopyCallbackOnMainThread,
                                               base::Passed(&request),
                                               base::Passed(&result)));
}

void Layer::PushPropertiesTo(LayerImpl* layer) {
  TRACE_EVENT0("cc", "Layer::PushPropertiesTo");
  DCHECK(layer_tree_host_);

  // If we did not SavePaintProperties() for the layer this frame, then push the
  // real property values, not the paint property values.
  bool use_paint_properties = paint_properties_.source_frame_number ==
                              layer_tree_host_->source_frame_number();

  layer->SetBackgroundColor(background_color_);
  layer->SetSafeOpaqueBackgroundColor(safe_opaque_background_color_);
  layer->SetBounds(use_paint_properties ? paint_properties_.bounds
                                        : bounds_);

#if defined(NDEBUG)
  if (frame_viewer_instrumentation::IsTracingLayerTreeSnapshots())
    layer->SetDebugInfo(TakeDebugInfo());
#else
  layer->SetDebugInfo(TakeDebugInfo());
#endif

  layer->SetTransformTreeIndex(transform_tree_index());
  layer->SetEffectTreeIndex(effect_tree_index());
  layer->SetClipTreeIndex(clip_tree_index());
  layer->SetScrollTreeIndex(scroll_tree_index());
  layer->set_offset_to_transform_parent(offset_to_transform_parent_);
  layer->SetDrawsContent(DrawsContent());
  // subtree_property_changed_ is propagated to all descendants while building
  // property trees. So, it is enough to check it only for the current layer.
  if (subtree_property_changed_ || layer_property_changed_)
    layer->NoteLayerPropertyChanged();
  if (!FilterIsAnimating())
    layer->SetFilters(filters_);
  layer->SetMasksToBounds(masks_to_bounds_);
  layer->set_main_thread_scrolling_reasons(main_thread_scrolling_reasons_);
  layer->SetNonFastScrollableRegion(non_fast_scrollable_region_);
  layer->SetTouchEventHandlerRegion(touch_event_handler_region_);
  layer->SetContentsOpaque(contents_opaque_);
  layer->SetBlendMode(blend_mode_);
  layer->SetPosition(position_);
  layer->set_should_flatten_transform_from_property_tree(
      should_flatten_transform_from_property_tree_);
  layer->set_draw_blend_mode(draw_blend_mode_);
  layer->SetUseParentBackfaceVisibility(use_parent_backface_visibility_);
  layer->SetUseLocalTransformForBackfaceVisibility(
      use_local_transform_for_backface_visibility_);
  layer->SetShouldCheckBackfaceVisibility(should_check_backface_visibility_);
  if (!TransformIsAnimating())
    layer->SetTransform(transform_);
  layer->Set3dSortingContextId(sorting_context_id_);

  layer->SetScrollClipLayer(scroll_clip_layer_id_);
  layer->set_user_scrollable_horizontal(user_scrollable_horizontal_);
  layer->set_user_scrollable_vertical(user_scrollable_vertical_);
  layer->SetElementId(element_id_);
  layer->SetMutableProperties(mutable_properties_);

  // When a scroll offset animation is interrupted the new scroll position on
  // the pending tree will clobber any impl-side scrolling occuring on the
  // active tree. To do so, avoid scrolling the pending tree along with it
  // instead of trying to undo that scrolling later.
  if (ScrollOffsetAnimationWasInterrupted())
    layer_tree_host()
        ->property_trees()
        ->scroll_tree.SetScrollOffsetClobberActiveValue(layer->id());

  // If the main thread commits multiple times before the impl thread actually
  // draws, then damage tracking will become incorrect if we simply clobber the
  // update_rect here. The LayerImpl's update_rect needs to accumulate (i.e.
  // union) any update changes that have occurred on the main thread.
  update_rect_.Union(layer->update_rect());
  layer->SetUpdateRect(update_rect_);

  layer->SetHasWillChangeTransformHint(has_will_change_transform_hint());
  layer->SetNeedsPushProperties();

  // Reset any state that should be cleared for the next update.
  subtree_property_changed_ = false;
  layer_property_changed_ = false;
  update_rect_ = gfx::Rect();

  layer_tree_host()->RemoveLayerShouldPushProperties(this);
}

void Layer::TakeCopyRequests(
    std::vector<std::unique_ptr<CopyOutputRequest>>* requests) {
  for (auto& it : copy_requests_) {
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner =
        layer_tree_host()->task_runner_provider()->MainThreadTaskRunner();
    std::unique_ptr<CopyOutputRequest> original_request = std::move(it);
    const CopyOutputRequest& original_request_ref = *original_request;
    std::unique_ptr<CopyOutputRequest> main_thread_request =
        CopyOutputRequest::CreateRelayRequest(
            original_request_ref,
            base::Bind(&PostCopyCallbackToMainThread, main_thread_task_runner,
                       base::Passed(&original_request)));
    if (main_thread_request->has_area()) {
      main_thread_request->set_area(gfx::IntersectRects(
          main_thread_request->area(), gfx::Rect(bounds())));
    }
    requests->push_back(std::move(main_thread_request));
  }

  copy_requests_.clear();
}

void Layer::SetTypeForProtoSerialization(proto::LayerNode* proto) const {
  proto->set_type(proto::LayerNode::LAYER);
}

void Layer::ToLayerNodeProto(proto::LayerNode* proto) const {
  proto->set_id(layer_id_);
  SetTypeForProtoSerialization(proto);

  if (parent_)
    proto->set_parent_id(parent_->id());

  DCHECK_EQ(0, proto->children_size());
  for (const auto& child : children_) {
    child->ToLayerNodeProto(proto->add_children());
  }

  if (mask_layer_)
    mask_layer_->ToLayerNodeProto(proto->mutable_mask_layer());
  if (replica_layer_)
    replica_layer_->ToLayerNodeProto(proto->mutable_replica_layer());
}

void Layer::ClearLayerTreePropertiesForDeserializationAndAddToMap(
    LayerIdMap* layer_map) {
  (*layer_map)[layer_id_] = this;

  if (layer_tree_host_)
    layer_tree_host_->UnregisterLayer(this);

  layer_tree_host_ = nullptr;
  parent_ = nullptr;

  // Clear these properties for all the children and add them to the map.
  for (auto& child : children_) {
    child->ClearLayerTreePropertiesForDeserializationAndAddToMap(layer_map);
  }

  children_.clear();

  if (mask_layer_) {
    mask_layer_->ClearLayerTreePropertiesForDeserializationAndAddToMap(
        layer_map);
    mask_layer_ = nullptr;
  }

  if (replica_layer_) {
    replica_layer_->ClearLayerTreePropertiesForDeserializationAndAddToMap(
        layer_map);
    replica_layer_ = nullptr;
  }
}

void Layer::FromLayerNodeProto(const proto::LayerNode& proto,
                               const LayerIdMap& layer_map,
                               LayerTreeHost* layer_tree_host) {
  DCHECK(!layer_tree_host_);
  DCHECK(children_.empty());
  DCHECK(!mask_layer_);
  DCHECK(!replica_layer_);
  DCHECK(layer_tree_host);
  DCHECK(proto.has_id());

  layer_id_ = proto.id();

  layer_tree_host_ = layer_tree_host;
  layer_tree_host_->RegisterLayer(this);

  for (int i = 0; i < proto.children_size(); ++i) {
    const proto::LayerNode& child_proto = proto.children(i);
    DCHECK(child_proto.has_type());
    scoped_refptr<Layer> child =
        LayerProtoConverter::FindOrAllocateAndConstruct(child_proto, layer_map);
    // The child must now refer to this layer as its parent, and must also have
    // the same LayerTreeHost. This must be done before deserializing children.
    DCHECK(!child->parent_);
    child->parent_ = this;
    child->FromLayerNodeProto(child_proto, layer_map, layer_tree_host_);
    children_.push_back(child);
  }

  if (proto.has_mask_layer()) {
    mask_layer_ = LayerProtoConverter::FindOrAllocateAndConstruct(
        proto.mask_layer(), layer_map);
    mask_layer_->parent_ = this;
    mask_layer_->FromLayerNodeProto(proto.mask_layer(), layer_map,
                                    layer_tree_host_);
  }

  if (proto.has_replica_layer()) {
    replica_layer_ = LayerProtoConverter::FindOrAllocateAndConstruct(
        proto.replica_layer(), layer_map);
    replica_layer_->parent_ = this;
    replica_layer_->FromLayerNodeProto(proto.replica_layer(), layer_map,
                                       layer_tree_host_);
  }
}

void Layer::ToLayerPropertiesProto(proto::LayerUpdate* layer_update) {
  // Always set properties metadata for serialized layers.
  proto::LayerProperties* proto = layer_update->add_layers();
  proto->set_id(layer_id_);
  LayerSpecificPropertiesToProto(proto);
}

void Layer::FromLayerPropertiesProto(const proto::LayerProperties& proto) {
  DCHECK(proto.has_id());
  DCHECK_EQ(layer_id_, proto.id());
  FromLayerSpecificPropertiesProto(proto);
}

void Layer::LayerSpecificPropertiesToProto(proto::LayerProperties* proto) {
  proto::BaseLayerProperties* base = proto->mutable_base();

  bool use_paint_properties = layer_tree_host_ &&
                              paint_properties_.source_frame_number ==
                                  layer_tree_host_->source_frame_number();

  Point3FToProto(transform_origin_, base->mutable_transform_origin());
  base->set_background_color(background_color_);
  base->set_safe_opaque_background_color(safe_opaque_background_color_);
  SizeToProto(use_paint_properties ? paint_properties_.bounds : bounds_,
              base->mutable_bounds());

  // TODO(nyquist): Figure out what to do with debug info. See crbug.com/570372.

  base->set_transform_free_index(transform_tree_index_);
  base->set_effect_tree_index(effect_tree_index_);
  base->set_clip_tree_index(clip_tree_index_);
  base->set_scroll_tree_index(scroll_tree_index_);
  Vector2dFToProto(offset_to_transform_parent_,
                   base->mutable_offset_to_transform_parent());
  base->set_double_sided(double_sided_);
  base->set_draws_content(draws_content_);
  base->set_hide_layer_and_subtree(hide_layer_and_subtree_);
  base->set_subtree_property_changed(subtree_property_changed_);
  base->set_layer_property_changed(layer_property_changed_);

  // TODO(nyquist): Add support for serializing FilterOperations for
  // |filters_| and |background_filters_|. See crbug.com/541321.

  base->set_masks_to_bounds(masks_to_bounds_);
  base->set_main_thread_scrolling_reasons(main_thread_scrolling_reasons_);
  RegionToProto(non_fast_scrollable_region_,
                base->mutable_non_fast_scrollable_region());
  RegionToProto(touch_event_handler_region_,
                base->mutable_touch_event_handler_region());
  base->set_contents_opaque(contents_opaque_);
  base->set_opacity(opacity_);
  base->set_blend_mode(SkXfermodeModeToProto(blend_mode_));
  base->set_is_root_for_isolated_group(is_root_for_isolated_group_);
  PointFToProto(position_, base->mutable_position());
  base->set_is_container_for_fixed_position_layers(
      is_container_for_fixed_position_layers_);
  position_constraint_.ToProtobuf(base->mutable_position_constraint());
  base->set_should_flatten_transform(should_flatten_transform_);
  base->set_should_flatten_transform_from_property_tree(
      should_flatten_transform_from_property_tree_);
  base->set_draw_blend_mode(SkXfermodeModeToProto(draw_blend_mode_));
  base->set_use_parent_backface_visibility(use_parent_backface_visibility_);
  TransformToProto(transform_, base->mutable_transform());
  base->set_sorting_context_id(sorting_context_id_);
  base->set_num_descendants_that_draw_content(
      num_descendants_that_draw_content_);

  base->set_scroll_clip_layer_id(scroll_clip_layer_id_);
  base->set_user_scrollable_horizontal(user_scrollable_horizontal_);
  base->set_user_scrollable_vertical(user_scrollable_vertical_);

  int scroll_parent_id = scroll_parent_ ? scroll_parent_->id() : INVALID_ID;
  base->set_scroll_parent_id(scroll_parent_id);

  if (scroll_children_) {
    for (auto* child : *scroll_children_)
      base->add_scroll_children_ids(child->id());
  }

  int clip_parent_id = clip_parent_ ? clip_parent_->id() : INVALID_ID;
  base->set_clip_parent_id(clip_parent_id);

  if (clip_children_) {
    for (auto* child : *clip_children_)
      base->add_clip_children_ids(child->id());
  }

  ScrollOffsetToProto(scroll_offset_, base->mutable_scroll_offset());

  // TODO(nyquist): Figure out what to do with CopyRequests.
  // See crbug.com/570374.

  RectToProto(update_rect_, base->mutable_update_rect());

  // TODO(nyquist): Figure out what to do with ElementAnimations.
  // See crbug.com/570376.

  update_rect_ = gfx::Rect();

  base->set_has_will_change_transform_hint(has_will_change_transform_hint_);
}

void Layer::FromLayerSpecificPropertiesProto(
    const proto::LayerProperties& proto) {
  DCHECK(proto.has_base());
  DCHECK(layer_tree_host_);
  const proto::BaseLayerProperties& base = proto.base();

  transform_origin_ = ProtoToPoint3F(base.transform_origin());
  background_color_ = base.background_color();
  safe_opaque_background_color_ = base.safe_opaque_background_color();
  bounds_ = ProtoToSize(base.bounds());

  transform_tree_index_ = base.transform_free_index();
  effect_tree_index_ = base.effect_tree_index();
  clip_tree_index_ = base.clip_tree_index();
  scroll_tree_index_ = base.scroll_tree_index();
  offset_to_transform_parent_ =
      ProtoToVector2dF(base.offset_to_transform_parent());
  double_sided_ = base.double_sided();
  draws_content_ = base.draws_content();
  hide_layer_and_subtree_ = base.hide_layer_and_subtree();
  subtree_property_changed_ = base.subtree_property_changed();
  layer_property_changed_ = base.layer_property_changed();
  masks_to_bounds_ = base.masks_to_bounds();
  main_thread_scrolling_reasons_ = base.main_thread_scrolling_reasons();
  non_fast_scrollable_region_ =
      RegionFromProto(base.non_fast_scrollable_region());
  touch_event_handler_region_ =
      RegionFromProto(base.touch_event_handler_region());
  contents_opaque_ = base.contents_opaque();
  opacity_ = base.opacity();
  blend_mode_ = SkXfermodeModeFromProto(base.blend_mode());
  is_root_for_isolated_group_ = base.is_root_for_isolated_group();
  position_ = ProtoToPointF(base.position());
  is_container_for_fixed_position_layers_ =
      base.is_container_for_fixed_position_layers();
  position_constraint_.FromProtobuf(base.position_constraint());
  should_flatten_transform_ = base.should_flatten_transform();
  should_flatten_transform_from_property_tree_ =
      base.should_flatten_transform_from_property_tree();
  draw_blend_mode_ = SkXfermodeModeFromProto(base.draw_blend_mode());
  use_parent_backface_visibility_ = base.use_parent_backface_visibility();
  transform_ = ProtoToTransform(base.transform());
  sorting_context_id_ = base.sorting_context_id();
  num_descendants_that_draw_content_ = base.num_descendants_that_draw_content();

  scroll_clip_layer_id_ = base.scroll_clip_layer_id();
  user_scrollable_horizontal_ = base.user_scrollable_horizontal();
  user_scrollable_vertical_ = base.user_scrollable_vertical();

  scroll_parent_ = base.scroll_parent_id() == INVALID_ID
                       ? nullptr
                       : layer_tree_host_->LayerById(base.scroll_parent_id());

  // If there have been scroll children entries in previous deserializations,
  // clear out the set. If there have been none, initialize the set of children.
  // After this, the set is in the correct state to only add the new children.
  // If the set of children has not changed, for now this code still rebuilds
  // the set.
  if (scroll_children_)
    scroll_children_->clear();
  else if (base.scroll_children_ids_size() > 0)
    scroll_children_.reset(new std::set<Layer*>);
  for (int i = 0; i < base.scroll_children_ids_size(); ++i) {
    int child_id = base.scroll_children_ids(i);
    scoped_refptr<Layer> child = layer_tree_host_->LayerById(child_id);
    scroll_children_->insert(child.get());
  }

  clip_parent_ = base.clip_parent_id() == INVALID_ID
                     ? nullptr
                     : layer_tree_host_->LayerById(base.clip_parent_id());

  // If there have been clip children entries in previous deserializations,
  // clear out the set. If there have been none, initialize the set of children.
  // After this, the set is in the correct state to only add the new children.
  // If the set of children has not changed, for now this code still rebuilds
  // the set.
  if (clip_children_)
    clip_children_->clear();
  else if (base.clip_children_ids_size() > 0)
    clip_children_.reset(new std::set<Layer*>);
  for (int i = 0; i < base.clip_children_ids_size(); ++i) {
    int child_id = base.clip_children_ids(i);
    scoped_refptr<Layer> child = layer_tree_host_->LayerById(child_id);
    clip_children_->insert(child.get());
  }

  scroll_offset_ = ProtoToScrollOffset(base.scroll_offset());

  update_rect_.Union(ProtoToRect(base.update_rect()));

  has_will_change_transform_hint_ = base.has_will_change_transform_hint();
}

std::unique_ptr<LayerImpl> Layer::CreateLayerImpl(LayerTreeImpl* tree_impl) {
  return LayerImpl::Create(tree_impl, layer_id_);
}

bool Layer::DrawsContent() const {
  return draws_content_;
}

bool Layer::HasDrawableContent() const {
  return is_drawable_;
}

void Layer::UpdateDrawsContent(bool has_drawable_content) {
  bool draws_content = has_drawable_content;
  DCHECK(is_drawable_ || !has_drawable_content);
  if (draws_content == draws_content_)
    return;

  if (parent())
    parent()->AddDrawableDescendants(draws_content ? 1 : -1);

  draws_content_ = draws_content;
  SetNeedsCommit();
}

int Layer::NumDescendantsThatDrawContent() const {
  return num_descendants_that_draw_content_;
}

void Layer::SavePaintProperties() {
  DCHECK(layer_tree_host_);

  // TODO(reveman): Save all layer properties that we depend on not
  // changing until PushProperties() has been called. crbug.com/231016
  paint_properties_.bounds = bounds_;
  paint_properties_.source_frame_number =
      layer_tree_host_->source_frame_number();
}

bool Layer::Update() {
  DCHECK(layer_tree_host_);
  DCHECK_EQ(layer_tree_host_->source_frame_number(),
            paint_properties_.source_frame_number) <<
      "SavePaintProperties must be called for any layer that is painted.";
  return false;
}

bool Layer::IsSuitableForGpuRasterization() const {
  return true;
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
Layer::TakeDebugInfo() {
  if (client_)
    return client_->TakeDebugInfo(this);
  else
    return nullptr;
}

void Layer::SetSubtreePropertyChanged() {
  if (subtree_property_changed_)
    return;
  subtree_property_changed_ = true;
  SetNeedsPushProperties();
}

void Layer::SetLayerPropertyChanged() {
  if (layer_property_changed_)
    return;
  layer_property_changed_ = true;
  SetNeedsPushProperties();
}

gfx::ScrollOffset Layer::ScrollOffsetForAnimation() const {
  return CurrentScrollOffset();
}

// On<Property>Animated is called due to an ongoing accelerated animation.
// Since this animation is also being run on the compositor thread, there
// is no need to request a commit to push this value over, so the value is
// set directly rather than by calling Set<Property>.
void Layer::OnFilterAnimated(const FilterOperations& filters) {
  filters_ = filters;
}

void Layer::OnOpacityAnimated(float opacity) {
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);

  if (opacity_ == opacity)
    return;
  opacity_ = opacity;
  // Changing the opacity may make a previously hidden layer visible, so a new
  // recording may be needed.
  SetNeedsUpdate();
  if (layer_tree_host_) {
    PropertyTrees* property_trees = layer_tree_host_->property_trees();
    if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::EFFECT,
                                         id())) {
      DCHECK_EQ(effect_tree_index(),
                property_trees->effect_id_to_index_map[id()]);
      EffectNode* node = property_trees->effect_tree.Node(effect_tree_index());
      node->data.opacity = opacity;
      property_trees->effect_tree.set_needs_update(true);
    }
  }
}

void Layer::OnTransformAnimated(const gfx::Transform& transform) {
  if (transform_ == transform)
    return;
  transform_ = transform;
  // Changing the transform may change the visible part of this layer, so a new
  // recording may be needed.
  SetNeedsUpdate();
  if (layer_tree_host_) {
    PropertyTrees* property_trees = layer_tree_host_->property_trees();
    if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::TRANSFORM,
                                         id())) {
      DCHECK_EQ(transform_tree_index(),
                property_trees->transform_id_to_index_map[id()]);
      TransformNode* node =
          property_trees->transform_tree.Node(transform_tree_index());
      node->data.local = transform;
      node->data.needs_local_transform_update = true;
      node->data.has_potential_animation = true;
      property_trees->transform_tree.set_needs_update(true);
    }
  }
}

void Layer::OnScrollOffsetAnimated(const gfx::ScrollOffset& scroll_offset) {
  // Do nothing. Scroll deltas will be sent from the compositor thread back
  // to the main thread in the same manner as during non-animated
  // compositor-driven scrolling.
}

void Layer::OnTransformIsCurrentlyAnimatingChanged(
    bool is_currently_animating) {
  DCHECK(layer_tree_host_);
  PropertyTrees* property_trees = layer_tree_host_->property_trees();
  if (!property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::TRANSFORM,
                                        id()))
    return;
  DCHECK_EQ(transform_tree_index(),
            property_trees->transform_id_to_index_map[id()]);
  TransformNode* node =
      property_trees->transform_tree.Node(transform_tree_index());
  node->data.is_currently_animating = is_currently_animating;
}

void Layer::OnTransformIsPotentiallyAnimatingChanged(
    bool has_potential_animation) {
  if (!layer_tree_host_)
    return;
  PropertyTrees* property_trees = layer_tree_host_->property_trees();
  if (!property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::TRANSFORM,
                                        id()))
    return;
  DCHECK_EQ(transform_tree_index(),
            property_trees->transform_id_to_index_map[id()]);
  TransformNode* node =
      property_trees->transform_tree.Node(transform_tree_index());

  node->data.has_potential_animation = has_potential_animation;
  if (has_potential_animation) {
    node->data.has_only_translation_animations = HasOnlyTranslationTransforms();
  } else {
    node->data.has_only_translation_animations = true;
  }
  property_trees->transform_tree.set_needs_update(true);
}

void Layer::OnOpacityIsCurrentlyAnimatingChanged(bool is_currently_animating) {
  DCHECK(layer_tree_host_);
  PropertyTrees* property_trees = layer_tree_host_->property_trees();
  if (!property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::EFFECT, id()))
    return;
  DCHECK_EQ(effect_tree_index(), property_trees->effect_id_to_index_map[id()]);
  EffectNode* node = property_trees->effect_tree.Node(effect_tree_index());
  node->data.is_currently_animating_opacity = is_currently_animating;
}

void Layer::OnOpacityIsPotentiallyAnimatingChanged(
    bool has_potential_animation) {
  DCHECK(layer_tree_host_);
  PropertyTrees* property_trees = layer_tree_host_->property_trees();
  if (!property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::EFFECT, id()))
    return;
  DCHECK_EQ(effect_tree_index(), property_trees->effect_id_to_index_map[id()]);
  EffectNode* node = property_trees->effect_tree.Node(effect_tree_index());
  node->data.has_potential_opacity_animation =
      has_potential_animation || OpacityCanAnimateOnImplThread();
  property_trees->effect_tree.set_needs_update(true);
}

bool Layer::HasActiveAnimationForTesting() const {
  return layer_tree_host_ ? layer_tree_host_->HasActiveAnimationForTesting(this)
                          : false;
}

void Layer::SetHasWillChangeTransformHint(bool has_will_change) {
  if (has_will_change_transform_hint_ == has_will_change)
    return;
  has_will_change_transform_hint_ = has_will_change;
  SetNeedsCommit();
}

ScrollbarLayerInterface* Layer::ToScrollbarLayer() {
  return nullptr;
}

RenderingStatsInstrumentation* Layer::rendering_stats_instrumentation() const {
  return layer_tree_host_->rendering_stats_instrumentation();
}

void Layer::RemoveFromScrollTree() {
  if (scroll_children_.get()) {
    std::set<Layer*> copy = *scroll_children_;
    for (std::set<Layer*>::iterator it = copy.begin(); it != copy.end(); ++it)
      (*it)->SetScrollParent(nullptr);
  }

  DCHECK(!scroll_children_);
  SetScrollParent(nullptr);
}

void Layer::RemoveFromClipTree() {
  if (clip_children_.get()) {
    std::set<Layer*> copy = *clip_children_;
    for (std::set<Layer*>::iterator it = copy.begin(); it != copy.end(); ++it)
      (*it)->SetClipParent(nullptr);
  }

  DCHECK(!clip_children_);
  SetClipParent(nullptr);
}

void Layer::AddDrawableDescendants(int num) {
  DCHECK_GE(num_descendants_that_draw_content_, 0);
  DCHECK_GE(num_descendants_that_draw_content_ + num, 0);
  if (num == 0)
    return;
  num_descendants_that_draw_content_ += num;
  SetNeedsCommit();
  if (parent())
    parent()->AddDrawableDescendants(num);
}

void Layer::RunMicroBenchmark(MicroBenchmark* benchmark) {
  benchmark->RunOnLayer(this);
}

void Layer::SetElementId(ElementId id) {
  DCHECK(IsPropertyChangeAllowed());
  if (element_id_ == id)
    return;
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "Layer::SetElementId", "element", id.AsValue().release());
  if (element_id_ && layer_tree_host()) {
    layer_tree_host()->animation_host()->UnregisterElement(
        element_id_, ElementListType::ACTIVE);
    layer_tree_host()->RemoveFromElementMap(this);
  }

  element_id_ = id;

  if (element_id_ && layer_tree_host()) {
    layer_tree_host()->animation_host()->RegisterElement(
        element_id_, ElementListType::ACTIVE);
    layer_tree_host()->AddToElementMap(this);
  }

  SetNeedsCommit();
}

void Layer::SetMutableProperties(uint32_t properties) {
  DCHECK(IsPropertyChangeAllowed());
  if (mutable_properties_ == properties)
    return;
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "Layer::SetMutableProperties", "properties", properties);
  mutable_properties_ = properties;
  SetNeedsCommit();
}

void Layer::DidBeginTracing() {
  // We'll be dumping layer trees as part of trace, so make sure
  // PushPropertiesTo() propagates layer debug info to the impl
  // side -- otherwise this won't happen for the the layers that
  // remain unchanged since tracing started.
  SetNeedsPushProperties();
}

int Layer::num_copy_requests_in_target_subtree() {
  return layer_tree_host()
      ->property_trees()
      ->effect_tree.Node(effect_tree_index())
      ->data.num_copy_requests_in_subtree;
}

gfx::Transform Layer::screen_space_transform() const {
  DCHECK_NE(transform_tree_index_, -1);
  return draw_property_utils::ScreenSpaceTransform(
      this, layer_tree_host_->property_trees()->transform_tree);
}

}  // namespace cc
