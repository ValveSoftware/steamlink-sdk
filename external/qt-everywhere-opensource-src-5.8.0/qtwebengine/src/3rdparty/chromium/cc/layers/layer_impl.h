// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_IMPL_H_
#define CC_LAYERS_LAYER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "cc/animation/element_id.h"
#include "cc/animation/target_property.h"
#include "cc/base/cc_export.h"
#include "cc/base/region.h"
#include "cc/base/synced_property.h"
#include "cc/input/input_handler.h"
#include "cc/layers/draw_properties.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/layer_impl_test_properties.h"
#include "cc/layers/layer_position_constraint.h"
#include "cc/layers/performance_properties.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/output/filter_operations.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/resources/resource_provider.h"
#include "cc/tiles/tile_priority.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/transform.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
class TracedValue;
}
class DictionaryValue;
}

namespace cc {

class LayerTreeHostImpl;
class LayerTreeImpl;
class MicroBenchmarkImpl;
class Occlusion;
class EffectTree;
class PrioritizedTile;
class RenderPass;
class RenderPassId;
class Renderer;
class ScrollbarLayerImplBase;
class SimpleEnclosedRegion;
class Tile;
class TransformTree;
class ScrollState;

struct AppendQuadsData;

enum DrawMode {
  DRAW_MODE_NONE,
  DRAW_MODE_HARDWARE,
  DRAW_MODE_SOFTWARE,
  DRAW_MODE_RESOURCELESS_SOFTWARE
};

class CC_EXPORT LayerImpl {
 public:
  typedef LayerImplList RenderSurfaceListType;
  typedef LayerImplList LayerListType;
  typedef RenderSurfaceImpl RenderSurfaceType;

  static std::unique_ptr<LayerImpl> Create(LayerTreeImpl* tree_impl, int id) {
    return base::WrapUnique(new LayerImpl(tree_impl, id));
  }

  virtual ~LayerImpl();

  int id() const { return layer_id_; }

  // Interactions with attached animations.
  gfx::ScrollOffset ScrollOffsetForAnimation() const;
  void OnFilterAnimated(const FilterOperations& filters);
  void OnOpacityAnimated(float opacity);
  void OnTransformAnimated(const gfx::Transform& transform);
  void OnScrollOffsetAnimated(const gfx::ScrollOffset& scroll_offset);
  void OnTransformIsCurrentlyAnimatingChanged(bool is_currently_animating);
  void OnTransformIsPotentiallyAnimatingChanged(bool has_potential_animation);
  void OnOpacityIsCurrentlyAnimatingChanged(bool is_currently_animating);
  void OnOpacityIsPotentiallyAnimatingChanged(bool has_potential_animation);
  bool IsActive() const;

  void DistributeScroll(ScrollState* scroll_state);
  void ApplyScroll(ScrollState* scroll_state);

  void set_property_tree_sequence_number(int sequence_number) {}

  void SetTransformTreeIndex(int index);
  int transform_tree_index() const { return transform_tree_index_; }

  void SetClipTreeIndex(int index);
  int clip_tree_index() const { return clip_tree_index_; }

  void SetEffectTreeIndex(int index);
  int effect_tree_index() const { return effect_tree_index_; }

  void SetScrollTreeIndex(int index);
  int scroll_tree_index() const { return scroll_tree_index_; }

  void set_offset_to_transform_parent(const gfx::Vector2dF& offset) {
    offset_to_transform_parent_ = offset;
  }
  gfx::Vector2dF offset_to_transform_parent() const {
    return offset_to_transform_parent_;
  }

  void set_should_flatten_transform_from_property_tree(bool should_flatten) {
    should_flatten_transform_from_property_tree_ = should_flatten;
  }
  bool should_flatten_transform_from_property_tree() const {
    return should_flatten_transform_from_property_tree_;
  }

  bool is_clipped() const { return draw_properties_.is_clipped; }

  void UpdatePropertyTreeTransform();
  void UpdatePropertyTreeTransformIsAnimated(bool is_animated);
  void UpdatePropertyTreeOpacity(float opacity);
  void UpdatePropertyTreeScrollOffset();

  // For compatibility with Layer.
  bool has_render_surface() const { return !!render_surface(); }

  LayerTreeImpl* layer_tree_impl() const { return layer_tree_impl_; }

  void PopulateSharedQuadState(SharedQuadState* state) const;
  void PopulateScaledSharedQuadState(SharedQuadState* state, float scale) const;
  // WillDraw must be called before AppendQuads. If WillDraw returns false,
  // AppendQuads and DidDraw will not be called. If WillDraw returns true,
  // DidDraw is guaranteed to be called before another WillDraw or before
  // the layer is destroyed. To enforce this, any class that overrides
  // WillDraw/DidDraw must call the base class version only if WillDraw
  // returns true.
  virtual bool WillDraw(DrawMode draw_mode,
                        ResourceProvider* resource_provider);
  virtual void AppendQuads(RenderPass* render_pass,
                           AppendQuadsData* append_quads_data) {}
  virtual void DidDraw(ResourceProvider* resource_provider);

  // Verify that the resource ids in the quad are valid.
  void ValidateQuadResources(DrawQuad* quad) const {
#if DCHECK_IS_ON()
    ValidateQuadResourcesInternal(quad);
#endif
  }

  virtual void GetContentsResourceId(ResourceId* resource_id,
                                     gfx::Size* resource_size) const;

  virtual void NotifyTileStateChanged(const Tile* tile) {}

  virtual ScrollbarLayerImplBase* ToScrollbarLayer();

  // Returns true if this layer has content to draw.
  void SetDrawsContent(bool draws_content);
  bool DrawsContent() const { return draws_content_; }

  LayerImplTestProperties* test_properties() {
    if (!test_properties_)
      test_properties_.reset(new LayerImplTestProperties(this));
    return test_properties_.get();
  }

  void SetBackgroundColor(SkColor background_color);
  SkColor background_color() const { return background_color_; }
  void SetSafeOpaqueBackgroundColor(SkColor background_color);
  // If contents_opaque(), return an opaque color else return a
  // non-opaque color.  Tries to return background_color(), if possible.
  SkColor SafeOpaqueBackgroundColor() const;

  void SetFilters(const FilterOperations& filters);
  const FilterOperations& filters() const { return filters_; }
  bool FilterIsAnimating() const;
  bool HasPotentiallyRunningFilterAnimation() const;

  void SetMasksToBounds(bool masks_to_bounds);
  bool masks_to_bounds() const { return masks_to_bounds_; }

  void SetContentsOpaque(bool opaque);
  bool contents_opaque() const { return contents_opaque_; }

  float Opacity() const;
  bool OpacityIsAnimating() const;
  bool HasPotentiallyRunningOpacityAnimation() const;

  void SetElementId(ElementId element_id);
  ElementId element_id() const { return element_id_; }

  void SetMutableProperties(uint32_t properties);
  uint32_t mutable_properties() const { return mutable_properties_; }

  void SetBlendMode(SkXfermode::Mode);
  SkXfermode::Mode blend_mode() const { return blend_mode_; }
  void set_draw_blend_mode(SkXfermode::Mode blend_mode) {
    draw_blend_mode_ = blend_mode;
  }
  SkXfermode::Mode draw_blend_mode() const { return draw_blend_mode_; }
  bool uses_default_blend_mode() const {
    return blend_mode_ == SkXfermode::kSrcOver_Mode;
  }

  void SetPosition(const gfx::PointF& position);
  gfx::PointF position() const { return position_; }

  bool IsAffectedByPageScale() const;

  gfx::Vector2dF FixedContainerSizeDelta() const;

  bool Is3dSorted() const { return sorting_context_id_ != 0; }

  void SetUseParentBackfaceVisibility(bool use) {
    use_parent_backface_visibility_ = use;
  }
  bool use_parent_backface_visibility() const {
    return use_parent_backface_visibility_;
  }

  void SetUseLocalTransformForBackfaceVisibility(bool use_local) {
    use_local_transform_for_backface_visibility_ = use_local;
  }
  bool use_local_transform_for_backface_visibility() const {
    return use_local_transform_for_backface_visibility_;
  }

  void SetShouldCheckBackfaceVisibility(bool should_check_backface_visibility) {
    should_check_backface_visibility_ = should_check_backface_visibility;
  }
  bool should_check_backface_visibility() const {
    return should_check_backface_visibility_;
  }

  bool ShowDebugBorders() const;

  // These invalidate the host's render surface layer list.  The caller
  // is responsible for calling set_needs_update_draw_properties on the tree
  // so that its list can be recreated.
  void ClearRenderSurfaceLayerList();
  void SetHasRenderSurface(bool has_render_surface);

  void SetForceRenderSurface(bool has_render_surface);

  RenderSurfaceImpl* render_surface() const { return render_surface_.get(); }

  // The render surface which this layer draws into. This can be either owned by
  // the same layer or an ancestor of this layer.
  RenderSurfaceImpl* render_target();
  const RenderSurfaceImpl* render_target() const;

  DrawProperties& draw_properties() { return draw_properties_; }
  const DrawProperties& draw_properties() const { return draw_properties_; }

  gfx::Transform DrawTransform() const;
  gfx::Transform ScreenSpaceTransform() const;
  PerformanceProperties<LayerImpl>& performance_properties() {
    return performance_properties_;
  }

  bool CanUseLCDText() const;

  // Setter for draw_properties_.
  void set_visible_layer_rect(const gfx::Rect& visible_rect) {
    draw_properties_.visible_layer_rect = visible_rect;
  }
  void set_clip_rect(const gfx::Rect& clip_rect) {
    draw_properties_.clip_rect = clip_rect;
  }

  // The following are shortcut accessors to get various information from
  // draw_properties_
  float draw_opacity() const { return draw_properties_.opacity; }
  bool screen_space_transform_is_animating() const {
    return draw_properties_.screen_space_transform_is_animating;
  }
  gfx::Rect clip_rect() const { return draw_properties_.clip_rect; }
  gfx::Rect drawable_content_rect() const {
    return draw_properties_.drawable_content_rect;
  }
  gfx::Rect visible_layer_rect() const {
    return draw_properties_.visible_layer_rect;
  }

  // The client should be responsible for setting bounds, content bounds and
  // contents scale to appropriate values. LayerImpl doesn't calculate any of
  // them from the other values.

  void SetBounds(const gfx::Size& bounds);
  gfx::Size bounds() const;
  // Like bounds() but doesn't snap to int. Lossy on giant pages (e.g. millions
  // of pixels) due to use of single precision float.
  gfx::SizeF BoundsForScrolling() const;
  void SetBoundsDelta(const gfx::Vector2dF& bounds_delta);
  gfx::Vector2dF bounds_delta() const { return bounds_delta_; }

  void SetCurrentScrollOffset(const gfx::ScrollOffset& scroll_offset);
  gfx::ScrollOffset CurrentScrollOffset() const;

  gfx::ScrollOffset MaxScrollOffset() const;
  gfx::ScrollOffset ClampScrollOffsetToLimits(gfx::ScrollOffset offset) const;
  gfx::Vector2dF ClampScrollToMaxScrollOffset();

  // Returns the delta of the scroll that was outside of the bounds of the
  // initial scroll
  gfx::Vector2dF ScrollBy(const gfx::Vector2dF& scroll);

  void SetScrollClipLayer(int scroll_clip_layer_id);
  int scroll_clip_layer_id() const { return scroll_clip_layer_id_; }
  LayerImpl* scroll_clip_layer() const;
  bool scrollable() const;

  void set_user_scrollable_horizontal(bool scrollable);
  bool user_scrollable_horizontal() const {
    return user_scrollable_horizontal_;
  }
  void set_user_scrollable_vertical(bool scrollable);
  bool user_scrollable_vertical() const { return user_scrollable_vertical_; }

  bool user_scrollable(ScrollbarOrientation orientation) const;

  void set_main_thread_scrolling_reasons(
      uint32_t main_thread_scrolling_reasons) {
    main_thread_scrolling_reasons_ = main_thread_scrolling_reasons;
  }
  uint32_t main_thread_scrolling_reasons() const {
    return main_thread_scrolling_reasons_;
  }

  void SetNonFastScrollableRegion(const Region& region) {
    non_fast_scrollable_region_ = region;
  }
  const Region& non_fast_scrollable_region() const {
    return non_fast_scrollable_region_;
  }

  void SetTouchEventHandlerRegion(const Region& region) {
    touch_event_handler_region_ = region;
  }
  const Region& touch_event_handler_region() const {
    return touch_event_handler_region_;
  }

  void SetTransform(const gfx::Transform& transform);
  const gfx::Transform& transform() const { return transform_; }
  bool TransformIsAnimating() const;
  bool HasPotentiallyRunningTransformAnimation() const;
  bool HasOnlyTranslationTransforms() const;
  bool AnimationsPreserveAxisAlignment() const;

  bool MaximumTargetScale(float* max_scale) const;
  bool AnimationStartScale(float* start_scale) const;

  // This includes all animations, even those that are finished but haven't yet
  // been deleted.
  bool HasAnyAnimationTargetingProperty(TargetProperty::Type property) const;

  bool HasFilterAnimationThatInflatesBounds() const;
  bool HasTransformAnimationThatInflatesBounds() const;
  bool HasAnimationThatInflatesBounds() const;

  bool FilterAnimationBoundsForBox(const gfx::BoxF& box,
                                   gfx::BoxF* bounds) const;
  bool TransformAnimationBoundsForBox(const gfx::BoxF& box,
                                      gfx::BoxF* bounds) const;

  // Note this rect is in layer space (not content space).
  void SetUpdateRect(const gfx::Rect& update_rect);
  const gfx::Rect& update_rect() const { return update_rect_; }

  void AddDamageRect(const gfx::Rect& damage_rect);
  const gfx::Rect& damage_rect() const { return damage_rect_; }

  virtual std::unique_ptr<base::DictionaryValue> LayerTreeAsJson();

  bool LayerPropertyChanged() const;

  void ResetChangeTracking();

  virtual SimpleEnclosedRegion VisibleOpaqueRegion() const;

  virtual void DidBecomeActive() {}

  virtual void DidBeginTracing();

  // Release resources held by this layer. Called when the output surface
  // that rendered this layer was lost or a rendering mode switch has occured.
  virtual void ReleaseResources();

  // Recreate resources that are required after they were released by a
  // ReleaseResources call.
  virtual void RecreateResources();

  virtual std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl);
  virtual void PushPropertiesTo(LayerImpl* layer);

  virtual void GetAllPrioritizedTilesForTracing(
      std::vector<PrioritizedTile>* prioritized_tiles) const;
  virtual void AsValueInto(base::trace_event::TracedValue* dict) const;

  virtual size_t GPUMemoryUsageInBytes() const;

  void SetNeedsPushProperties();

  virtual void RunMicroBenchmark(MicroBenchmarkImpl* benchmark);

  void SetDebugInfo(
      std::unique_ptr<base::trace_event::ConvertableToTraceFormat> debug_info);

  void set_is_drawn_render_surface_layer_list_member(bool is_member) {
    is_drawn_render_surface_layer_list_member_ = is_member;
  }

  bool is_drawn_render_surface_layer_list_member() const {
    return is_drawn_render_surface_layer_list_member_;
  }

  void Set3dSortingContextId(int id);
  int sorting_context_id() { return sorting_context_id_; }

  // Get the correct invalidation region instead of conservative Rect
  // for layers that provide it.
  virtual Region GetInvalidationRegionForDebugging();

  virtual gfx::Rect GetEnclosingRectInTargetSpace() const;

  void set_scrolls_drawn_descendant(bool scrolls_drawn_descendant) {
    scrolls_drawn_descendant_ = scrolls_drawn_descendant;
  }

  bool scrolls_drawn_descendant() { return scrolls_drawn_descendant_; }

  int num_copy_requests_in_target_subtree();

  void UpdatePropertyTreeForScrollingAndAnimationIfNeeded();

  bool IsHidden() const;

  bool InsideReplica() const;

  float GetIdealContentsScale() const;

  bool was_ever_ready_since_last_transform_animation() const {
    return was_ever_ready_since_last_transform_animation_;
  }

  void set_was_ever_ready_since_last_transform_animation(bool was_ready) {
    was_ever_ready_since_last_transform_animation_ = was_ready;
  }

  void NoteLayerPropertyChanged();

  void SetHasWillChangeTransformHint(bool has_will_change);
  bool has_will_change_transform_hint() const {
    return has_will_change_transform_hint_;
  }

 protected:
  LayerImpl(LayerTreeImpl* layer_impl,
            int id,
            scoped_refptr<SyncedScrollOffset> scroll_offset);
  LayerImpl(LayerTreeImpl* layer_impl, int id);

  // Get the color and size of the layer's debug border.
  virtual void GetDebugBorderProperties(SkColor* color, float* width) const;

  void AppendDebugBorderQuad(RenderPass* render_pass,
                             const gfx::Size& bounds,
                             const SharedQuadState* shared_quad_state,
                             AppendQuadsData* append_quads_data) const;
  void AppendDebugBorderQuad(RenderPass* render_pass,
                             const gfx::Size& bounds,
                             const SharedQuadState* shared_quad_state,
                             AppendQuadsData* append_quads_data,
                             SkColor color,
                             float width) const;

  gfx::Rect GetScaledEnclosingRectInTargetSpace(float scale) const;

 private:
  void ValidateQuadResourcesInternal(DrawQuad* quad) const;

  virtual const char* LayerTypeAsString() const;

  int layer_id_;
  LayerTreeImpl* layer_tree_impl_;

  std::unique_ptr<LayerImplTestProperties> test_properties_;

  gfx::Vector2dF bounds_delta_;

  // Properties synchronized from the associated Layer.
  gfx::Size bounds_;
  int scroll_clip_layer_id_;

  gfx::Vector2dF offset_to_transform_parent_;
  uint32_t main_thread_scrolling_reasons_;

  bool user_scrollable_horizontal_ : 1;
  bool user_scrollable_vertical_ : 1;
  bool should_flatten_transform_from_property_tree_ : 1;

  // Tracks if drawing-related properties have changed since last redraw.
  bool layer_property_changed_ : 1;

  bool masks_to_bounds_ : 1;
  bool contents_opaque_ : 1;
  bool use_parent_backface_visibility_ : 1;
  bool use_local_transform_for_backface_visibility_ : 1;
  bool should_check_backface_visibility_ : 1;
  bool draws_content_ : 1;
  bool is_drawn_render_surface_layer_list_member_ : 1;

  // This is true if and only if the layer was ever ready since it last animated
  // (all content was complete).
  bool was_ever_ready_since_last_transform_animation_ : 1;

  Region non_fast_scrollable_region_;
  Region touch_event_handler_region_;
  SkColor background_color_;
  SkColor safe_opaque_background_color_;

  SkXfermode::Mode blend_mode_;
  // draw_blend_mode may be different than blend_mode_,
  // when a RenderSurface re-parents the layer's blend_mode.
  SkXfermode::Mode draw_blend_mode_;
  gfx::PointF position_;
  gfx::Transform transform_;

  gfx::Rect clip_rect_in_target_space_;
  int transform_tree_index_;
  int effect_tree_index_;
  int clip_tree_index_;
  int scroll_tree_index_;

  FilterOperations filters_;

 protected:
  friend class TreeSynchronizer;

  // Layers that share a sorting context id will be sorted together in 3d
  // space.  0 is a special value that means this layer will not be sorted and
  // will be drawn in paint order.
  int sorting_context_id_;

  DrawMode current_draw_mode_;

 private:
  ElementId element_id_;
  uint32_t mutable_properties_;
  // Rect indicating what was repainted/updated during update.
  // Note that plugin layers bypass this and leave it empty.
  // This is in the layer's space.
  gfx::Rect update_rect_;

  // Denotes an area that is damaged and needs redraw. This is in the layer's
  // space.
  gfx::Rect damage_rect_;

  // Group of properties that need to be computed based on the layer tree
  // hierarchy before layers can be drawn.
  DrawProperties draw_properties_;
  PerformanceProperties<LayerImpl> performance_properties_;

  std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
      owned_debug_info_;
  base::trace_event::ConvertableToTraceFormat* debug_info_;
  std::unique_ptr<RenderSurfaceImpl> render_surface_;

  bool scrolls_drawn_descendant_ : 1;
  bool has_will_change_transform_hint_ : 1;
  bool needs_push_properties_ : 1;

  DISALLOW_COPY_AND_ASSIGN(LayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_IMPL_H_
