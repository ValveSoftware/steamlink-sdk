// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_COMMON_H_
#define CC_TREES_LAYER_TREE_HOST_COMMON_H_

#include <limits>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/layers/layer_lists.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/vector2d.h"

namespace cc {

class LayerImpl;
class Layer;

class CC_EXPORT LayerTreeHostCommon {
 public:
  static gfx::Rect CalculateVisibleRect(const gfx::Rect& target_surface_rect,
                                        const gfx::Rect& layer_bound_rect,
                                        const gfx::Transform& transform);

  template <typename LayerType, typename RenderSurfaceLayerListType>
  struct CalcDrawPropsInputs {
   public:
    CalcDrawPropsInputs(LayerType* root_layer,
                        const gfx::Size& device_viewport_size,
                        const gfx::Transform& device_transform,
                        float device_scale_factor,
                        float page_scale_factor,
                        const LayerType* page_scale_application_layer,
                        int max_texture_size,
                        bool can_use_lcd_text,
                        bool can_render_to_separate_surface,
                        bool can_adjust_raster_scales,
                        RenderSurfaceLayerListType* render_surface_layer_list,
                        int current_render_surface_layer_list_id)
        : root_layer(root_layer),
          device_viewport_size(device_viewport_size),
          device_transform(device_transform),
          device_scale_factor(device_scale_factor),
          page_scale_factor(page_scale_factor),
          page_scale_application_layer(page_scale_application_layer),
          max_texture_size(max_texture_size),
          can_use_lcd_text(can_use_lcd_text),
          can_render_to_separate_surface(can_render_to_separate_surface),
          can_adjust_raster_scales(can_adjust_raster_scales),
          render_surface_layer_list(render_surface_layer_list),
          current_render_surface_layer_list_id(
              current_render_surface_layer_list_id) {}

    LayerType* root_layer;
    gfx::Size device_viewport_size;
    const gfx::Transform& device_transform;
    float device_scale_factor;
    float page_scale_factor;
    const LayerType* page_scale_application_layer;
    int max_texture_size;
    bool can_use_lcd_text;
    bool can_render_to_separate_surface;
    bool can_adjust_raster_scales;
    RenderSurfaceLayerListType* render_surface_layer_list;
    int current_render_surface_layer_list_id;
  };

  template <typename LayerType, typename RenderSurfaceLayerListType>
  struct CalcDrawPropsInputsForTesting
      : public CalcDrawPropsInputs<LayerType, RenderSurfaceLayerListType> {
    CalcDrawPropsInputsForTesting(
        LayerType* root_layer,
        const gfx::Size& device_viewport_size,
        const gfx::Transform& device_transform,
        RenderSurfaceLayerListType* render_surface_layer_list);
    CalcDrawPropsInputsForTesting(
        LayerType* root_layer,
        const gfx::Size& device_viewport_size,
        RenderSurfaceLayerListType* render_surface_layer_list);

   private:
    const gfx::Transform identity_transform_;
  };

  typedef CalcDrawPropsInputs<Layer, RenderSurfaceLayerList>
      CalcDrawPropsMainInputs;
  typedef CalcDrawPropsInputsForTesting<Layer, RenderSurfaceLayerList>
      CalcDrawPropsMainInputsForTesting;
  static void CalculateDrawProperties(CalcDrawPropsMainInputs* inputs);

  typedef CalcDrawPropsInputs<LayerImpl, LayerImplList> CalcDrawPropsImplInputs;
  typedef CalcDrawPropsInputsForTesting<LayerImpl, LayerImplList>
      CalcDrawPropsImplInputsForTesting;
  static void CalculateDrawProperties(CalcDrawPropsImplInputs* inputs);

  template <typename LayerType>
  static bool RenderSurfaceContributesToTarget(LayerType*,
                                               int target_surface_layer_id);

  template <typename LayerType>
  static void CallFunctionForSubtree(
      LayerType* root_layer,
      const base::Callback<void(LayerType* layer)>& function);

  // Returns a layer with the given id if one exists in the subtree starting
  // from the given root layer (including mask and replica layers).
  template <typename LayerType>
  static LayerType* FindLayerInSubtree(LayerType* root_layer, int layer_id);

  static Layer* get_layer_as_raw_ptr(const LayerList& layers, size_t index) {
    return layers[index].get();
  }

  static LayerImpl* get_layer_as_raw_ptr(const OwnedLayerImplList& layers,
                                         size_t index) {
    return layers[index];
  }

  static LayerImpl* get_layer_as_raw_ptr(const LayerImplList& layers,
                                         size_t index) {
    return layers[index];
  }

  struct ScrollUpdateInfo {
    int layer_id;
    gfx::Vector2d scroll_delta;
  };
};

struct CC_EXPORT ScrollAndScaleSet {
  ScrollAndScaleSet();
  ~ScrollAndScaleSet();

  std::vector<LayerTreeHostCommon::ScrollUpdateInfo> scrolls;
  float page_scale_delta;
};

template <typename LayerType>
bool LayerTreeHostCommon::RenderSurfaceContributesToTarget(
    LayerType* layer,
    int target_surface_layer_id) {
  // A layer will either contribute its own content, or its render surface's
  // content, to the target surface. The layer contributes its surface's content
  // when both the following are true:
  //  (1) The layer actually has a render surface, and
  //  (2) The layer's render surface is not the same as the target surface.
  //
  // Otherwise, the layer just contributes itself to the target surface.

  return layer->render_surface() && layer->id() != target_surface_layer_id;
}

template <typename LayerType>
LayerType* LayerTreeHostCommon::FindLayerInSubtree(LayerType* root_layer,
                                                   int layer_id) {
  if (!root_layer)
    return NULL;

  if (root_layer->id() == layer_id)
    return root_layer;

  if (root_layer->mask_layer() && root_layer->mask_layer()->id() == layer_id)
    return root_layer->mask_layer();

  if (root_layer->replica_layer() &&
      root_layer->replica_layer()->id() == layer_id)
    return root_layer->replica_layer();

  for (size_t i = 0; i < root_layer->children().size(); ++i) {
    if (LayerType* found = FindLayerInSubtree(
            get_layer_as_raw_ptr(root_layer->children(), i), layer_id))
      return found;
  }
  return NULL;
}

template <typename LayerType>
void LayerTreeHostCommon::CallFunctionForSubtree(
    LayerType* root_layer,
    const base::Callback<void(LayerType* layer)>& function) {
  function.Run(root_layer);

  if (LayerType* mask_layer = root_layer->mask_layer())
    function.Run(mask_layer);
  if (LayerType* replica_layer = root_layer->replica_layer()) {
    function.Run(replica_layer);
    if (LayerType* mask_layer = replica_layer->mask_layer())
      function.Run(mask_layer);
  }

  for (size_t i = 0; i < root_layer->children().size(); ++i) {
    CallFunctionForSubtree(get_layer_as_raw_ptr(root_layer->children(), i),
                           function);
  }
}

template <typename LayerType, typename RenderSurfaceLayerListType>
LayerTreeHostCommon::CalcDrawPropsInputsForTesting<LayerType,
                                                   RenderSurfaceLayerListType>::
    CalcDrawPropsInputsForTesting(
        LayerType* root_layer,
        const gfx::Size& device_viewport_size,
        const gfx::Transform& device_transform,
        RenderSurfaceLayerListType* render_surface_layer_list)
    : CalcDrawPropsInputs<LayerType, RenderSurfaceLayerListType>(
          root_layer,
          device_viewport_size,
          device_transform,
          1.f,
          1.f,
          NULL,
          std::numeric_limits<int>::max() / 2,
          false,
          true,
          false,
          render_surface_layer_list,
          0) {
  DCHECK(root_layer);
  DCHECK(render_surface_layer_list);
}

template <typename LayerType, typename RenderSurfaceLayerListType>
LayerTreeHostCommon::CalcDrawPropsInputsForTesting<LayerType,
                                                   RenderSurfaceLayerListType>::
    CalcDrawPropsInputsForTesting(
        LayerType* root_layer,
        const gfx::Size& device_viewport_size,
        RenderSurfaceLayerListType* render_surface_layer_list)
    : CalcDrawPropsInputs<LayerType, RenderSurfaceLayerListType>(
          root_layer,
          device_viewport_size,
          identity_transform_,
          1.f,
          1.f,
          NULL,
          std::numeric_limits<int>::max() / 2,
          false,
          true,
          false,
          render_surface_layer_list,
          0) {
  DCHECK(root_layer);
  DCHECK(render_surface_layer_list);
}

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_COMMON_H_
