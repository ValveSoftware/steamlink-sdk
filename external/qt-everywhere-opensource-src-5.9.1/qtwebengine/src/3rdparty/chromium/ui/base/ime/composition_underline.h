// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_COMPOSITION_UNDERLINE_H_
#define UI_BASE_IME_COMPOSITION_UNDERLINE_H_

#include <stdint.h>

#include <vector>

#include "third_party/skia/include/core/SkColor.h"

namespace ui {

// Intentionally keep sync with blink::WebCompositionUnderline defined in:
// third_party/WebKit/public/web/WebCompositionUnderline.h
struct CompositionUnderline {
  CompositionUnderline()
      : start_offset(0),
        end_offset(0),
        color(SK_ColorTRANSPARENT),
        thick(false),
        background_color(SK_ColorTRANSPARENT) {}

  // TODO(huangs): remove this constructor.
  CompositionUnderline(uint32_t s, uint32_t e, SkColor c, bool t)
      : start_offset(s),
        end_offset(e),
        color(c),
        thick(t),
        background_color(SK_ColorTRANSPARENT) {}

  CompositionUnderline(uint32_t s, uint32_t e, SkColor c, bool t, SkColor bc)
      : start_offset(s),
        end_offset(e),
        color(c),
        thick(t),
        background_color(bc) {}

  bool operator==(const CompositionUnderline& rhs) const {
    return (this->start_offset == rhs.start_offset) &&
           (this->end_offset == rhs.end_offset) && (this->color == rhs.color) &&
           (this->thick == rhs.thick) &&
           (this->background_color == rhs.background_color);
  }

  bool operator!=(const CompositionUnderline& rhs) const {
    return !(*this == rhs);
  }

  uint32_t start_offset;
  uint32_t end_offset;
  SkColor color;
  bool thick;
  SkColor background_color;
};

typedef std::vector<CompositionUnderline> CompositionUnderlines;

}  // namespace ui

#endif  // UI_BASE_IME_COMPOSITION_UNDERLINE_H_
