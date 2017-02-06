// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutPartItem_h
#define LayoutPartItem_h

#include "core/layout/LayoutPart.h"
#include "core/layout/api/LayoutBoxItem.h"

namespace blink {

class LayoutPartItem : public LayoutBoxItem {
public:
    explicit LayoutPartItem(LayoutPart* layoutPart)
        : LayoutBoxItem(layoutPart)
    {
    }

    explicit LayoutPartItem(const LayoutItem& item)
        : LayoutBoxItem(item)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!item || item.isLayoutPart());
    }

    explicit LayoutPartItem(std::nullptr_t) : LayoutBoxItem(nullptr) { }

    LayoutPartItem() { }

    void updateOnWidgetChange()
    {
        toPart()->updateOnWidgetChange();
    }

private:
    LayoutPart* toPart()
    {
        return toLayoutPart(layoutObject());
    }

    const LayoutPart* toPart() const
    {
        return toLayoutPart(layoutObject());
    }
};

} // namespace blink

#endif // LayoutPartItem_h
