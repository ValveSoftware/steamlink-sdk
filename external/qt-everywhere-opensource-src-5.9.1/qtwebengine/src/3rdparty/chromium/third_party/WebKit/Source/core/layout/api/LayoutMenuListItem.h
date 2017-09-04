
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutMenuListItem_h
#define LayoutMenuListItem_h

#include "core/layout/LayoutMenuList.h"
#include "core/layout/api/LayoutBlockItem.h"

namespace blink {

class LayoutMenuListItem : public LayoutBlockItem {
 public:
  explicit LayoutMenuListItem(LayoutBlock* layoutBlock)
      : LayoutBlockItem(layoutBlock) {}

  explicit LayoutMenuListItem(const LayoutBlockItem& item)
      : LayoutBlockItem(item) {
    SECURITY_DCHECK(!item || item.isMenuList());
  }

  explicit LayoutMenuListItem(std::nullptr_t) : LayoutBlockItem(nullptr) {}

  LayoutMenuListItem() {}

  String text() const { return toMenuList()->text(); }

 private:
  LayoutMenuList* toMenuList() { return toLayoutMenuList(layoutObject()); }
  const LayoutMenuList* toMenuList() const {
    return toLayoutMenuList(layoutObject());
  }
};

}  // namespace blink

#endif  // LayoutMenuListItem_h
