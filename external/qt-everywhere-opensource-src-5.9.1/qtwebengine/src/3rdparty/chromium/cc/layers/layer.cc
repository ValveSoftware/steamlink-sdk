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
#include "cc/base/simple_enclosed_region.h"
#include "cc/debug/frame_viewer_instrumentation.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/layer_client.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/scrollbar_layer_interface.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/proto/cc_conversions.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/layer.pb.h"
#include "cc/proto/skia_conversions.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/mutable_properties.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/transform_node.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {

base::StaticAtomicSequenceNumber g_next_layer_id;

Layer::Inputs::Inputs(int layer_id)
    : layer_id(layer_id),
      masks_to_bounds(false),
      mask_layer(nullptr),
      opacity(1.f),
      blend_mode(SkXfermode::kSrcOver_Mode),
      is_root_for_isolated_group(false),
      contents_opaque(false),
      is_drawable(false),
      double_sided(true),
      should_flatten_transform(true),
      sorting_context_id(0),
      use_parent_backface_visibility(false),
      background_color(0),
      scroll_clip_layer_id(INVALID_ID),
      user_scrollable_horizontal(true),
      user_scrollable_vertical(true),
      main_thread_scrolling_reasons(
          MainThreadScrollingReason::kNotScrollingOnMain),
      is_container_for_fixed_position_layers(false),
      mutable_properties(MutableProperty::kNone),
      scroll_parent(nullptr),
      clip_parent(nullptr),
      has_will_change_transform_hint(false),
      has_preferred_raster_bounds(false),
      hide_layer_and_subtree(false),
      client(nullptr) {}

Layer::Inputs::~Inputs() {}

scoped_refptr<Layer> Layer::Create() {
  return make_scoped_refptr(new Layer());
}

Layer::Layer()
    : ignore_set_needs_commit_(false),
      parent_(nullptr),
      layer_tree_host_(nullptr),
      layer_tree_(nullptr),
      // Layer IDs start from 1.
      inputs_(g_next_layer_id.GetNext() + 1),
      num_descendants_that_draw_content_(0),
      transform_tree_index_(TransformTree::kInvalidNodeId),
      effect_tree_index_(EffectTree::kInvalidNodeId),
      clip_tree_index_(ClipTree::kInvalidNodeId),
      scroll_tree_index_(ScrollTree::kInvalidNodeId),
      property_tree_sequence_number_(-1),
      should_flatten_transform_from_property_tree_(false),
      draws_content_(false),
      use_local_transform_for_backface_visibility_(false),
      should_check_backface_visibility_(false),
      force_render_surface_for_testing_(false),
      subtree_property_changed_(false),
      may_contain_video_(false),
      safe_opaque_background_color_(0),
      draw_blend_mode_(SkXfermode::kSrcOver_Mode),
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
  if (inputs_.mask_layer.get()) {
    DCHECK_EQ(this, inputs_.mask_layer->parent());
    inputs_.mask_layer->RemoveFromParent();
  }
}

void Layer::SetLayerTreeHost(LayerTreeHost* host) {
  if (layer_tree_host_ == host)
    return;

  if (layer_tree_host_) {
    layer_tree_->property_trees()->RemoveIdFromIdToIndexMaps(id());
    layer_tree_->property_trees()->needs_rebuild = true;
    layer_tree_->UnregisterLayer(this);
    if (inputs_.element_id) {
      layer_tree_->UnregisterElement(inputs_.element_id,
                                     ElementListType::ACTIVE, this);
    }
  }
  if (host) {
    host->GetLayerTree()->property_trees()->needs_rebuild = true;
    host->GetLayerTree()->RegisterLayer(this);
    if (inputs_.element_id) {
      host->GetLayerTree()->RegisterElement(inputs_.element_id,
                                            ElementListType::ACTIVE, this);
    }
  }

  layer_tree_host_ = host;
  layer_tree_ = host ? host->GetLayerTree() : nullptr;
  InvalidatePropertyTreesIndices();

  // When changing hosts, the layer needs to commit its properties to the impl
  // side for the new host.
  SetNeedsPushProperties();

  for (size_t i = 0; i < inputs_.children.size(); ++i)
    inputs_.children[i]->SetLayerTreeHost(host);

  if (inputs_.mask_layer.get())
    inputs_.mask_layer->SetLayerTreeHost(host);

  const bool has_any_animation =
      layer_tree_host_ ? GetMutatorHost()->HasAnyAnimation(element_id())
                       : false;

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
  layer_tree_->property_trees()->needs_rebuild = true;

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
  if (!layer_tree_)
    return;

  layer_tree_->SetNeedsFullTreeSync();
}

void Layer::SetNextCommitWaitsForActivation() {
  if (!layer_tree_host_)
    return;

  layer_tree_host_->SetNextCommitWaitsForActivation();
}

void Layer::SetNeedsPushProperties() {
  if (layer_tree_)
    layer_tree_->AddLayerShouldPushProperties(this);
}

void Layer::ResetNeedsPushPropertiesForTesting() {
  if (layer_tree_)
    layer_tree_->RemoveLayerShouldPushProperties(this);
}

bool Layer::IsPropertyChangeAllowed() const {
  if (!layer_tree_)
    return true;

  return !layer_tree_->in_paint_layer_contents();
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

  layer_tree_->property_trees()->needs_rebuild = true;
}

void Layer::AddChild(scoped_refptr<Layer> child) {
  InsertChild(child, inputs_.children.size());
}

void Layer::InsertChild(scoped_refptr<Layer> child, size_t index) {
  DCHECK(IsPropertyChangeAllowed());
  child->RemoveFromParent();
  AddDrawableDescendants(child->NumDescendantsThatDrawContent() +
                         (child->DrawsContent() ? 1 : 0));
  child->SetParent(this);
  child->SetSubtreePropertyChanged();

  index = std::min(index, inputs_.children.size());
  inputs_.children.insert(inputs_.children.begin() + index, child);
  SetNeedsFullTreeSync();
}

void Layer::RemoveFromParent() {
  DCHECK(IsPropertyChangeAllowed());
  if (parent_)
    parent_->RemoveChildOrDependent(this);
}

void Layer::RemoveChildOrDependent(Layer* child) {
  if (inputs_.mask_layer.get() == child) {
    inputs_.mask_layer->SetParent(nullptr);
    inputs_.mask_layer = nullptr;
    SetNeedsFullTreeSync();
    return;
  }

  for (LayerList::iterator iter = inputs_.children.begin();
       iter != inputs_.children.end(); ++iter) {
    if (iter->get() != child)
      continue;

    child->SetParent(nullptr);
    AddDrawableDescendants(-child->NumDescendantsThatDrawContent() -
                           (child->DrawsContent() ? 1 : 0));
    inputs_.children.erase(iter);
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
      std::find_if(inputs_.children.begin(), inputs_.children.end(),
                   [reference](const scoped_refptr<Layer>& layer) {
                     return layer.get() == reference;
                   });
  DCHECK(reference_it != inputs_.children.end());
  size_t reference_index = reference_it - inputs_.children.begin();
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
  inputs_.bounds = size;

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
  while (inputs_.children.size()) {
    Layer* layer = inputs_.children[0].get();
    DCHECK_EQ(this, layer->parent());
    layer->RemoveFromParent();
  }
}

void Layer::SetChildren(const LayerList& children) {
  DCHECK(IsPropertyChangeAllowed());
  if (children == inputs_.children)
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
        std::find_if(inputs_.copy_requests.begin(), inputs_.copy_requests.end(),
                     [source](const std::unique_ptr<CopyOutputRequest>& x) {
                       return x->source() == source;
                     });
    if (it != inputs_.copy_requests.end())
      inputs_.copy_requests.erase(it);
  }
  if (request->IsEmpty())
    return;
  inputs_.copy_requests.push_back(std::move(request));
  SetSubtreePropertyChanged();
  SetNeedsCommit();
}

void Layer::SetBackgroundColor(SkColor background_color) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.background_color == background_color)
    return;
  inputs_.background_color = background_color;
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
  if (inputs_.masks_to_bounds == masks_to_bounds)
    return;
  inputs_.masks_to_bounds = masks_to_bounds;
  SetNeedsCommit();
  SetSubtreePropertyChanged();
}

void Layer::SetMaskLayer(Layer* mask_layer) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.mask_layer.get() == mask_layer)
    return;
  if (inputs_.mask_layer.get()) {
    DCHECK_EQ(this, inputs_.mask_layer->parent());
    inputs_.mask_layer->RemoveFromParent();
  }
  inputs_.mask_layer = mask_layer;
  if (inputs_.mask_layer.get()) {
    // The mask layer should not have any children.
    DCHECK(inputs_.mask_layer->children().empty());

    inputs_.mask_layer->RemoveFromParent();
    DCHECK(!inputs_.mask_layer->parent());
    inputs_.mask_layer->SetParent(this);
    inputs_.mask_layer->SetIsMask(true);
  }
  SetSubtreePropertyChanged();
  SetNeedsFullTreeSync();
}

void Layer::SetFilters(const FilterOperations& filters) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.filters == filters)
    return;
  inputs_.filters = filters;
  SetSubtreePropertyChanged();
  SetNeedsCommit();
}

void Layer::SetBackgroundFilters(const FilterOperations& filters) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.background_filters == filters)
    return;
  inputs_.background_filters = filters;
  SetSubtreePropertyChanged();
  SetNeedsCommit();
}

void Layer::SetFiltersOrigin(const gfx::PointF& filters_origin) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.filters_origin == filters_origin)
    return;
  inputs_.filters_origin = filters_origin;
  SetSubtreePropertyChanged();
  SetNeedsCommit();
}

void Layer::SetOpacity(float opacity) {
  DCHECK(IsPropertyChangeAllowed());
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);

  if (inputs_.opacity == opacity)
    return;
  // We need to force a property tree rebuild when opacity changes from 1 to a
  // non-1 value or vice-versa as render surfaces can change.
  bool force_rebuild = opacity == 1.f || inputs_.opacity == 1.f;
  inputs_.opacity = opacity;
  SetSubtreePropertyChanged();
  if (layer_tree_host_ && !force_rebuild) {
    PropertyTrees* property_trees = layer_tree_->property_trees();
    auto effect_id_to_index = property_trees->effect_id_to_index_map.find(id());
    if (effect_id_to_index != property_trees->effect_id_to_index_map.end()) {
      EffectNode* node =
          property_trees->effect_tree.Node(effect_id_to_index->second);
      node->opacity = opacity;
      node->effect_changed = true;
      property_trees->effect_tree.set_needs_update(true);
      SetNeedsCommitNoRebuild();
      return;
    }
  }
  SetNeedsCommit();
}

float Layer::EffectiveOpacity() const {
  return inputs_.hide_layer_and_subtree ? 0.f : inputs_.opacity;
}

bool Layer::OpacityCanAnimateOnImplThread() const {
  return false;
}

bool Layer::AlwaysUseActiveTreeOpacity() const {
  return false;
}

void Layer::SetBlendMode(SkXfermode::Mode blend_mode) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.blend_mode == blend_mode)
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

  inputs_.blend_mode = blend_mode;
  SetNeedsCommit();
  SetSubtreePropertyChanged();
}

void Layer::SetIsRootForIsolatedGroup(bool root) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.is_root_for_isolated_group == root)
    return;
  inputs_.is_root_for_isolated_group = root;
  SetNeedsCommit();
}

void Layer::SetContentsOpaque(bool opaque) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.contents_opaque == opaque)
    return;
  inputs_.contents_opaque = opaque;
  SetNeedsCommit();
  SetSubtreePropertyChanged();
}

void Layer::SetPosition(const gfx::PointF& position) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.position == position)
    return;
  inputs_.position = position;

  if (!layer_tree_host_)
    return;

  SetSubtreePropertyChanged();
  PropertyTrees* property_trees = layer_tree_->property_trees();
  if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::TRANSFORM,
                                       id())) {
    DCHECK_EQ(transform_tree_index(),
              property_trees->transform_id_to_index_map[id()]);
    TransformNode* transform_node =
        property_trees->transform_tree.Node(transform_tree_index());
    transform_node->update_post_local_transform(position, transform_origin());
    if (transform_node->sticky_position_constraint_id >= 0) {
      StickyPositionNodeData* sticky_data =
          property_trees->transform_tree.StickyPositionData(
              transform_tree_index());
      sticky_data->main_thread_offset =
          position.OffsetFromOrigin() -
          sticky_data->constraints.parent_relative_sticky_box_offset
              .OffsetFromOrigin();
    }
    transform_node->needs_local_transform_update = true;
    transform_node->transform_changed = true;
    layer_tree_->property_trees()->transform_tree.set_needs_update(true);
    SetNeedsCommitNoRebuild();
    return;
  }

  SetNeedsCommit();
}

bool Layer::IsContainerForFixedPositionLayers() const {
  if (!inputs_.transform.IsIdentityOrTranslation())
    return true;
  if (parent_ && !parent_->inputs_.transform.IsIdentityOrTranslation())
    return true;
  return inputs_.is_container_for_fixed_position_layers;
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
  if (inputs_.transform == transform)
    return;

  SetSubtreePropertyChanged();
  if (layer_tree_host_) {
    PropertyTrees* property_trees = layer_tree_->property_trees();
    if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::TRANSFORM,
                                         id())) {
      // We need to trigger a rebuild if we could have affected 2d axis
      // alignment. We'll check to see if transform and inputs_.transform
      // are axis
      // align with respect to one another.
      DCHECK_EQ(transform_tree_index(),
                property_trees->transform_id_to_index_map[id()]);
      TransformNode* transform_node =
          property_trees->transform_tree.Node(transform_tree_index());
      bool preserves_2d_axis_alignment =
          Are2dAxisAligned(inputs_.transform, transform);
      transform_node->local = transform;
      transform_node->needs_local_transform_update = true;
      transform_node->transform_changed = true;
      layer_tree_->property_trees()->transform_tree.set_needs_update(true);
      if (preserves_2d_axis_alignment)
        SetNeedsCommitNoRebuild();
      else
        SetNeedsCommit();
      inputs_.transform = transform;
      return;
    }
  }

  inputs_.transform = transform;

  SetNeedsCommit();
}

void Layer::SetTransformOrigin(const gfx::Point3F& transform_origin) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.transform_origin == transform_origin)
    return;
  inputs_.transform_origin = transform_origin;

  if (!layer_tree_host_)
    return;

  SetSubtreePropertyChanged();
  PropertyTrees* property_trees = layer_tree_->property_trees();
  if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::TRANSFORM,
                                       id())) {
    DCHECK_EQ(transform_tree_index(),
              property_trees->transform_id_to_index_map[id()]);
    TransformNode* transform_node =
        property_trees->transform_tree.Node(transform_tree_index());
    transform_node->update_pre_local_transform(transform_origin);
    transform_node->update_post_local_transform(position(), transform_origin);
    transform_node->needs_local_transform_update = true;
    transform_node->transform_changed = true;
    layer_tree_->property_trees()->transform_tree.set_needs_update(true);
    SetNeedsCommitNoRebuild();
    return;
  }

  SetNeedsCommit();
}

bool Layer::ScrollOffsetAnimationWasInterrupted() const {
  return GetMutatorHost()->ScrollOffsetAnimationWasInterrupted(element_id());
}

bool Layer::HasOnlyTranslationTransforms() const {
  return GetMutatorHost()->HasOnlyTranslationTransforms(
      element_id(), GetElementTypeForAnimation());
}

void Layer::SetScrollParent(Layer* parent) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.scroll_parent == parent)
    return;

  if (inputs_.scroll_parent)
    inputs_.scroll_parent->RemoveScrollChild(this);

  inputs_.scroll_parent = parent;

  if (inputs_.scroll_parent)
    inputs_.scroll_parent->AddScrollChild(this);

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
  if (inputs_.clip_parent == ancestor)
    return;

  if (inputs_.clip_parent)
    inputs_.clip_parent->RemoveClipChild(this);

  inputs_.clip_parent = ancestor;

  if (inputs_.clip_parent)
    inputs_.clip_parent->AddClipChild(this);

  SetNeedsCommit();
  if (layer_tree_)
    layer_tree_->SetNeedsMetaInfoRecomputation(true);
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

  if (inputs_.scroll_offset == scroll_offset)
    return;
  inputs_.scroll_offset = scroll_offset;

  if (!layer_tree_host_)
    return;

  PropertyTrees* property_trees = layer_tree_->property_trees();
  if (scroll_tree_index() != ScrollTree::kInvalidNodeId && scrollable())
    property_trees->scroll_tree.SetScrollOffset(id(), scroll_offset);

  if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::TRANSFORM,
                                       id())) {
    DCHECK_EQ(transform_tree_index(),
              property_trees->transform_id_to_index_map[id()]);
    TransformNode* transform_node =
        property_trees->transform_tree.Node(transform_tree_index());
    transform_node->scroll_offset = CurrentScrollOffset();
    transform_node->needs_local_transform_update = true;
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
  if (inputs_.scroll_offset == scroll_offset)
    return;
  inputs_.scroll_offset = scroll_offset;
  SetNeedsPushProperties();

  bool needs_rebuild = true;

  PropertyTrees* property_trees = layer_tree_->property_trees();
  if (scroll_tree_index() != ScrollTree::kInvalidNodeId && scrollable())
    property_trees->scroll_tree.SetScrollOffset(id(), scroll_offset);

  if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::TRANSFORM,
                                       id())) {
    DCHECK_EQ(transform_tree_index(),
              property_trees->transform_id_to_index_map[id()]);
    TransformNode* transform_node =
        property_trees->transform_tree.Node(transform_tree_index());
    transform_node->scroll_offset = CurrentScrollOffset();
    transform_node->needs_local_transform_update = true;
    property_trees->transform_tree.set_needs_update(true);
    needs_rebuild = false;
  }

  if (needs_rebuild)
    property_trees->needs_rebuild = true;

  if (!inputs_.did_scroll_callback.is_null())
    inputs_.did_scroll_callback.Run();
  // The callback could potentially change the layer structure:
  // "this" may have been destroyed during the process.
}

void Layer::SetScrollClipLayerId(int clip_layer_id) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.scroll_clip_layer_id == clip_layer_id)
    return;
  inputs_.scroll_clip_layer_id = clip_layer_id;
  SetNeedsCommit();
}

Layer* Layer::scroll_clip_layer() const {
  DCHECK(layer_tree_);
  return layer_tree_->LayerById(inputs_.scroll_clip_layer_id);
}

void Layer::SetUserScrollable(bool horizontal, bool vertical) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.user_scrollable_horizontal == horizontal &&
      inputs_.user_scrollable_vertical == vertical)
    return;
  inputs_.user_scrollable_horizontal = horizontal;
  inputs_.user_scrollable_vertical = vertical;
  SetNeedsCommit();
}

void Layer::AddMainThreadScrollingReasons(
    uint32_t main_thread_scrolling_reasons) {
  DCHECK(IsPropertyChangeAllowed());
  DCHECK(main_thread_scrolling_reasons);
  uint32_t new_reasons =
      inputs_.main_thread_scrolling_reasons | main_thread_scrolling_reasons;
  if (inputs_.main_thread_scrolling_reasons == new_reasons)
    return;
  inputs_.main_thread_scrolling_reasons = new_reasons;
  didUpdateMainThreadScrollingReasons();
  SetNeedsCommit();
}

void Layer::ClearMainThreadScrollingReasons(
    uint32_t main_thread_scrolling_reasons_to_clear) {
  DCHECK(IsPropertyChangeAllowed());
  DCHECK(main_thread_scrolling_reasons_to_clear);
  uint32_t new_reasons = ~main_thread_scrolling_reasons_to_clear &
                         inputs_.main_thread_scrolling_reasons;
  if (new_reasons == inputs_.main_thread_scrolling_reasons)
    return;
  inputs_.main_thread_scrolling_reasons = new_reasons;
  didUpdateMainThreadScrollingReasons();
  SetNeedsCommit();
}

void Layer::SetNonFastScrollableRegion(const Region& region) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.non_fast_scrollable_region == region)
    return;
  inputs_.non_fast_scrollable_region = region;
  SetNeedsCommit();
}

void Layer::SetTouchEventHandlerRegion(const Region& region) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.touch_event_handler_region == region)
    return;

  inputs_.touch_event_handler_region = region;
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
  if (inputs_.double_sided == double_sided)
    return;
  inputs_.double_sided = double_sided;
  SetNeedsCommit();
  SetSubtreePropertyChanged();
}

void Layer::Set3dSortingContextId(int id) {
  DCHECK(IsPropertyChangeAllowed());
  if (id == inputs_.sorting_context_id)
    return;
  inputs_.sorting_context_id = id;
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
      layer_tree_->property_trees()->sequence_number !=
          property_tree_sequence_number_) {
    return TransformTree::kInvalidNodeId;
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
      layer_tree_->property_trees()->sequence_number !=
          property_tree_sequence_number_) {
    return ClipTree::kInvalidNodeId;
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
      layer_tree_->property_trees()->sequence_number !=
          property_tree_sequence_number_) {
    return EffectTree::kInvalidNodeId;
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
      layer_tree_->property_trees()->sequence_number !=
          property_tree_sequence_number_) {
    return ScrollTree::kInvalidNodeId;
  }
  return scroll_tree_index_;
}

void Layer::InvalidatePropertyTreesIndices() {
  SetTransformTreeIndex(TransformTree::kInvalidNodeId);
  SetClipTreeIndex(ClipTree::kInvalidNodeId);
  SetEffectTreeIndex(EffectTree::kInvalidNodeId);
  SetScrollTreeIndex(ScrollTree::kInvalidNodeId);
}

void Layer::SetShouldFlattenTransform(bool should_flatten) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.should_flatten_transform == should_flatten)
    return;
  inputs_.should_flatten_transform = should_flatten;
  SetNeedsCommit();
  SetSubtreePropertyChanged();
}

void Layer::SetUseParentBackfaceVisibility(bool use) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.use_parent_backface_visibility == use)
    return;
  inputs_.use_parent_backface_visibility = use;
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
  if (inputs_.is_drawable == is_drawable)
    return;

  inputs_.is_drawable = is_drawable;
  UpdateDrawsContent(HasDrawableContent());
}

void Layer::SetHideLayerAndSubtree(bool hide) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.hide_layer_and_subtree == hide)
    return;

  inputs_.hide_layer_and_subtree = hide;
  SetNeedsCommit();
  SetSubtreePropertyChanged();
}

void Layer::SetNeedsDisplayRect(const gfx::Rect& dirty_rect) {
  if (dirty_rect.IsEmpty())
    return;

  SetNeedsPushProperties();
  inputs_.update_rect.Union(dirty_rect);

  if (DrawsContent())
    SetNeedsUpdate();
}

bool Layer::DescendantIsFixedToContainerLayer() const {
  for (size_t i = 0; i < inputs_.children.size(); ++i) {
    if (inputs_.children[i]->inputs_.position_constraint.is_fixed_position() ||
        inputs_.children[i]->DescendantIsFixedToContainerLayer())
      return true;
  }
  return false;
}

void Layer::SetIsContainerForFixedPositionLayers(bool container) {
  if (inputs_.is_container_for_fixed_position_layers == container)
    return;
  inputs_.is_container_for_fixed_position_layers = container;

  if (layer_tree_host_ && layer_tree_host_->CommitRequested())
    return;

  // Only request a commit if we have a fixed positioned descendant.
  if (DescendantIsFixedToContainerLayer())
    SetNeedsCommit();
}

void Layer::SetPositionConstraint(const LayerPositionConstraint& constraint) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.position_constraint == constraint)
    return;
  inputs_.position_constraint = constraint;
  SetNeedsCommit();
}

void Layer::SetStickyPositionConstraint(
    const LayerStickyPositionConstraint& constraint) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.sticky_position_constraint == constraint)
    return;
  inputs_.sticky_position_constraint = constraint;
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

bool Layer::IsSnapped() {
  return scrollable();
}

void Layer::PushPropertiesTo(LayerImpl* layer) {
  TRACE_EVENT0("cc", "Layer::PushPropertiesTo");
  DCHECK(layer_tree_host_);

  // If we did not SavePaintProperties() for the layer this frame, then push the
  // real property values, not the paint property values.
  bool use_paint_properties = paint_properties_.source_frame_number ==
                              layer_tree_host_->SourceFrameNumber();

  layer->SetBackgroundColor(inputs_.background_color);
  layer->SetSafeOpaqueBackgroundColor(safe_opaque_background_color_);
  layer->SetBounds(use_paint_properties ? paint_properties_.bounds
                                        : inputs_.bounds);

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
  if (subtree_property_changed_)
    layer->NoteLayerPropertyChanged();
  layer->set_may_contain_video(may_contain_video_);
  layer->SetMasksToBounds(inputs_.masks_to_bounds);
  layer->set_main_thread_scrolling_reasons(
      inputs_.main_thread_scrolling_reasons);
  layer->SetNonFastScrollableRegion(inputs_.non_fast_scrollable_region);
  layer->SetTouchEventHandlerRegion(inputs_.touch_event_handler_region);
  layer->SetContentsOpaque(inputs_.contents_opaque);
  layer->SetPosition(inputs_.position);
  layer->set_should_flatten_transform_from_property_tree(
      should_flatten_transform_from_property_tree_);
  layer->set_draw_blend_mode(draw_blend_mode_);
  layer->SetUseParentBackfaceVisibility(inputs_.use_parent_backface_visibility);
  layer->SetUseLocalTransformForBackfaceVisibility(
      use_local_transform_for_backface_visibility_);
  layer->SetShouldCheckBackfaceVisibility(should_check_backface_visibility_);
  layer->Set3dSortingContextId(inputs_.sorting_context_id);

  layer->SetScrollClipLayer(inputs_.scroll_clip_layer_id);
  layer->set_user_scrollable_horizontal(inputs_.user_scrollable_horizontal);
  layer->set_user_scrollable_vertical(inputs_.user_scrollable_vertical);
  layer->SetElementId(inputs_.element_id);
  layer->SetMutableProperties(inputs_.mutable_properties);

  // When a scroll offset animation is interrupted the new scroll position on
  // the pending tree will clobber any impl-side scrolling occuring on the
  // active tree. To do so, avoid scrolling the pending tree along with it
  // instead of trying to undo that scrolling later.
  if (ScrollOffsetAnimationWasInterrupted())
    layer_tree_->property_trees()
        ->scroll_tree.SetScrollOffsetClobberActiveValue(layer->id());

  // If the main thread commits multiple times before the impl thread actually
  // draws, then damage tracking will become incorrect if we simply clobber the
  // update_rect here. The LayerImpl's update_rect needs to accumulate (i.e.
  // union) any update changes that have occurred on the main thread.
  inputs_.update_rect.Union(layer->update_rect());
  layer->SetUpdateRect(inputs_.update_rect);

  layer->SetHasWillChangeTransformHint(has_will_change_transform_hint());
  if (has_preferred_raster_bounds())
    layer->SetPreferredRasterBounds(preferred_raster_bounds());
  else
    layer->ClearPreferredRasterBounds();
  layer->SetNeedsPushProperties();

  // Reset any state that should be cleared for the next update.
  subtree_property_changed_ = false;
  inputs_.update_rect = gfx::Rect();

  layer_tree_->RemoveLayerShouldPushProperties(this);
}

void Layer::TakeCopyRequests(
    std::vector<std::unique_ptr<CopyOutputRequest>>* requests) {
  for (auto& it : inputs_.copy_requests) {
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner =
        layer_tree_host()->GetTaskRunnerProvider()->MainThreadTaskRunner();
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

  inputs_.copy_requests.clear();
}

void Layer::SetTypeForProtoSerialization(proto::LayerNode* proto) const {
  proto->set_type(proto::LayerNode::LAYER);
}

void Layer::ToLayerNodeProto(proto::LayerNode* proto) const {
  proto->set_id(inputs_.layer_id);
  SetTypeForProtoSerialization(proto);

  if (parent_)
    proto->set_parent_id(parent_->id());

  DCHECK_EQ(0, proto->children_size());
  for (const auto& child : inputs_.children) {
    child->ToLayerNodeProto(proto->add_children());
  }

  if (inputs_.mask_layer)
    inputs_.mask_layer->ToLayerNodeProto(proto->mutable_mask_layer());
}

void Layer::ToLayerPropertiesProto(proto::LayerProperties* proto) {
  proto->set_id(inputs_.layer_id);

  proto::BaseLayerProperties* base = proto->mutable_base();
  RectToProto(inputs_.update_rect, base->mutable_update_rect());
  inputs_.update_rect = gfx::Rect();

  bool use_paint_properties = layer_tree_host_ &&
                              paint_properties_.source_frame_number ==
                                  layer_tree_host_->SourceFrameNumber();
  SizeToProto(use_paint_properties ? paint_properties_.bounds : inputs_.bounds,
              base->mutable_bounds());

  base->set_masks_to_bounds(inputs_.masks_to_bounds);
  base->set_opacity(inputs_.opacity);
  base->set_blend_mode(SkXfermodeModeToProto(inputs_.blend_mode));
  base->set_is_root_for_isolated_group(inputs_.is_root_for_isolated_group);
  base->set_contents_opaque(inputs_.contents_opaque);
  PointFToProto(inputs_.position, base->mutable_position());
  TransformToProto(inputs_.transform, base->mutable_transform());
  Point3FToProto(inputs_.transform_origin, base->mutable_transform_origin());
  base->set_is_drawable(inputs_.is_drawable);
  base->set_double_sided(inputs_.double_sided);
  base->set_should_flatten_transform(inputs_.should_flatten_transform);
  base->set_sorting_context_id(inputs_.sorting_context_id);
  base->set_use_parent_backface_visibility(
      inputs_.use_parent_backface_visibility);
  base->set_background_color(inputs_.background_color);
  ScrollOffsetToProto(inputs_.scroll_offset, base->mutable_scroll_offset());
  base->set_scroll_clip_layer_id(inputs_.scroll_clip_layer_id);
  base->set_user_scrollable_horizontal(inputs_.user_scrollable_horizontal);
  base->set_user_scrollable_vertical(inputs_.user_scrollable_vertical);
  base->set_main_thread_scrolling_reasons(
      inputs_.main_thread_scrolling_reasons);
  RegionToProto(inputs_.non_fast_scrollable_region,
                base->mutable_non_fast_scrollable_region());
  RegionToProto(inputs_.touch_event_handler_region,
                base->mutable_touch_event_handler_region());
  base->set_is_container_for_fixed_position_layers(
      inputs_.is_container_for_fixed_position_layers);
  inputs_.position_constraint.ToProtobuf(base->mutable_position_constraint());
  inputs_.sticky_position_constraint.ToProtobuf(
      base->mutable_sticky_position_constraint());

  int scroll_parent_id =
      inputs_.scroll_parent ? inputs_.scroll_parent->id() : INVALID_ID;
  base->set_scroll_parent_id(scroll_parent_id);

  int clip_parent_id =
      inputs_.clip_parent ? inputs_.clip_parent->id() : INVALID_ID;
  base->set_clip_parent_id(clip_parent_id);

  base->set_has_will_change_transform_hint(
      inputs_.has_will_change_transform_hint);
  base->set_hide_layer_and_subtree(inputs_.hide_layer_and_subtree);

  // TODO(nyquist): Add support for serializing FilterOperations for
  // |filters_| and |background_filters_|. See crbug.com/541321.
}

std::unique_ptr<LayerImpl> Layer::CreateLayerImpl(LayerTreeImpl* tree_impl) {
  return LayerImpl::Create(tree_impl, inputs_.layer_id);
}

bool Layer::DrawsContent() const {
  return draws_content_;
}

bool Layer::HasDrawableContent() const {
  return inputs_.is_drawable;
}

void Layer::UpdateDrawsContent(bool has_drawable_content) {
  bool draws_content = has_drawable_content;
  DCHECK(inputs_.is_drawable || !has_drawable_content);
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
  paint_properties_.bounds = inputs_.bounds;
  paint_properties_.source_frame_number = layer_tree_host_->SourceFrameNumber();
}

bool Layer::Update() {
  DCHECK(layer_tree_host_);
  DCHECK_EQ(layer_tree_host_->SourceFrameNumber(),
            paint_properties_.source_frame_number)
      << "SavePaintProperties must be called for any layer that is painted.";
  return false;
}

bool Layer::IsSuitableForGpuRasterization() const {
  return true;
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
Layer::TakeDebugInfo() {
  if (inputs_.client)
    return inputs_.client->TakeDebugInfo(this);
  else
    return nullptr;
}

void Layer::didUpdateMainThreadScrollingReasons() {
  if (inputs_.client)
    inputs_.client->didUpdateMainThreadScrollingReasons();
}

void Layer::SetSubtreePropertyChanged() {
  if (subtree_property_changed_)
    return;
  subtree_property_changed_ = true;
  SetNeedsPushProperties();
}

void Layer::SetMayContainVideo(bool yes) {
  if (may_contain_video_ == yes)
    return;
  may_contain_video_ = yes;
  SetNeedsPushProperties();
}

void Layer::SetScrollbarsHiddenFromImplSide(bool hidden) {
  if (inputs_.client)
    inputs_.client->didChangeScrollbarsHidden(hidden);
}

bool Layer::FilterIsAnimating() const {
  return GetMutatorHost()->IsAnimatingFilterProperty(
      element_id(), GetElementTypeForAnimation());
}

bool Layer::TransformIsAnimating() const {
  return GetMutatorHost()->IsAnimatingTransformProperty(
      element_id(), GetElementTypeForAnimation());
}

gfx::ScrollOffset Layer::ScrollOffsetForAnimation() const {
  return CurrentScrollOffset();
}

// On<Property>Animated is called due to an ongoing accelerated animation.
// Since this animation is also being run on the compositor thread, there
// is no need to request a commit to push this value over, so the value is
// set directly rather than by calling Set<Property>.
void Layer::OnFilterAnimated(const FilterOperations& filters) {
  inputs_.filters = filters;
}

void Layer::OnOpacityAnimated(float opacity) {
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);

  if (inputs_.opacity == opacity)
    return;
  inputs_.opacity = opacity;
  // Changing the opacity may make a previously hidden layer visible, so a new
  // recording may be needed.
  SetNeedsUpdate();
  if (layer_tree_host_) {
    PropertyTrees* property_trees = layer_tree_->property_trees();
    if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::EFFECT,
                                         id())) {
      DCHECK_EQ(effect_tree_index(),
                property_trees->effect_id_to_index_map[id()]);
      EffectNode* node = property_trees->effect_tree.Node(effect_tree_index());
      node->opacity = opacity;
      property_trees->effect_tree.set_needs_update(true);
    }
  }
}

void Layer::OnTransformAnimated(const gfx::Transform& transform) {
  if (inputs_.transform == transform)
    return;
  inputs_.transform = transform;
  // Changing the transform may change the visible part of this layer, so a new
  // recording may be needed.
  SetNeedsUpdate();
  if (layer_tree_host_) {
    PropertyTrees* property_trees = layer_tree_->property_trees();
    if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::TRANSFORM,
                                         id())) {
      DCHECK_EQ(transform_tree_index(),
                property_trees->transform_id_to_index_map[id()]);
      TransformNode* node =
          property_trees->transform_tree.Node(transform_tree_index());
      node->local = transform;
      node->needs_local_transform_update = true;
      node->has_potential_animation = true;
      property_trees->transform_tree.set_needs_update(true);
    }
  }
}

void Layer::OnScrollOffsetAnimated(const gfx::ScrollOffset& scroll_offset) {
  // Do nothing. Scroll deltas will be sent from the compositor thread back
  // to the main thread in the same manner as during non-animated
  // compositor-driven scrolling.
}

void Layer::OnIsAnimatingChanged(const PropertyAnimationState& mask,
                                 const PropertyAnimationState& state) {
  DCHECK(layer_tree_host_);
  PropertyTrees* property_trees = layer_tree_->property_trees();

  TransformNode* transform_node = nullptr;
  if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::TRANSFORM,
                                       id())) {
    DCHECK_EQ(transform_tree_index(),
              property_trees->transform_id_to_index_map[id()]);
    transform_node =
        property_trees->transform_tree.Node(transform_tree_index());
  }

  EffectNode* effect_node = nullptr;
  if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::EFFECT, id())) {
    DCHECK_EQ(effect_tree_index(),
              property_trees->effect_id_to_index_map[id()]);
    effect_node = property_trees->effect_tree.Node(effect_tree_index());
  }

  for (int property = TargetProperty::FIRST_TARGET_PROPERTY;
       property <= TargetProperty::LAST_TARGET_PROPERTY; ++property) {
    switch (property) {
      case TargetProperty::TRANSFORM:
        if (transform_node) {
          if (mask.currently_running[property])
            transform_node->is_currently_animating =
                state.currently_running[property];
          if (mask.potentially_animating[property]) {
            transform_node->has_potential_animation =
                state.potentially_animating[property];
            if (state.potentially_animating[property]) {
              transform_node->has_only_translation_animations =
                  HasOnlyTranslationTransforms();
            } else {
              transform_node->has_only_translation_animations = true;
            }
            property_trees->transform_tree.set_needs_update(true);
          }
        }
        break;
      case TargetProperty::OPACITY:
        if (effect_node) {
          if (mask.currently_running[property])
            effect_node->is_currently_animating_opacity =
                state.currently_running[property];
          if (mask.potentially_animating[property]) {
            effect_node->has_potential_opacity_animation =
                state.potentially_animating[property] ||
                OpacityCanAnimateOnImplThread();
            property_trees->effect_tree.set_needs_update(true);
          }
        }
        break;
      case TargetProperty::FILTER:
        if (effect_node) {
          if (mask.currently_running[property])
            effect_node->is_currently_animating_filter =
                state.currently_running[property];
          if (mask.potentially_animating[property])
            effect_node->has_potential_filter_animation =
                state.potentially_animating[property];
        }
        break;
      default:
        break;
    }
  }
}

bool Layer::HasActiveAnimationForTesting() const {
  return layer_tree_host_
             ? GetMutatorHost()->HasActiveAnimationForTesting(element_id())
             : false;
}

void Layer::SetHasWillChangeTransformHint(bool has_will_change) {
  if (inputs_.has_will_change_transform_hint == has_will_change)
    return;
  inputs_.has_will_change_transform_hint = has_will_change;
  SetNeedsCommit();
}

void Layer::SetPreferredRasterBounds(const gfx::Size& preferred_raster_bounds) {
  if (inputs_.has_preferred_raster_bounds &&
      inputs_.preferred_raster_bounds == preferred_raster_bounds)
    return;

  inputs_.has_preferred_raster_bounds = true;
  inputs_.preferred_raster_bounds = preferred_raster_bounds;
  SetNeedsCommit();
}

void Layer::ClearPreferredRasterBounds() {
  if (!inputs_.has_preferred_raster_bounds)
    return;
  inputs_.has_preferred_raster_bounds = false;
  inputs_.preferred_raster_bounds = gfx::Size();
  SetNeedsCommit();
}

MutatorHost* Layer::GetMutatorHost() const {
  return layer_tree_ ? layer_tree_->mutator_host() : nullptr;
}

ElementListType Layer::GetElementTypeForAnimation() const {
  return ElementListType::ACTIVE;
}

ScrollbarLayerInterface* Layer::ToScrollbarLayer() {
  return nullptr;
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
  if (inputs_.element_id == id)
    return;
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "Layer::SetElementId", "element", id.AsValue().release());
  if (inputs_.element_id && layer_tree_host()) {
    layer_tree_->UnregisterElement(inputs_.element_id, ElementListType::ACTIVE,
                                   this);
  }

  inputs_.element_id = id;

  if (inputs_.element_id && layer_tree_host()) {
    layer_tree_->RegisterElement(inputs_.element_id, ElementListType::ACTIVE,
                                 this);
  }

  SetNeedsCommit();
}

void Layer::SetMutableProperties(uint32_t properties) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.mutable_properties == properties)
    return;
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "Layer::SetMutableProperties", "properties", properties);
  inputs_.mutable_properties = properties;
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
  return layer_tree_->property_trees()
      ->effect_tree.Node(effect_tree_index())
      ->num_copy_requests_in_subtree;
}

gfx::Transform Layer::screen_space_transform() const {
  DCHECK_NE(transform_tree_index_, TransformTree::kInvalidNodeId);
  return draw_property_utils::ScreenSpaceTransform(
      this, layer_tree_->property_trees()->transform_tree);
}

LayerTree* Layer::GetLayerTree() const {
  return layer_tree_;
}

void Layer::SetLayerIdForTesting(int id) {
  inputs_.layer_id = id;
}

}  // namespace cc
