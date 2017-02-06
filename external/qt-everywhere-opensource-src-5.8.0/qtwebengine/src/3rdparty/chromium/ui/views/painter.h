// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_PAINTER_H_
#define UI_VIEWS_PAINTER_H_

#include <stddef.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/nine_image_painter_factory.h"
#include "ui/views/views_export.h"

namespace gfx {
class Canvas;
class ImageSkia;
class Insets;
class Rect;
class Size;
}

namespace views {

class View;

// Painter, as the name implies, is responsible for painting in a particular
// region. Think of Painter as a Border or Background that can be painted
// in any region of a View.
class VIEWS_EXPORT Painter {
 public:
  Painter();
  virtual ~Painter();

  // A convenience method for painting a Painter in a particular region.
  // This translates the canvas to x/y and paints the painter.
  static void PaintPainterAt(gfx::Canvas* canvas,
                             Painter* painter,
                             const gfx::Rect& rect);

  // Convenience that paints |focus_painter| only if |view| HasFocus() and
  // |focus_painter| is non-NULL.
  static void PaintFocusPainter(View* view,
                                gfx::Canvas* canvas,
                                Painter* focus_painter);

  // Creates a painter that draws a RoundRect with a solid color and given
  // corner radius.
  static Painter* CreateSolidRoundRectPainter(SkColor color, float radius);

  // Creates a painter that draws a RoundRect with a solid color and a given
  // corner radius, and also adds a 1px border (inset) in the given color.
  static Painter* CreateRoundRectWith1PxBorderPainter(SkColor bg_color,
                                                      SkColor stroke_color,
                                                      float radius);

  // Creates a painter that draws a gradient between the two colors.
  static Painter* CreateHorizontalGradient(SkColor c1, SkColor c2);
  static Painter* CreateVerticalGradient(SkColor c1, SkColor c2);

  // Creates a painter that draws a multi-color gradient. |colors| contains the
  // gradient colors and |pos| the relative positions of the colors. The first
  // element in |pos| must be 0.0 and the last element 1.0. |count| contains
  // the number of elements in |colors| and |pos|.
  static Painter* CreateVerticalMultiColorGradient(SkColor* colors,
                                                   SkScalar* pos,
                                                   size_t count);

  // Creates a painter that divides |image| into nine regions. The four corners
  // are rendered at the size specified in insets (eg. the upper-left corner is
  // rendered at 0 x 0 with a size of insets.left() x insets.top()). The center
  // and edge images are stretched to fill the painted area.
  static Painter* CreateImagePainter(const gfx::ImageSkia& image,
                                     const gfx::Insets& insets);

  // Creates a painter that paints images in a scalable grid. The images must
  // share widths by column and heights by row. The corners are painted at full
  // size, while center and edge images are stretched to fill the painted area.
  // The center image may be zero (to be skipped). This ordering must be used:
  // Top-Left/Top/Top-Right/Left/[Center]/Right/Bottom-Left/Bottom/Bottom-Right.
  static Painter* CreateImageGridPainter(const int image_ids[]);

  // Factory methods for creating painters intended for rendering focus.
  static std::unique_ptr<Painter> CreateDashedFocusPainter();
  static std::unique_ptr<Painter> CreateDashedFocusPainterWithInsets(
      const gfx::Insets& insets);
  static std::unique_ptr<Painter> CreateSolidFocusPainter(
      SkColor color,
      const gfx::Insets& insets);

  // Returns the minimum size this painter can paint without obvious graphical
  // problems (e.g. overlapping images).
  virtual gfx::Size GetMinimumSize() const = 0;

  // Paints the painter in the specified region.
  virtual void Paint(gfx::Canvas* canvas, const gfx::Size& size) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Painter);
};

// HorizontalPainter paints 3 images into a box: left, center and right. The
// left and right images are drawn to size at the left/right edges of the
// region. The center is tiled in the remaining space. All images must have the
// same height.
class VIEWS_EXPORT HorizontalPainter : public Painter {
 public:
  // Constructs a new HorizontalPainter loading the specified image names.
  // The images must be in the order left, right and center.
  explicit HorizontalPainter(const int image_resource_names[]);
  ~HorizontalPainter() override;

  // Painter:
  gfx::Size GetMinimumSize() const override;
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override;

 private:
  // The image chunks.
  enum BorderElements {
    LEFT,
    CENTER,
    RIGHT
  };

  // NOTE: the images are owned by ResourceBundle. Don't free them.
  const gfx::ImageSkia* images_[3];

  DISALLOW_COPY_AND_ASSIGN(HorizontalPainter);
};

}  // namespace views

#endif  // UI_VIEWS_PAINTER_H_
