/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef LayoutBoxModelObject_h
#define LayoutBoxModelObject_h

#include "core/CoreExport.h"
#include "core/layout/BackgroundBleedAvoidance.h"
#include "core/layout/ContentChangeType.h"
#include "core/layout/LayoutObject.h"
#include "core/page/scrolling/StickyPositionScrollingConstraints.h"
#include "platform/geometry/LayoutRect.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class LineLayoutBoxModel;
class PaintLayer;
class PaintLayerScrollableArea;

enum PaintLayerType {
    NoPaintLayer,
    NormalPaintLayer,
    // A forced or overflow clip layer is required for bookkeeping purposes,
    // but does not force a layer to be self painting.
    OverflowClipPaintLayer,
    ForcedPaintLayer
};

// Modes for some of the line-related functions.
enum LinePositionMode { PositionOnContainingLine, PositionOfInteriorLineBoxes };
enum LineDirectionMode { HorizontalLine, VerticalLine };

class InlineFlowBox;

struct LayoutBoxModelObjectRareData {
    WTF_MAKE_NONCOPYABLE(LayoutBoxModelObjectRareData);
    USING_FAST_MALLOC(LayoutBoxModelObjectRareData);
public:
    LayoutBoxModelObjectRareData() {}

    StickyPositionScrollingConstraints m_stickyPositionScrollingConstraints;
};

// This class is the base class for all CSS objects.
//
// All CSS objects follow the box model object. See THE BOX MODEL section in
// LayoutBox for more information.
//
// This class actually doesn't have the box model but it exposes some common
// functions or concepts that sub-classes can extend upon. For example, there
// are accessors for margins, borders, paddings and borderBoundingBox().
//
// The reason for this partial implementation is that the 2 classes inheriting
// from it (LayoutBox and LayoutInline) have different requirements but need to
// have a PaintLayer.
// For a full implementation of the box model, see LayoutBox.
//
// An important member of this class is PaintLayer. This class is
// central to painting and hit-testing (see its class comment).
// PaintLayers are instantiated for several reasons based on the
// return value of layerTypeRequired().
// Interestingly, most SVG objects inherit from LayoutSVGModelObject and thus
// can't have a PaintLayer. This is an unfortunate artifact of our
// design as it limits code sharing and prevents hardware accelerating SVG
// (the current design require a PaintLayer for compositing).
//
//
// ***** COORDINATE SYSTEMS *****
//
// In order to fully understand LayoutBoxModelObject and the inherited classes,
// we need to introduce the concept of coordinate systems.
// There is 3 main coordinate systems:
// - physical coordinates: it is the coordinate system used for painting and
//   correspond to physical direction as seen on the physical display (screen,
//   printed page). In CSS, 'top', 'right', 'bottom', 'left' are all in physical
//   coordinates. The code matches this convention too.
//
// - logical coordinates: this is the coordinate system used for layout. It is
//   determined by 'writing-mode' and 'direction'. Any property using 'before',
//   'after', 'start' or 'end' is in logical coordinates. Those are also named
//   respectively 'logical top', 'logical bottom', 'logical left' and
//   'logical right'.
//
// Example with writing-mode: vertical-rl; direction: ltr;
//
//                    'top' / 'start' side
//
//                     block-flow direction
//           <------------------------------------ |
//           ------------------------------------- |
//           |        c   |          s           | |
// 'left'    |        o   |          o           | |   inline     'right'
//    /      |        n   |          m           | |  direction      /
// 'after'   |        t   |          e           | |              'before'
//  side     |        e   |                      | |                side
//           |        n   |                      | |
//           |        t   |                      | |
//           ------------------------------------- v
//
//                 'bottom' / 'end' side
//
// See https://drafts.csswg.org/css-writing-modes-3/#text-flow for some
// extra details.
//
// - physical coordinates with flipped block-flow direction: those are physical
//   coordinates but we flipped the block direction. See
//   LayoutBox::noOverflowRect.
//   TODO(jchaffraix): I don't fully understand why we need this coordinate
//   system someone should fill in those details.
class CORE_EXPORT LayoutBoxModelObject : public LayoutObject {
public:
    LayoutBoxModelObject(ContainerNode*);
    ~LayoutBoxModelObject() override;

    // This is the only way layers should ever be destroyed.
    void destroyLayer();

    LayoutSize relativePositionOffset() const;
    LayoutSize relativePositionLogicalOffset() const { return style()->isHorizontalWritingMode() ? relativePositionOffset() : relativePositionOffset().transposedSize(); }

    // Populates StickyPositionConstraints, setting the sticky box rect, containing block rect and updating
    // the constraint offsets according to the available space.
    FloatRect computeStickyConstrainingRect() const;
    void updateStickyPositionConstraints() const;
    LayoutSize stickyPositionOffset() const;

    LayoutSize offsetForInFlowPosition() const;

    // IE extensions. Used to calculate offsetWidth/Height.  Overridden by inlines (LayoutFlow)
    // to return the remaining width on a given line (and the height of a single line).
    virtual LayoutUnit offsetLeft(const Element*) const;
    virtual LayoutUnit offsetTop(const Element*) const;
    virtual LayoutUnit offsetWidth() const = 0;
    virtual LayoutUnit offsetHeight() const = 0;

    int pixelSnappedOffsetLeft(const Element* parent) const { return roundToInt(offsetLeft(parent)); }
    int pixelSnappedOffsetTop(const Element* parent) const { return roundToInt(offsetTop(parent)); }
    virtual int pixelSnappedOffsetWidth(const Element*) const;
    virtual int pixelSnappedOffsetHeight(const Element*) const;

    bool hasSelfPaintingLayer() const;
    PaintLayer* layer() const { return m_layer.get(); }
    PaintLayerScrollableArea* getScrollableArea() const;

    virtual void updateFromStyle();

    // The type of PaintLayer to instantiate.
    // Any value returned from this function other than NoPaintLayer
    // will populate |m_layer|.
    virtual PaintLayerType layerTypeRequired() const = 0;

    // This will work on inlines to return the bounding box of all of the lines' border boxes.
    virtual IntRect borderBoundingBox() const = 0;

    virtual LayoutRect visualOverflowRect() const = 0;

    // Checks if this box, or any of it's descendants, or any of it's continuations,
    // will take up space in the layout of the page.
    bool hasNonEmptyLayoutSize() const;
    bool usesCompositedScrolling() const;

    // These return the CSS computed padding values.
    LayoutUnit computedCSSPaddingTop() const { return computedCSSPadding(style()->paddingTop()); }
    LayoutUnit computedCSSPaddingBottom() const { return computedCSSPadding(style()->paddingBottom()); }
    LayoutUnit computedCSSPaddingLeft() const { return computedCSSPadding(style()->paddingLeft()); }
    LayoutUnit computedCSSPaddingRight() const { return computedCSSPadding(style()->paddingRight()); }
    LayoutUnit computedCSSPaddingBefore() const { return computedCSSPadding(style()->paddingBefore()); }
    LayoutUnit computedCSSPaddingAfter() const { return computedCSSPadding(style()->paddingAfter()); }
    LayoutUnit computedCSSPaddingStart() const { return computedCSSPadding(style()->paddingStart()); }
    LayoutUnit computedCSSPaddingEnd() const { return computedCSSPadding(style()->paddingEnd()); }
    LayoutUnit computedCSSPaddingOver() const { return computedCSSPadding(style()->paddingOver()); }
    LayoutUnit computedCSSPaddingUnder() const { return computedCSSPadding(style()->paddingUnder()); }

    // These functions are used during layout.
    // - Table cells override them to include the intrinsic padding (see
    // explanations in LayoutTableCell).
    // - Table override them to exclude padding with collapsing borders.
    virtual LayoutUnit paddingTop() const { return computedCSSPaddingTop(); }
    virtual LayoutUnit paddingBottom() const { return computedCSSPaddingBottom(); }
    virtual LayoutUnit paddingLeft() const { return computedCSSPaddingLeft(); }
    virtual LayoutUnit paddingRight() const { return computedCSSPaddingRight(); }
    virtual LayoutUnit paddingBefore() const { return computedCSSPaddingBefore(); }
    virtual LayoutUnit paddingAfter() const { return computedCSSPaddingAfter(); }
    virtual LayoutUnit paddingStart() const { return computedCSSPaddingStart(); }
    virtual LayoutUnit paddingEnd() const { return computedCSSPaddingEnd(); }
    LayoutUnit paddingOver() const { return computedCSSPaddingOver(); }
    LayoutUnit paddingUnder() const { return computedCSSPaddingUnder(); }

    virtual int borderTop() const { return style()->borderTopWidth(); }
    virtual int borderBottom() const { return style()->borderBottomWidth(); }
    virtual int borderLeft() const { return style()->borderLeftWidth(); }
    virtual int borderRight() const { return style()->borderRightWidth(); }
    virtual int borderBefore() const { return style()->borderBeforeWidth(); }
    virtual int borderAfter() const { return style()->borderAfterWidth(); }
    virtual int borderStart() const { return style()->borderStartWidth(); }
    virtual int borderEnd() const { return style()->borderEndWidth(); }
    int borderOver() const { return style()->borderOverWidth(); }
    int borderUnder() const { return style()->borderUnderWidth(); }

    int borderWidth() const { return borderLeft() + borderRight(); }
    int borderHeight() const { return borderTop() + borderBottom(); }

    // Insets from the border box to the inside of the border.
    LayoutRectOutsets borderInsets() const { return LayoutRectOutsets(-borderTop(), -borderRight(), -borderBottom(), -borderLeft()); }

    bool hasBorderOrPadding() const { return style()->hasBorder() || style()->hasPadding(); }

    LayoutUnit borderAndPaddingStart() const { return borderStart() + paddingStart(); }
    LayoutUnit borderAndPaddingBefore() const { return borderBefore() + paddingBefore(); }
    LayoutUnit borderAndPaddingAfter() const { return borderAfter() + paddingAfter(); }
    LayoutUnit borderAndPaddingOver() const { return borderOver() + paddingOver(); }
    LayoutUnit borderAndPaddingUnder() const { return borderUnder() + paddingUnder(); }

    LayoutUnit borderAndPaddingHeight() const { return borderTop() + borderBottom() + paddingTop() + paddingBottom(); }
    LayoutUnit borderAndPaddingWidth() const { return borderLeft() + borderRight() + paddingLeft() + paddingRight(); }
    LayoutUnit borderAndPaddingLogicalHeight() const { return hasBorderOrPadding() ? borderAndPaddingBefore() + borderAndPaddingAfter() : LayoutUnit(); }
    LayoutUnit borderAndPaddingLogicalWidth() const { return borderStart() + borderEnd() + paddingStart() + paddingEnd(); }
    LayoutUnit borderAndPaddingLogicalLeft() const { return style()->isHorizontalWritingMode() ? borderLeft() + paddingLeft() : borderTop() + paddingTop(); }

    LayoutUnit borderLogicalLeft() const { return LayoutUnit(style()->isHorizontalWritingMode() ? borderLeft() : borderTop()); }
    LayoutUnit borderLogicalRight() const { return LayoutUnit(style()->isHorizontalWritingMode() ? borderRight() : borderBottom()); }

    LayoutUnit paddingLogicalWidth() const { return paddingStart() + paddingEnd(); }
    LayoutUnit paddingLogicalHeight() const { return paddingBefore() + paddingAfter(); }

    virtual LayoutRectOutsets marginBoxOutsets() const = 0;
    virtual LayoutUnit marginTop() const = 0;
    virtual LayoutUnit marginBottom() const = 0;
    virtual LayoutUnit marginLeft() const = 0;
    virtual LayoutUnit marginRight() const = 0;
    virtual LayoutUnit marginBefore(const ComputedStyle* otherStyle = nullptr) const = 0;
    virtual LayoutUnit marginAfter(const ComputedStyle* otherStyle = nullptr) const = 0;
    virtual LayoutUnit marginStart(const ComputedStyle* otherStyle = nullptr) const = 0;
    virtual LayoutUnit marginEnd(const ComputedStyle* otherStyle = nullptr) const = 0;
    virtual LayoutUnit marginOver() const = 0;
    virtual LayoutUnit marginUnder() const = 0;
    LayoutUnit marginHeight() const { return marginTop() + marginBottom(); }
    LayoutUnit marginWidth() const { return marginLeft() + marginRight(); }
    LayoutUnit marginLogicalHeight() const { return marginBefore() + marginAfter(); }
    LayoutUnit marginLogicalWidth() const { return marginStart() + marginEnd(); }

    bool hasInlineDirectionBordersPaddingOrMargin() const { return hasInlineDirectionBordersOrPadding() || marginStart() || marginEnd(); }
    bool hasInlineDirectionBordersOrPadding() const { return borderStart() || borderEnd() || paddingStart() || paddingEnd(); }

    virtual LayoutUnit containingBlockLogicalWidthForContent() const;

    virtual void childBecameNonInline(LayoutObject* /*child*/) { }

    virtual bool boxShadowShouldBeAppliedToBackground(BackgroundBleedAvoidance, const InlineFlowBox* = nullptr) const;

    // Overridden by subclasses to determine line height and baseline position.
    virtual LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const = 0;
    virtual int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const = 0;

    const LayoutObject* pushMappingToContainer(const LayoutBoxModelObject* ancestorToStopAt, LayoutGeometryMap&) const override;

    void setSelectionState(SelectionState) override;

    void contentChanged(ContentChangeType);
    bool hasAcceleratedCompositing() const;

    void computeLayerHitTestRects(LayerHitTestRects&) const override;

    // Returns true if the background is painted opaque in the given rect.
    // The query rect is given in local coordinate system.
    virtual bool backgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const { return false; }

    void invalidateTreeIfNeeded(const PaintInvalidationState&) override;

    // Indicate that the contents of this layoutObject need to be repainted.
    // This only has an effect if compositing is being used.
    // The rect is in the physical coordinate space of this layout object.
    void setBackingNeedsPaintInvalidationInRect(const LayoutRect&, PaintInvalidationReason, const LayoutObject&) const;

    // http://www.w3.org/TR/css3-background/#body-background
    // <html> root element with no background steals background from its first <body> child.
    // The used background for such body element should be the initial value. (i.e. transparent)
    bool backgroundStolenForBeingBody(const ComputedStyle* rootElementStyle = nullptr) const;

protected:
    void willBeDestroyed() override;

    LayoutPoint adjustedPositionRelativeTo(const LayoutPoint&, const Element*) const;

    bool calculateHasBoxDecorations() const;

    // Returns the continuation associated with |this|.
    // Returns nullptr if no continuation is associated with |this|.
    //
    // See the section about CONTINUATIONS AND ANONYMOUS LAYOUTBLOCKFLOWS in
    // LayoutInline for more details about them.
    //
    // Our implementation uses a HashMap to store them to avoid paying the cost
    // for each LayoutBoxModelObject (|continuationMap| in the cpp file).
    LayoutBoxModelObject* continuation() const;

    // Set the next link in the continuation chain.
    //
    // See continuation above for more details.
    void setContinuation(LayoutBoxModelObject*);

    virtual LayoutSize accumulateInFlowPositionOffsets() const { return LayoutSize(); }

    LayoutRect localCaretRectForEmptyElement(LayoutUnit width, LayoutUnit textIndentOffset);

    bool hasAutoHeightOrContainingBlockWithAutoHeight() const;
    LayoutBlock* containingBlockForAutoHeightDetection(Length logicalHeight) const;

    void addOutlineRectsForNormalChildren(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, IncludeBlockVisualOverflowOrNot) const;
    void addOutlineRectsForDescendant(const LayoutObject& descendant, Vector<LayoutRect>&, const LayoutPoint& additionalOffset, IncludeBlockVisualOverflowOrNot) const;

    void addLayerHitTestRects(LayerHitTestRects&, const PaintLayer*, const LayoutPoint&, const LayoutRect&) const override;

    void styleWillChange(StyleDifference, const ComputedStyle& newStyle) override;
    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;

    void invalidateStickyConstraints();

public:
    // These functions are only used internally to manipulate the layout tree structure via remove/insert/appendChildNode.
    // Since they are typically called only to move objects around within anonymous blocks (which only have layers in
    // the case of column spans), the default for fullRemoveInsert is false rather than true.
    void moveChildTo(LayoutBoxModelObject* toBoxModelObject, LayoutObject* child, LayoutObject* beforeChild, bool fullRemoveInsert = false);
    void moveChildTo(LayoutBoxModelObject* toBoxModelObject, LayoutObject* child, bool fullRemoveInsert = false)
    {
        moveChildTo(toBoxModelObject, child, 0, fullRemoveInsert);
    }
    void moveAllChildrenTo(LayoutBoxModelObject* toBoxModelObject, bool fullRemoveInsert = false)
    {
        moveAllChildrenTo(toBoxModelObject, 0, fullRemoveInsert);
    }
    void moveAllChildrenTo(LayoutBoxModelObject* toBoxModelObject, LayoutObject* beforeChild, bool fullRemoveInsert = false)
    {
        moveChildrenTo(toBoxModelObject, slowFirstChild(), 0, beforeChild, fullRemoveInsert);
    }
    // Move all of the kids from |startChild| up to but excluding |endChild|. 0 can be passed as the |endChild| to denote
    // that all the kids from |startChild| onwards should be moved.
    void moveChildrenTo(LayoutBoxModelObject* toBoxModelObject, LayoutObject* startChild, LayoutObject* endChild, bool fullRemoveInsert = false)
    {
        moveChildrenTo(toBoxModelObject, startChild, endChild, 0, fullRemoveInsert);
    }
    virtual void moveChildrenTo(LayoutBoxModelObject* toBoxModelObject, LayoutObject* startChild, LayoutObject* endChild, LayoutObject* beforeChild, bool fullRemoveInsert = false);

private:
    void createLayer();

    LayoutUnit computedCSSPadding(const Length&) const;
    bool isBoxModelObject() const final { return true; }

    LayoutBoxModelObjectRareData& ensureRareData()
    {
        if (!m_rareData)
            m_rareData = wrapUnique(new LayoutBoxModelObjectRareData());
        return *m_rareData.get();
    }

    // The PaintLayer associated with this object.
    // |m_layer| can be nullptr depending on the return value of layerTypeRequired().
    std::unique_ptr<PaintLayer> m_layer;

    std::unique_ptr<LayoutBoxModelObjectRareData> m_rareData;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutBoxModelObject, isBoxModelObject());

} // namespace blink

#endif // LayoutBoxModelObject_h
