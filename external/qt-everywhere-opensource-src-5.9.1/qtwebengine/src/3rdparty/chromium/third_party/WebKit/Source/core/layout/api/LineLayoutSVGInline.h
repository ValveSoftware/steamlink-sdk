// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutSVGInline_h
#define LineLayoutSVGInline_h

#include "core/layout/api/LineLayoutInline.h"
#include "core/layout/svg/LayoutSVGInline.h"

namespace blink {

class LineLayoutSVGInline : public LineLayoutInline {
 public:
  explicit LineLayoutSVGInline(LayoutSVGInline* layoutSVGInline)
      : LineLayoutInline(layoutSVGInline) {}

  explicit LineLayoutSVGInline(const LineLayoutItem& item)
      : LineLayoutInline(item) {
    SECURITY_DCHECK(!item || item.isSVGInline());
  }

  explicit LineLayoutSVGInline(std::nullptr_t) : LineLayoutInline(nullptr) {}

  LineLayoutSVGInline() {}

 private:
  LayoutSVGInline* toSVGInline() { return toLayoutSVGInline(layoutObject()); }

  const LayoutSVGInline* toSVGInline() const {
    return toLayoutSVGInline(layoutObject());
  }
};

}  // namespace blink

#endif  // LineLayoutSVGInline_h
