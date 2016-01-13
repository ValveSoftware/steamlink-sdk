// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_border.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"

namespace views {

namespace internal {

// A helper that combines each border image-set painter with arrows and metrics.
struct BorderImages {
  BorderImages(const int border_image_ids[],
               const int arrow_image_ids[],
               int border_interior_thickness,
               int arrow_interior_thickness,
               int corner_radius);

  scoped_ptr<Painter> border_painter;
  gfx::ImageSkia left_arrow;
  gfx::ImageSkia top_arrow;
  gfx::ImageSkia right_arrow;
  gfx::ImageSkia bottom_arrow;

  // The thickness of border and arrow images and their interior areas.
  // Thickness is the width of left/right and the height of top/bottom images.
  // The interior is measured without including stroke or shadow pixels.
  int border_thickness;
  int border_interior_thickness;
  int arrow_thickness;
  int arrow_interior_thickness;
  // The corner radius of the bubble's rounded-rect interior area.
  int corner_radius;
};

BorderImages::BorderImages(const int border_image_ids[],
                           const int arrow_image_ids[],
                           int border_interior_thickness,
                           int arrow_interior_thickness,
                           int corner_radius)
    : border_painter(Painter::CreateImageGridPainter(border_image_ids)),
      border_thickness(0),
      border_interior_thickness(border_interior_thickness),
      arrow_thickness(0),
      arrow_interior_thickness(arrow_interior_thickness),
      corner_radius(corner_radius) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  border_thickness = rb.GetImageSkiaNamed(border_image_ids[0])->width();
  if (arrow_image_ids[0] != 0) {
    left_arrow = *rb.GetImageSkiaNamed(arrow_image_ids[0]);
    top_arrow = *rb.GetImageSkiaNamed(arrow_image_ids[1]);
    right_arrow = *rb.GetImageSkiaNamed(arrow_image_ids[2]);
    bottom_arrow = *rb.GetImageSkiaNamed(arrow_image_ids[3]);
    arrow_thickness = top_arrow.height();
  }
}

}  // namespace internal

namespace {

// The border and arrow stroke size used in image assets, in pixels.
const int kStroke = 1;

// Bubble border and arrow image resource ids. They don't use the IMAGE_GRID
// macro because there is no center image.
const int kNoShadowImages[] = {
    IDR_BUBBLE_TL, IDR_BUBBLE_T, IDR_BUBBLE_TR,
    IDR_BUBBLE_L,  0,            IDR_BUBBLE_R,
    IDR_BUBBLE_BL, IDR_BUBBLE_B, IDR_BUBBLE_BR };
const int kNoShadowArrows[] = {
    IDR_BUBBLE_L_ARROW, IDR_BUBBLE_T_ARROW,
    IDR_BUBBLE_R_ARROW, IDR_BUBBLE_B_ARROW, };

const int kBigShadowImages[] = {
    IDR_WINDOW_BUBBLE_SHADOW_BIG_TOP_LEFT,
    IDR_WINDOW_BUBBLE_SHADOW_BIG_TOP,
    IDR_WINDOW_BUBBLE_SHADOW_BIG_TOP_RIGHT,
    IDR_WINDOW_BUBBLE_SHADOW_BIG_LEFT,
    0,
    IDR_WINDOW_BUBBLE_SHADOW_BIG_RIGHT,
    IDR_WINDOW_BUBBLE_SHADOW_BIG_BOTTOM_LEFT,
    IDR_WINDOW_BUBBLE_SHADOW_BIG_BOTTOM,
    IDR_WINDOW_BUBBLE_SHADOW_BIG_BOTTOM_RIGHT };
const int kBigShadowArrows[] = {
    IDR_WINDOW_BUBBLE_SHADOW_SPIKE_BIG_LEFT,
    IDR_WINDOW_BUBBLE_SHADOW_SPIKE_BIG_TOP,
    IDR_WINDOW_BUBBLE_SHADOW_SPIKE_BIG_RIGHT,
    IDR_WINDOW_BUBBLE_SHADOW_SPIKE_BIG_BOTTOM };

const int kSmallShadowImages[] = {
    IDR_WINDOW_BUBBLE_SHADOW_SMALL_TOP_LEFT,
    IDR_WINDOW_BUBBLE_SHADOW_SMALL_TOP,
    IDR_WINDOW_BUBBLE_SHADOW_SMALL_TOP_RIGHT,
    IDR_WINDOW_BUBBLE_SHADOW_SMALL_LEFT,
    0,
    IDR_WINDOW_BUBBLE_SHADOW_SMALL_RIGHT,
    IDR_WINDOW_BUBBLE_SHADOW_SMALL_BOTTOM_LEFT,
    IDR_WINDOW_BUBBLE_SHADOW_SMALL_BOTTOM,
    IDR_WINDOW_BUBBLE_SHADOW_SMALL_BOTTOM_RIGHT };
const int kSmallShadowArrows[] = {
    IDR_WINDOW_BUBBLE_SHADOW_SPIKE_SMALL_LEFT,
    IDR_WINDOW_BUBBLE_SHADOW_SPIKE_SMALL_TOP,
    IDR_WINDOW_BUBBLE_SHADOW_SPIKE_SMALL_RIGHT,
    IDR_WINDOW_BUBBLE_SHADOW_SPIKE_SMALL_BOTTOM };

using internal::BorderImages;

// Returns the cached BorderImages for the given |shadow| type.
BorderImages* GetBorderImages(BubbleBorder::Shadow shadow) {
  // Keep a cache of bubble border image-set painters, arrows, and metrics.
  static BorderImages* kBorderImages[BubbleBorder::SHADOW_COUNT] = { NULL };

  CHECK_LT(shadow, BubbleBorder::SHADOW_COUNT);
  struct BorderImages*& set = kBorderImages[shadow];
  if (set)
    return set;

  switch (shadow) {
    case BubbleBorder::NO_SHADOW:
    case BubbleBorder::NO_SHADOW_OPAQUE_BORDER:
      set = new BorderImages(kNoShadowImages, kNoShadowArrows, 6, 7, 4);
      break;
    case BubbleBorder::BIG_SHADOW:
      set = new BorderImages(kBigShadowImages, kBigShadowArrows, 23, 9, 2);
      break;
    case BubbleBorder::SMALL_SHADOW:
      set = new BorderImages(kSmallShadowImages, kSmallShadowArrows, 5, 6, 2);
      break;
    case BubbleBorder::SHADOW_COUNT:
      NOTREACHED();
      break;
  }

  return set;
}

}  // namespace

BubbleBorder::BubbleBorder(Arrow arrow, Shadow shadow, SkColor color)
    : arrow_(arrow),
      arrow_offset_(0),
      arrow_paint_type_(PAINT_NORMAL),
      alignment_(ALIGN_ARROW_TO_MID_ANCHOR),
      shadow_(shadow),
      background_color_(color),
      use_theme_background_color_(false) {
  DCHECK(shadow < SHADOW_COUNT);
  images_ = GetBorderImages(shadow);
}

BubbleBorder::~BubbleBorder() {}

gfx::Rect BubbleBorder::GetBounds(const gfx::Rect& anchor_rect,
                                  const gfx::Size& contents_size) const {
  int x = anchor_rect.x();
  int y = anchor_rect.y();
  int w = anchor_rect.width();
  int h = anchor_rect.height();
  const gfx::Size size(GetSizeForContentsSize(contents_size));
  const int arrow_offset = GetArrowOffset(size);
  const int arrow_size =
      images_->arrow_interior_thickness + kStroke - images_->arrow_thickness;
  const bool mid_anchor = alignment_ == ALIGN_ARROW_TO_MID_ANCHOR;

  // Calculate the bubble coordinates based on the border and arrow settings.
  if (is_arrow_on_horizontal(arrow_)) {
    if (is_arrow_on_left(arrow_)) {
      x += mid_anchor ? w / 2 - arrow_offset : kStroke - GetBorderThickness();
    } else if (is_arrow_at_center(arrow_)) {
      x += w / 2 - arrow_offset;
    } else {
      x += mid_anchor ? w / 2 + arrow_offset - size.width() :
                        w - size.width() + GetBorderThickness() - kStroke;
    }
    y += is_arrow_on_top(arrow_) ? h + arrow_size : -arrow_size - size.height();
  } else if (has_arrow(arrow_)) {
    x += is_arrow_on_left(arrow_) ? w + arrow_size : -arrow_size - size.width();
    if (is_arrow_on_top(arrow_)) {
      y += mid_anchor ? h / 2 - arrow_offset : kStroke - GetBorderThickness();
    } else if (is_arrow_at_center(arrow_)) {
      y += h / 2 - arrow_offset;
    } else {
      y += mid_anchor ? h / 2 + arrow_offset - size.height() :
                        h - size.height() + GetBorderThickness() - kStroke;
    }
  } else {
    x += (w - size.width()) / 2;
    y += (arrow_ == NONE) ? h : (h - size.height()) / 2;
  }

  return gfx::Rect(x, y, size.width(), size.height());
}

int BubbleBorder::GetBorderThickness() const {
  return images_->border_thickness - images_->border_interior_thickness;
}

int BubbleBorder::GetBorderCornerRadius() const {
  return images_->corner_radius;
}

int BubbleBorder::GetArrowOffset(const gfx::Size& border_size) const {
  const int edge_length = is_arrow_on_horizontal(arrow_) ?
      border_size.width() : border_size.height();
  if (is_arrow_at_center(arrow_) && arrow_offset_ == 0)
    return edge_length / 2;

  // Calculate the minimum offset to not overlap arrow and corner images.
  const int min = images_->border_thickness + (images_->top_arrow.width() / 2);
  // Ensure the returned value will not cause image overlap, if possible.
  return std::max(min, std::min(arrow_offset_, edge_length - min));
}

void BubbleBorder::Paint(const views::View& view, gfx::Canvas* canvas) {
  gfx::Rect bounds(view.GetContentsBounds());
  bounds.Inset(-GetBorderThickness(), -GetBorderThickness());
  const gfx::Rect arrow_bounds = GetArrowRect(view.GetLocalBounds());
  if (arrow_bounds.IsEmpty()) {
    Painter::PaintPainterAt(canvas, images_->border_painter.get(), bounds);
    return;
  }

  // Clip the arrow bounds out to avoid painting the overlapping edge area.
  canvas->Save();
  SkRect arrow_rect(gfx::RectToSkRect(arrow_bounds));
  canvas->sk_canvas()->clipRect(arrow_rect, SkRegion::kDifference_Op);
  Painter::PaintPainterAt(canvas, images_->border_painter.get(), bounds);
  canvas->Restore();

  DrawArrow(canvas, arrow_bounds);
}

gfx::Insets BubbleBorder::GetInsets() const {
  // The insets contain the stroke and shadow pixels outside the bubble fill.
  const int inset = GetBorderThickness();
  if ((arrow_paint_type_ == PAINT_NONE) || !has_arrow(arrow_))
    return gfx::Insets(inset, inset, inset, inset);

  int first_inset = inset;
  int second_inset = std::max(inset, images_->arrow_thickness);
  if (is_arrow_on_horizontal(arrow_) ?
      is_arrow_on_top(arrow_) : is_arrow_on_left(arrow_))
    std::swap(first_inset, second_inset);
  return is_arrow_on_horizontal(arrow_) ?
      gfx::Insets(first_inset, inset, second_inset, inset) :
      gfx::Insets(inset, first_inset, inset, second_inset);
}

gfx::Size BubbleBorder::GetMinimumSize() const {
  return GetSizeForContentsSize(gfx::Size());
}

gfx::Size BubbleBorder::GetSizeForContentsSize(
    const gfx::Size& contents_size) const {
  // Enlarge the contents size by the thickness of the border images.
  gfx::Size size(contents_size);
  const gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());

  // Ensure the bubble is large enough to not overlap border and arrow images.
  const int min = 2 * images_->border_thickness;
  const int min_with_arrow_width = min + images_->top_arrow.width();
  const int min_with_arrow_thickness = images_->border_thickness +
      std::max(images_->arrow_thickness + images_->border_interior_thickness,
               images_->border_thickness);
  // Only take arrow image sizes into account when the bubble tip is shown.
  if (arrow_paint_type_ == PAINT_TRANSPARENT || !has_arrow(arrow_))
    size.SetToMax(gfx::Size(min, min));
  else if (is_arrow_on_horizontal(arrow_))
    size.SetToMax(gfx::Size(min_with_arrow_width, min_with_arrow_thickness));
  else
    size.SetToMax(gfx::Size(min_with_arrow_thickness, min_with_arrow_width));
  return size;
}

gfx::ImageSkia* BubbleBorder::GetArrowImage() const {
  if (!has_arrow(arrow_))
    return NULL;
  if (is_arrow_on_horizontal(arrow_)) {
    return is_arrow_on_top(arrow_) ?
        &images_->top_arrow : &images_->bottom_arrow;
  }
  return is_arrow_on_left(arrow_) ?
      &images_->left_arrow : &images_->right_arrow;
}

gfx::Rect BubbleBorder::GetArrowRect(const gfx::Rect& bounds) const {
  if (!has_arrow(arrow_) || arrow_paint_type_ != PAINT_NORMAL)
    return gfx::Rect();

  gfx::Point origin;
  int offset = GetArrowOffset(bounds.size());
  const int half_length = images_->top_arrow.width() / 2;
  const gfx::Insets insets = GetInsets();

  if (is_arrow_on_horizontal(arrow_)) {
    origin.set_x(is_arrow_on_left(arrow_) || is_arrow_at_center(arrow_) ?
        offset : bounds.width() - offset);
    origin.Offset(-half_length, 0);
    if (is_arrow_on_top(arrow_))
      origin.set_y(insets.top() - images_->arrow_thickness);
    else
      origin.set_y(bounds.height() - insets.bottom());
  } else {
    origin.set_y(is_arrow_on_top(arrow_)  || is_arrow_at_center(arrow_) ?
        offset : bounds.height() - offset);
    origin.Offset(0, -half_length);
    if (is_arrow_on_left(arrow_))
      origin.set_x(insets.left() - images_->arrow_thickness);
    else
      origin.set_x(bounds.width() - insets.right());
  }
  return gfx::Rect(origin, GetArrowImage()->size());
}

void BubbleBorder::DrawArrow(gfx::Canvas* canvas,
                             const gfx::Rect& arrow_bounds) const {
  canvas->DrawImageInt(*GetArrowImage(), arrow_bounds.x(), arrow_bounds.y());
  const bool horizontal = is_arrow_on_horizontal(arrow_);
  const int thickness = images_->arrow_interior_thickness;
  float tip_x = horizontal ? arrow_bounds.CenterPoint().x() :
      is_arrow_on_left(arrow_) ? arrow_bounds.right() - thickness :
                                 arrow_bounds.x() + thickness;
  float tip_y = !horizontal ? arrow_bounds.CenterPoint().y() + 0.5f :
      is_arrow_on_top(arrow_) ? arrow_bounds.bottom() - thickness :
                                arrow_bounds.y() + thickness;
  const bool positive_offset = horizontal ?
      is_arrow_on_top(arrow_) : is_arrow_on_left(arrow_);
  const int offset_to_next_vertex = positive_offset ?
      images_->arrow_interior_thickness : -images_->arrow_interior_thickness;

  SkPath path;
  path.incReserve(4);
  path.moveTo(SkDoubleToScalar(tip_x), SkDoubleToScalar(tip_y));
  path.lineTo(SkDoubleToScalar(tip_x + offset_to_next_vertex),
              SkDoubleToScalar(tip_y + offset_to_next_vertex));
  const int multiplier = horizontal ? 1 : -1;
  path.lineTo(SkDoubleToScalar(tip_x - multiplier * offset_to_next_vertex),
              SkDoubleToScalar(tip_y + multiplier * offset_to_next_vertex));
  path.close();

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(background_color_);

  canvas->DrawPath(path, paint);
}

void BubbleBackground::Paint(gfx::Canvas* canvas, views::View* view) const {
  if (border_->shadow() == BubbleBorder::NO_SHADOW_OPAQUE_BORDER)
    canvas->DrawColor(border_->background_color());

  // Fill the contents with a round-rect region to match the border images.
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(border_->background_color());
  SkPath path;
  gfx::Rect bounds(view->GetLocalBounds());
  bounds.Inset(border_->GetInsets());

  SkScalar radius = SkIntToScalar(border_->GetBorderCornerRadius());
  path.addRoundRect(gfx::RectToSkRect(bounds), radius, radius);
  canvas->DrawPath(path, paint);
}

}  // namespace views
