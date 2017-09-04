// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/nine_patch_layer_impl.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cc {

// Maximum number of patches that can be produced for one NinePatchLayer.
static const int kMaxOcclusionPatches = 12;
static const int kMaxPatches = 9;

namespace {

gfx::Rect ToRect(const gfx::RectF& rect) {
  return gfx::Rect(rect.x(), rect.y(), rect.width(), rect.height());
}

}  // namespace

NinePatchLayerImpl::Patch::Patch(const gfx::RectF& image_rect,
                                 const gfx::RectF& layer_rect)
    : image_rect(image_rect), layer_rect(layer_rect) {}

NinePatchLayerImpl::NinePatchLayerImpl(LayerTreeImpl* tree_impl, int id)
    : UIResourceLayerImpl(tree_impl, id),
      fill_center_(false),
      nearest_neighbor_(false) {}

NinePatchLayerImpl::~NinePatchLayerImpl() {}

std::unique_ptr<LayerImpl> NinePatchLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return NinePatchLayerImpl::Create(tree_impl, id());
}

void NinePatchLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  UIResourceLayerImpl::PushPropertiesTo(layer);
  NinePatchLayerImpl* layer_impl = static_cast<NinePatchLayerImpl*>(layer);

  layer_impl->SetLayout(image_aperture_, border_, layer_occlusion_,
                        fill_center_, nearest_neighbor_);
}

static gfx::RectF BoundsToRect(int x1, int y1, int x2, int y2) {
  return gfx::RectF(x1, y1, x2 - x1, y2 - y1);
}

static gfx::RectF NormalizedRect(const gfx::RectF& rect,
                                 float total_width,
                                 float total_height) {
  return gfx::RectF(rect.x() / total_width, rect.y() / total_height,
                    rect.width() / total_width, rect.height() / total_height);
}

void NinePatchLayerImpl::SetLayout(const gfx::Rect& aperture,
                                   const gfx::Rect& border,
                                   const gfx::Rect& layer_occlusion,
                                   bool fill_center,
                                   bool nearest_neighbor) {
  // This check imposes an ordering on the call sequence.  An UIResource must
  // exist before SetLayout can be called.
  DCHECK(ui_resource_id_);

  if (image_aperture_ == aperture && border_ == border &&
      fill_center_ == fill_center && nearest_neighbor_ == nearest_neighbor &&
      layer_occlusion_ == layer_occlusion)
    return;

  image_aperture_ = aperture;
  border_ = border;
  fill_center_ = fill_center;
  nearest_neighbor_ = nearest_neighbor;
  layer_occlusion_ = layer_occlusion;

  NoteLayerPropertyChanged();
}

void NinePatchLayerImpl::CheckGeometryLimitations() {
  // |border| is in layer space.  It cannot exceed the bounds of the layer.
  DCHECK_GE(bounds().width(), border_.width());
  DCHECK_GE(bounds().height(), border_.height());

  // Sanity Check on |border|
  DCHECK_LE(border_.x(), border_.width());
  DCHECK_LE(border_.y(), border_.height());
  DCHECK_GE(border_.x(), 0);
  DCHECK_GE(border_.y(), 0);

  // |aperture| is in image space.  It cannot exceed the bounds of the bitmap.
  DCHECK(!image_aperture_.size().IsEmpty());
  DCHECK(gfx::Rect(image_bounds_).Contains(image_aperture_))
      << "image_bounds_ " << gfx::Rect(image_bounds_).ToString()
      << " image_aperture_ " << image_aperture_.ToString();

  // Sanity check on |layer_occlusion_|. It should always be within the
  // border.
  gfx::Rect border_rect(border_.x(), border_.y(),
                        bounds().width() - border_.width(),
                        bounds().height() - border_.height());
  DCHECK(layer_occlusion_.IsEmpty() || layer_occlusion_.Contains(border_rect))
      << "border_rect " << border_rect.ToString() << " layer_occlusion_ "
      << layer_occlusion_.ToString();
}

std::vector<NinePatchLayerImpl::Patch>
NinePatchLayerImpl::ComputeQuadsWithoutOcclusion() const {
  float image_width = image_bounds_.width();
  float image_height = image_bounds_.height();
  float layer_width = bounds().width();
  float layer_height = bounds().height();
  gfx::RectF layer_aperture(border_.x(), border_.y(),
                            layer_width - border_.width(),
                            layer_height - border_.height());

  std::vector<Patch> patches;
  patches.reserve(kMaxPatches);

  // Top-left.
  patches.push_back(
      Patch(BoundsToRect(0, 0, image_aperture_.x(), image_aperture_.y()),
            BoundsToRect(0, 0, layer_aperture.x(), layer_aperture.y())));

  // Top-right.
  patches.push_back(Patch(BoundsToRect(image_aperture_.right(), 0, image_width,
                                       image_aperture_.y()),
                          BoundsToRect(layer_aperture.right(), 0, layer_width,
                                       layer_aperture.y())));

  // Bottom-left.
  patches.push_back(Patch(BoundsToRect(0, image_aperture_.bottom(),
                                       image_aperture_.x(), image_height),
                          BoundsToRect(0, layer_aperture.bottom(),
                                       layer_aperture.x(), layer_height)));

  // Bottom-right.
  patches.push_back(
      Patch(BoundsToRect(image_aperture_.right(), image_aperture_.bottom(),
                         image_width, image_height),
            BoundsToRect(layer_aperture.right(), layer_aperture.bottom(),
                         layer_width, layer_height)));

  // Top.
  patches.push_back(
      Patch(BoundsToRect(image_aperture_.x(), 0, image_aperture_.right(),
                         image_aperture_.y()),
            BoundsToRect(layer_aperture.x(), 0, layer_aperture.right(),
                         layer_aperture.y())));

  // Left.
  patches.push_back(
      Patch(BoundsToRect(0, image_aperture_.y(), image_aperture_.x(),
                         image_aperture_.bottom()),
            BoundsToRect(0, layer_aperture.y(), layer_aperture.x(),
                         layer_aperture.bottom())));

  // Right.
  patches.push_back(
      Patch(BoundsToRect(image_aperture_.right(), image_aperture_.y(),
                         image_width, image_aperture_.bottom()),
            BoundsToRect(layer_aperture.right(), layer_aperture.y(),
                         layer_width, layer_aperture.bottom())));

  // Bottom.
  patches.push_back(
      Patch(BoundsToRect(image_aperture_.x(), image_aperture_.bottom(),
                         image_aperture_.right(), image_height),
            BoundsToRect(layer_aperture.x(), layer_aperture.bottom(),
                         layer_aperture.right(), layer_height)));

  // Center.
  if (fill_center_) {
    patches.push_back(
        Patch(BoundsToRect(image_aperture_.x(), image_aperture_.y(),
                           image_aperture_.right(), image_aperture_.bottom()),
              BoundsToRect(layer_aperture.x(), layer_aperture.y(),
                           layer_aperture.right(), layer_aperture.bottom())));
  }

  return patches;
}

std::vector<NinePatchLayerImpl::Patch>
NinePatchLayerImpl::ComputeQuadsWithOcclusion() const {
  float image_width = image_bounds_.width(),
        image_height = image_bounds_.height();
  float layer_width = bounds().width(), layer_height = bounds().height();
  float layer_border_right = border_.width() - border_.x(),
        layer_border_bottom = border_.height() - border_.y();
  float image_aperture_right = image_width - image_aperture_.right(),
        image_aperture_bottom = image_height - image_aperture_.bottom();
  float layer_occlusion_right = layer_width - layer_occlusion_.right(),
        layer_occlusion_bottom = layer_height - layer_occlusion_.bottom();
  gfx::RectF image_occlusion(BoundsToRect(
      border_.x() == 0 ? 0 : (layer_occlusion_.x() * image_aperture_.x() /
                              border_.x()),
      border_.y() == 0 ? 0 : (layer_occlusion_.y() * image_aperture_.y() /
                              border_.y()),
      image_width - (layer_border_right == 0 ? 0 : layer_occlusion_right *
                                                       image_aperture_right /
                                                       layer_border_right),
      image_height - (layer_border_bottom == 0 ? 0 : layer_occlusion_bottom *
                                                         image_aperture_bottom /
                                                         layer_border_bottom)));
  gfx::RectF layer_aperture(border_.x(), border_.y(),
                            layer_width - border_.width(),
                            layer_height - border_.height());

  std::vector<Patch> patches;
  patches.reserve(kMaxOcclusionPatches);

  // Top-left-left.
  patches.push_back(
      Patch(BoundsToRect(0, 0, image_occlusion.x(), image_aperture_.y()),
            BoundsToRect(0, 0, layer_occlusion_.x(), layer_aperture.y())));

  // Top-left-right.
  patches.push_back(
      Patch(BoundsToRect(image_occlusion.x(), 0, image_aperture_.x(),
                         image_occlusion.y()),
            BoundsToRect(layer_occlusion_.x(), 0, layer_aperture.x(),
                         layer_occlusion_.y())));

  // Top-center.
  patches.push_back(
      Patch(BoundsToRect(image_aperture_.x(), 0, image_aperture_.right(),
                         image_occlusion.y()),
            BoundsToRect(layer_aperture.x(), 0, layer_aperture.right(),
                         layer_occlusion_.y())));

  // Top-right-left.
  patches.push_back(
      Patch(BoundsToRect(image_aperture_.right(), 0, image_occlusion.right(),
                         image_occlusion.y()),
            BoundsToRect(layer_aperture.right(), 0, layer_occlusion_.right(),
                         layer_occlusion_.y())));

  // Top-right-right.
  patches.push_back(Patch(BoundsToRect(image_occlusion.right(), 0, image_width,
                                       image_aperture_.y()),
                          BoundsToRect(layer_occlusion_.right(), 0, layer_width,
                                       layer_aperture.y())));

  // Left-center.
  patches.push_back(
      Patch(BoundsToRect(0, image_aperture_.y(), image_occlusion.x(),
                         image_aperture_.bottom()),
            BoundsToRect(0, layer_aperture.y(), layer_occlusion_.x(),
                         layer_aperture.bottom())));

  // Right-center.
  patches.push_back(
      Patch(BoundsToRect(image_occlusion.right(), image_aperture_.y(),
                         image_width, image_aperture_.bottom()),
            BoundsToRect(layer_occlusion_.right(), layer_aperture.y(),
                         layer_width, layer_aperture.bottom())));

  // Bottom-left-left.
  patches.push_back(Patch(BoundsToRect(0, image_aperture_.bottom(),
                                       image_occlusion.x(), image_height),
                          BoundsToRect(0, layer_aperture.bottom(),
                                       layer_occlusion_.x(), layer_height)));

  // Bottom-left-right.
  patches.push_back(
      Patch(BoundsToRect(image_occlusion.x(), image_occlusion.bottom(),
                         image_aperture_.x(), image_height),
            BoundsToRect(layer_occlusion_.x(), layer_occlusion_.bottom(),
                         layer_aperture.x(), layer_height)));

  // Bottom-center.
  patches.push_back(
      Patch(BoundsToRect(image_aperture_.x(), image_occlusion.bottom(),
                         image_aperture_.right(), image_height),
            BoundsToRect(layer_aperture.x(), layer_occlusion_.bottom(),
                         layer_aperture.right(), layer_height)));

  // Bottom-right-left.
  patches.push_back(
      Patch(BoundsToRect(image_aperture_.right(), image_occlusion.bottom(),
                         image_occlusion.right(), image_height),
            BoundsToRect(layer_aperture.right(), layer_occlusion_.bottom(),
                         layer_occlusion_.right(), layer_height)));

  // Bottom-right-right.
  patches.push_back(
      Patch(BoundsToRect(image_occlusion.right(), image_aperture_.bottom(),
                         image_width, image_height),
            BoundsToRect(layer_occlusion_.right(), layer_aperture.bottom(),
                         layer_width, layer_height)));

  return patches;
}

void NinePatchLayerImpl::AppendQuads(
    RenderPass* render_pass,
    AppendQuadsData* append_quads_data) {
  CheckGeometryLimitations();
  SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateSharedQuadState(shared_quad_state);

  AppendDebugBorderQuad(render_pass, bounds(), shared_quad_state,
                        append_quads_data);

  if (!ui_resource_id_)
    return;

  ResourceId resource =
      layer_tree_impl()->ResourceIdForUIResource(ui_resource_id_);

  if (!resource)
    return;

  DCHECK(!bounds().IsEmpty());

  std::vector<Patch> patches;

  if (layer_occlusion_.IsEmpty() || fill_center_)
    patches = ComputeQuadsWithoutOcclusion();
  else
    patches = ComputeQuadsWithOcclusion();

  const float vertex_opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
  const bool opaque = layer_tree_impl()->IsUIResourceOpaque(ui_resource_id_);
  static const bool flipped = false;
  static const bool premultiplied_alpha = true;

  for (const auto& patch : patches) {
    gfx::Rect visible_rect =
        draw_properties().occlusion_in_content_space.GetUnoccludedContentRect(
            ToRect(patch.layer_rect));
    gfx::Rect opaque_rect = opaque ? visible_rect : gfx::Rect();
    if (!visible_rect.IsEmpty()) {
      gfx::RectF image_rect(NormalizedRect(
          patch.image_rect, image_bounds_.width(), image_bounds_.height()));
      TextureDrawQuad* quad =
          render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
      quad->SetNew(shared_quad_state, ToRect(patch.layer_rect), opaque_rect,
                   visible_rect, resource, premultiplied_alpha,
                   image_rect.origin(), image_rect.bottom_right(),
                   SK_ColorTRANSPARENT, vertex_opacity, flipped,
                   nearest_neighbor_, false);
      ValidateQuadResources(quad);
    }
  }
}

const char* NinePatchLayerImpl::LayerTypeAsString() const {
  return "cc::NinePatchLayerImpl";
}

std::unique_ptr<base::DictionaryValue> NinePatchLayerImpl::LayerTreeAsJson() {
  std::unique_ptr<base::DictionaryValue> result = LayerImpl::LayerTreeAsJson();

  base::ListValue* list = new base::ListValue;
  list->AppendInteger(image_aperture_.origin().x());
  list->AppendInteger(image_aperture_.origin().y());
  list->AppendInteger(image_aperture_.size().width());
  list->AppendInteger(image_aperture_.size().height());
  result->Set("ImageAperture", list);

  list = new base::ListValue;
  list->AppendInteger(image_bounds_.width());
  list->AppendInteger(image_bounds_.height());
  result->Set("ImageBounds", list);

  result->Set("Border", MathUtil::AsValue(border_).release());

  result->SetBoolean("FillCenter", fill_center_);

  list = new base::ListValue;
  list->AppendInteger(layer_occlusion_.x());
  list->AppendInteger(layer_occlusion_.y());
  list->AppendInteger(layer_occlusion_.width());
  list->AppendInteger(layer_occlusion_.height());
  result->Set("LayerOcclusion", list);

  return result;
}

}  // namespace cc
