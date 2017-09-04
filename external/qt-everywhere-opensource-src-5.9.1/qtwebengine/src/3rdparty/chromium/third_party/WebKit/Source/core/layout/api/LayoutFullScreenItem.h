
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutFullScreenItem_h
#define LayoutFullScreenItem_h

#include "core/layout/LayoutBlock.h"
#include "core/layout/api/LayoutBlockItem.h"

namespace blink {

class LayoutFullScreenItem : public LayoutBlockItem {
 public:
  explicit LayoutFullScreenItem(LayoutBlock* layoutBlock)
      : LayoutBlockItem(layoutBlock) {}

  explicit LayoutFullScreenItem(const LayoutBlockItem& item)
      : LayoutBlockItem(item) {
    SECURITY_DCHECK(!item || item.isLayoutFullScreen());
  }

  explicit LayoutFullScreenItem(std::nullptr_t) : LayoutBlockItem(nullptr) {}

  LayoutFullScreenItem() {}

  void unwrapLayoutObject() { return toFullScreen()->unwrapLayoutObject(); }

 private:
  LayoutFullScreen* toFullScreen() {
    return toLayoutFullScreen(layoutObject());
  }
  const LayoutFullScreen* toFullScreen() const {
    return toLayoutFullScreen(layoutObject());
  }
};

}  // namespace blink

#endif  // LayoutFullScreenItem_h
