// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutBlockFlow_h
#define LineLayoutBlockFlow_h

#include "core/layout/FloatingObjects.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/api/LineLayoutBox.h"
#include "platform/LayoutUnit.h"

namespace blink {

class LayoutBlockFlow;
class FloatingObject;
class LineInfo;
class LineWidth;

class LineLayoutBlockFlow : public LineLayoutBox {
public:
    explicit LineLayoutBlockFlow(LayoutBlockFlow* blockFlow)
        : LineLayoutBox(blockFlow)
    {
    }

    explicit LineLayoutBlockFlow(const LineLayoutItem& item)
        : LineLayoutBox(item)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!item || item.isLayoutBlockFlow());
    }

    explicit LineLayoutBlockFlow(std::nullptr_t) : LineLayoutBox(nullptr) { }

    LineLayoutBlockFlow() { }

    LineLayoutItem firstChild() const
    {
        return LineLayoutItem(toBlockFlow()->firstChild());
    }
    LineLayoutItem lastChild() const
    {
        return LineLayoutItem(toBlockFlow()->lastChild());
    }

    LayoutUnit startAlignedOffsetForLine(LayoutUnit position, IndentTextOrNot indentText)
    {
        return toBlockFlow()->startAlignedOffsetForLine(position, indentText);
    }

    LayoutUnit textIndentOffset() const
    {
        return toBlockFlow()->textIndentOffset();
    }

    // TODO(dgrogan/eae): *ForChild methods: callers should call
    // child.logicalWidth etc, and the API should access the parent BlockFlow.
    LayoutUnit logicalWidthForChild(const LayoutBox& child) const
    {
        return toBlockFlow()->logicalWidthForChild(child);
    }

    LayoutUnit logicalWidthForChild(LineLayoutBox child) const
    {
        return toBlockFlow()->logicalWidthForChild(*toLayoutBox(child.layoutObject()));
    }

    LayoutUnit marginStartForChild(const LayoutBoxModelObject& child) const
    {
        return toBlockFlow()->marginStartForChild(child);
    }

    LayoutUnit marginStartForChild(LineLayoutBox child) const
    {
        return toBlockFlow()->marginStartForChild(*toLayoutBoxModelObject(child.layoutObject()));
    }

    LayoutUnit marginEndForChild(const LayoutBoxModelObject& child) const
    {
        return toBlockFlow()->marginEndForChild(child);
    }

    LayoutUnit marginEndForChild(LineLayoutBox child) const
    {
        return toBlockFlow()->marginEndForChild(*toLayoutBoxModelObject(child.layoutObject()));
    }

    LayoutUnit marginBeforeForChild(const LayoutBoxModelObject& child) const
    {
        return toBlockFlow()->marginBeforeForChild(child);
    }

    LayoutUnit startOffsetForContent() const
    {
        return toBlockFlow()->startOffsetForContent();
    }

    LayoutUnit lineHeight(bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
    {
        return toBlockFlow()->lineHeight(firstLine, direction, linePositionMode);
    }

    LayoutUnit minLineHeightForReplacedObject(bool isFirstLine, LayoutUnit replacedHeight) const
    {
        return toBlockFlow()->minLineHeightForReplacedObject(isFirstLine, replacedHeight);
    }

    void setStaticInlinePositionForChild(LineLayoutBox box, LayoutUnit inlinePosition)
    {
        toBlockFlow()->setStaticInlinePositionForChild(*toLayoutBox(box.layoutObject()), inlinePosition);
    }

    void updateStaticInlinePositionForChild(LineLayoutBox box, LayoutUnit logicalTop, IndentTextOrNot indentText = DoNotIndentText)
    {
        toBlockFlow()->updateStaticInlinePositionForChild(*toLayoutBox(box.layoutObject()), logicalTop, indentText);
    }

    FloatingObject* insertFloatingObject(LayoutBox& box)
    {
        return toBlockFlow()->insertFloatingObject(box);
    }

    FloatingObject* insertFloatingObject(LineLayoutBox box)
    {
        return toBlockFlow()->insertFloatingObject(*toLayoutBox(box.layoutObject()));
    }

    bool positionNewFloats(LineWidth* width)
    {
        return toBlockFlow()->positionNewFloats(width);
    }

    LayoutUnit nextFloatLogicalBottomBelow(LayoutUnit logicalHeight) const
    {
        return toBlockFlow()->nextFloatLogicalBottomBelow(logicalHeight);
    }

    FloatingObject* lastFloatFromPreviousLine() const
    {
        return toBlockFlow()->lastFloatFromPreviousLine();
    }

    // TODO(dgrogan/eae): *ForFloat: add these methods to the FloatingObject
    // class. Be consistent with use of start/end/before/after instead of
    // logicalTop/Left etc.
    LayoutUnit logicalTopForFloat(const FloatingObject& floatingObject) const
    {
        return toBlockFlow()->logicalTopForFloat(floatingObject);
    }

    LayoutUnit logicalBottomForFloat(const FloatingObject& floatingObject) const
    {
        return toBlockFlow()->logicalBottomForFloat(floatingObject);
    }

    LayoutUnit logicalLeftForFloat(const FloatingObject& floatingObject) const
    {
        return toBlockFlow()->logicalLeftForFloat(floatingObject);
    }

    LayoutUnit logicalRightForFloat(const FloatingObject& floatingObject) const
    {
        return toBlockFlow()->logicalRightForFloat(floatingObject);
    }

    LayoutUnit logicalWidthForFloat(const FloatingObject& floatingObject) const
    {
        return toBlockFlow()->logicalWidthForFloat(floatingObject);
    }

    LayoutUnit logicalRightOffsetForLine(LayoutUnit position, IndentTextOrNot indentText, LayoutUnit logicalHeight = LayoutUnit()) const
    {
        return toBlockFlow()->logicalRightOffsetForLine(position, indentText, logicalHeight);
    }

    LayoutUnit logicalLeftOffsetForLine(LayoutUnit position, IndentTextOrNot indentText, LayoutUnit logicalHeight = LayoutUnit()) const
    {
        return toBlockFlow()->logicalLeftOffsetForLine(position, indentText, logicalHeight);
    }

    void setHasMarkupTruncation(bool b)
    {
        toBlockFlow()->setHasMarkupTruncation(b);
    }

    LayoutUnit logicalWidth()
    {
        return toBlockFlow()->logicalWidth();
    }

    LineBoxList* lineBoxes()
    {
        return toBlockFlow()->lineBoxes();
    }

    bool containsFloats() const
    {
        return toBlockFlow()->containsFloats();
    }

    LayoutBlock* blockBeforeWithinSelectionRoot(LayoutSize& offset) const
    {
        return toBlockFlow()->blockBeforeWithinSelectionRoot(offset);
    }

    InlineBox* createAndAppendRootInlineBox()
    {
        return toBlockFlow()->createAndAppendRootInlineBox();
    }

    InlineFlowBox* lastLineBox()
    {
        return toBlockFlow()->lastLineBox();
    }

    InlineFlowBox* firstLineBox()
    {
        return toBlockFlow()->firstLineBox();
    }

    RootInlineBox* firstRootBox() const
    {
        return toBlockFlow()->firstRootBox();
    }

    RootInlineBox* lastRootBox() const
    {
        return toBlockFlow()->lastRootBox();
    }

private:
    LayoutBlockFlow* toBlockFlow() { return toLayoutBlockFlow(layoutObject()); };
    const LayoutBlockFlow* toBlockFlow() const { return toLayoutBlockFlow(layoutObject()); };
};

} // namespace blink

#endif // LineLayoutBlockFlow_h
