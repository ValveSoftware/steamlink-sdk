
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutBlockItem_h
#define LayoutBlockItem_h

#include "core/layout/LayoutBlock.h"
#include "core/layout/api/LayoutBoxItem.h"

namespace blink {

class LayoutBlockItem : public LayoutBoxItem {
public:
    explicit LayoutBlockItem(LayoutBlock* layoutBlock)
        : LayoutBoxItem(layoutBlock)
    {
    }

    explicit LayoutBlockItem(const LayoutBoxItem& item)
        : LayoutBoxItem(item)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!item || item.isLayoutBlock());
    }

    explicit LayoutBlockItem(std::nullptr_t) : LayoutBoxItem(nullptr) { }

    LayoutBlockItem() { }

    void invalidatePaintRectangle(const LayoutRect& layoutRect) const
    {
        toBlock()->invalidatePaintRectangle(layoutRect);
    }

    bool recalcOverflowAfterStyleChange()
    {
        return toBlock()->recalcOverflowAfterStyleChange();
    }

private:
    LayoutBlock* toBlock() { return toLayoutBlock(layoutObject()); }
    const LayoutBlock* toBlock() const { return toLayoutBlock(layoutObject()); }
};

} // namespace blink

#endif // LayoutBlockItem_h
