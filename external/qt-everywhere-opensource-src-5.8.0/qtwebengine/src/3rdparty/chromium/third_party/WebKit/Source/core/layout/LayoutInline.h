/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef LayoutInline_h
#define LayoutInline_h

#include "core/CoreExport.h"
#include "core/editing/PositionWithAffinity.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/api/LineLayoutItem.h"
#include "core/layout/line/InlineFlowBox.h"
#include "core/layout/line/LineBoxList.h"

namespace blink {

class LayoutBlockFlow;

// LayoutInline is the LayoutObject associated with display: inline.
// This is called an "inline box" in CSS 2.1.
// http://www.w3.org/TR/CSS2/visuren.html#inline-boxes
//
// It is also the base class for content that behaves in similar way (like
// quotes and "display: ruby").
//
// Note that LayoutInline is always 'inline-level' but other LayoutObject
// can be 'inline-level', which is why it's stored as a boolean on LayoutObject
// (see LayoutObject::isInline()).
//
// For performance and memory consumption, this class ignores some inline-boxes
// during line layout because they don't impact layout (they still exist and are
// inserted into the layout tree). An example of this is
//             <span><span>Text</span></span>
// where the 2 spans have the same size as the inner text-node so they can be
// ignored for layout purpose, generating a single inline-box instead of 3.
// One downside of this optimization is that we have extra work to do when
// asking for bounding rects (see generateLineBoxRects).
// This optimization is called "culled inline" in the code.
//
// LayoutInlines are expected to be laid out by their containing
// LayoutBlockFlow. See LayoutBlockFlow::layoutInlineChildren.
//
//
// ***** CONTINUATIONS AND ANONYMOUS LAYOUTBLOCKFLOWS *****
// LayoutInline enforces the following invariant:
// "All in-flow children of an inline box are inline."
// When a non-inline child is inserted, LayoutInline::addChild splits the inline
// and potentially enclosing inlines too. It then wraps layout objects into
// anonymous block-flow containers. This creates complexity in the code as:
// - a DOM node can have several associated LayoutObjects (we don't currently
//   expose this information to the DOM code though).
// - more importantly, nodes that are parent/child in the DOM have no natural
//   relationship anymore (see example below).
// In order to do a correct tree walk over this synthetic tree, a single linked
// list is stored called *continuation*. See splitFlow() about how it is
// populated during LayoutInline split.
//
// Continuations can only be a LayoutInline or an anonymous LayoutBlockFlow.
// That's why continuations are handled by LayoutBoxModelObject (common class
// between the 2). See LayoutBoxModelObject::continuation and setContinuation.
//
// Let's take the following example:
//   <!DOCTYPE html>
//   <b>Bold inline.<div>Bold block.</div>More bold inlines.</b>
//
// The generated layout tree is:
//   LayoutBlockFlow {HTML}
//    LayoutBlockFlow {BODY}
//      LayoutBlockFlow (anonymous)
//        LayoutInline {B}
//          LayoutText {#text}
//            text run: "Bold inline."
//      LayoutBlockFlow (anonymous)
//        LayoutBlockFlow {DIV}
//          LayoutText {#text}
//            text run: "Bold block."
//      LayoutBlockFlow (anonymous)
//        LayoutInline {B}
//          LayoutText {#text}
//            text run: "More bold inlines."
//
// The insertion of the <div> inside the <b> forces the latter to be split
// into 2 LayoutInlines and the insertion of anonymous LayoutBlockFlows. The 2
// LayoutInlines are done so that we can apply the correct (bold) style to both
// sides of the <div>. The continuation chain starts with the first
// LayoutInline {B}, continues to the middle anonymous LayoutBlockFlow and
// finishes with the last LayoutInline {B}.
//
// Note that the middle anonymous LayoutBlockFlow duplicates the content.
// TODO(jchaffraix): Find out why we made the decision to always insert the
//                   anonymous LayoutBlockFlows.
//
// This section was inspired by an older article by Dave Hyatt:
// https://www.webkit.org/blog/115/webcore-rendering-ii-blocks-and-inlines/
class CORE_EXPORT LayoutInline : public LayoutBoxModelObject {
public:
    explicit LayoutInline(Element*);

    LayoutObject* firstChild() const { ASSERT(children() == virtualChildren()); return children()->firstChild(); }
    LayoutObject* lastChild() const { ASSERT(children() == virtualChildren()); return children()->lastChild(); }

    // If you have a LayoutInline, use firstChild or lastChild instead.
    void slowFirstChild() const = delete;
    void slowLastChild() const = delete;

    void addChild(LayoutObject* newChild, LayoutObject* beforeChild = nullptr) override;

    Element* node() const { return toElement(LayoutBoxModelObject::node()); }

    LayoutRectOutsets marginBoxOutsets() const final;
    LayoutUnit marginLeft() const final;
    LayoutUnit marginRight() const final;
    LayoutUnit marginTop() const final;
    LayoutUnit marginBottom() const final;
    LayoutUnit marginBefore(const ComputedStyle* otherStyle = nullptr) const final;
    LayoutUnit marginAfter(const ComputedStyle* otherStyle = nullptr) const final;
    LayoutUnit marginStart(const ComputedStyle* otherStyle = nullptr) const final;
    LayoutUnit marginEnd(const ComputedStyle* otherStyle = nullptr) const final;
    LayoutUnit marginOver() const final;
    LayoutUnit marginUnder() const final;

    void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const final;
    void absoluteQuads(Vector<FloatQuad>&) const override;

    LayoutSize offsetFromContainer(const LayoutObject*) const final;

    LayoutRect linesBoundingBox() const;
    LayoutRect visualOverflowRect() const final;

    InlineFlowBox* createAndAppendInlineFlowBox();

    void dirtyLineBoxes(bool fullLayout);

    LineBoxList* lineBoxes() { return &m_lineBoxes; }
    const LineBoxList* lineBoxes() const { return &m_lineBoxes; }

    InlineFlowBox* firstLineBox() const { return m_lineBoxes.firstLineBox(); }
    InlineFlowBox* lastLineBox() const { return m_lineBoxes.lastLineBox(); }
    InlineBox* firstLineBoxIncludingCulling() const { return alwaysCreateLineBoxes() ? firstLineBox() : culledInlineFirstLineBox(); }
    InlineBox* lastLineBoxIncludingCulling() const { return alwaysCreateLineBoxes() ? lastLineBox() : culledInlineLastLineBox(); }

    LayoutBoxModelObject* virtualContinuation() const final { return continuation(); }
    LayoutInline* inlineElementContinuation() const;

    void updateDragState(bool dragOn) final;

    LayoutSize offsetForInFlowPositionedInline(const LayoutBox& child) const;

    void addOutlineRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, IncludeBlockVisualOverflowOrNot) const final;
    // The following methods are called from the container if it has already added outline rects for line boxes
    // and/or children of this LayoutInline.
    void addOutlineRectsForChildrenAndContinuations(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, IncludeBlockVisualOverflowOrNot) const;
    void addOutlineRectsForContinuations(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, IncludeBlockVisualOverflowOrNot) const;

    using LayoutBoxModelObject::continuation;
    using LayoutBoxModelObject::setContinuation;

    bool alwaysCreateLineBoxes() const { return alwaysCreateLineBoxesForLayoutInline(); }
    void setAlwaysCreateLineBoxes(bool alwaysCreateLineBoxes = true) { setAlwaysCreateLineBoxesForLayoutInline(alwaysCreateLineBoxes); }
    void updateAlwaysCreateLineBoxes(bool fullLayout);

    LayoutRect localCaretRect(InlineBox*, int, LayoutUnit* extraWidthToEndOfLine) final;

    bool hitTestCulledInline(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset);

    const char* name() const override { return "LayoutInline"; }

protected:
    void willBeDestroyed() override;

    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;

    void computeSelfHitTestRects(Vector<LayoutRect>& rects, const LayoutPoint& layerOffset) const override;

    void invalidateDisplayItemClients(PaintInvalidationReason) const override;

private:
    LayoutObjectChildList* virtualChildren() final { return children(); }
    const LayoutObjectChildList* virtualChildren() const final { return children(); }
    const LayoutObjectChildList* children() const { return &m_children; }
    LayoutObjectChildList* children() { return &m_children; }

    bool isLayoutInline() const final { return true; }

    LayoutRect culledInlineVisualOverflowBoundingBox() const;
    InlineBox* culledInlineFirstLineBox() const;
    InlineBox* culledInlineLastLineBox() const;

    // For visualOverflowRect() only, to get bounding box of visual overflow of line boxes.
    LayoutRect linesVisualOverflowBoundingBox() const;

    template<typename GeneratorContext>
    void generateLineBoxRects(GeneratorContext& yield) const;
    template<typename GeneratorContext>
    void generateCulledLineBoxRects(GeneratorContext& yield, const LayoutInline* container) const;

    void addChildToContinuation(LayoutObject* newChild, LayoutObject* beforeChild);
    void addChildIgnoringContinuation(LayoutObject* newChild, LayoutObject* beforeChild = nullptr) final;

    void moveChildrenToIgnoringContinuation(LayoutInline* to, LayoutObject* startChild);

    void splitInlines(LayoutBlockFlow* fromBlock, LayoutBlockFlow* toBlock, LayoutBlockFlow* middleBlock,
        LayoutObject* beforeChild, LayoutBoxModelObject* oldCont);
    void splitFlow(LayoutObject* beforeChild, LayoutBlockFlow* newBlockBox,
        LayoutObject* newChild, LayoutBoxModelObject* oldCont);

    void layout() final { ASSERT_NOT_REACHED(); } // Do nothing for layout()

    void paint(const PaintInfo&, const LayoutPoint&) const final;

    bool nodeAtPoint(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) final;

    PaintLayerType layerTypeRequired() const override;

    LayoutUnit offsetLeft(const Element*) const final;
    LayoutUnit offsetTop(const Element*) const final;
    LayoutUnit offsetWidth() const final { return linesBoundingBox().width(); }
    LayoutUnit offsetHeight() const final { return linesBoundingBox().height(); }

    LayoutRect absoluteClippedOverflowRect() const override;

    // This method differs from visualOverflowRect in that it doesn't include the rects
    // for culled inline boxes, which aren't necessary for paint invalidation.
    LayoutRect localOverflowRectForPaintInvalidation() const override;

    bool mapToVisualRectInAncestorSpace(const LayoutBoxModelObject* ancestor, LayoutRect&, VisualRectFlags = DefaultVisualRectFlags) const final;

    PositionWithAffinity positionForPoint(const LayoutPoint&) final;

    IntRect borderBoundingBox() const final
    {
        IntRect boundingBox = enclosingIntRect(linesBoundingBox());
        return IntRect(0, 0, boundingBox.width(), boundingBox.height());
    }

    virtual InlineFlowBox* createInlineFlowBox(); // Subclassed by SVG and Ruby

    void dirtyLinesFromChangedChild(LayoutObject* child, MarkingBehavior markingBehaviour = MarkContainerChain) final { m_lineBoxes.dirtyLinesFromChangedChild(LineLayoutItem(this), LineLayoutItem(child), markingBehaviour == MarkContainerChain); }

    // TODO(leviw): This should probably be an int. We don't snap equivalent lines to different heights.
    LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const final;
    int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const final;

    void childBecameNonInline(LayoutObject* child) final;

    void updateHitTestResult(HitTestResult&, const LayoutPoint&) final;

    void imageChanged(WrappedImagePtr, const IntRect* = nullptr) final;

    void addAnnotatedRegions(Vector<AnnotatedRegionValue>&) final;

    void updateFromStyle() final;

    LayoutInline* clone() const;

    LayoutBoxModelObject* continuationBefore(LayoutObject* beforeChild);

    LayoutObjectChildList m_children;
    LineBoxList m_lineBoxes; // All of the line boxes created for this inline flow.  For example, <i>Hello<br>world.</i> will have two <i> line boxes.
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutInline, isLayoutInline());

} // namespace blink

#endif // LayoutInline_h
