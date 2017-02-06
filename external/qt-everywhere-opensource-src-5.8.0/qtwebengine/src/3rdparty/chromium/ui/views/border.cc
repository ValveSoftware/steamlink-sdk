// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/border.h"

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"

namespace views {

namespace {

// A simple border with different thicknesses on each side and single color.
class SolidSidedBorder : public Border {
 public:
  SolidSidedBorder(const gfx::Insets& insets, SkColor color);

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  const gfx::Insets insets_;
  const SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(SolidSidedBorder);
};

SolidSidedBorder::SolidSidedBorder(const gfx::Insets& insets, SkColor color)
    : insets_(insets),
      color_(color) {
}

void SolidSidedBorder::Paint(const View& view, gfx::Canvas* canvas) {
  // Top border.
  canvas->FillRect(gfx::Rect(0, 0, view.width(), insets_.top()), color_);
  // Left border.
  canvas->FillRect(gfx::Rect(0, insets_.top(), insets_.left(),
                             view.height() - insets_.height()), color_);
  // Bottom border.
  canvas->FillRect(gfx::Rect(0, view.height() - insets_.bottom(), view.width(),
                             insets_.bottom()), color_);
  // Right border.
  canvas->FillRect(gfx::Rect(view.width() - insets_.right(), insets_.top(),
                             insets_.right(), view.height() - insets_.height()),
                   color_);
}

gfx::Insets SolidSidedBorder::GetInsets() const {
  return insets_;
}

gfx::Size SolidSidedBorder::GetMinimumSize() const {
  return gfx::Size(insets_.width(), insets_.height());
}

// A border with a rounded rectangle and single color.
class RoundedRectBorder : public Border {
 public:
  RoundedRectBorder(int thickness, int corner_radius, SkColor color);

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  const int thickness_;
  const int corner_radius_;
  const SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(RoundedRectBorder);
};

RoundedRectBorder::RoundedRectBorder(int thickness,
                                     int corner_radius,
                                     SkColor color)
    : thickness_(thickness), corner_radius_(corner_radius), color_(color) {}

void RoundedRectBorder::Paint(const View& view, gfx::Canvas* canvas) {
  SkPaint paint;
  paint.setStrokeWidth(thickness_);
  paint.setColor(color_);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setAntiAlias(true);

  float half_thickness = thickness_ / 2.0f;
  gfx::RectF bounds(view.GetLocalBounds());
  bounds.Inset(half_thickness, half_thickness);
  canvas->DrawRoundRect(bounds, corner_radius_, paint);
}

gfx::Insets RoundedRectBorder::GetInsets() const {
  return gfx::Insets(thickness_);
}

gfx::Size RoundedRectBorder::GetMinimumSize() const {
  return gfx::Size(thickness_ * 2, thickness_ * 2);
}

class EmptyBorder : public Border {
 public:
  explicit EmptyBorder(const gfx::Insets& insets);

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override ;
  gfx::Size GetMinimumSize() const override;

 private:
  const gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(EmptyBorder);
};

EmptyBorder::EmptyBorder(const gfx::Insets& insets) : insets_(insets) {
}

void EmptyBorder::Paint(const View& view, gfx::Canvas* canvas) {
}

gfx::Insets EmptyBorder::GetInsets() const {
  return insets_;
}

gfx::Size EmptyBorder::GetMinimumSize() const {
  return gfx::Size();
}

class BorderPainter : public Border {
 public:
  BorderPainter(std::unique_ptr<Painter> painter, const gfx::Insets& insets);

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  std::unique_ptr<Painter> painter_;
  const gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(BorderPainter);
};

BorderPainter::BorderPainter(std::unique_ptr<Painter> painter,
                             const gfx::Insets& insets)
    : painter_(std::move(painter)), insets_(insets) {
  DCHECK(painter_);
}

void BorderPainter::Paint(const View& view, gfx::Canvas* canvas) {
  Painter::PaintPainterAt(canvas, painter_.get(), view.GetLocalBounds());
}

gfx::Insets BorderPainter::GetInsets() const {
  return insets_;
}

gfx::Size BorderPainter::GetMinimumSize() const {
  return painter_->GetMinimumSize();
}

}  // namespace

Border::Border() {
}

Border::~Border() {
}

// static
std::unique_ptr<Border> Border::NullBorder() {
  return nullptr;
}

// static
std::unique_ptr<Border> Border::CreateSolidBorder(int thickness,
                                                  SkColor color) {
  return base::WrapUnique(new SolidSidedBorder(gfx::Insets(thickness), color));
}

// static
std::unique_ptr<Border> Border::CreateEmptyBorder(const gfx::Insets& insets) {
  return base::WrapUnique(new EmptyBorder(insets));
}

// static
std::unique_ptr<Border> Border::CreateRoundedRectBorder(int thickness,
                                                        int corner_radius,
                                                        SkColor color) {
  return base::WrapUnique(
      new RoundedRectBorder(thickness, corner_radius, color));
}

// static
std::unique_ptr<Border> Border::CreateEmptyBorder(int top,
                                                  int left,
                                                  int bottom,
                                                  int right) {
  return CreateEmptyBorder(gfx::Insets(top, left, bottom, right));
}

// static
std::unique_ptr<Border> Border::CreateSolidSidedBorder(int top,
                                                       int left,
                                                       int bottom,
                                                       int right,
                                                       SkColor color) {
  return base::WrapUnique(
      new SolidSidedBorder(gfx::Insets(top, left, bottom, right), color));
}

// static
std::unique_ptr<Border> Border::CreateBorderPainter(
    std::unique_ptr<Painter> painter,
    const gfx::Insets& insets) {
  return base::WrapUnique(new BorderPainter(std::move(painter), insets));
}

}  // namespace views
