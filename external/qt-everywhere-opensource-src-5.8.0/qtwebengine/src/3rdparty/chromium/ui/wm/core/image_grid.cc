// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/image_grid.h"

#include <algorithm>

#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/transform.h"

using std::max;
using std::min;

namespace wm {
namespace {

// Sets the scaling for the transform applied to a layer.  The left, top,
// right and bottom layers are stretched to the height or width of the
// center image.

void ScaleWidth(const gfx::Size& center,
                ui::Layer* layer,
                gfx::Transform* transform) {
  float layer_width = layer->bounds().width() * layer->device_scale_factor();
  float scale = static_cast<float>(center.width()) / layer_width;
  transform->Scale(scale, 1.0);
}

void ScaleHeight(const gfx::Size& center,
                 ui::Layer* layer,
                 gfx::Transform* transform) {
  float layer_height = layer->bounds().height() * layer->device_scale_factor();
  float scale = static_cast<float>(center.height()) / layer_height;
  transform->Scale(1.0, scale);
}

// Returns the dimensions of |image| if non-NULL or gfx::Size(0, 0) otherwise.
gfx::Size GetImageSize(const gfx::Image* image) {
  return image ? gfx::Size(image->ToImageSkia()->width(),
                           image->ToImageSkia()->height())
               : gfx::Size();
}

// Returns true if |layer|'s bounds don't fit within |size|.
bool LayerExceedsSize(const ui::Layer* layer, const gfx::Size& size) {
  return layer->bounds().width() > size.width() ||
         layer->bounds().height() > size.height();
}

}  // namespace

gfx::RectF ImageGrid::TestAPI::GetTransformedLayerBounds(
    const ui::Layer& layer) {
  gfx::RectF bounds = gfx::RectF(layer.bounds());
  layer.transform().TransformRect(&bounds);
  return bounds;
}

ImageGrid::ImageGrid()
    : layer_(new ui::Layer(ui::LAYER_NOT_DRAWN)),
      top_image_height_(0),
      bottom_image_height_(0),
      left_image_width_(0),
      right_image_width_(0),
      base_top_row_height_(0),
      base_bottom_row_height_(0),
      base_left_column_width_(0),
      base_right_column_width_(0) {
}

ImageGrid::~ImageGrid() {
}

void ImageGrid::SetImages(const gfx::Image* top_left_image,
                          const gfx::Image* top_image,
                          const gfx::Image* top_right_image,
                          const gfx::Image* left_image,
                          const gfx::Image* center_image,
                          const gfx::Image* right_image,
                          const gfx::Image* bottom_left_image,
                          const gfx::Image* bottom_image,
                          const gfx::Image* bottom_right_image) {
  SetImage(top_left_image, &top_left_layer_, &top_left_painter_, NONE);
  SetImage(top_image, &top_layer_, &top_painter_, HORIZONTAL);
  SetImage(top_right_image, &top_right_layer_, &top_right_painter_, NONE);
  SetImage(left_image, &left_layer_, &left_painter_, VERTICAL);
  SetImage(center_image, &center_layer_, &center_painter_, NONE);
  SetImage(right_image, &right_layer_, &right_painter_, VERTICAL);
  SetImage(bottom_left_image, &bottom_left_layer_, &bottom_left_painter_, NONE);
  SetImage(bottom_image, &bottom_layer_, &bottom_painter_, HORIZONTAL);
  SetImage(
      bottom_right_image, &bottom_right_layer_, &bottom_right_painter_, NONE);

  top_image_height_ = GetImageSize(top_image).height();
  bottom_image_height_ = GetImageSize(bottom_image).height();
  left_image_width_ = GetImageSize(left_image).width();
  right_image_width_ = GetImageSize(right_image).width();

  base_top_row_height_ = max(GetImageSize(top_left_image).height(),
                             max(GetImageSize(top_image).height(),
                                 GetImageSize(top_right_image).height()));
  base_bottom_row_height_ = max(GetImageSize(bottom_left_image).height(),
                                max(GetImageSize(bottom_image).height(),
                                    GetImageSize(bottom_right_image).height()));
  base_left_column_width_ = max(GetImageSize(top_left_image).width(),
                                max(GetImageSize(left_image).width(),
                                    GetImageSize(bottom_left_image).width()));
  base_right_column_width_ = max(GetImageSize(top_right_image).width(),
                                 max(GetImageSize(right_image).width(),
                                     GetImageSize(bottom_right_image).width()));

  // Invalidate previous |size_| so calls to SetSize() will recompute it.
  size_.SetSize(0, 0);
}

void ImageGrid::SetSize(const gfx::Size& size) {
  if (size_ == size)
    return;

  size_ = size;

  gfx::Rect updated_bounds = layer_->bounds();
  updated_bounds.set_size(size);
  layer_->SetBounds(updated_bounds);

  // Calculate the available amount of space for corner images on all sides of
  // the grid.  If the images don't fit, we need to clip them.
  const int left = min(base_left_column_width_, size_.width() / 2);
  const int right = min(base_right_column_width_, size_.width() - left);
  const int top = min(base_top_row_height_, size_.height() / 2);
  const int bottom = min(base_bottom_row_height_, size_.height() - top);

  // The remaining space goes to the center image.
  int center_width = std::max(size.width() - left - right, 0);
  int center_height = std::max(size.height() - top - bottom, 0);

  // At non-integer scale factors, the ratio of dimensions in DIP is not
  // necessarily the same as the ratio in physical pixels due to rounding.  Set
  // the transform on each of the layers based on dimensions in pixels.
  gfx::Size center_size_in_pixels = gfx::ScaleToFlooredSize(
      gfx::Size(center_width, center_height), layer_->device_scale_factor());

  if (top_layer_.get()) {
    if (center_width > 0) {
      gfx::Transform transform;
      transform.Translate(left, 0);
      ScaleWidth(center_size_in_pixels, top_layer_.get(), &transform);
      top_layer_->SetTransform(transform);
    }
    top_layer_->SetVisible(center_width > 0);
  }
  if (bottom_layer_.get()) {
    if (center_width > 0) {
      gfx::Transform transform;
      transform.Translate(
          left, size.height() - bottom_layer_->bounds().height());
      ScaleWidth(center_size_in_pixels, bottom_layer_.get(), &transform);
      bottom_layer_->SetTransform(transform);
    }
    bottom_layer_->SetVisible(center_width > 0);
  }
  if (left_layer_.get()) {
    if (center_height > 0) {
      gfx::Transform transform;
      transform.Translate(0, top);
      ScaleHeight(center_size_in_pixels, left_layer_.get(), &transform);
      left_layer_->SetTransform(transform);
    }
    left_layer_->SetVisible(center_height > 0);
  }
  if (right_layer_.get()) {
    if (center_height > 0) {
      gfx::Transform transform;
      transform.Translate(
          size.width() - right_layer_->bounds().width(), top);
      ScaleHeight(center_size_in_pixels, right_layer_.get(), &transform);
      right_layer_->SetTransform(transform);
    }
    right_layer_->SetVisible(center_height > 0);
  }

  if (top_left_layer_.get()) {
    // No transformation needed; it should be at (0, 0) and unscaled.
    top_left_painter_->SetClipRect(
        LayerExceedsSize(top_left_layer_.get(), gfx::Size(left, top)) ?
            gfx::Rect(gfx::Rect(0, 0, left, top)) :
            gfx::Rect(),
        top_left_layer_.get());
  }
  if (top_right_layer_.get()) {
    gfx::Transform transform;
    transform.Translate(size.width() - top_right_layer_->bounds().width(), 0.0);
    top_right_layer_->SetTransform(transform);
    top_right_painter_->SetClipRect(
        LayerExceedsSize(top_right_layer_.get(), gfx::Size(right, top)) ?
            gfx::Rect(top_right_layer_->bounds().width() - right, 0,
                      right, top) :
            gfx::Rect(),
        top_right_layer_.get());
  }
  if (bottom_left_layer_.get()) {
    gfx::Transform transform;
    transform.Translate(
        0.0, size.height() - bottom_left_layer_->bounds().height());
    bottom_left_layer_->SetTransform(transform);
    bottom_left_painter_->SetClipRect(
        LayerExceedsSize(bottom_left_layer_.get(), gfx::Size(left, bottom)) ?
            gfx::Rect(0, bottom_left_layer_->bounds().height() - bottom,
                      left, bottom) :
            gfx::Rect(),
        bottom_left_layer_.get());
  }
  if (bottom_right_layer_.get()) {
    gfx::Transform transform;
    transform.Translate(
        size.width() - bottom_right_layer_->bounds().width(),
        size.height() - bottom_right_layer_->bounds().height());
    bottom_right_layer_->SetTransform(transform);
    bottom_right_painter_->SetClipRect(
        LayerExceedsSize(bottom_right_layer_.get(), gfx::Size(right, bottom)) ?
            gfx::Rect(bottom_right_layer_->bounds().width() - right,
                      bottom_right_layer_->bounds().height() - bottom,
                      right, bottom) :
            gfx::Rect(),
        bottom_right_layer_.get());
  }

  if (center_layer_.get()) {
    if (center_width > 0 && center_height > 0) {
      gfx::Transform transform;
      transform.Translate(left, top);
      transform.Scale(center_width / center_layer_->bounds().width(),
                      center_height / center_layer_->bounds().height());
      center_layer_->SetTransform(transform);
    }
    center_layer_->SetVisible(center_width > 0 && center_height > 0);
  }
}

void ImageGrid::SetContentBounds(const gfx::Rect& content_bounds) {

  SetSize(gfx::Size(
      content_bounds.width() + left_image_width_ + right_image_width_,
      content_bounds.height() + top_image_height_ +
      bottom_image_height_));
  layer_->SetBounds(
      gfx::Rect(content_bounds.x() - left_image_width_,
                content_bounds.y() - top_image_height_,
                layer_->bounds().width(),
                layer_->bounds().height()));
}

void ImageGrid::ImagePainter::SetClipRect(const gfx::Rect& clip_rect,
                                          ui::Layer* layer) {
  if (clip_rect != clip_rect_) {
    clip_rect_ = clip_rect;
    layer->SchedulePaint(layer->bounds());
  }
}

void ImageGrid::ImagePainter::OnPaintLayer(const ui::PaintContext& context) {
  gfx::Size bounding_size(image_.width(), image_.height());
  ui::PaintRecorder recorder(context, bounding_size);
  if (!clip_rect_.IsEmpty())
    recorder.canvas()->ClipRect(clip_rect_);
  recorder.canvas()->DrawImageInt(image_, 0, 0);
}

void ImageGrid::ImagePainter::OnDelegatedFrameDamage(
    const gfx::Rect& damage_rect_in_dip) {
}

void ImageGrid::ImagePainter::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  // Redrawing will take care of scale factor change.
}

base::Closure ImageGrid::ImagePainter::PrepareForLayerBoundsChange() {
  return base::Closure();
}

void ImageGrid::SetImage(const gfx::Image* image,
                         std::unique_ptr<ui::Layer>* layer_ptr,
                         std::unique_ptr<ImagePainter>* painter_ptr,
                         ImageType type) {
  // Minimum width (for HORIZONTAL) or height (for VERTICAL) of the
  // |image| so that layers are scaled property if the device scale
  // factor is non integral.
  const int kMinimumSize = 20;

  // Clean out old layers and painters.
  layer_ptr->reset();
  painter_ptr->reset();

  // If we're not using an image, we're done.
  if (!image)
    return;

  gfx::ImageSkia image_skia = image->AsImageSkia();
  switch (type) {
    case HORIZONTAL:
      if (image_skia.width() < kMinimumSize) {
        image_skia = gfx::ImageSkiaOperations::CreateResizedImage(
            image_skia,
            skia::ImageOperations::RESIZE_GOOD,
            gfx::Size(kMinimumSize, image_skia.height()));
      }
      break;
    case VERTICAL:
      if (image_skia.height() < kMinimumSize) {
        image_skia = gfx::ImageSkiaOperations::CreateResizedImage(
            image_skia,
            skia::ImageOperations::RESIZE_GOOD,
            gfx::Size(image_skia.width(), kMinimumSize));
      }
      break;
    case NONE:
      break;
  }

  // Set up the new layer and painter.
  layer_ptr->reset(new ui::Layer(ui::LAYER_TEXTURED));

  const gfx::Size size = image_skia.size();
  layer_ptr->get()->SetBounds(gfx::Rect(0, 0, size.width(), size.height()));

  painter_ptr->reset(new ImagePainter(image_skia));
  layer_ptr->get()->set_delegate(painter_ptr->get());
  layer_ptr->get()->SetFillsBoundsOpaquely(false);
  layer_ptr->get()->SetVisible(true);
  layer_->Add(layer_ptr->get());
}

}  // namespace wm
