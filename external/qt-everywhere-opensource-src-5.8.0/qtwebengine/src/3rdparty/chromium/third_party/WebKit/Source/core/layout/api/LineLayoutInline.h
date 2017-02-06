// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutInline_h
#define LineLayoutInline_h

#include "core/layout/LayoutInline.h"
#include "core/layout/api/LineLayoutBoxModel.h"
#include "platform/LayoutUnit.h"

namespace blink {

class ComputedStyle;
class LayoutInline;
class LayoutObject;

class LineLayoutInline : public LineLayoutBoxModel {
public:
    explicit LineLayoutInline(LayoutInline* layoutInline)
        : LineLayoutBoxModel(layoutInline)
    {
    }

    explicit LineLayoutInline(const LineLayoutItem& item)
        : LineLayoutBoxModel(item)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!item || item.isLayoutInline());
    }

    explicit LineLayoutInline(std::nullptr_t) : LineLayoutBoxModel(nullptr) { }

    LineLayoutInline() { }

    LineLayoutItem firstChild() const
    {
        return LineLayoutItem(toInline()->firstChild());
    }

    LineLayoutItem lastChild() const
    {
        return LineLayoutItem(toInline()->lastChild());
    }

    LayoutUnit marginStart() const
    {
        return toInline()->marginStart();
    }

    LayoutUnit marginEnd() const
    {
        return toInline()->marginEnd();
    }

    int borderStart() const
    {
        return toInline()->borderStart();
    }

    int borderEnd() const
    {
        return toInline()->borderEnd();
    }

    LayoutUnit paddingStart() const
    {
        return toInline()->paddingStart();
    }

    LayoutUnit paddingEnd() const
    {
        return toInline()->paddingEnd();
    }

    bool hasInlineDirectionBordersPaddingOrMargin() const
    {
        return toInline()->hasInlineDirectionBordersPaddingOrMargin();
    }

    bool alwaysCreateLineBoxes() const
    {
        return toInline()->alwaysCreateLineBoxes();
    }

    InlineBox* firstLineBoxIncludingCulling() const
    {
        return toInline()->firstLineBoxIncludingCulling();
    }

    InlineBox* lastLineBoxIncludingCulling() const
    {
        return toInline()->lastLineBoxIncludingCulling();
    }

    LineBoxList* lineBoxes()
    {
        return toInline()->lineBoxes();
    }

    bool hitTestCulledInline(HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset)
    {
        return toInline()->hitTestCulledInline(result, locationInContainer, accumulatedOffset);
    }

    LayoutBoxModelObject* continuation() const
    {
        return toInline()->continuation();
    }

    InlineBox* createAndAppendInlineFlowBox()
    {
        return toInline()->createAndAppendInlineFlowBox();
    }

    InlineFlowBox* lastLineBox()
    {
        return toInline()->lastLineBox();
    }

protected:
    LayoutInline* toInline()
    {
        return toLayoutInline(layoutObject());
    }

    const LayoutInline* toInline() const
    {
        return toLayoutInline(layoutObject());
    }

};

} // namespace blink

#endif // LineLayoutInline_h
