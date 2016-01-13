// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/border.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/canvas.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"

namespace views {

namespace {

// A simple border with different thicknesses on each side and single color.
class SidedSolidBorder : public Border {
 public:
  SidedSolidBorder(int top, int left, int bottom, int right, SkColor color);

  // Overridden from Border:
  virtual void Paint(const View& view, gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::Insets GetInsets() const OVERRIDE;
  virtual gfx::Size GetMinimumSize() const OVERRIDE;

 private:
  const SkColor color_;
  const gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(SidedSolidBorder);
};

SidedSolidBorder::SidedSolidBorder(int top,
                                   int left,
                                   int bottom,
                                   int right,
                                   SkColor color)
    : color_(color),
      insets_(top, left, bottom, right) {
}

void SidedSolidBorder::Paint(const View& view, gfx::Canvas* canvas) {
  // Top border.
  canvas->FillRect(gfx::Rect(0, 0, view.width(), insets_.top()), color_);
  // Left border.
  canvas->FillRect(gfx::Rect(0, 0, insets_.left(), view.height()), color_);
  // Bottom border.
  canvas->FillRect(gfx::Rect(0, view.height() - insets_.bottom(), view.width(),
                             insets_.bottom()), color_);
  // Right border.
  canvas->FillRect(gfx::Rect(view.width() - insets_.right(), 0, insets_.right(),
                             view.height()), color_);
}

gfx::Insets SidedSolidBorder::GetInsets() const {
  return insets_;
}

gfx::Size SidedSolidBorder::GetMinimumSize() const {
  return gfx::Size(insets_.width(), insets_.height());
}

// A variation of SidedSolidBorder, where each side has the same thickness.
class SolidBorder : public SidedSolidBorder {
 public:
  SolidBorder(int thickness, SkColor color)
      : SidedSolidBorder(thickness, thickness, thickness, thickness, color) {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SolidBorder);
};

class EmptyBorder : public Border {
 public:
  EmptyBorder(int top, int left, int bottom, int right)
      : insets_(top, left, bottom, right) {}

  // Overridden from Border:
  virtual void Paint(const View& view, gfx::Canvas* canvas) OVERRIDE {}

  virtual gfx::Insets GetInsets() const OVERRIDE {
    return insets_;
  }

  virtual gfx::Size GetMinimumSize() const OVERRIDE {
    return gfx::Size();
  }

 private:
  const gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(EmptyBorder);
};

class BorderPainter : public Border {
 public:
  explicit BorderPainter(Painter* painter, const gfx::Insets& insets)
      : painter_(painter),
        insets_(insets) {
    DCHECK(painter);
  }

  virtual ~BorderPainter() {}

  // Overridden from Border:
  virtual void Paint(const View& view, gfx::Canvas* canvas) OVERRIDE {
    Painter::PaintPainterAt(canvas, painter_.get(), view.GetLocalBounds());
  }

  virtual gfx::Insets GetInsets() const OVERRIDE {
    return insets_;
  }

  virtual gfx::Size GetMinimumSize() const OVERRIDE {
    return painter_->GetMinimumSize();
  }

 private:
  scoped_ptr<Painter> painter_;
  const gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(BorderPainter);
};

}  // namespace

Border::Border() {
}

Border::~Border() {
}

// static
scoped_ptr<Border> Border::NullBorder() {
  return scoped_ptr<Border>();
}

// static
scoped_ptr<Border> Border::CreateSolidBorder(int thickness, SkColor color) {
  return scoped_ptr<Border>(new SolidBorder(thickness, color));
}

// static
scoped_ptr<Border> Border::CreateEmptyBorder(int top,
                                             int left,
                                             int bottom,
                                             int right) {
  return scoped_ptr<Border>(new EmptyBorder(top, left, bottom, right));
}

// static
scoped_ptr<Border> Border::CreateSolidSidedBorder(int top,
                                                  int left,
                                                  int bottom,
                                                  int right,
                                                  SkColor color) {
  return scoped_ptr<Border>(
      new SidedSolidBorder(top, left, bottom, right, color));
}

// static
scoped_ptr<Border> Border::CreateBorderPainter(Painter* painter,
                                               const gfx::Insets& insets) {
  return scoped_ptr<Border>(new BorderPainter(painter, insets));
}

}  // namespace views
