// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutBoxItem_h
#define LayoutBoxItem_h

#include "core/layout/LayoutBox.h"
#include "core/layout/api/LayoutBoxModel.h"
#include "platform/scroll/ScrollTypes.h"

namespace blink {

class LayoutPoint;
class LayoutSize;
class LayoutUnit;

class LayoutBoxItem : public LayoutBoxModel {
public:
    explicit LayoutBoxItem(LayoutBox* layoutBox)
        : LayoutBoxModel(layoutBox)
    {
    }

    explicit LayoutBoxItem(const LayoutItem& item)
        : LayoutBoxModel(item)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!item || item.isBox());
    }

    explicit LayoutBoxItem(std::nullptr_t) : LayoutBoxModel(nullptr) { }

    LayoutBoxItem() { }

    LayoutBoxItem enclosingBox() const
    {
        return LayoutBoxItem(toBox()->enclosingBox());
    }

    ScrollResult scroll(ScrollGranularity granularity, const FloatSize& delta)
    {
        return toBox()->scroll(granularity, delta);
    }

    LayoutSize size() const
    {
        return toBox()->size();
    }

    LayoutPoint location() const
    {
        return toBox()->location();
    }

    LayoutUnit logicalWidth() const
    {
        return toBox()->logicalWidth();
    }

    LayoutUnit logicalHeight() const
    {
        return toBox()->logicalHeight();
    }

    LayoutUnit minPreferredLogicalWidth() const
    {
        return toBox()->minPreferredLogicalWidth();
    }

    LayoutRect overflowClipRect(const LayoutPoint& location, OverlayScrollbarClipBehavior behavior = IgnoreOverlayScrollbarSize) const
    {
        return toBox()->overflowClipRect(location, behavior);
    }

private:
    LayoutBox* toBox()
    {
        return toLayoutBox(layoutObject());
    }

    const LayoutBox* toBox() const
    {
        return toLayoutBox(layoutObject());
    }
};

} // namespace blink

#endif // LayoutBoxItem_h
