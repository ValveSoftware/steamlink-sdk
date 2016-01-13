// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_SHADOW_BORDER_H_
#define UI_VIEWS_SHADOW_BORDER_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/border.h"
#include "ui/views/views_export.h"

namespace views {

// Creates a css box-shadow like border which fades into SK_ColorTRANSPARENT.
class VIEWS_EXPORT ShadowBorder : public views::Border {
 public:
  ShadowBorder(int blur,
               SkColor color,
               int vertical_offset,
               int horizontal_offset);
  virtual ~ShadowBorder();

 protected:
  // Overridden from views::Border:
  virtual void Paint(const views::View& view, gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::Insets GetInsets() const OVERRIDE;
  virtual gfx::Size GetMinimumSize() const OVERRIDE;

 private:
  // Blur amount of the shadow in pixels. For details on how blur is defined see
  // comments for blur_ in class ShadowValue.
  const int blur_;

  // Shadow color.
  const SkColor color_;

  // Number of pixels to shift shadow to bottom.
  const int vertical_offset_;

  // Number of pixels to shift shadow to right.
  const int horizontal_offset_;

  DISALLOW_COPY_AND_ASSIGN(ShadowBorder);
};

}  // namespace views

#endif  // UI_VIEWS_SHADOW_BORDER_H_
