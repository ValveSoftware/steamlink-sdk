// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutLIItem_h
#define LayoutLIItem_h

#include "core/layout/LayoutListItem.h"
#include "core/layout/api/LayoutBoxItem.h"

namespace blink {

class LayoutLIItem : public LayoutBoxItem {
 public:
  explicit LayoutLIItem(LayoutListItem* layoutListItem)
      : LayoutBoxItem(layoutListItem) {}

  explicit LayoutLIItem(const LayoutItem& item) : LayoutBoxItem(item) {
    SECURITY_DCHECK(!item || item.isListItem());
  }

  explicit LayoutLIItem(std::nullptr_t) : LayoutBoxItem(nullptr) {}

  LayoutLIItem() {}

  void setNotInList(bool notInList) {
    return toListItem()->setNotInList(notInList);
  }

 private:
  LayoutListItem* toListItem() { return toLayoutListItem(layoutObject()); }

  const LayoutListItem* toListItem() const {
    return toLayoutListItem(layoutObject());
  }
};

}  // namespace blink

#endif  // LayoutLIItem_h
