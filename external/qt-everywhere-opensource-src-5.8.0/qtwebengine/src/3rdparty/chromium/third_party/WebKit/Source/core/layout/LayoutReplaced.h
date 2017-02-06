/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
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

#ifndef LayoutReplaced_h
#define LayoutReplaced_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBox.h"

namespace blink {

// LayoutReplaced is the base class for a replaced element as defined by CSS:
//
// "An element whose content is outside the scope of the CSS formatting model,
// such as an image, embedded document, or applet."
// http://www.w3.org/TR/CSS2/conform.html#defs
//
// Blink consider that replaced elements have an intrinsic sizes (e.g. the
// natural size of an image or a video). The intrinsic size is stored by
// m_intrinsicSize.
//
// The computation sometimes ask for the intrinsic ratio, defined as follow:
//
//                      intrinsicWidth
//   intrinsicRatio = -------------------
//                      intrinsicHeight
//
// The intrinsic ratio is used to keep the same proportion as the intrinsic
// size (thus avoiding visual distortions if width / height doesn't match
// the intrinsic value).
class CORE_EXPORT LayoutReplaced : public LayoutBox {
public:
    LayoutReplaced(Element*);
    LayoutReplaced(Element*, const LayoutSize& intrinsicSize);
    ~LayoutReplaced() override;

    LayoutUnit computeReplacedLogicalWidth(ShouldComputePreferred = ComputeActual) const override;
    LayoutUnit computeReplacedLogicalHeight(LayoutUnit estimatedUsedWidth = LayoutUnit()) const override;

    bool hasReplacedLogicalHeight() const;
    LayoutRect replacedContentRect(const LayoutSize* overriddenIntrinsicSize = nullptr) const;

    bool needsPreferredWidthsRecalculation() const override;

    // These values are specified to be 300 and 150 pixels in the CSS 2.1 spec.
    // http://www.w3.org/TR/CSS2/visudet.html#inline-replaced-width
    static const int defaultWidth;
    static const int defaultHeight;
    bool canHaveChildren() const override { return false; }
    virtual void paintReplaced(const PaintInfo&, const LayoutPoint&) const { }
    LayoutRect localSelectionRect() const final;

    bool hasObjectFit() const { return style()->getObjectFit() != ComputedStyle::initialObjectFit(); }

    void paint(const PaintInfo&, const LayoutPoint&) const override;

    struct IntrinsicSizingInfo {
        STACK_ALLOCATED();
        IntrinsicSizingInfo() : hasWidth(true), hasHeight(true) {}

        FloatSize size;
        FloatSize aspectRatio;
        bool hasWidth;
        bool hasHeight;

        void transpose();
    };

protected:
    void willBeDestroyed() override;

    void layout() override;

    LayoutSize intrinsicSize() const final { return m_intrinsicSize; }
    virtual void computeIntrinsicSizingInfo(IntrinsicSizingInfo&) const;

    void computePositionedLogicalWidth(LogicalExtentComputedValues&) const override;
    void computePositionedLogicalHeight(LogicalExtentComputedValues&) const override;

    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const final;

    virtual LayoutUnit intrinsicContentLogicalHeight() const { return intrinsicLogicalHeight(); }

    virtual LayoutUnit minimumReplacedHeight() const { return LayoutUnit(); }

    void setSelectionState(SelectionState) final;

    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;

    void setIntrinsicSize(const LayoutSize& intrinsicSize) { m_intrinsicSize = intrinsicSize; }

    // This callback is invoked whenever the intrinsic size changed.
    //
    // The intrinsic size can change due to the network (from the default
    // intrinsic size [see above] to the actual intrinsic size) or to some
    // CSS properties like 'zoom' or 'image-orientation'.
    virtual void intrinsicSizeChanged();

    virtual LayoutReplaced* embeddedReplacedContent() const { return nullptr; }

    PositionWithAffinity positionForPoint(const LayoutPoint&) override;

private:
    void computePreferredLogicalWidths() final;

    bool canBeSelectionLeaf() const override { return true; }

    void computeIntrinsicSizingInfoForReplacedContent(LayoutReplaced*, IntrinsicSizingInfo&) const;
    FloatSize constrainIntrinsicSizeToMinMax(const IntrinsicSizingInfo&) const;

    LayoutUnit computeConstrainedLogicalWidth(ShouldComputePreferred) const;

    mutable LayoutSize m_intrinsicSize;
};

} // namespace blink

#endif
