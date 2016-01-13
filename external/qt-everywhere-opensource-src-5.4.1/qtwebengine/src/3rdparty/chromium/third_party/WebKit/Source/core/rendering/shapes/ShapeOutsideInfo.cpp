/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "core/rendering/shapes/ShapeOutsideInfo.h"

#include "core/rendering/FloatingObjects.h"
#include "core/rendering/RenderBlockFlow.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderImage.h"
#include "platform/LengthFunctions.h"

namespace WebCore {

CSSBoxType referenceBox(const ShapeValue& shapeValue)
{
    if (shapeValue.cssBox() == BoxMissing)
        return MarginBox;
    return shapeValue.cssBox();
}

void ShapeOutsideInfo::setReferenceBoxLogicalSize(LayoutSize newReferenceBoxLogicalSize)
{
    bool isHorizontalWritingMode = m_renderer.containingBlock()->style()->isHorizontalWritingMode();
    switch (referenceBox(*m_renderer.style()->shapeOutside())) {
    case MarginBox:
        if (isHorizontalWritingMode)
            newReferenceBoxLogicalSize.expand(m_renderer.marginWidth(), m_renderer.marginHeight());
        else
            newReferenceBoxLogicalSize.expand(m_renderer.marginHeight(), m_renderer.marginWidth());
        break;
    case BorderBox:
        break;
    case PaddingBox:
        if (isHorizontalWritingMode)
            newReferenceBoxLogicalSize.shrink(m_renderer.borderWidth(), m_renderer.borderHeight());
        else
            newReferenceBoxLogicalSize.shrink(m_renderer.borderHeight(), m_renderer.borderWidth());
        break;
    case ContentBox:
        if (isHorizontalWritingMode)
            newReferenceBoxLogicalSize.shrink(m_renderer.borderAndPaddingWidth(), m_renderer.borderAndPaddingHeight());
        else
            newReferenceBoxLogicalSize.shrink(m_renderer.borderAndPaddingHeight(), m_renderer.borderAndPaddingWidth());
        break;
    case BoxMissing:
        ASSERT_NOT_REACHED();
        break;
    }

    if (m_referenceBoxLogicalSize == newReferenceBoxLogicalSize)
        return;
    markShapeAsDirty();
    m_referenceBoxLogicalSize = newReferenceBoxLogicalSize;
}

static bool checkShapeImageOrigin(Document& document, const StyleImage& styleImage)
{
    if (styleImage.isGeneratedImage())
        return true;

    ASSERT(styleImage.cachedImage());
    ImageResource& imageResource = *(styleImage.cachedImage());
    if (imageResource.isAccessAllowed(document.securityOrigin()))
        return true;

    const KURL& url = imageResource.url();
    String urlString = url.isNull() ? "''" : url.elidedString();
    document.addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, "Unsafe attempt to load URL " + urlString + ".");

    return false;
}

static LayoutRect getShapeImageMarginRect(const RenderBox& renderBox, const LayoutSize& referenceBoxLogicalSize)
{
    LayoutPoint marginBoxOrigin(-renderBox.marginLogicalLeft() - renderBox.borderAndPaddingLogicalLeft(), -renderBox.marginBefore() - renderBox.borderBefore() - renderBox.paddingBefore());
    LayoutSize marginBoxSizeDelta(renderBox.marginLogicalWidth() + renderBox.borderAndPaddingLogicalWidth(), renderBox.marginLogicalHeight() + renderBox.borderAndPaddingLogicalHeight());
    return LayoutRect(marginBoxOrigin, referenceBoxLogicalSize + marginBoxSizeDelta);
}

PassOwnPtr<Shape> ShapeOutsideInfo::createShapeForImage(StyleImage* styleImage, float shapeImageThreshold, WritingMode writingMode, float margin) const
{
    const IntSize& imageSize = m_renderer.calculateImageIntrinsicDimensions(styleImage, roundedIntSize(m_referenceBoxLogicalSize), RenderImage::ScaleByEffectiveZoom);
    styleImage->setContainerSizeForRenderer(&m_renderer, imageSize, m_renderer.style()->effectiveZoom());

    const LayoutRect& marginRect = getShapeImageMarginRect(m_renderer, m_referenceBoxLogicalSize);
    const LayoutRect& imageRect = (m_renderer.isRenderImage())
        ? toRenderImage(&m_renderer)->replacedContentRect()
        : LayoutRect(LayoutPoint(), imageSize);

    ASSERT(!styleImage->isPendingImage());
    RefPtr<Image> image = styleImage->image(const_cast<RenderBox*>(&m_renderer), imageSize);

    return Shape::createRasterShape(image.get(), shapeImageThreshold, imageRect, marginRect, writingMode, margin);
}

const Shape& ShapeOutsideInfo::computedShape() const
{
    if (Shape* shape = m_shape.get())
        return *shape;

    const RenderStyle& style = *m_renderer.style();
    ASSERT(m_renderer.containingBlock());
    const RenderStyle& containingBlockStyle = *m_renderer.containingBlock()->style();

    WritingMode writingMode = containingBlockStyle.writingMode();
    LayoutUnit maximumValue = m_renderer.containingBlock() ? m_renderer.containingBlock()->contentWidth() : LayoutUnit();
    float margin = floatValueForLength(m_renderer.style()->shapeMargin(), maximumValue.toFloat());

    float shapeImageThreshold = style.shapeImageThreshold();
    ASSERT(style.shapeOutside());
    const ShapeValue& shapeValue = *style.shapeOutside();

    switch (shapeValue.type()) {
    case ShapeValue::Shape:
        ASSERT(shapeValue.shape());
        m_shape = Shape::createShape(shapeValue.shape(), m_referenceBoxLogicalSize, writingMode, margin);
        break;
    case ShapeValue::Image:
        ASSERT(shapeValue.isImageValid());
        m_shape = createShapeForImage(shapeValue.image(), shapeImageThreshold, writingMode, margin);
        break;
    case ShapeValue::Box: {
        const RoundedRect& shapeRect = style.getRoundedBorderFor(LayoutRect(LayoutPoint(), m_referenceBoxLogicalSize), m_renderer.view());
        m_shape = Shape::createLayoutBoxShape(shapeRect, writingMode, margin);
        break;
    }
    }

    ASSERT(m_shape);
    return *m_shape;
}

SegmentList ShapeOutsideInfo::computeSegmentsForLine(LayoutUnit lineTop, LayoutUnit lineHeight) const
{
    ASSERT(lineHeight >= 0);
    SegmentList segments;

    computedShape().getExcludedIntervals((lineTop - logicalTopOffset()), std::min(lineHeight, shapeLogicalBottom() - lineTop), segments);

    for (size_t i = 0; i < segments.size(); i++) {
        segments[i].logicalLeft += logicalLeftOffset();
        segments[i].logicalRight += logicalLeftOffset();
    }

    return segments;
}

inline LayoutUnit borderBeforeInWritingMode(const RenderBox& renderer, WritingMode writingMode)
{
    switch (writingMode) {
    case TopToBottomWritingMode: return renderer.borderTop();
    case BottomToTopWritingMode: return renderer.borderBottom();
    case LeftToRightWritingMode: return renderer.borderLeft();
    case RightToLeftWritingMode: return renderer.borderRight();
    }

    ASSERT_NOT_REACHED();
    return renderer.borderBefore();
}

inline LayoutUnit borderAndPaddingBeforeInWritingMode(const RenderBox& renderer, WritingMode writingMode)
{
    switch (writingMode) {
    case TopToBottomWritingMode: return renderer.borderTop() + renderer.paddingTop();
    case BottomToTopWritingMode: return renderer.borderBottom() + renderer.paddingBottom();
    case LeftToRightWritingMode: return renderer.borderLeft() + renderer.paddingLeft();
    case RightToLeftWritingMode: return renderer.borderRight() + renderer.paddingRight();
    }

    ASSERT_NOT_REACHED();
    return renderer.borderAndPaddingBefore();
}

LayoutUnit ShapeOutsideInfo::logicalTopOffset() const
{
    switch (referenceBox(*m_renderer.style()->shapeOutside())) {
    case MarginBox: return -m_renderer.marginBefore(m_renderer.containingBlock()->style());
    case BorderBox: return LayoutUnit();
    case PaddingBox: return borderBeforeInWritingMode(m_renderer, m_renderer.containingBlock()->style()->writingMode());
    case ContentBox: return borderAndPaddingBeforeInWritingMode(m_renderer, m_renderer.containingBlock()->style()->writingMode());
    case BoxMissing: break;
    }

    ASSERT_NOT_REACHED();
    return LayoutUnit();
}

inline LayoutUnit borderStartWithStyleForWritingMode(const RenderBox& renderer, const RenderStyle* style)
{
    if (style->isHorizontalWritingMode()) {
        if (style->isLeftToRightDirection())
            return renderer.borderLeft();

        return renderer.borderRight();
    }
    if (style->isLeftToRightDirection())
        return renderer.borderTop();

    return renderer.borderBottom();
}

inline LayoutUnit borderAndPaddingStartWithStyleForWritingMode(const RenderBox& renderer, const RenderStyle* style)
{
    if (style->isHorizontalWritingMode()) {
        if (style->isLeftToRightDirection())
            return renderer.borderLeft() + renderer.paddingLeft();

        return renderer.borderRight() + renderer.paddingRight();
    }
    if (style->isLeftToRightDirection())
        return renderer.borderTop() + renderer.paddingTop();

    return renderer.borderBottom() + renderer.paddingBottom();
}

LayoutUnit ShapeOutsideInfo::logicalLeftOffset() const
{
    switch (referenceBox(*m_renderer.style()->shapeOutside())) {
    case MarginBox: return -m_renderer.marginStart(m_renderer.containingBlock()->style());
    case BorderBox: return LayoutUnit();
    case PaddingBox: return borderStartWithStyleForWritingMode(m_renderer, m_renderer.containingBlock()->style());
    case ContentBox: return borderAndPaddingStartWithStyleForWritingMode(m_renderer, m_renderer.containingBlock()->style());
    case BoxMissing: break;
    }

    ASSERT_NOT_REACHED();
    return LayoutUnit();
}


bool ShapeOutsideInfo::isEnabledFor(const RenderBox& box)
{
    ShapeValue* shapeValue = box.style()->shapeOutside();
    if (!box.isFloating() || !shapeValue)
        return false;

    switch (shapeValue->type()) {
    case ShapeValue::Shape:
        return shapeValue->shape();
    case ShapeValue::Image:
        return shapeValue->isImageValid() && checkShapeImageOrigin(box.document(), *(shapeValue->image()));
    case ShapeValue::Box:
        return true;
    }

    return false;
}

void ShapeOutsideInfo::updateDeltasForContainingBlockLine(const RenderBlockFlow& containingBlock, const FloatingObject& floatingObject, LayoutUnit lineTop, LayoutUnit lineHeight)
{
    LayoutUnit borderBoxTop = containingBlock.logicalTopForFloat(&floatingObject) + containingBlock.marginBeforeForChild(&m_renderer);
    LayoutUnit borderBoxLineTop = lineTop - borderBoxTop;

    if (isShapeDirty() || m_borderBoxLineTop != borderBoxLineTop || m_lineHeight != lineHeight) {
        m_borderBoxLineTop = borderBoxLineTop;
        m_referenceBoxLineTop = borderBoxLineTop - logicalTopOffset();
        m_lineHeight = lineHeight;

        LayoutUnit floatMarginBoxWidth = containingBlock.logicalWidthForFloat(&floatingObject);

        if (lineOverlapsShapeBounds()) {
            SegmentList segments = computeSegmentsForLine(borderBoxLineTop, lineHeight);
            if (segments.size()) {
                LayoutUnit logicalLeftMargin = containingBlock.style()->isLeftToRightDirection() ? containingBlock.marginStartForChild(&m_renderer) : containingBlock.marginEndForChild(&m_renderer);
                LayoutUnit rawLeftMarginBoxDelta = segments.first().logicalLeft + logicalLeftMargin;
                m_leftMarginBoxDelta = clampToLayoutUnit(rawLeftMarginBoxDelta, LayoutUnit(), floatMarginBoxWidth);

                LayoutUnit logicalRightMargin = containingBlock.style()->isLeftToRightDirection() ? containingBlock.marginEndForChild(&m_renderer) : containingBlock.marginStartForChild(&m_renderer);
                LayoutUnit rawRightMarginBoxDelta = segments.last().logicalRight - containingBlock.logicalWidthForChild(&m_renderer) - logicalRightMargin;
                m_rightMarginBoxDelta = clampToLayoutUnit(rawRightMarginBoxDelta, -floatMarginBoxWidth, LayoutUnit());
                m_lineOverlapsShape = true;
                return;
            }
        }

        // Lines that do not overlap the shape should act as if the float
        // wasn't there for layout purposes. So we set the deltas to remove the
        // entire width of the float.
        m_leftMarginBoxDelta = floatMarginBoxWidth;
        m_rightMarginBoxDelta = -floatMarginBoxWidth;
        m_lineOverlapsShape = false;
    }
}

LayoutRect ShapeOutsideInfo::computedShapePhysicalBoundingBox() const
{
    LayoutRect physicalBoundingBox = computedShape().shapeMarginLogicalBoundingBox();
    physicalBoundingBox.setX(physicalBoundingBox.x() + logicalLeftOffset());

    if (m_renderer.style()->isFlippedBlocksWritingMode())
        physicalBoundingBox.setY(m_renderer.logicalHeight() - physicalBoundingBox.maxY());
    else
        physicalBoundingBox.setY(physicalBoundingBox.y() + logicalTopOffset());

    if (!m_renderer.style()->isHorizontalWritingMode())
        physicalBoundingBox = physicalBoundingBox.transposedRect();
    else
        physicalBoundingBox.setY(physicalBoundingBox.y() + logicalTopOffset());

    return physicalBoundingBox;
}

FloatPoint ShapeOutsideInfo::shapeToRendererPoint(FloatPoint point) const
{
    FloatPoint result = FloatPoint(point.x() + logicalLeftOffset(), point.y() + logicalTopOffset());
    if (m_renderer.style()->isFlippedBlocksWritingMode())
        result.setY(m_renderer.logicalHeight() - result.y());
    if (!m_renderer.style()->isHorizontalWritingMode())
        result = result.transposedPoint();
    return result;
}

FloatSize ShapeOutsideInfo::shapeToRendererSize(FloatSize size) const
{
    if (!m_renderer.style()->isHorizontalWritingMode())
        return size.transposedSize();
    return size;
}

}
