// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_impl.h"

#include <limits>
#include <set>

#include "base/debug/trace_event.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/scrollbar_animation_controller.h"
#include "cc/animation/scrollbar_animation_controller_linear_fade.h"
#include "cc/animation/scrollbar_animation_controller_thinning.h"
#include "cc/base/math_util.h"
#include "cc/base/util.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_iterator.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/resources/ui_resource_request.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/vector2d_conversions.h"

namespace cc {

// This class exists to split the LayerScrollOffsetDelegate between the
// InnerViewportScrollLayer and the OuterViewportScrollLayer in a manner
// that never requires the embedder or LayerImpl to know about.
class LayerScrollOffsetDelegateProxy : public LayerImpl::ScrollOffsetDelegate {
 public:
  LayerScrollOffsetDelegateProxy(LayerImpl* layer,
                                 LayerScrollOffsetDelegate* delegate,
                                 LayerTreeImpl* layer_tree)
      : layer_(layer), delegate_(delegate), layer_tree_impl_(layer_tree) {}
  virtual ~LayerScrollOffsetDelegateProxy() {}

  gfx::Vector2dF last_set_scroll_offset() const {
    return last_set_scroll_offset_;
  }

  // LayerScrollOffsetDelegate implementation.
  virtual void SetTotalScrollOffset(const gfx::Vector2dF& new_offset) OVERRIDE {
    last_set_scroll_offset_ = new_offset;
    layer_tree_impl_->UpdateScrollOffsetDelegate();
  }

  virtual gfx::Vector2dF GetTotalScrollOffset() OVERRIDE {
    return layer_tree_impl_->GetDelegatedScrollOffset(layer_);
  }

  virtual bool IsExternalFlingActive() const OVERRIDE {
    return delegate_->IsExternalFlingActive();
  }

 private:
  LayerImpl* layer_;
  LayerScrollOffsetDelegate* delegate_;
  LayerTreeImpl* layer_tree_impl_;
  gfx::Vector2dF last_set_scroll_offset_;
};

LayerTreeImpl::LayerTreeImpl(LayerTreeHostImpl* layer_tree_host_impl)
    : layer_tree_host_impl_(layer_tree_host_impl),
      source_frame_number_(-1),
      hud_layer_(0),
      currently_scrolling_layer_(NULL),
      root_layer_scroll_offset_delegate_(NULL),
      background_color_(0),
      has_transparent_background_(false),
      page_scale_layer_(NULL),
      inner_viewport_scroll_layer_(NULL),
      outer_viewport_scroll_layer_(NULL),
      page_scale_factor_(1),
      page_scale_delta_(1),
      sent_page_scale_delta_(1),
      min_page_scale_factor_(0),
      max_page_scale_factor_(0),
      scrolling_layer_id_from_previous_tree_(0),
      contents_textures_purged_(false),
      requires_high_res_to_draw_(false),
      viewport_size_invalid_(false),
      needs_update_draw_properties_(true),
      needs_full_tree_sync_(true),
      next_activation_forces_redraw_(false),
      render_surface_layer_list_id_(0) {
}

LayerTreeImpl::~LayerTreeImpl() {
  // Need to explicitly clear the tree prior to destroying this so that
  // the LayerTreeImpl pointer is still valid in the LayerImpl dtor.
  DCHECK(!root_layer_);
  DCHECK(layers_with_copy_output_request_.empty());
}

void LayerTreeImpl::Shutdown() { root_layer_.reset(); }

void LayerTreeImpl::ReleaseResources() {
  if (root_layer_)
    ReleaseResourcesRecursive(root_layer_.get());
}

void LayerTreeImpl::SetRootLayer(scoped_ptr<LayerImpl> layer) {
  if (inner_viewport_scroll_layer_)
    inner_viewport_scroll_layer_->SetScrollOffsetDelegate(NULL);
  if (outer_viewport_scroll_layer_)
    outer_viewport_scroll_layer_->SetScrollOffsetDelegate(NULL);
  inner_viewport_scroll_delegate_proxy_.reset();
  outer_viewport_scroll_delegate_proxy_.reset();

  root_layer_ = layer.Pass();
  currently_scrolling_layer_ = NULL;
  inner_viewport_scroll_layer_ = NULL;
  outer_viewport_scroll_layer_ = NULL;
  page_scale_layer_ = NULL;

  layer_tree_host_impl_->OnCanDrawStateChangedForTree();
}

LayerImpl* LayerTreeImpl::InnerViewportScrollLayer() const {
  return inner_viewport_scroll_layer_;
}

LayerImpl* LayerTreeImpl::OuterViewportScrollLayer() const {
  return outer_viewport_scroll_layer_;
}

gfx::Vector2dF LayerTreeImpl::TotalScrollOffset() const {
  gfx::Vector2dF offset;

  if (inner_viewport_scroll_layer_)
    offset += inner_viewport_scroll_layer_->TotalScrollOffset();

  if (outer_viewport_scroll_layer_)
    offset += outer_viewport_scroll_layer_->TotalScrollOffset();

  return offset;
}

gfx::Vector2dF LayerTreeImpl::TotalMaxScrollOffset() const {
  gfx::Vector2dF offset;

  if (inner_viewport_scroll_layer_)
    offset += inner_viewport_scroll_layer_->MaxScrollOffset();

  if (outer_viewport_scroll_layer_)
    offset += outer_viewport_scroll_layer_->MaxScrollOffset();

  return offset;
}
gfx::Vector2dF LayerTreeImpl::TotalScrollDelta() const {
  DCHECK(inner_viewport_scroll_layer_);
  gfx::Vector2dF delta = inner_viewport_scroll_layer_->ScrollDelta();

  if (outer_viewport_scroll_layer_)
    delta += outer_viewport_scroll_layer_->ScrollDelta();

  return delta;
}

scoped_ptr<LayerImpl> LayerTreeImpl::DetachLayerTree() {
  // Clear all data structures that have direct references to the layer tree.
  scrolling_layer_id_from_previous_tree_ =
    currently_scrolling_layer_ ? currently_scrolling_layer_->id() : 0;
  if (inner_viewport_scroll_layer_)
    inner_viewport_scroll_layer_->SetScrollOffsetDelegate(NULL);
  if (outer_viewport_scroll_layer_)
    outer_viewport_scroll_layer_->SetScrollOffsetDelegate(NULL);
  inner_viewport_scroll_delegate_proxy_.reset();
  outer_viewport_scroll_delegate_proxy_.reset();
  inner_viewport_scroll_layer_ = NULL;
  outer_viewport_scroll_layer_ = NULL;
  page_scale_layer_ = NULL;
  currently_scrolling_layer_ = NULL;

  render_surface_layer_list_.clear();
  set_needs_update_draw_properties();
  return root_layer_.Pass();
}

void LayerTreeImpl::PushPropertiesTo(LayerTreeImpl* target_tree) {
  // The request queue should have been processed and does not require a push.
  DCHECK_EQ(ui_resource_request_queue_.size(), 0u);

  if (next_activation_forces_redraw_) {
    target_tree->ForceRedrawNextActivation();
    next_activation_forces_redraw_ = false;
  }

  target_tree->PassSwapPromises(&swap_promise_list_);

  target_tree->SetPageScaleValues(
      page_scale_factor(), min_page_scale_factor(), max_page_scale_factor(),
      target_tree->page_scale_delta() / target_tree->sent_page_scale_delta());
  target_tree->set_sent_page_scale_delta(1);

  if (page_scale_layer_ && inner_viewport_scroll_layer_) {
    target_tree->SetViewportLayersFromIds(
        page_scale_layer_->id(),
        inner_viewport_scroll_layer_->id(),
        outer_viewport_scroll_layer_ ? outer_viewport_scroll_layer_->id()
                                     : Layer::INVALID_ID);
  } else {
    target_tree->ClearViewportLayers();
  }
  // This should match the property synchronization in
  // LayerTreeHost::finishCommitOnImplThread().
  target_tree->set_source_frame_number(source_frame_number());
  target_tree->set_background_color(background_color());
  target_tree->set_has_transparent_background(has_transparent_background());

  if (ContentsTexturesPurged())
    target_tree->SetContentsTexturesPurged();
  else
    target_tree->ResetContentsTexturesPurged();

  // Always reset this flag on activation, as we would only have activated
  // if we were in a good state.
  target_tree->ResetRequiresHighResToDraw();

  if (ViewportSizeInvalid())
    target_tree->SetViewportSizeInvalid();
  else
    target_tree->ResetViewportSizeInvalid();

  if (hud_layer())
    target_tree->set_hud_layer(static_cast<HeadsUpDisplayLayerImpl*>(
        LayerTreeHostCommon::FindLayerInSubtree(
            target_tree->root_layer(), hud_layer()->id())));
  else
    target_tree->set_hud_layer(NULL);
}

LayerImpl* LayerTreeImpl::InnerViewportContainerLayer() const {
  return inner_viewport_scroll_layer_
             ? inner_viewport_scroll_layer_->scroll_clip_layer()
             : NULL;
}

LayerImpl* LayerTreeImpl::CurrentlyScrollingLayer() const {
  DCHECK(IsActiveTree());
  return currently_scrolling_layer_;
}

void LayerTreeImpl::SetCurrentlyScrollingLayer(LayerImpl* layer) {
  if (currently_scrolling_layer_ == layer)
    return;

  if (currently_scrolling_layer_ &&
      currently_scrolling_layer_->scrollbar_animation_controller())
    currently_scrolling_layer_->scrollbar_animation_controller()
        ->DidScrollEnd();
  currently_scrolling_layer_ = layer;
  if (layer && layer->scrollbar_animation_controller())
    layer->scrollbar_animation_controller()->DidScrollBegin();
}

void LayerTreeImpl::ClearCurrentlyScrollingLayer() {
  SetCurrentlyScrollingLayer(NULL);
  scrolling_layer_id_from_previous_tree_ = 0;
}

float LayerTreeImpl::VerticalAdjust(const int clip_layer_id) const {
  LayerImpl* container_layer = InnerViewportContainerLayer();
  if (!container_layer || clip_layer_id != container_layer->id())
    return 0.f;

  return layer_tree_host_impl_->VerticalAdjust();
}

namespace {

void ForceScrollbarParameterUpdateAfterScaleChange(LayerImpl* current_layer) {
  if (!current_layer)
    return;

  while (current_layer) {
    current_layer->ScrollbarParametersDidChange();
    current_layer = current_layer->parent();
  }
}

}  // namespace

void LayerTreeImpl::SetPageScaleFactorAndLimits(float page_scale_factor,
    float min_page_scale_factor, float max_page_scale_factor) {
  SetPageScaleValues(page_scale_factor, min_page_scale_factor,
      max_page_scale_factor, page_scale_delta_);
}

void LayerTreeImpl::SetPageScaleDelta(float delta) {
  SetPageScaleValues(page_scale_factor_, min_page_scale_factor_,
      max_page_scale_factor_, delta);
}

void LayerTreeImpl::SetPageScaleValues(float page_scale_factor,
      float min_page_scale_factor, float max_page_scale_factor,
      float page_scale_delta) {
  bool page_scale_changed =
      min_page_scale_factor != min_page_scale_factor_ ||
      max_page_scale_factor != max_page_scale_factor_ ||
      page_scale_factor != page_scale_factor_;

  min_page_scale_factor_ = min_page_scale_factor;
  max_page_scale_factor_ = max_page_scale_factor;
  page_scale_factor_ = page_scale_factor;

  float total = page_scale_factor_ * page_scale_delta;
  if (min_page_scale_factor_ && total < min_page_scale_factor_)
    page_scale_delta = min_page_scale_factor_ / page_scale_factor_;
  else if (max_page_scale_factor_ && total > max_page_scale_factor_)
    page_scale_delta = max_page_scale_factor_ / page_scale_factor_;

  if (page_scale_delta_ == page_scale_delta && !page_scale_changed)
    return;

  if (page_scale_delta_ != page_scale_delta) {
    page_scale_delta_ = page_scale_delta;

    if (IsActiveTree()) {
      LayerTreeImpl* pending_tree = layer_tree_host_impl_->pending_tree();
      if (pending_tree) {
        DCHECK_EQ(1, pending_tree->sent_page_scale_delta());
        pending_tree->SetPageScaleDelta(
            page_scale_delta_ / sent_page_scale_delta_);
      }
    }

    set_needs_update_draw_properties();
  }

  if (root_layer_scroll_offset_delegate_) {
    root_layer_scroll_offset_delegate_->UpdateRootLayerState(
        TotalScrollOffset(),
        TotalMaxScrollOffset(),
        ScrollableSize(),
        total_page_scale_factor(),
        min_page_scale_factor_,
        max_page_scale_factor_);
  }

  ForceScrollbarParameterUpdateAfterScaleChange(page_scale_layer());
}

gfx::SizeF LayerTreeImpl::ScrollableViewportSize() const {
  if (outer_viewport_scroll_layer_)
    return layer_tree_host_impl_->UnscaledScrollableViewportSize();
  else
    return gfx::ScaleSize(
        layer_tree_host_impl_->UnscaledScrollableViewportSize(),
        1.0f / total_page_scale_factor());
}

gfx::Rect LayerTreeImpl::RootScrollLayerDeviceViewportBounds() const {
  LayerImpl* root_scroll_layer = OuterViewportScrollLayer()
                                     ? OuterViewportScrollLayer()
                                     : InnerViewportScrollLayer();
  if (!root_scroll_layer || root_scroll_layer->children().empty())
    return gfx::Rect();
  LayerImpl* layer = root_scroll_layer->children()[0];
  return MathUtil::MapEnclosingClippedRect(layer->screen_space_transform(),
                                           gfx::Rect(layer->content_bounds()));
}

static void ApplySentScrollDeltasFromAbortedCommitTo(LayerImpl* layer) {
  layer->ApplySentScrollDeltasFromAbortedCommit();
}

void LayerTreeImpl::ApplySentScrollAndScaleDeltasFromAbortedCommit() {
  DCHECK(IsActiveTree());

  page_scale_factor_ *= sent_page_scale_delta_;
  page_scale_delta_ /= sent_page_scale_delta_;
  sent_page_scale_delta_ = 1.f;

  if (!root_layer())
    return;

  LayerTreeHostCommon::CallFunctionForSubtree(
      root_layer(), base::Bind(&ApplySentScrollDeltasFromAbortedCommitTo));
}

static void ApplyScrollDeltasSinceBeginMainFrameTo(LayerImpl* layer) {
  layer->ApplyScrollDeltasSinceBeginMainFrame();
}

void LayerTreeImpl::ApplyScrollDeltasSinceBeginMainFrame() {
  DCHECK(IsPendingTree());
  if (!root_layer())
    return;

  LayerTreeHostCommon::CallFunctionForSubtree(
      root_layer(), base::Bind(&ApplyScrollDeltasSinceBeginMainFrameTo));
}

void LayerTreeImpl::SetViewportLayersFromIds(
    int page_scale_layer_id,
    int inner_viewport_scroll_layer_id,
    int outer_viewport_scroll_layer_id) {
  page_scale_layer_ = LayerById(page_scale_layer_id);
  DCHECK(page_scale_layer_);

  inner_viewport_scroll_layer_ =
      LayerById(inner_viewport_scroll_layer_id);
  DCHECK(inner_viewport_scroll_layer_);

  outer_viewport_scroll_layer_ =
      LayerById(outer_viewport_scroll_layer_id);
  DCHECK(outer_viewport_scroll_layer_ ||
         outer_viewport_scroll_layer_id == Layer::INVALID_ID);

  if (!root_layer_scroll_offset_delegate_)
    return;

  inner_viewport_scroll_delegate_proxy_ = make_scoped_ptr(
      new LayerScrollOffsetDelegateProxy(inner_viewport_scroll_layer_,
                                         root_layer_scroll_offset_delegate_,
                                         this));

  if (outer_viewport_scroll_layer_)
    outer_viewport_scroll_delegate_proxy_ = make_scoped_ptr(
        new LayerScrollOffsetDelegateProxy(outer_viewport_scroll_layer_,
                                           root_layer_scroll_offset_delegate_,
                                           this));
}

void LayerTreeImpl::ClearViewportLayers() {
  page_scale_layer_ = NULL;
  inner_viewport_scroll_layer_ = NULL;
  outer_viewport_scroll_layer_ = NULL;
}

bool LayerTreeImpl::UpdateDrawProperties() {
  if (!needs_update_draw_properties_)
    return true;

  // For max_texture_size.
  if (!layer_tree_host_impl_->renderer())
    return false;

  if (!root_layer())
    return false;

  needs_update_draw_properties_ = false;
  render_surface_layer_list_.clear();

  {
    TRACE_EVENT2("cc",
                 "LayerTreeImpl::UpdateDrawProperties",
                 "IsActive",
                 IsActiveTree(),
                 "SourceFrameNumber",
                 source_frame_number_);
    LayerImpl* page_scale_layer =
        page_scale_layer_ ? page_scale_layer_ : InnerViewportContainerLayer();
    bool can_render_to_separate_surface =
        !output_surface()->ForcedDrawToSoftwareDevice();

    ++render_surface_layer_list_id_;
    LayerTreeHostCommon::CalcDrawPropsImplInputs inputs(
        root_layer(),
        DrawViewportSize(),
        layer_tree_host_impl_->DrawTransform(),
        device_scale_factor(),
        total_page_scale_factor(),
        page_scale_layer,
        MaxTextureSize(),
        settings().can_use_lcd_text,
        can_render_to_separate_surface,
        settings().layer_transforms_should_scale_layer_contents,
        &render_surface_layer_list_,
        render_surface_layer_list_id_);
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
  }

  {
    TRACE_EVENT2("cc",
                 "LayerTreeImpl::UpdateTilePriorities",
                 "IsActive",
                 IsActiveTree(),
                 "SourceFrameNumber",
                 source_frame_number_);
    // LayerIterator is used here instead of CallFunctionForSubtree to only
    // UpdateTilePriorities on layers that will be visible (and thus have valid
    // draw properties) and not because any ordering is required.
    typedef LayerIterator<LayerImpl> LayerIteratorType;
    LayerIteratorType end = LayerIteratorType::End(&render_surface_layer_list_);
    for (LayerIteratorType it =
             LayerIteratorType::Begin(&render_surface_layer_list_);
         it != end;
         ++it) {
      LayerImpl* layer = *it;
      if (it.represents_itself())
        layer->UpdateTiles();

      if (!it.represents_contributing_render_surface())
        continue;

      if (layer->mask_layer())
        layer->mask_layer()->UpdateTiles();
      if (layer->replica_layer() && layer->replica_layer()->mask_layer())
        layer->replica_layer()->mask_layer()->UpdateTiles();
    }
  }

  DCHECK(!needs_update_draw_properties_) <<
      "CalcDrawProperties should not set_needs_update_draw_properties()";
  return true;
}

const LayerImplList& LayerTreeImpl::RenderSurfaceLayerList() const {
  // If this assert triggers, then the list is dirty.
  DCHECK(!needs_update_draw_properties_);
  return render_surface_layer_list_;
}

gfx::Size LayerTreeImpl::ScrollableSize() const {
  LayerImpl* root_scroll_layer = OuterViewportScrollLayer()
                                     ? OuterViewportScrollLayer()
                                     : InnerViewportScrollLayer();
  if (!root_scroll_layer || root_scroll_layer->children().empty())
    return gfx::Size();
  return root_scroll_layer->children()[0]->bounds();
}

LayerImpl* LayerTreeImpl::LayerById(int id) {
  LayerIdMap::iterator iter = layer_id_map_.find(id);
  return iter != layer_id_map_.end() ? iter->second : NULL;
}

void LayerTreeImpl::RegisterLayer(LayerImpl* layer) {
  DCHECK(!LayerById(layer->id()));
  layer_id_map_[layer->id()] = layer;
}

void LayerTreeImpl::UnregisterLayer(LayerImpl* layer) {
  DCHECK(LayerById(layer->id()));
  layer_id_map_.erase(layer->id());
}

void LayerTreeImpl::PushPersistedState(LayerTreeImpl* pending_tree) {
  pending_tree->SetCurrentlyScrollingLayer(
      LayerTreeHostCommon::FindLayerInSubtree(pending_tree->root_layer(),
          currently_scrolling_layer_ ? currently_scrolling_layer_->id() : 0));
}

static void DidBecomeActiveRecursive(LayerImpl* layer) {
  layer->DidBecomeActive();
  for (size_t i = 0; i < layer->children().size(); ++i)
    DidBecomeActiveRecursive(layer->children()[i]);
}

void LayerTreeImpl::DidBecomeActive() {
  if (!root_layer())
    return;

  if (next_activation_forces_redraw_) {
    layer_tree_host_impl_->SetFullRootLayerDamage();
    next_activation_forces_redraw_ = false;
  }

  if (scrolling_layer_id_from_previous_tree_) {
    currently_scrolling_layer_ = LayerTreeHostCommon::FindLayerInSubtree(
        root_layer_.get(), scrolling_layer_id_from_previous_tree_);
  }

  DidBecomeActiveRecursive(root_layer());
  devtools_instrumentation::DidActivateLayerTree(layer_tree_host_impl_->id(),
                                                 source_frame_number_);
}

bool LayerTreeImpl::ContentsTexturesPurged() const {
  return contents_textures_purged_;
}

void LayerTreeImpl::SetContentsTexturesPurged() {
  if (contents_textures_purged_)
    return;
  contents_textures_purged_ = true;
  layer_tree_host_impl_->OnCanDrawStateChangedForTree();
}

void LayerTreeImpl::ResetContentsTexturesPurged() {
  if (!contents_textures_purged_)
    return;
  contents_textures_purged_ = false;
  layer_tree_host_impl_->OnCanDrawStateChangedForTree();
}

void LayerTreeImpl::SetRequiresHighResToDraw() {
  requires_high_res_to_draw_ = true;
}

void LayerTreeImpl::ResetRequiresHighResToDraw() {
  requires_high_res_to_draw_ = false;
}

bool LayerTreeImpl::RequiresHighResToDraw() const {
  return requires_high_res_to_draw_;
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

Proxy* LayerTreeImpl::proxy() const {
  return layer_tree_host_impl_->proxy();
}

const LayerTreeSettings& LayerTreeImpl::settings() const {
  return layer_tree_host_impl_->settings();
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

FrameRateCounter* LayerTreeImpl::frame_rate_counter() const {
  return layer_tree_host_impl_->fps_counter();
}

PaintTimeCounter* LayerTreeImpl::paint_time_counter() const {
  return layer_tree_host_impl_->paint_time_counter();
}

MemoryHistory* LayerTreeImpl::memory_history() const {
  return layer_tree_host_impl_->memory_history();
}

bool LayerTreeImpl::device_viewport_valid_for_tile_management() const {
  return layer_tree_host_impl_->device_viewport_valid_for_tile_management();
}

gfx::Size LayerTreeImpl::device_viewport_size() const {
  return layer_tree_host_impl_->device_viewport_size();
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

int LayerTreeImpl::MaxTextureSize() const {
  return layer_tree_host_impl_->GetRendererCapabilities().max_texture_size;
}

bool LayerTreeImpl::PinchGestureActive() const {
  return layer_tree_host_impl_->pinch_gesture_active();
}

base::TimeTicks LayerTreeImpl::CurrentFrameTimeTicks() const {
  return layer_tree_host_impl_->CurrentFrameTimeTicks();
}

base::TimeDelta LayerTreeImpl::begin_impl_frame_interval() const {
  return layer_tree_host_impl_->begin_impl_frame_interval();
}

void LayerTreeImpl::SetNeedsCommit() {
  layer_tree_host_impl_->SetNeedsCommit();
}

gfx::Size LayerTreeImpl::DrawViewportSize() const {
  return layer_tree_host_impl_->DrawViewportSize();
}

scoped_ptr<ScrollbarAnimationController>
LayerTreeImpl::CreateScrollbarAnimationController(LayerImpl* scrolling_layer) {
  DCHECK(settings().scrollbar_fade_delay_ms);
  DCHECK(settings().scrollbar_fade_duration_ms);
  base::TimeDelta delay =
      base::TimeDelta::FromMilliseconds(settings().scrollbar_fade_delay_ms);
  base::TimeDelta duration =
      base::TimeDelta::FromMilliseconds(settings().scrollbar_fade_duration_ms);
  switch (settings().scrollbar_animator) {
    case LayerTreeSettings::LinearFade: {
      return ScrollbarAnimationControllerLinearFade::Create(
                 scrolling_layer, layer_tree_host_impl_, delay, duration)
          .PassAs<ScrollbarAnimationController>();
    }
    case LayerTreeSettings::Thinning: {
      return ScrollbarAnimationControllerThinning::Create(
                 scrolling_layer, layer_tree_host_impl_, delay, duration)
          .PassAs<ScrollbarAnimationController>();
    }
    case LayerTreeSettings::NoAnimator:
      NOTREACHED();
      break;
  }
  return scoped_ptr<ScrollbarAnimationController>();
}

void LayerTreeImpl::DidAnimateScrollOffset() {
  layer_tree_host_impl_->DidAnimateScrollOffset();
}

bool LayerTreeImpl::use_gpu_rasterization() const {
  return layer_tree_host_impl_->use_gpu_rasterization();
}

bool LayerTreeImpl::create_low_res_tiling() const {
  return layer_tree_host_impl_->create_low_res_tiling();
}

void LayerTreeImpl::SetNeedsRedraw() {
  layer_tree_host_impl_->SetNeedsRedraw();
}

const LayerTreeDebugState& LayerTreeImpl::debug_state() const {
  return layer_tree_host_impl_->debug_state();
}

float LayerTreeImpl::device_scale_factor() const {
  return layer_tree_host_impl_->device_scale_factor();
}

DebugRectHistory* LayerTreeImpl::debug_rect_history() const {
  return layer_tree_host_impl_->debug_rect_history();
}

AnimationRegistrar* LayerTreeImpl::animationRegistrar() const {
  return layer_tree_host_impl_->animation_registrar();
}

scoped_ptr<base::Value> LayerTreeImpl::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  TracedValue::MakeDictIntoImplicitSnapshot(
      state.get(), "cc::LayerTreeImpl", this);

  state->Set("root_layer", root_layer_->AsValue().release());

  scoped_ptr<base::ListValue> render_surface_layer_list(new base::ListValue());
  typedef LayerIterator<LayerImpl> LayerIteratorType;
  LayerIteratorType end = LayerIteratorType::End(&render_surface_layer_list_);
  for (LayerIteratorType it = LayerIteratorType::Begin(
           &render_surface_layer_list_); it != end; ++it) {
    if (!it.represents_itself())
      continue;
    render_surface_layer_list->Append(TracedValue::CreateIDRef(*it).release());
  }

  state->Set("render_surface_layer_list",
             render_surface_layer_list.release());
  return state.PassAs<base::Value>();
}

void LayerTreeImpl::SetRootLayerScrollOffsetDelegate(
    LayerScrollOffsetDelegate* root_layer_scroll_offset_delegate) {
  if (root_layer_scroll_offset_delegate_ == root_layer_scroll_offset_delegate)
    return;

  if (!root_layer_scroll_offset_delegate) {
    // Make sure we remove the proxies from their layers before
    // releasing them.
    if (InnerViewportScrollLayer())
      InnerViewportScrollLayer()->SetScrollOffsetDelegate(NULL);
    if (OuterViewportScrollLayer())
      OuterViewportScrollLayer()->SetScrollOffsetDelegate(NULL);
    inner_viewport_scroll_delegate_proxy_.reset();
    outer_viewport_scroll_delegate_proxy_.reset();
  }

  root_layer_scroll_offset_delegate_ = root_layer_scroll_offset_delegate;

  if (root_layer_scroll_offset_delegate_) {
    root_layer_scroll_offset_delegate_->UpdateRootLayerState(
        TotalScrollOffset(),
        TotalMaxScrollOffset(),
        ScrollableSize(),
        total_page_scale_factor(),
        min_page_scale_factor(),
        max_page_scale_factor());

    if (inner_viewport_scroll_layer_) {
      inner_viewport_scroll_delegate_proxy_ = make_scoped_ptr(
          new LayerScrollOffsetDelegateProxy(InnerViewportScrollLayer(),
                                             root_layer_scroll_offset_delegate_,
                                             this));
      inner_viewport_scroll_layer_->SetScrollOffsetDelegate(
          inner_viewport_scroll_delegate_proxy_.get());
    }

    if (outer_viewport_scroll_layer_) {
      outer_viewport_scroll_delegate_proxy_ = make_scoped_ptr(
          new LayerScrollOffsetDelegateProxy(OuterViewportScrollLayer(),
                                             root_layer_scroll_offset_delegate_,
                                             this));
      outer_viewport_scroll_layer_->SetScrollOffsetDelegate(
          outer_viewport_scroll_delegate_proxy_.get());
    }
  }
}

void LayerTreeImpl::UpdateScrollOffsetDelegate() {
  DCHECK(InnerViewportScrollLayer());
  DCHECK(root_layer_scroll_offset_delegate_);

  gfx::Vector2dF offset =
      inner_viewport_scroll_delegate_proxy_->last_set_scroll_offset();

  if (OuterViewportScrollLayer())
    offset += outer_viewport_scroll_delegate_proxy_->last_set_scroll_offset();

  root_layer_scroll_offset_delegate_->UpdateRootLayerState(
      offset,
      TotalMaxScrollOffset(),
      ScrollableSize(),
      total_page_scale_factor(),
      min_page_scale_factor(),
      max_page_scale_factor());
}

gfx::Vector2dF LayerTreeImpl::GetDelegatedScrollOffset(LayerImpl* layer) {
  DCHECK(root_layer_scroll_offset_delegate_);
  DCHECK(InnerViewportScrollLayer());
  if (layer == InnerViewportScrollLayer() && !OuterViewportScrollLayer())
    return root_layer_scroll_offset_delegate_->GetTotalScrollOffset();

  // If we get here, we have both inner/outer viewports, and need to distribute
  // the scroll offset between them.
  DCHECK(inner_viewport_scroll_delegate_proxy_);
  DCHECK(outer_viewport_scroll_delegate_proxy_);
  gfx::Vector2dF inner_viewport_offset =
      inner_viewport_scroll_delegate_proxy_->last_set_scroll_offset();
  gfx::Vector2dF outer_viewport_offset =
      outer_viewport_scroll_delegate_proxy_->last_set_scroll_offset();

  // It may be nothing has changed.
  gfx::Vector2dF delegate_offset =
      root_layer_scroll_offset_delegate_->GetTotalScrollOffset();
  if (inner_viewport_offset + outer_viewport_offset == delegate_offset) {
    if (layer == InnerViewportScrollLayer())
      return inner_viewport_offset;
    else
      return outer_viewport_offset;
  }

  gfx::Vector2d max_outer_viewport_scroll_offset =
      OuterViewportScrollLayer()->MaxScrollOffset();

  outer_viewport_offset = delegate_offset - inner_viewport_offset;
  outer_viewport_offset.SetToMin(max_outer_viewport_scroll_offset);
  outer_viewport_offset.SetToMax(gfx::Vector2d());

  if (layer == OuterViewportScrollLayer())
    return outer_viewport_offset;

  inner_viewport_offset = delegate_offset - outer_viewport_offset;

  return inner_viewport_offset;
}

void LayerTreeImpl::QueueSwapPromise(scoped_ptr<SwapPromise> swap_promise) {
  DCHECK(swap_promise);
  if (swap_promise_list_.size() > kMaxQueuedSwapPromiseNumber)
    BreakSwapPromises(SwapPromise::SWAP_PROMISE_LIST_OVERFLOW);
  swap_promise_list_.push_back(swap_promise.Pass());
}

void LayerTreeImpl::PassSwapPromises(
    ScopedPtrVector<SwapPromise>* new_swap_promise) {
  swap_promise_list_.insert_and_take(swap_promise_list_.end(),
                                     *new_swap_promise);
  new_swap_promise->clear();
}

void LayerTreeImpl::FinishSwapPromises(CompositorFrameMetadata* metadata) {
  for (size_t i = 0; i < swap_promise_list_.size(); i++)
    swap_promise_list_[i]->DidSwap(metadata);
  swap_promise_list_.clear();
}

void LayerTreeImpl::BreakSwapPromises(SwapPromise::DidNotSwapReason reason) {
  for (size_t i = 0; i < swap_promise_list_.size(); i++)
    swap_promise_list_[i]->DidNotSwap(reason);
  swap_promise_list_.clear();
}

void LayerTreeImpl::DidModifyTilePriorities() {
  layer_tree_host_impl_->DidModifyTilePriorities();
}

void LayerTreeImpl::set_ui_resource_request_queue(
    const UIResourceRequestQueue& queue) {
  ui_resource_request_queue_ = queue;
}

ResourceProvider::ResourceId LayerTreeImpl::ResourceIdForUIResource(
    UIResourceId uid) const {
  return layer_tree_host_impl_->ResourceIdForUIResource(uid);
}

bool LayerTreeImpl::IsUIResourceOpaque(UIResourceId uid) const {
  return layer_tree_host_impl_->IsUIResourceOpaque(uid);
}

void LayerTreeImpl::ProcessUIResourceRequestQueue() {
  while (ui_resource_request_queue_.size() > 0) {
    UIResourceRequest req = ui_resource_request_queue_.front();
    ui_resource_request_queue_.pop_front();

    switch (req.GetType()) {
      case UIResourceRequest::UIResourceCreate:
        layer_tree_host_impl_->CreateUIResource(req.GetId(), req.GetBitmap());
        break;
      case UIResourceRequest::UIResourceDelete:
        layer_tree_host_impl_->DeleteUIResource(req.GetId());
        break;
      case UIResourceRequest::UIResourceInvalidRequest:
        NOTREACHED();
        break;
    }
  }

  // If all UI resource evictions were not recreated by processing this queue,
  // then another commit is required.
  if (layer_tree_host_impl_->EvictedUIResourcesExist())
    layer_tree_host_impl_->SetNeedsCommit();
}

void LayerTreeImpl::AddLayerWithCopyOutputRequest(LayerImpl* layer) {
  // Only the active tree needs to know about layers with copy requests, as
  // they are aborted if not serviced during draw.
  DCHECK(IsActiveTree());

  // DCHECK(std::find(layers_with_copy_output_request_.begin(),
  //                 layers_with_copy_output_request_.end(),
  //                 layer) == layers_with_copy_output_request_.end());
  // TODO(danakj): Remove this once crash is found crbug.com/309777
  for (size_t i = 0; i < layers_with_copy_output_request_.size(); ++i) {
    CHECK(layers_with_copy_output_request_[i] != layer)
        << i << " of " << layers_with_copy_output_request_.size();
  }
  layers_with_copy_output_request_.push_back(layer);
}

void LayerTreeImpl::RemoveLayerWithCopyOutputRequest(LayerImpl* layer) {
  // Only the active tree needs to know about layers with copy requests, as
  // they are aborted if not serviced during draw.
  DCHECK(IsActiveTree());

  std::vector<LayerImpl*>::iterator it = std::find(
      layers_with_copy_output_request_.begin(),
      layers_with_copy_output_request_.end(),
      layer);
  DCHECK(it != layers_with_copy_output_request_.end());
  layers_with_copy_output_request_.erase(it);

  // TODO(danakj): Remove this once crash is found crbug.com/309777
  for (size_t i = 0; i < layers_with_copy_output_request_.size(); ++i) {
    CHECK(layers_with_copy_output_request_[i] != layer)
        << i << " of " << layers_with_copy_output_request_.size();
  }
}

const std::vector<LayerImpl*>& LayerTreeImpl::LayersWithCopyOutputRequest()
    const {
  // Only the active tree needs to know about layers with copy requests, as
  // they are aborted if not serviced during draw.
  DCHECK(IsActiveTree());

  return layers_with_copy_output_request_;
}

void LayerTreeImpl::ReleaseResourcesRecursive(LayerImpl* current) {
  DCHECK(current);
  current->ReleaseResources();
  if (current->mask_layer())
    ReleaseResourcesRecursive(current->mask_layer());
  if (current->replica_layer())
    ReleaseResourcesRecursive(current->replica_layer());
  for (size_t i = 0; i < current->children().size(); ++i)
    ReleaseResourcesRecursive(current->children()[i]);
}

template <typename LayerType>
static inline bool LayerClipsSubtree(LayerType* layer) {
  return layer->masks_to_bounds() || layer->mask_layer();
}

static bool PointHitsRect(
    const gfx::PointF& screen_space_point,
    const gfx::Transform& local_space_to_screen_space_transform,
    const gfx::RectF& local_space_rect,
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

  if (!local_space_rect.Contains(hit_test_point_in_local_space))
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
                            const Region& layer_space_region,
                            float layer_content_scale_x,
                            float layer_content_scale_y) {
  // If the transform is not invertible, then assume that this point doesn't hit
  // this region.
  gfx::Transform inverse_screen_space_transform(
      gfx::Transform::kSkipInitialization);
  if (!screen_space_transform.GetInverse(&inverse_screen_space_transform))
    return false;

  // Transform the hit test point from screen space to the local space of the
  // given region.
  bool clipped = false;
  gfx::PointF hit_test_point_in_content_space = MathUtil::ProjectPoint(
      inverse_screen_space_transform, screen_space_point, &clipped);
  gfx::PointF hit_test_point_in_layer_space =
      gfx::ScalePoint(hit_test_point_in_content_space,
                      1.f / layer_content_scale_x,
                      1.f / layer_content_scale_y);

  // If ProjectPoint could not project to a valid value, then we assume that
  // this point doesn't hit this region.
  if (clipped)
    return false;

  return layer_space_region.Contains(
      gfx::ToRoundedPoint(hit_test_point_in_layer_space));
}

static LayerImpl* GetNextClippingLayer(LayerImpl* layer) {
  if (layer->scroll_parent())
    return layer->scroll_parent();
  if (layer->clip_parent())
    return layer->clip_parent();
  return layer->parent();
}

static bool PointIsClippedBySurfaceOrClipRect(
    const gfx::PointF& screen_space_point,
    LayerImpl* layer) {
  // Walk up the layer tree and hit-test any render_surfaces and any layer
  // clip rects that are active.
  for (; layer; layer = GetNextClippingLayer(layer)) {
    if (layer->render_surface() &&
        !PointHitsRect(screen_space_point,
                       layer->render_surface()->screen_space_transform(),
                       layer->render_surface()->content_rect(),
                       NULL))
      return true;

    if (LayerClipsSubtree(layer) &&
        !PointHitsRect(screen_space_point,
                       layer->screen_space_transform(),
                       gfx::Rect(layer->content_bounds()),
                       NULL))
      return true;
  }

  // If we have finished walking all ancestors without having already exited,
  // then the point is not clipped by any ancestors.
  return false;
}

static bool PointHitsLayer(LayerImpl* layer,
                           const gfx::PointF& screen_space_point,
                           float* distance_to_intersection) {
  gfx::RectF content_rect(layer->content_bounds());
  if (!PointHitsRect(screen_space_point,
                     layer->screen_space_transform(),
                     content_rect,
                     distance_to_intersection))
    return false;

  // At this point, we think the point does hit the layer, but we need to walk
  // up the parents to ensure that the layer was not clipped in such a way
  // that the hit point actually should not hit the layer.
  if (PointIsClippedBySurfaceOrClipRect(screen_space_point, layer))
    return false;

  // Skip the HUD layer.
  if (layer == layer->layer_tree_impl()->hud_layer())
    return false;

  return true;
}

struct FindClosestMatchingLayerDataForRecursion {
  FindClosestMatchingLayerDataForRecursion()
      : closest_match(NULL),
        closest_distance(-std::numeric_limits<float>::infinity()) {}
  LayerImpl* closest_match;
  // Note that the positive z-axis points towards the camera, so bigger means
  // closer in this case, counterintuitively.
  float closest_distance;
};

template <typename Functor>
static void FindClosestMatchingLayer(
    const gfx::PointF& screen_space_point,
    LayerImpl* layer,
    const Functor& func,
    FindClosestMatchingLayerDataForRecursion* data_for_recursion) {
  for (int i = layer->children().size() - 1; i >= 0; --i) {
    FindClosestMatchingLayer(
        screen_space_point, layer->children()[i], func, data_for_recursion);
  }

  float distance_to_intersection = 0.f;
  if (func(layer) &&
      PointHitsLayer(layer, screen_space_point, &distance_to_intersection) &&
      ((!data_for_recursion->closest_match ||
        distance_to_intersection > data_for_recursion->closest_distance))) {
    data_for_recursion->closest_distance = distance_to_intersection;
    data_for_recursion->closest_match = layer;
  }
}

static bool ScrollsAnyDrawnRenderSurfaceLayerListMember(LayerImpl* layer) {
  if (!layer->scrollable())
    return false;
  if (layer->IsDrawnRenderSurfaceLayerListMember())
    return true;
  if (!layer->scroll_children())
    return false;
  for (std::set<LayerImpl*>::const_iterator it =
           layer->scroll_children()->begin();
       it != layer->scroll_children()->end();
       ++it) {
    if ((*it)->IsDrawnRenderSurfaceLayerListMember())
      return true;
  }
  return false;
}

struct FindScrollingLayerFunctor {
  bool operator()(LayerImpl* layer) const {
    return ScrollsAnyDrawnRenderSurfaceLayerListMember(layer);
  }
};

LayerImpl* LayerTreeImpl::FindFirstScrollingLayerThatIsHitByPoint(
    const gfx::PointF& screen_space_point) {
  FindClosestMatchingLayerDataForRecursion data_for_recursion;
  FindClosestMatchingLayer(screen_space_point,
                           root_layer(),
                           FindScrollingLayerFunctor(),
                           &data_for_recursion);
  return data_for_recursion.closest_match;
}

struct HitTestVisibleScrollableOrTouchableFunctor {
  bool operator()(LayerImpl* layer) const {
    return layer->IsDrawnRenderSurfaceLayerListMember() ||
           ScrollsAnyDrawnRenderSurfaceLayerListMember(layer) ||
           !layer->touch_event_handler_region().IsEmpty() ||
           layer->have_wheel_event_handlers();
  }
};

LayerImpl* LayerTreeImpl::FindLayerThatIsHitByPoint(
    const gfx::PointF& screen_space_point) {
  if (!root_layer())
    return NULL;
  if (!UpdateDrawProperties())
    return NULL;
  FindClosestMatchingLayerDataForRecursion data_for_recursion;
  FindClosestMatchingLayer(screen_space_point,
                           root_layer(),
                           HitTestVisibleScrollableOrTouchableFunctor(),
                           &data_for_recursion);
  return data_for_recursion.closest_match;
}

static bool LayerHasTouchEventHandlersAt(const gfx::PointF& screen_space_point,
                                         LayerImpl* layer_impl) {
  if (layer_impl->touch_event_handler_region().IsEmpty())
    return false;

  if (!PointHitsRegion(screen_space_point,
                       layer_impl->screen_space_transform(),
                       layer_impl->touch_event_handler_region(),
                       layer_impl->contents_scale_x(),
                       layer_impl->contents_scale_y()))
    return false;

  // At this point, we think the point does hit the touch event handler region
  // on the layer, but we need to walk up the parents to ensure that the layer
  // was not clipped in such a way that the hit point actually should not hit
  // the layer.
  if (PointIsClippedBySurfaceOrClipRect(screen_space_point, layer_impl))
    return false;

  return true;
}

struct FindTouchEventLayerFunctor {
  bool operator()(LayerImpl* layer) const {
    return LayerHasTouchEventHandlersAt(screen_space_point, layer);
  }
  const gfx::PointF screen_space_point;
};

LayerImpl* LayerTreeImpl::FindLayerThatIsHitByPointInTouchHandlerRegion(
    const gfx::PointF& screen_space_point) {
  if (!root_layer())
    return NULL;
  if (!UpdateDrawProperties())
    return NULL;
  FindTouchEventLayerFunctor func = {screen_space_point};
  FindClosestMatchingLayerDataForRecursion data_for_recursion;
  FindClosestMatchingLayer(
      screen_space_point, root_layer(), func, &data_for_recursion);
  return data_for_recursion.closest_match;
}

void LayerTreeImpl::RegisterPictureLayerImpl(PictureLayerImpl* layer) {
  layer_tree_host_impl_->RegisterPictureLayerImpl(layer);
}

void LayerTreeImpl::UnregisterPictureLayerImpl(PictureLayerImpl* layer) {
  layer_tree_host_impl_->UnregisterPictureLayerImpl(layer);
}

}  // namespace cc
