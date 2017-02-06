/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011-2012. All rights reserved.
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

#include "core/layout/LayoutReplaced.h"

#include "core/editing/PositionWithAffinity.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutImage.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/api/LineLayoutBlockFlow.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/ReplacedPainter.h"
#include "platform/LengthFunctions.h"

namespace blink {

const int LayoutReplaced::defaultWidth = 300;
const int LayoutReplaced::defaultHeight = 150;

LayoutReplaced::LayoutReplaced(Element* element)
    : LayoutBox(element)
    , m_intrinsicSize(defaultWidth, defaultHeight)
{
    // TODO(jchaffraix): We should not set this boolean for block-level
    // replaced elements (crbug.com/567964).
    setIsAtomicInlineLevel(true);
}

LayoutReplaced::LayoutReplaced(Element* element, const LayoutSize& intrinsicSize)
    : LayoutBox(element)
    , m_intrinsicSize(intrinsicSize)
{
    // TODO(jchaffraix): We should not set this boolean for block-level
    // replaced elements (crbug.com/567964).
    setIsAtomicInlineLevel(true);
}

LayoutReplaced::~LayoutReplaced()
{
}

void LayoutReplaced::willBeDestroyed()
{
    if (!documentBeingDestroyed() && parent())
        parent()->dirtyLinesFromChangedChild(this);

    LayoutBox::willBeDestroyed();
}

void LayoutReplaced::styleDidChange(StyleDifference diff, const ComputedStyle* oldStyle)
{
    LayoutBox::styleDidChange(diff, oldStyle);

    bool hadStyle = (oldStyle != 0);
    float oldZoom = hadStyle ? oldStyle->effectiveZoom() : ComputedStyle::initialZoom();
    if (style() && style()->effectiveZoom() != oldZoom)
        intrinsicSizeChanged();
}

void LayoutReplaced::layout()
{
    ASSERT(needsLayout());
    LayoutAnalyzer::Scope analyzer(*this);

    LayoutRect oldContentRect = replacedContentRect();

    setHeight(minimumReplacedHeight());

    updateLogicalWidth();
    updateLogicalHeight();

    m_overflow.reset();
    addVisualEffectOverflow();
    updateLayerTransformAfterLayout();
    invalidateBackgroundObscurationStatus();

    clearNeedsLayout();

    if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled() && replacedContentRect() != oldContentRect)
        setShouldDoFullPaintInvalidation();
}

void LayoutReplaced::intrinsicSizeChanged()
{
    int scaledWidth = static_cast<int>(defaultWidth * style()->effectiveZoom());
    int scaledHeight = static_cast<int>(defaultHeight * style()->effectiveZoom());
    m_intrinsicSize = LayoutSize(scaledWidth, scaledHeight);
    setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(LayoutInvalidationReason::SizeChanged);
}

void LayoutReplaced::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset) const
{
    ReplacedPainter(*this).paint(paintInfo, paintOffset);
}

bool LayoutReplaced::hasReplacedLogicalHeight() const
{
    if (style()->logicalHeight().isAuto())
        return false;

    if (style()->logicalHeight().isSpecified()) {
        if (hasAutoHeightOrContainingBlockWithAutoHeight())
            return false;
        return true;
    }

    if (style()->logicalHeight().isIntrinsic())
        return true;

    return false;
}

bool LayoutReplaced::needsPreferredWidthsRecalculation() const
{
    // If the height is a percentage and the width is auto, then the containingBlocks's height changing can cause
    // this node to change it's preferred width because it maintains aspect ratio.
    return hasRelativeLogicalHeight() && style()->logicalWidth().isAuto() && !hasAutoHeightOrContainingBlockWithAutoHeight();
}

static inline bool layoutObjectHasAspectRatio(const LayoutObject* layoutObject)
{
    ASSERT(layoutObject);
    return layoutObject->isImage() || layoutObject->isCanvas() || layoutObject->isVideo();
}

void LayoutReplaced::computeIntrinsicSizingInfoForReplacedContent(LayoutReplaced* contentLayoutObject, IntrinsicSizingInfo& intrinsicSizingInfo) const
{
    if (contentLayoutObject) {
        contentLayoutObject->computeIntrinsicSizingInfo(intrinsicSizingInfo);

        // Handle zoom & vertical writing modes here, as the embedded document doesn't know about them.
        intrinsicSizingInfo.size.scale(style()->effectiveZoom());
        if (isLayoutImage())
            intrinsicSizingInfo.size.scale(toLayoutImage(this)->imageDevicePixelRatio());

        // Update our intrinsic size to match what the content layoutObject has computed, so that when we
        // constrain the size below, the correct intrinsic size will be obtained for comparison against
        // min and max widths.
        if (!intrinsicSizingInfo.aspectRatio.isEmpty() && !intrinsicSizingInfo.size.isEmpty())
            m_intrinsicSize = LayoutSize(intrinsicSizingInfo.size);

        if (!isHorizontalWritingMode())
            intrinsicSizingInfo.transpose();
    } else {
        computeIntrinsicSizingInfo(intrinsicSizingInfo);
        if (!intrinsicSizingInfo.aspectRatio.isEmpty() && !intrinsicSizingInfo.size.isEmpty())
            m_intrinsicSize = LayoutSize(isHorizontalWritingMode() ? intrinsicSizingInfo.size : intrinsicSizingInfo.size.transposedSize());
    }
}

FloatSize LayoutReplaced::constrainIntrinsicSizeToMinMax(const IntrinsicSizingInfo& intrinsicSizingInfo) const
{
    // Constrain the intrinsic size along each axis according to minimum and maximum width/heights along the opposite
    // axis. So for example a maximum width that shrinks our width will result in the height we compute here having
    // to shrink in order to preserve the aspect ratio. Because we compute these values independently along each
    // axis, the final returned size may in fact not preserve the aspect ratio.
    // TODO(davve): Investigate using only the intrinsic aspect ratio here.
    FloatSize constrainedSize = intrinsicSizingInfo.size;
    if (!intrinsicSizingInfo.aspectRatio.isEmpty() && !intrinsicSizingInfo.size.isEmpty() && style()->logicalWidth().isAuto() && style()->logicalHeight().isAuto()) {
        // We can't multiply or divide by 'intrinsicSizingInfo.aspectRatio' here, it breaks tests, like fast/images/zoomed-img-size.html, which
        // can only be fixed once subpixel precision is available for things like intrinsicWidth/Height - which include zoom!
        constrainedSize.setWidth(LayoutBox::computeReplacedLogicalHeight() * intrinsicSizingInfo.size.width() / intrinsicSizingInfo.size.height());
        constrainedSize.setHeight(LayoutBox::computeReplacedLogicalWidth() * intrinsicSizingInfo.size.height() / intrinsicSizingInfo.size.width());
    }
    return constrainedSize;
}

void LayoutReplaced::computePositionedLogicalWidth(LogicalExtentComputedValues& computedValues) const
{
    // The following is based off of the W3C Working Draft from April 11, 2006 of
    // CSS 2.1: Section 10.3.8 "Absolutely positioned, replaced elements"
    // <http://www.w3.org/TR/2005/WD-CSS21-20050613/visudet.html#abs-replaced-width>
    // (block-style-comments in this function correspond to text from the spec and
    // the numbers correspond to numbers in spec)

    // We don't use containingBlock(), since we may be positioned by an enclosing
    // relative positioned inline.
    const LayoutBoxModelObject* containerBlock = toLayoutBoxModelObject(container());

    const LayoutUnit containerLogicalWidth = containingBlockLogicalWidthForPositioned(containerBlock);
    const LayoutUnit containerRelativeLogicalWidth = containingBlockLogicalWidthForPositioned(containerBlock, false);

    // To match WinIE, in quirks mode use the parent's 'direction' property
    // instead of the the container block's.
    TextDirection containerDirection = containerBlock->style()->direction();

    // Variables to solve.
    bool isHorizontal = isHorizontalWritingMode();
    Length logicalLeft = style()->logicalLeft();
    Length logicalRight = style()->logicalRight();
    Length marginLogicalLeft = isHorizontal ? style()->marginLeft() : style()->marginTop();
    Length marginLogicalRight = isHorizontal ? style()->marginRight() : style()->marginBottom();
    LayoutUnit& marginLogicalLeftAlias = style()->isLeftToRightDirection() ? computedValues.m_margins.m_start : computedValues.m_margins.m_end;
    LayoutUnit& marginLogicalRightAlias = style()->isLeftToRightDirection() ? computedValues.m_margins.m_end : computedValues.m_margins.m_start;

    /*-----------------------------------------------------------------------*\
     * 1. The used value of 'width' is determined as for inline replaced
     *    elements.
    \*-----------------------------------------------------------------------*/
    // NOTE: This value of width is final in that the min/max width calculations
    // are dealt with in computeReplacedWidth().  This means that the steps to produce
    // correct max/min in the non-replaced version, are not necessary.
    computedValues.m_extent = computeReplacedLogicalWidth() + borderAndPaddingLogicalWidth();

    const LayoutUnit availableSpace = containerLogicalWidth - computedValues.m_extent;

    /*-----------------------------------------------------------------------*\
     * 2. If both 'left' and 'right' have the value 'auto', then if 'direction'
     *    of the containing block is 'ltr', set 'left' to the static position;
     *    else if 'direction' is 'rtl', set 'right' to the static position.
    \*-----------------------------------------------------------------------*/
    // see FIXME 1
    computeInlineStaticDistance(logicalLeft, logicalRight, this, containerBlock, containerLogicalWidth);

    /*-----------------------------------------------------------------------*\
     * 3. If 'left' or 'right' are 'auto', replace any 'auto' on 'margin-left'
     *    or 'margin-right' with '0'.
    \*-----------------------------------------------------------------------*/
    if (logicalLeft.isAuto() || logicalRight.isAuto()) {
        if (marginLogicalLeft.isAuto())
            marginLogicalLeft.setValue(Fixed, 0);
        if (marginLogicalRight.isAuto())
            marginLogicalRight.setValue(Fixed, 0);
    }

    /*-----------------------------------------------------------------------*\
     * 4. If at this point both 'margin-left' and 'margin-right' are still
     *    'auto', solve the equation under the extra constraint that the two
     *    margins must get equal values, unless this would make them negative,
     *    in which case when the direction of the containing block is 'ltr'
     *    ('rtl'), set 'margin-left' ('margin-right') to zero and solve for
     *    'margin-right' ('margin-left').
    \*-----------------------------------------------------------------------*/
    LayoutUnit logicalLeftValue;
    LayoutUnit logicalRightValue;

    if (marginLogicalLeft.isAuto() && marginLogicalRight.isAuto()) {
        // 'left' and 'right' cannot be 'auto' due to step 3
        ASSERT(!(logicalLeft.isAuto() && logicalRight.isAuto()));

        logicalLeftValue = valueForLength(logicalLeft, containerLogicalWidth);
        logicalRightValue = valueForLength(logicalRight, containerLogicalWidth);

        LayoutUnit difference = availableSpace - (logicalLeftValue + logicalRightValue);
        if (difference > LayoutUnit()) {
            marginLogicalLeftAlias = difference / 2; // split the difference
            marginLogicalRightAlias = difference - marginLogicalLeftAlias; // account for odd valued differences
        } else {
            // Use the containing block's direction rather than the parent block's
            // per CSS 2.1 reference test abspos-replaced-width-margin-000.
            if (containerDirection == LTR) {
                marginLogicalLeftAlias = LayoutUnit();
                marginLogicalRightAlias = difference; // will be negative
            } else {
                marginLogicalLeftAlias = difference; // will be negative
                marginLogicalRightAlias = LayoutUnit();
            }
        }

    /*-----------------------------------------------------------------------*\
     * 5. If at this point there is an 'auto' left, solve the equation for
     *    that value.
    \*-----------------------------------------------------------------------*/
    } else if (logicalLeft.isAuto()) {
        marginLogicalLeftAlias = valueForLength(marginLogicalLeft, containerRelativeLogicalWidth);
        marginLogicalRightAlias = valueForLength(marginLogicalRight, containerRelativeLogicalWidth);
        logicalRightValue = valueForLength(logicalRight, containerLogicalWidth);

        // Solve for 'left'
        logicalLeftValue = availableSpace - (logicalRightValue + marginLogicalLeftAlias + marginLogicalRightAlias);
    } else if (logicalRight.isAuto()) {
        marginLogicalLeftAlias = valueForLength(marginLogicalLeft, containerRelativeLogicalWidth);
        marginLogicalRightAlias = valueForLength(marginLogicalRight, containerRelativeLogicalWidth);
        logicalLeftValue = valueForLength(logicalLeft, containerLogicalWidth);

        // Solve for 'right'
        logicalRightValue = availableSpace - (logicalLeftValue + marginLogicalLeftAlias + marginLogicalRightAlias);
    } else if (marginLogicalLeft.isAuto()) {
        marginLogicalRightAlias = valueForLength(marginLogicalRight, containerRelativeLogicalWidth);
        logicalLeftValue = valueForLength(logicalLeft, containerLogicalWidth);
        logicalRightValue = valueForLength(logicalRight, containerLogicalWidth);

        // Solve for 'margin-left'
        marginLogicalLeftAlias = availableSpace - (logicalLeftValue + logicalRightValue + marginLogicalRightAlias);
    } else if (marginLogicalRight.isAuto()) {
        marginLogicalLeftAlias = valueForLength(marginLogicalLeft, containerRelativeLogicalWidth);
        logicalLeftValue = valueForLength(logicalLeft, containerLogicalWidth);
        logicalRightValue = valueForLength(logicalRight, containerLogicalWidth);

        // Solve for 'margin-right'
        marginLogicalRightAlias = availableSpace - (logicalLeftValue + logicalRightValue + marginLogicalLeftAlias);
    } else {
        // Nothing is 'auto', just calculate the values.
        marginLogicalLeftAlias = valueForLength(marginLogicalLeft, containerRelativeLogicalWidth);
        marginLogicalRightAlias = valueForLength(marginLogicalRight, containerRelativeLogicalWidth);
        logicalRightValue = valueForLength(logicalRight, containerLogicalWidth);
        logicalLeftValue = valueForLength(logicalLeft, containerLogicalWidth);
        // If the containing block is right-to-left, then push the left position as far to the right as possible
        if (containerDirection == RTL) {
            int totalLogicalWidth = computedValues.m_extent + logicalLeftValue + logicalRightValue +  marginLogicalLeftAlias + marginLogicalRightAlias;
            logicalLeftValue = containerLogicalWidth - (totalLogicalWidth - logicalLeftValue);
        }
    }

    /*-----------------------------------------------------------------------*\
     * 6. If at this point the values are over-constrained, ignore the value
     *    for either 'left' (in case the 'direction' property of the
     *    containing block is 'rtl') or 'right' (in case 'direction' is
     *    'ltr') and solve for that value.
    \*-----------------------------------------------------------------------*/
    // NOTE: Constraints imposed by the width of the containing block and its content have already been accounted for above.

    // FIXME: Deal with differing writing modes here.  Our offset needs to be in the containing block's coordinate space, so that
    // can make the result here rather complicated to compute.

    // Use computed values to calculate the horizontal position.

    // FIXME: This hack is needed to calculate the logical left position for a 'rtl' relatively
    // positioned, inline containing block because right now, it is using the logical left position
    // of the first line box when really it should use the last line box.  When
    // this is fixed elsewhere, this block should be removed.
    if (containerBlock->isLayoutInline() && !containerBlock->style()->isLeftToRightDirection()) {
        const LayoutInline* flow = toLayoutInline(containerBlock);
        InlineFlowBox* firstLine = flow->firstLineBox();
        InlineFlowBox* lastLine = flow->lastLineBox();
        if (firstLine && lastLine && firstLine != lastLine) {
            computedValues.m_position = logicalLeftValue + marginLogicalLeftAlias + lastLine->borderLogicalLeft() + (lastLine->logicalLeft() - firstLine->logicalLeft());
            return;
        }
    }

    LayoutUnit logicalLeftPos = logicalLeftValue + marginLogicalLeftAlias;
    computeLogicalLeftPositionedOffset(logicalLeftPos, this, computedValues.m_extent, containerBlock, containerLogicalWidth);
    computedValues.m_position = logicalLeftPos;
}

void LayoutReplaced::computePositionedLogicalHeight(LogicalExtentComputedValues& computedValues) const
{
    // The following is based off of the W3C Working Draft from April 11, 2006 of
    // CSS 2.1: Section 10.6.5 "Absolutely positioned, replaced elements"
    // <http://www.w3.org/TR/2005/WD-CSS21-20050613/visudet.html#abs-replaced-height>
    // (block-style-comments in this function correspond to text from the spec and
    // the numbers correspond to numbers in spec)

    // We don't use containingBlock(), since we may be positioned by an enclosing relpositioned inline.
    const LayoutBoxModelObject* containerBlock = toLayoutBoxModelObject(container());

    const LayoutUnit containerLogicalHeight = containingBlockLogicalHeightForPositioned(containerBlock);
    const LayoutUnit containerRelativeLogicalWidth = containingBlockLogicalWidthForPositioned(containerBlock, false);

    // Variables to solve.
    Length marginBefore = style()->marginBefore();
    Length marginAfter = style()->marginAfter();
    LayoutUnit& marginBeforeAlias = computedValues.m_margins.m_before;
    LayoutUnit& marginAfterAlias = computedValues.m_margins.m_after;

    Length logicalTop = style()->logicalTop();
    Length logicalBottom = style()->logicalBottom();

    /*-----------------------------------------------------------------------*\
     * 1. The used value of 'height' is determined as for inline replaced
     *    elements.
    \*-----------------------------------------------------------------------*/
    // NOTE: This value of height is final in that the min/max height calculations
    // are dealt with in computeReplacedHeight().  This means that the steps to produce
    // correct max/min in the non-replaced version, are not necessary.
    computedValues.m_extent = computeReplacedLogicalHeight() + borderAndPaddingLogicalHeight();
    const LayoutUnit availableSpace = containerLogicalHeight - computedValues.m_extent;

    /*-----------------------------------------------------------------------*\
     * 2. If both 'top' and 'bottom' have the value 'auto', replace 'top'
     *    with the element's static position.
    \*-----------------------------------------------------------------------*/
    // see FIXME 1
    computeBlockStaticDistance(logicalTop, logicalBottom, this, containerBlock);

    /*-----------------------------------------------------------------------*\
     * 3. If 'bottom' is 'auto', replace any 'auto' on 'margin-top' or
     *    'margin-bottom' with '0'.
    \*-----------------------------------------------------------------------*/
    // FIXME: The spec. says that this step should only be taken when bottom is
    // auto, but if only top is auto, this makes step 4 impossible.
    if (logicalTop.isAuto() || logicalBottom.isAuto()) {
        if (marginBefore.isAuto())
            marginBefore.setValue(Fixed, 0);
        if (marginAfter.isAuto())
            marginAfter.setValue(Fixed, 0);
    }

    /*-----------------------------------------------------------------------*\
     * 4. If at this point both 'margin-top' and 'margin-bottom' are still
     *    'auto', solve the equation under the extra constraint that the two
     *    margins must get equal values.
    \*-----------------------------------------------------------------------*/
    LayoutUnit logicalTopValue;
    LayoutUnit logicalBottomValue;

    if (marginBefore.isAuto() && marginAfter.isAuto()) {
        // 'top' and 'bottom' cannot be 'auto' due to step 2 and 3 combined.
        ASSERT(!(logicalTop.isAuto() || logicalBottom.isAuto()));

        logicalTopValue = valueForLength(logicalTop, containerLogicalHeight);
        logicalBottomValue = valueForLength(logicalBottom, containerLogicalHeight);

        LayoutUnit difference = availableSpace - (logicalTopValue + logicalBottomValue);
        // NOTE: This may result in negative values.
        marginBeforeAlias =  difference / 2; // split the difference
        marginAfterAlias = difference - marginBeforeAlias; // account for odd valued differences

    /*-----------------------------------------------------------------------*\
     * 5. If at this point there is only one 'auto' left, solve the equation
     *    for that value.
    \*-----------------------------------------------------------------------*/
    } else if (logicalTop.isAuto()) {
        marginBeforeAlias = valueForLength(marginBefore, containerRelativeLogicalWidth);
        marginAfterAlias = valueForLength(marginAfter, containerRelativeLogicalWidth);
        logicalBottomValue = valueForLength(logicalBottom, containerLogicalHeight);

        // Solve for 'top'
        logicalTopValue = availableSpace - (logicalBottomValue + marginBeforeAlias + marginAfterAlias);
    } else if (logicalBottom.isAuto()) {
        marginBeforeAlias = valueForLength(marginBefore, containerRelativeLogicalWidth);
        marginAfterAlias = valueForLength(marginAfter, containerRelativeLogicalWidth);
        logicalTopValue = valueForLength(logicalTop, containerLogicalHeight);

        // Solve for 'bottom'
        // NOTE: It is not necessary to solve for 'bottom' because we don't ever
        // use the value.
    } else if (marginBefore.isAuto()) {
        marginAfterAlias = valueForLength(marginAfter, containerRelativeLogicalWidth);
        logicalTopValue = valueForLength(logicalTop, containerLogicalHeight);
        logicalBottomValue = valueForLength(logicalBottom, containerLogicalHeight);

        // Solve for 'margin-top'
        marginBeforeAlias = availableSpace - (logicalTopValue + logicalBottomValue + marginAfterAlias);
    } else if (marginAfter.isAuto()) {
        marginBeforeAlias = valueForLength(marginBefore, containerRelativeLogicalWidth);
        logicalTopValue = valueForLength(logicalTop, containerLogicalHeight);
        logicalBottomValue = valueForLength(logicalBottom, containerLogicalHeight);

        // Solve for 'margin-bottom'
        marginAfterAlias = availableSpace - (logicalTopValue + logicalBottomValue + marginBeforeAlias);
    } else {
        // Nothing is 'auto', just calculate the values.
        marginBeforeAlias = valueForLength(marginBefore, containerRelativeLogicalWidth);
        marginAfterAlias = valueForLength(marginAfter, containerRelativeLogicalWidth);
        logicalTopValue = valueForLength(logicalTop, containerLogicalHeight);
        // NOTE: It is not necessary to solve for 'bottom' because we don't ever
        // use the value.
    }

    /*-----------------------------------------------------------------------*\
     * 6. If at this point the values are over-constrained, ignore the value
     *    for 'bottom' and solve for that value.
    \*-----------------------------------------------------------------------*/
    // NOTE: It is not necessary to do this step because we don't end up using
    // the value of 'bottom' regardless of whether the values are over-constrained
    // or not.

    // Use computed values to calculate the vertical position.
    LayoutUnit logicalTopPos = logicalTopValue + marginBeforeAlias;
    computeLogicalTopPositionedOffset(logicalTopPos, this, computedValues.m_extent, containerBlock, containerLogicalHeight);
    computedValues.m_position = logicalTopPos;
}

LayoutRect LayoutReplaced::replacedContentRect(const LayoutSize* overriddenIntrinsicSize) const
{
    LayoutRect contentRect = contentBoxRect();
    ObjectFit objectFit = style()->getObjectFit();

    if (objectFit == ObjectFitFill && style()->objectPosition() == ComputedStyle::initialObjectPosition()) {
        return contentRect;
    }

    // TODO(davve): intrinsicSize doubles as both intrinsic size and intrinsic ratio. In the case of
    // SVG images this isn't correct since they can have intrinsic ratio but no intrinsic size. In
    // order to maintain aspect ratio, the intrinsic size for SVG might be faked from the aspect
    // ratio, see SVGImage::containerSize().
    LayoutSize intrinsicSize = overriddenIntrinsicSize ? *overriddenIntrinsicSize : this->intrinsicSize();
    if (!intrinsicSize.width() || !intrinsicSize.height())
        return contentRect;

    LayoutRect finalRect = contentRect;
    switch (objectFit) {
    case ObjectFitContain:
    case ObjectFitScaleDown:
    case ObjectFitCover:
        finalRect.setSize(finalRect.size().fitToAspectRatio(intrinsicSize, objectFit == ObjectFitCover ? AspectRatioFitGrow : AspectRatioFitShrink));
        if (objectFit != ObjectFitScaleDown || finalRect.width() <= intrinsicSize.width())
            break;
        // fall through
    case ObjectFitNone:
        finalRect.setSize(intrinsicSize);
        break;
    case ObjectFitFill:
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    LayoutUnit xOffset = minimumValueForLength(style()->objectPosition().x(), contentRect.width() - finalRect.width());
    LayoutUnit yOffset = minimumValueForLength(style()->objectPosition().y(), contentRect.height() - finalRect.height());
    finalRect.move(xOffset, yOffset);

    return finalRect;
}

void LayoutReplaced::computeIntrinsicSizingInfo(IntrinsicSizingInfo& intrinsicSizingInfo) const
{
    // If there's an embeddedReplacedContent() of a remote, referenced document available, this code-path should never be used.
    ASSERT(!embeddedReplacedContent());
    intrinsicSizingInfo.size = FloatSize(intrinsicLogicalWidth().toFloat(), intrinsicLogicalHeight().toFloat());

    // Figure out if we need to compute an intrinsic ratio.
    if (intrinsicSizingInfo.size.isEmpty() || !layoutObjectHasAspectRatio(this))
        return;

    intrinsicSizingInfo.aspectRatio = intrinsicSizingInfo.size;
}

static inline LayoutUnit resolveWidthForRatio(LayoutUnit height, const FloatSize& aspectRatio)
{
    return LayoutUnit(height * aspectRatio.width() / aspectRatio.height());
}

static inline LayoutUnit resolveHeightForRatio(LayoutUnit width, const FloatSize& aspectRatio)
{
    return LayoutUnit(width * aspectRatio.height() / aspectRatio.width());
}

LayoutUnit LayoutReplaced::computeConstrainedLogicalWidth(ShouldComputePreferred shouldComputePreferred) const
{
    if (shouldComputePreferred == ComputePreferred)
        return computeReplacedLogicalWidthRespectingMinMaxWidth(LayoutUnit(), ComputePreferred);
    // The aforementioned 'constraint equation' used for block-level, non-replaced elements in normal flow:
    // 'margin-left' + 'border-left-width' + 'padding-left' + 'width' + 'padding-right' + 'border-right-width' + 'margin-right' = width of containing block
    LayoutUnit logicalWidth = containingBlock()->availableLogicalWidth();

    // This solves above equation for 'width' (== logicalWidth).
    LayoutUnit marginStart = minimumValueForLength(style()->marginStart(), logicalWidth);
    LayoutUnit marginEnd = minimumValueForLength(style()->marginEnd(), logicalWidth);
    logicalWidth = (logicalWidth - (marginStart + marginEnd + (size().width() - clientWidth()))).clampNegativeToZero();
    return computeReplacedLogicalWidthRespectingMinMaxWidth(logicalWidth, shouldComputePreferred);
}

LayoutUnit LayoutReplaced::computeReplacedLogicalWidth(ShouldComputePreferred shouldComputePreferred) const
{
    if (style()->logicalWidth().isSpecified() || style()->logicalWidth().isIntrinsic())
        return computeReplacedLogicalWidthRespectingMinMaxWidth(computeReplacedLogicalWidthUsing(MainOrPreferredSize, style()->logicalWidth()), shouldComputePreferred);

    LayoutReplaced* contentLayoutObject = embeddedReplacedContent();

    // 10.3.2 Inline, replaced elements: http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width
    IntrinsicSizingInfo intrinsicSizingInfo;
    computeIntrinsicSizingInfoForReplacedContent(contentLayoutObject, intrinsicSizingInfo);
    FloatSize constrainedSize = constrainIntrinsicSizeToMinMax(intrinsicSizingInfo);

    if (style()->logicalWidth().isAuto()) {
        bool computedHeightIsAuto = style()->logicalHeight().isAuto();

        // If 'height' and 'width' both have computed values of 'auto' and the element also has an intrinsic width, then that intrinsic width is the used value of 'width'.
        if (computedHeightIsAuto && intrinsicSizingInfo.hasWidth)
            return computeReplacedLogicalWidthRespectingMinMaxWidth(LayoutUnit(constrainedSize.width()), shouldComputePreferred);

        if (!intrinsicSizingInfo.aspectRatio.isEmpty()) {
            // If 'height' and 'width' both have computed values of 'auto' and the element has no intrinsic width, but does have an intrinsic height and intrinsic ratio;
            // or if 'width' has a computed value of 'auto', 'height' has some other computed value, and the element does have an intrinsic ratio; then the used value
            // of 'width' is: (used height) * (intrinsic ratio)
            if ((computedHeightIsAuto && !intrinsicSizingInfo.hasWidth && intrinsicSizingInfo.hasHeight) || !computedHeightIsAuto) {
                LayoutUnit estimatedUsedWidth = intrinsicSizingInfo.hasWidth ? LayoutUnit(constrainedSize.width()) : computeConstrainedLogicalWidth(shouldComputePreferred);
                LayoutUnit logicalHeight = computeReplacedLogicalHeight(estimatedUsedWidth);
                return computeReplacedLogicalWidthRespectingMinMaxWidth(resolveWidthForRatio(logicalHeight, intrinsicSizingInfo.aspectRatio), shouldComputePreferred);
            }

            // If 'height' and 'width' both have computed values of 'auto' and the element has an intrinsic ratio but no intrinsic height or width, then the used value of
            // 'width' is undefined in CSS 2.1. However, it is suggested that, if the containing block's width does not itself depend on the replaced element's width, then
            // the used value of 'width' is calculated from the constraint equation used for block-level, non-replaced elements in normal flow.
            if (computedHeightIsAuto && !intrinsicSizingInfo.hasWidth && !intrinsicSizingInfo.hasHeight)
                return computeConstrainedLogicalWidth(shouldComputePreferred);
        }

        // Otherwise, if 'width' has a computed value of 'auto', and the element has an intrinsic width, then that intrinsic width is the used value of 'width'.
        if (intrinsicSizingInfo.hasWidth)
            return computeReplacedLogicalWidthRespectingMinMaxWidth(LayoutUnit(constrainedSize.width()), shouldComputePreferred);

        // Otherwise, if 'width' has a computed value of 'auto', but none of the conditions above are met, then the used value of 'width' becomes 300px. If 300px is too
        // wide to fit the device, UAs should use the width of the largest rectangle that has a 2:1 ratio and fits the device instead.
        // Note: We fall through and instead return intrinsicLogicalWidth() here - to preserve existing WebKit behavior, which might or might not be correct, or desired.
        // Changing this to return cDefaultWidth, will affect lots of test results. Eg. some tests assume that a blank <img> tag (which implies width/height=auto)
        // has no intrinsic size, which is wrong per CSS 2.1, but matches our behavior since a long time.
    }

    return computeReplacedLogicalWidthRespectingMinMaxWidth(intrinsicLogicalWidth(), shouldComputePreferred);
}

LayoutUnit LayoutReplaced::computeReplacedLogicalHeight(LayoutUnit estimatedUsedWidth) const
{
    // 10.5 Content height: the 'height' property: http://www.w3.org/TR/CSS21/visudet.html#propdef-height
    if (hasReplacedLogicalHeight())
        return computeReplacedLogicalHeightRespectingMinMaxHeight(computeReplacedLogicalHeightUsing(MainOrPreferredSize, style()->logicalHeight()));

    LayoutReplaced* contentLayoutObject = embeddedReplacedContent();

    // 10.6.2 Inline, replaced elements: http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-height
    IntrinsicSizingInfo intrinsicSizingInfo;
    computeIntrinsicSizingInfoForReplacedContent(contentLayoutObject, intrinsicSizingInfo);
    FloatSize constrainedSize = constrainIntrinsicSizeToMinMax(intrinsicSizingInfo);

    bool widthIsAuto = style()->logicalWidth().isAuto();

    // If 'height' and 'width' both have computed values of 'auto' and the element also has an intrinsic height, then that intrinsic height is the used value of 'height'.
    if (widthIsAuto && intrinsicSizingInfo.hasHeight)
        return computeReplacedLogicalHeightRespectingMinMaxHeight(LayoutUnit(constrainedSize.height()));

    // Otherwise, if 'height' has a computed value of 'auto', and the element has an intrinsic ratio then the used value of 'height' is:
    // (used width) / (intrinsic ratio)
    if (!intrinsicSizingInfo.aspectRatio.isEmpty()) {
        LayoutUnit usedWidth = estimatedUsedWidth ? estimatedUsedWidth : availableLogicalWidth();
        return computeReplacedLogicalHeightRespectingMinMaxHeight(resolveHeightForRatio(usedWidth, intrinsicSizingInfo.aspectRatio));
    }

    // Otherwise, if 'height' has a computed value of 'auto', and the element has an intrinsic height, then that intrinsic height is the used value of 'height'.
    if (intrinsicSizingInfo.hasHeight)
        return computeReplacedLogicalHeightRespectingMinMaxHeight(LayoutUnit(constrainedSize.height()));

    // Otherwise, if 'height' has a computed value of 'auto', but none of the conditions above are met, then the used value of 'height' must be set to the height
    // of the largest rectangle that has a 2:1 ratio, has a height not greater than 150px, and has a width not greater than the device width.
    return computeReplacedLogicalHeightRespectingMinMaxHeight(intrinsicLogicalHeight());
}

void LayoutReplaced::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    minLogicalWidth = maxLogicalWidth = intrinsicLogicalWidth();
}

void LayoutReplaced::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    // We cannot resolve some logical width here (i.e. percent, fill-available or fit-content)
    // as the available logical width may not be set on our containing block.
    const Length& logicalWidth = style()->logicalWidth();
    if (logicalWidth.hasPercent() || logicalWidth.isFillAvailable() || logicalWidth.isFitContent())
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);
    else
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = computeReplacedLogicalWidth(ComputePreferred);

    const ComputedStyle& styleToUse = styleRef();
    if (styleToUse.logicalWidth().hasPercent() || styleToUse.logicalMaxWidth().hasPercent())
        m_minPreferredLogicalWidth = LayoutUnit();

    if (styleToUse.logicalMinWidth().isFixed() && styleToUse.logicalMinWidth().value() > 0) {
        m_maxPreferredLogicalWidth = std::max(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMinWidth().value()));
        m_minPreferredLogicalWidth = std::max(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMinWidth().value()));
    }

    if (styleToUse.logicalMaxWidth().isFixed()) {
        m_maxPreferredLogicalWidth = std::min(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMaxWidth().value()));
        m_minPreferredLogicalWidth = std::min(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMaxWidth().value()));
    }

    LayoutUnit borderAndPadding = borderAndPaddingLogicalWidth();
    m_minPreferredLogicalWidth += borderAndPadding;
    m_maxPreferredLogicalWidth += borderAndPadding;

    clearPreferredLogicalWidthsDirty();
}

PositionWithAffinity LayoutReplaced::positionForPoint(const LayoutPoint& point)
{
    // FIXME: This code is buggy if the replaced element is relative positioned.
    InlineBox* box = inlineBoxWrapper();
    RootInlineBox* rootBox = box ? &box->root() : 0;

    LayoutUnit top = rootBox ? rootBox->selectionTop() : logicalTop();
    LayoutUnit bottom = rootBox ? rootBox->selectionBottom() : logicalBottom();

    LayoutUnit blockDirectionPosition = isHorizontalWritingMode() ? point.y() + location().y() : point.x() + location().x();
    LayoutUnit lineDirectionPosition = isHorizontalWritingMode() ? point.x() + location().x() : point.y() + location().y();

    if (blockDirectionPosition < top)
        return createPositionWithAffinity(caretMinOffset()); // coordinates are above

    if (blockDirectionPosition >= bottom)
        return createPositionWithAffinity(caretMaxOffset()); // coordinates are below

    if (node()) {
        if (lineDirectionPosition <= logicalLeft() + (logicalWidth() / 2))
            return createPositionWithAffinity(0);
        return createPositionWithAffinity(1);
    }

    return LayoutBox::positionForPoint(point);
}

LayoutRect LayoutReplaced::localSelectionRect() const
{
    if (getSelectionState() == SelectionNone)
        return LayoutRect();

    if (!inlineBoxWrapper()) {
        // We're a block-level replaced element.  Just return our own dimensions.
        return LayoutRect(LayoutPoint(), size());
    }

    RootInlineBox& root = inlineBoxWrapper()->root();
    LayoutUnit newLogicalTop = root.block().style()->isFlippedBlocksWritingMode() ? inlineBoxWrapper()->logicalBottom() - root.selectionBottom() : root.selectionTop() - inlineBoxWrapper()->logicalTop();
    if (root.block().style()->isHorizontalWritingMode())
        return LayoutRect(LayoutUnit(), newLogicalTop, size().width(), root.selectionHeight());
    return LayoutRect(newLogicalTop, LayoutUnit(), root.selectionHeight(), size().height());
}

void LayoutReplaced::setSelectionState(SelectionState state)
{
    // The selection state for our containing block hierarchy is updated by the base class call.
    LayoutBox::setSelectionState(state);

    if (!inlineBoxWrapper())
        return;

    // We only include the space below the baseline in our layer's cached paint invalidation rect if the
    // image is selected. Since the selection state has changed update the rect.
    if (hasLayer()) {
        LayoutRect rect = localOverflowRectForPaintInvalidation();
        PaintLayer::mapRectToPaintInvalidationBacking(*this, containerForPaintInvalidation(), rect);
        setPreviousPaintInvalidationRect(rect);
    }

    if (canUpdateSelectionOnRootLineBoxes())
        inlineBoxWrapper()->root().setHasSelectedChildren(state != SelectionNone);
}

void LayoutReplaced::IntrinsicSizingInfo::transpose()
{
    size = size.transposedSize();
    aspectRatio = aspectRatio.transposedSize();
    std::swap(hasWidth, hasHeight);
}

} // namespace blink
