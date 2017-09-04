// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutListMarker_h
#define LineLayoutListMarker_h

#include "core/layout/LayoutListMarker.h"
#include "core/layout/api/LineLayoutBox.h"

namespace blink {

class LineLayoutListMarker : public LineLayoutBox {
 public:
  explicit LineLayoutListMarker(LayoutListMarker* layoutListMarker)
      : LineLayoutBox(layoutListMarker) {}

  explicit LineLayoutListMarker(const LineLayoutItem& item)
      : LineLayoutBox(item) {
    SECURITY_DCHECK(!item || item.isListMarker());
  }

  explicit LineLayoutListMarker(std::nullptr_t) : LineLayoutBox(nullptr) {}

  LineLayoutListMarker() {}

  bool isInside() const { return toListMarker()->isInside(); }

 private:
  LayoutListMarker* toListMarker() {
    return toLayoutListMarker(layoutObject());
  }

  const LayoutListMarker* toListMarker() const {
    return toLayoutListMarker(layoutObject());
  }
};

}  // namespace blink

#endif  // LineLayoutListMarker_h
