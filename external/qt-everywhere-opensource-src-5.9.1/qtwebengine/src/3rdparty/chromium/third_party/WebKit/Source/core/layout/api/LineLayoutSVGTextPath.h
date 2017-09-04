// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutSVGTextPath_h
#define LineLayoutSVGTextPath_h

#include "core/layout/api/LineLayoutSVGInline.h"
#include "core/layout/svg/LayoutSVGTextPath.h"
#include <memory>

namespace blink {

class LineLayoutSVGTextPath : public LineLayoutSVGInline {
 public:
  explicit LineLayoutSVGTextPath(LayoutSVGTextPath* layoutSVGTextPath)
      : LineLayoutSVGInline(layoutSVGTextPath) {}

  explicit LineLayoutSVGTextPath(const LineLayoutItem& item)
      : LineLayoutSVGInline(item) {
    SECURITY_DCHECK(!item || item.isSVGTextPath());
  }

  explicit LineLayoutSVGTextPath(std::nullptr_t)
      : LineLayoutSVGInline(nullptr) {}

  LineLayoutSVGTextPath() {}

  std::unique_ptr<PathPositionMapper> layoutPath() const {
    return toSVGTextPath()->layoutPath();
  }

  float calculateStartOffset(float pathLength) const {
    return toSVGTextPath()->calculateStartOffset(pathLength);
  }

 private:
  LayoutSVGTextPath* toSVGTextPath() {
    return toLayoutSVGTextPath(layoutObject());
  }

  const LayoutSVGTextPath* toSVGTextPath() const {
    return toLayoutSVGTextPath(layoutObject());
  }
};

}  // namespace blink

#endif  // LineLayoutSVGTextPath_h
