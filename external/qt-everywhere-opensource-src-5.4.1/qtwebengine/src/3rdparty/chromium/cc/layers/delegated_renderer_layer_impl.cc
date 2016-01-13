// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/delegated_renderer_layer_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "cc/base/math_util.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/quad_sink.h"
#include "cc/layers/render_pass_sink.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

DelegatedRendererLayerImpl::DelegatedRendererLayerImpl(LayerTreeImpl* tree_impl,
                                                       int id)
    : LayerImpl(tree_impl, id),
      have_render_passes_to_push_(false),
      inverse_device_scale_factor_(1.0f),
      child_id_(0),
      own_child_id_(false) {
}

DelegatedRendererLayerImpl::~DelegatedRendererLayerImpl() {
  ClearRenderPasses();
  ClearChildId();
}

bool DelegatedRendererLayerImpl::HasDelegatedContent() const { return true; }

bool DelegatedRendererLayerImpl::HasContributingDelegatedRenderPasses() const {
  // The root RenderPass for the layer is merged with its target
  // RenderPass in each frame. So we only have extra RenderPasses
  // to merge when we have a non-root RenderPass present.
  return render_passes_in_draw_order_.size() > 1;
}

static ResourceProvider::ResourceId ResourceRemapHelper(
    bool* invalid_frame,
    const ResourceProvider::ResourceIdMap& child_to_parent_map,
    ResourceProvider::ResourceIdArray* resources_in_frame,
    ResourceProvider::ResourceId id) {

  ResourceProvider::ResourceIdMap::const_iterator it =
      child_to_parent_map.find(id);
  if (it == child_to_parent_map.end()) {
    *invalid_frame = true;
    return 0;
  }

  DCHECK_EQ(it->first, id);
  ResourceProvider::ResourceId remapped_id = it->second;
  resources_in_frame->push_back(id);
  return remapped_id;
}

void DelegatedRendererLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);

  DelegatedRendererLayerImpl* delegated_layer =
      static_cast<DelegatedRendererLayerImpl*>(layer);

  // If we have a new child_id to give to the active layer, it should
  // have already deleted its old child_id.
  DCHECK(delegated_layer->child_id_ == 0 ||
         delegated_layer->child_id_ == child_id_);
  delegated_layer->inverse_device_scale_factor_ = inverse_device_scale_factor_;
  delegated_layer->child_id_ = child_id_;
  delegated_layer->own_child_id_ = true;
  own_child_id_ = false;

  if (have_render_passes_to_push_) {
    // This passes ownership of the render passes to the active tree.
    delegated_layer->SetRenderPasses(&render_passes_in_draw_order_);
    DCHECK(render_passes_in_draw_order_.empty());
    have_render_passes_to_push_ = false;
  }

  // This is just a copy for testing, since resources are added to the
  // ResourceProvider in the pending tree.
  delegated_layer->resources_ = resources_;
}

void DelegatedRendererLayerImpl::CreateChildIdIfNeeded(
    const ReturnCallback& return_callback) {
  if (child_id_)
    return;

  ResourceProvider* resource_provider = layer_tree_impl()->resource_provider();
  child_id_ = resource_provider->CreateChild(return_callback);
  own_child_id_ = true;
}

void DelegatedRendererLayerImpl::SetFrameData(
    const DelegatedFrameData* frame_data,
    const gfx::RectF& damage_in_frame) {
  DCHECK(child_id_) << "CreateChildIdIfNeeded must be called first.";
  DCHECK(frame_data);
  DCHECK(!frame_data->render_pass_list.empty());
  // A frame with an empty root render pass is invalid.
  DCHECK(!frame_data->render_pass_list.back()->output_rect.IsEmpty());

  ResourceProvider* resource_provider = layer_tree_impl()->resource_provider();
    const ResourceProvider::ResourceIdMap& resource_map =
        resource_provider->GetChildToParentMap(child_id_);

  resource_provider->ReceiveFromChild(child_id_, frame_data->resource_list);

  ScopedPtrVector<RenderPass> render_pass_list;
  RenderPass::CopyAll(frame_data->render_pass_list, &render_pass_list);

  bool invalid_frame = false;
  ResourceProvider::ResourceIdArray resources_in_frame;
  DrawQuad::ResourceIteratorCallback remap_resources_to_parent_callback =
      base::Bind(&ResourceRemapHelper,
                 &invalid_frame,
                 resource_map,
                 &resources_in_frame);
  for (size_t i = 0; i < render_pass_list.size(); ++i) {
    RenderPass* pass = render_pass_list[i];
    for (size_t j = 0; j < pass->quad_list.size(); ++j) {
      DrawQuad* quad = pass->quad_list[j];
      quad->IterateResources(remap_resources_to_parent_callback);
    }
  }

  if (invalid_frame) {
    // Declare we are still using the last frame's resources.
    resource_provider->DeclareUsedResourcesFromChild(child_id_, resources_);
    return;
  }

  // Declare we are using the new frame's resources.
  resources_.swap(resources_in_frame);
  resource_provider->DeclareUsedResourcesFromChild(child_id_, resources_);

  inverse_device_scale_factor_ = 1.0f / frame_data->device_scale_factor;
  // Display size is already set so we can compute what the damage rect
  // will be in layer space. The damage may exceed the visible portion of
  // the frame, so intersect the damage to the layer's bounds.
  RenderPass* new_root_pass = render_pass_list.back();
  gfx::Size frame_size = new_root_pass->output_rect.size();
  gfx::RectF damage_in_layer = damage_in_frame;
  damage_in_layer.Scale(inverse_device_scale_factor_);
  SetUpdateRect(gfx::IntersectRects(
      gfx::UnionRects(update_rect(), damage_in_layer), gfx::Rect(bounds())));

  SetRenderPasses(&render_pass_list);
  have_render_passes_to_push_ = true;
}

void DelegatedRendererLayerImpl::SetRenderPasses(
    ScopedPtrVector<RenderPass>* render_passes_in_draw_order) {
  ClearRenderPasses();

  for (size_t i = 0; i < render_passes_in_draw_order->size(); ++i) {
    ScopedPtrVector<RenderPass>::iterator to_take =
        render_passes_in_draw_order->begin() + i;
    render_passes_index_by_id_.insert(
        std::pair<RenderPass::Id, int>((*to_take)->id, i));
    scoped_ptr<RenderPass> taken_render_pass =
        render_passes_in_draw_order->take(to_take);
    render_passes_in_draw_order_.push_back(taken_render_pass.Pass());
  }

  // Give back an empty array instead of nulls.
  render_passes_in_draw_order->clear();
}

void DelegatedRendererLayerImpl::ClearRenderPasses() {
  render_passes_index_by_id_.clear();
  render_passes_in_draw_order_.clear();
}

scoped_ptr<LayerImpl> DelegatedRendererLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return DelegatedRendererLayerImpl::Create(
      tree_impl, id()).PassAs<LayerImpl>();
}

void DelegatedRendererLayerImpl::ReleaseResources() {
  ClearRenderPasses();
  ClearChildId();
}

static inline int IndexToId(int index) { return index + 1; }
static inline int IdToIndex(int id) { return id - 1; }

RenderPass::Id DelegatedRendererLayerImpl::FirstContributingRenderPassId()
    const {
  return RenderPass::Id(id(), IndexToId(0));
}

RenderPass::Id DelegatedRendererLayerImpl::NextContributingRenderPassId(
    RenderPass::Id previous) const {
  return RenderPass::Id(previous.layer_id, previous.index + 1);
}

bool DelegatedRendererLayerImpl::ConvertDelegatedRenderPassId(
    RenderPass::Id delegated_render_pass_id,
    RenderPass::Id* output_render_pass_id) const {
  base::hash_map<RenderPass::Id, int>::const_iterator found =
      render_passes_index_by_id_.find(delegated_render_pass_id);
  if (found == render_passes_index_by_id_.end()) {
    // Be robust against a RenderPass id that isn't part of the frame.
    return false;
  }
  unsigned delegated_render_pass_index = found->second;
  *output_render_pass_id =
      RenderPass::Id(id(), IndexToId(delegated_render_pass_index));
  return true;
}

void DelegatedRendererLayerImpl::AppendContributingRenderPasses(
    RenderPassSink* render_pass_sink) {
  DCHECK(HasContributingDelegatedRenderPasses());

  const RenderPass* root_delegated_render_pass =
      render_passes_in_draw_order_.back();
  gfx::Size frame_size = root_delegated_render_pass->output_rect.size();
  gfx::Transform delegated_frame_to_root_transform = screen_space_transform();
  delegated_frame_to_root_transform.Scale(inverse_device_scale_factor_,
                                          inverse_device_scale_factor_);

  for (size_t i = 0; i < render_passes_in_draw_order_.size() - 1; ++i) {
    RenderPass::Id output_render_pass_id(-1, -1);
    bool present =
        ConvertDelegatedRenderPassId(render_passes_in_draw_order_[i]->id,
                                     &output_render_pass_id);

    // Don't clash with the RenderPass we generate if we own a RenderSurface.
    DCHECK(present) << render_passes_in_draw_order_[i]->id.layer_id << ", "
                    << render_passes_in_draw_order_[i]->id.index;
    DCHECK_GT(output_render_pass_id.index, 0);

    scoped_ptr<RenderPass> copy_pass =
        render_passes_in_draw_order_[i]->Copy(output_render_pass_id);
    copy_pass->transform_to_root_target.ConcatTransform(
        delegated_frame_to_root_transform);
    render_pass_sink->AppendRenderPass(copy_pass.Pass());
  }
}

bool DelegatedRendererLayerImpl::WillDraw(DrawMode draw_mode,
                                          ResourceProvider* resource_provider) {
  if (draw_mode == DRAW_MODE_RESOURCELESS_SOFTWARE)
    return false;
  return LayerImpl::WillDraw(draw_mode, resource_provider);
}

void DelegatedRendererLayerImpl::AppendQuads(
    QuadSink* quad_sink,
    AppendQuadsData* append_quads_data) {
  AppendRainbowDebugBorder(quad_sink, append_quads_data);

  // This list will be empty after a lost context until a new frame arrives.
  if (render_passes_in_draw_order_.empty())
    return;

  RenderPass::Id target_render_pass_id = append_quads_data->render_pass_id;

  const RenderPass* root_delegated_render_pass =
      render_passes_in_draw_order_.back();

  DCHECK(root_delegated_render_pass->output_rect.origin().IsOrigin());
  gfx::Size frame_size = root_delegated_render_pass->output_rect.size();

  // If the index of the RenderPassId is 0, then it is a RenderPass generated
  // for a layer in this compositor, not the delegating renderer. Then we want
  // to merge our root RenderPass with the target RenderPass. Otherwise, it is
  // some RenderPass which we added from the delegating renderer.
  bool should_merge_root_render_pass_with_target = !target_render_pass_id.index;
  if (should_merge_root_render_pass_with_target) {
    // Verify that the RenderPass we are appending to is created by our
    // render_target.
    DCHECK(target_render_pass_id.layer_id == render_target()->id());

    AppendRenderPassQuads(
        quad_sink, append_quads_data, root_delegated_render_pass, frame_size);
  } else {
    // Verify that the RenderPass we are appending to was created by us.
    DCHECK(target_render_pass_id.layer_id == id());

    int render_pass_index = IdToIndex(target_render_pass_id.index);
    const RenderPass* delegated_render_pass =
        render_passes_in_draw_order_[render_pass_index];
    AppendRenderPassQuads(
        quad_sink, append_quads_data, delegated_render_pass, frame_size);
  }
}

void DelegatedRendererLayerImpl::AppendRainbowDebugBorder(
    QuadSink* quad_sink,
    AppendQuadsData* append_quads_data) {
  if (!ShowDebugBorders())
    return;

  SharedQuadState* shared_quad_state = quad_sink->CreateSharedQuadState();
  PopulateSharedQuadState(shared_quad_state);

  SkColor color;
  float border_width;
  GetDebugBorderProperties(&color, &border_width);

  SkColor colors[] = {
    0x80ff0000,  // Red.
    0x80ffa500,  // Orange.
    0x80ffff00,  // Yellow.
    0x80008000,  // Green.
    0x800000ff,  // Blue.
    0x80ee82ee,  // Violet.
  };
  const int kNumColors = arraysize(colors);

  const int kStripeWidth = 300;
  const int kStripeHeight = 300;

  for (size_t i = 0; ; ++i) {
    // For horizontal lines.
    int x =  kStripeWidth * i;
    int width = std::min(kStripeWidth, content_bounds().width() - x - 1);

    // For vertical lines.
    int y = kStripeHeight * i;
    int height = std::min(kStripeHeight, content_bounds().height() - y - 1);

    gfx::Rect top(x, 0, width, border_width);
    gfx::Rect bottom(x,
                     content_bounds().height() - border_width,
                     width,
                     border_width);
    gfx::Rect left(0, y, border_width, height);
    gfx::Rect right(content_bounds().width() - border_width,
                    y,
                    border_width,
                    height);

    if (top.IsEmpty() && left.IsEmpty())
      break;

    if (!top.IsEmpty()) {
      scoped_ptr<SolidColorDrawQuad> top_quad = SolidColorDrawQuad::Create();
      top_quad->SetNew(
          shared_quad_state, top, top, colors[i % kNumColors], false);
      quad_sink->Append(top_quad.PassAs<DrawQuad>());

      scoped_ptr<SolidColorDrawQuad> bottom_quad = SolidColorDrawQuad::Create();
      bottom_quad->SetNew(shared_quad_state,
                          bottom,
                          bottom,
                          colors[kNumColors - 1 - (i % kNumColors)],
                          false);
      quad_sink->Append(bottom_quad.PassAs<DrawQuad>());
    }
    if (!left.IsEmpty()) {
      scoped_ptr<SolidColorDrawQuad> left_quad = SolidColorDrawQuad::Create();
      left_quad->SetNew(shared_quad_state,
                        left,
                        left,
                        colors[kNumColors - 1 - (i % kNumColors)],
                        false);
      quad_sink->Append(left_quad.PassAs<DrawQuad>());

      scoped_ptr<SolidColorDrawQuad> right_quad = SolidColorDrawQuad::Create();
      right_quad->SetNew(
          shared_quad_state, right, right, colors[i % kNumColors], false);
      quad_sink->Append(right_quad.PassAs<DrawQuad>());
    }
  }
}

void DelegatedRendererLayerImpl::AppendRenderPassQuads(
    QuadSink* quad_sink,
    AppendQuadsData* append_quads_data,
    const RenderPass* delegated_render_pass,
    const gfx::Size& frame_size) const {

  const SharedQuadState* delegated_shared_quad_state = NULL;
  SharedQuadState* output_shared_quad_state = NULL;

  for (size_t i = 0; i < delegated_render_pass->quad_list.size(); ++i) {
    const DrawQuad* delegated_quad = delegated_render_pass->quad_list[i];

    bool is_root_delegated_render_pass =
        delegated_render_pass == render_passes_in_draw_order_.back();

    if (delegated_quad->shared_quad_state != delegated_shared_quad_state) {
      delegated_shared_quad_state = delegated_quad->shared_quad_state;
      output_shared_quad_state = quad_sink->CreateSharedQuadState();
      output_shared_quad_state->CopyFrom(delegated_shared_quad_state);

      if (is_root_delegated_render_pass) {
        gfx::Transform delegated_frame_to_target_transform = draw_transform();
        delegated_frame_to_target_transform.Scale(inverse_device_scale_factor_,
                                                  inverse_device_scale_factor_);

        output_shared_quad_state->content_to_target_transform.ConcatTransform(
            delegated_frame_to_target_transform);

        if (render_target() == this) {
          DCHECK(!is_clipped());
          DCHECK(render_surface());
          DCHECK_EQ(0, num_unclipped_descendants());
          output_shared_quad_state->clip_rect =
              MathUtil::MapEnclosingClippedRect(
                  delegated_frame_to_target_transform,
                  output_shared_quad_state->clip_rect);
        } else {
          gfx::Rect clip_rect = drawable_content_rect();
          if (output_shared_quad_state->is_clipped) {
            clip_rect.Intersect(MathUtil::MapEnclosingClippedRect(
                delegated_frame_to_target_transform,
                output_shared_quad_state->clip_rect));
          }
          output_shared_quad_state->clip_rect = clip_rect;
          output_shared_quad_state->is_clipped = true;
        }

        output_shared_quad_state->opacity *= draw_opacity();
      }
    }
    DCHECK(output_shared_quad_state);

    gfx::Transform quad_content_to_delegated_target_space =
        output_shared_quad_state->content_to_target_transform;
    if (!is_root_delegated_render_pass) {
      quad_content_to_delegated_target_space.ConcatTransform(
          quad_sink->render_pass()->transform_to_root_target);
      quad_content_to_delegated_target_space.ConcatTransform(draw_transform());
    }

    gfx::Rect quad_visible_rect = quad_sink->UnoccludedContentRect(
        delegated_quad->visible_rect, quad_content_to_delegated_target_space);

    if (quad_visible_rect.IsEmpty())
      continue;

    scoped_ptr<DrawQuad> output_quad;
    if (delegated_quad->material != DrawQuad::RENDER_PASS) {
      output_quad = delegated_quad->Copy(output_shared_quad_state);
      output_quad->visible_rect = quad_visible_rect;
    } else {
      RenderPass::Id delegated_contributing_render_pass_id =
          RenderPassDrawQuad::MaterialCast(delegated_quad)->render_pass_id;
      RenderPass::Id output_contributing_render_pass_id(-1, -1);

      bool present =
          ConvertDelegatedRenderPassId(delegated_contributing_render_pass_id,
                                       &output_contributing_render_pass_id);

      // The frame may have a RenderPassDrawQuad that points to a RenderPass not
      // part of the frame. Just ignore these quads.
      if (present) {
        DCHECK(output_contributing_render_pass_id !=
               append_quads_data->render_pass_id);

        output_quad = RenderPassDrawQuad::MaterialCast(delegated_quad)->Copy(
            output_shared_quad_state,
            output_contributing_render_pass_id).PassAs<DrawQuad>();
        output_quad->visible_rect = quad_visible_rect;
      }
    }

    if (output_quad)
      quad_sink->Append(output_quad.Pass());
  }
}

const char* DelegatedRendererLayerImpl::LayerTypeAsString() const {
  return "cc::DelegatedRendererLayerImpl";
}

void DelegatedRendererLayerImpl::ClearChildId() {
  if (!child_id_)
    return;

  if (own_child_id_) {
    ResourceProvider* provider = layer_tree_impl()->resource_provider();
    provider->DestroyChild(child_id_);
  }

  resources_.clear();
  child_id_ = 0;
}

}  // namespace cc
