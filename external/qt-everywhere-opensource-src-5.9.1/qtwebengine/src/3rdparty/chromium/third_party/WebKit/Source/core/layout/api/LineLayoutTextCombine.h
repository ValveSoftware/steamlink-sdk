// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutTextCombine_h
#define LineLayoutTextCombine_h

#include "core/layout/LayoutTextCombine.h"
#include "core/layout/api/LineLayoutText.h"

namespace blink {

class LineLayoutTextCombine : public LineLayoutText {
 public:
  explicit LineLayoutTextCombine(LayoutTextCombine* layoutTextCombine)
      : LineLayoutText(layoutTextCombine) {}

  explicit LineLayoutTextCombine(const LineLayoutItem& item)
      : LineLayoutText(item) {
    SECURITY_DCHECK(!item || item.isCombineText());
  }

  explicit LineLayoutTextCombine(std::nullptr_t) : LineLayoutText(nullptr) {}

  LineLayoutTextCombine() {}

  bool isCombined() const { return toTextCombine()->isCombined(); }

 private:
  LayoutTextCombine* toTextCombine() {
    return toLayoutTextCombine(layoutObject());
  }

  const LayoutTextCombine* toTextCombine() const {
    return toLayoutTextCombine(layoutObject());
  }
};

}  // namespace blink

#endif  // LineLayoutTextCombine_h
