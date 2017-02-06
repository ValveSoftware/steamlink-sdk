// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_IMAGE_GRID_H_
#define UI_WM_CORE_IMAGE_GRID_H_

#include <memory>

#include "base/macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/wm/wm_export.h"

namespace gfx {
class Image;
}  // namespace gfx

namespace wm {

// An ImageGrid is a 3x3 array of ui::Layers, each containing an image.
//
// As the grid is resized, its images fill the requested space:
// - corner images are not scaled
// - top and bottom images are scaled horizontally
// - left and right images are scaled vertically
// - the center image is scaled in both directions
//
// If one of the non-center images is smaller than the largest images in its
// row or column, it will be aligned with the outside of the grid.  For
// example, given 4x4 top-left and top-right images and a 1x2 top images:
//
//   +--------+---------------------+--------+
//   |        |         top         |        |
//   | top-   +---------------------+  top-  +
//   | left   |                     | right  |
//   +----+---+                     +---+----+
//   |    |                             |    |
//   ...
//
// This may seem odd at first, but it lets ImageGrid be used to draw shadows
// with curved corners that extend inwards beyond a window's borders.  In the
// below example, the top-left corner image is overlaid on top of the window's
// top-left corner:
//
//   +---------+-----------------------
//   |    ..xxx|XXXXXXXXXXXXXXXXXX
//   |  .xXXXXX|XXXXXXXXXXXXXXXXXX_____
//   | .xXX    |                    ^ window's top edge
//   | .xXX    |
//   +---------+
//   | xXX|
//   | xXX|< window's left edge
//   | xXX|
//   ...
//
class WM_EXPORT ImageGrid {
 public:
  // Helper class for use by tests.
  class WM_EXPORT TestAPI {
   public:
    explicit TestAPI(ImageGrid* grid) : grid_(grid) {}

    gfx::Rect top_left_clip_rect() const {
      return grid_->top_left_painter_->clip_rect_;
    }
    gfx::Rect top_right_clip_rect() const {
      return grid_->top_right_painter_->clip_rect_;
    }
    gfx::Rect bottom_left_clip_rect() const {
      return grid_->bottom_left_painter_->clip_rect_;
    }
    gfx::Rect bottom_right_clip_rect() const {
      return grid_->bottom_right_painter_->clip_rect_;
    }

    // Returns |layer|'s bounds after applying the layer's current transform.
    gfx::RectF GetTransformedLayerBounds(const ui::Layer& layer);

   private:
    ImageGrid* grid_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestAPI);
  };

  ImageGrid();
  ~ImageGrid();

  ui::Layer* layer() { return layer_.get(); }
  int top_image_height() const { return top_image_height_; }
  int bottom_image_height() const { return bottom_image_height_; }
  int left_image_width() const { return left_image_width_; }
  int right_image_width() const { return right_image_width_; }

  // Visible to allow independent layer animations and for testing.
  ui::Layer* top_left_layer() const { return top_left_layer_.get(); }
  ui::Layer* top_layer() const { return top_layer_.get(); }
  ui::Layer* top_right_layer() const { return top_right_layer_.get(); }
  ui::Layer* left_layer() const { return left_layer_.get(); }
  ui::Layer* center_layer() const { return center_layer_.get(); }
  ui::Layer* right_layer() const { return right_layer_.get(); }
  ui::Layer* bottom_left_layer() const { return bottom_left_layer_.get(); }
  ui::Layer* bottom_layer() const { return bottom_layer_.get(); }
  ui::Layer* bottom_right_layer() const { return bottom_right_layer_.get(); }

  // Sets the grid to display the passed-in images (any of which can be NULL).
  // Ownership of the images remains with the caller.  May be called more than
  // once to switch images.
  void SetImages(const gfx::Image* top_left_image,
                 const gfx::Image* top_image,
                 const gfx::Image* top_right_image,
                 const gfx::Image* left_image,
                 const gfx::Image* center_image,
                 const gfx::Image* right_image,
                 const gfx::Image* bottom_left_image,
                 const gfx::Image* bottom_image,
                 const gfx::Image* bottom_right_image);

  void SetSize(const gfx::Size& size);

  // Sets the grid to a position and size such that the inner edges of the top,
  // bottom, left and right images will be flush with |content_bounds_in_dip|.
  void SetContentBounds(const gfx::Rect& content_bounds_in_dip);

 private:
  // Delegate responsible for painting a specific image on a layer.
  class ImagePainter : public ui::LayerDelegate {
   public:
    explicit ImagePainter(const gfx::ImageSkia& image) : image_(image) {}
    ~ImagePainter() override {}

    // Clips |layer| to |clip_rect|.  Triggers a repaint if the clipping
    // rectangle has changed.  An empty rectangle disables clipping.
    void SetClipRect(const gfx::Rect& clip_rect, ui::Layer* layer);

    // ui::LayerDelegate implementation:
    void OnPaintLayer(const ui::PaintContext& context) override;
    void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override;
    void OnDeviceScaleFactorChanged(float device_scale_factor) override;
    base::Closure PrepareForLayerBoundsChange() override;

   private:
    friend class TestAPI;

    const gfx::ImageSkia image_;

    gfx::Rect clip_rect_;

    DISALLOW_COPY_AND_ASSIGN(ImagePainter);
  };

  enum ImageType {
    HORIZONTAL,
    VERTICAL,
    NONE,
  };

  // Sets |layer_ptr| and |painter_ptr| to display |image| and adds the
  // passed-in layer to |layer_|.  If image is NULL resets |layer_ptr| and
  // |painter_ptr| and removes any existing layer from |layer_|.
  // If |type| is either HORIZONTAL or VERTICAL, it may resize the image to
  // guarantee that it has minimum size in order for scaling work properly
  // with fractional device scale factors. crbug.com/376519.
  void SetImage(const gfx::Image* image,
                std::unique_ptr<ui::Layer>* layer_ptr,
                std::unique_ptr<ImagePainter>* painter_ptr,
                ImageType type);

  // Layer that contains all of the image layers.
  std::unique_ptr<ui::Layer> layer_;

  // The grid's dimensions.
  gfx::Size size_;

  // Heights and widths of the images displayed by |top_layer_|,
  // |bottom_layer_|, |left_layer_|, and |right_layer_|.
  int top_image_height_;
  int bottom_image_height_;
  int left_image_width_;
  int right_image_width_;

  // Heights of the tallest images in the top and bottom rows and the widest
  // images in the left and right columns.  Note that we may have less actual
  // space than this available if the images are large and |size_| is small.
  int base_top_row_height_;
  int base_bottom_row_height_;
  int base_left_column_width_;
  int base_right_column_width_;

  // Layers used to display the various images.  Children of |layer_|.
  // Positions for which no images were supplied are NULL.
  std::unique_ptr<ui::Layer> top_left_layer_;
  std::unique_ptr<ui::Layer> top_layer_;
  std::unique_ptr<ui::Layer> top_right_layer_;
  std::unique_ptr<ui::Layer> left_layer_;
  std::unique_ptr<ui::Layer> center_layer_;
  std::unique_ptr<ui::Layer> right_layer_;
  std::unique_ptr<ui::Layer> bottom_left_layer_;
  std::unique_ptr<ui::Layer> bottom_layer_;
  std::unique_ptr<ui::Layer> bottom_right_layer_;

  // Delegates responsible for painting the above layers.
  // Positions for which no images were supplied are NULL.
  std::unique_ptr<ImagePainter> top_left_painter_;
  std::unique_ptr<ImagePainter> top_painter_;
  std::unique_ptr<ImagePainter> top_right_painter_;
  std::unique_ptr<ImagePainter> left_painter_;
  std::unique_ptr<ImagePainter> center_painter_;
  std::unique_ptr<ImagePainter> right_painter_;
  std::unique_ptr<ImagePainter> bottom_left_painter_;
  std::unique_ptr<ImagePainter> bottom_painter_;
  std::unique_ptr<ImagePainter> bottom_right_painter_;

  DISALLOW_COPY_AND_ASSIGN(ImageGrid);
};

}  // namespace wm

#endif  // UI_WM_CORE_IMAGE_GRID_H_
