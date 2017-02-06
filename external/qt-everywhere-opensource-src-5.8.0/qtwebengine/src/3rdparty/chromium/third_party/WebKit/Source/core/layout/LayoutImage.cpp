/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "core/layout/LayoutImage.h"

#include "core/HTMLNames.h"
#include "core/fetch/ImageResource.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLAreaElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/layout/HitTestResult.h"
#include "core/paint/ImagePainter.h"
#include "core/svg/graphics/SVGImage.h"

namespace blink {

using namespace HTMLNames;

LayoutImage::LayoutImage(Element* element)
    : LayoutReplaced(element, LayoutSize())
    , m_didIncrementVisuallyNonEmptyPixelCount(false)
    , m_isGeneratedContent(false)
    , m_imageDevicePixelRatio(1.0f)
{
}

LayoutImage* LayoutImage::createAnonymous(Document* document)
{
    LayoutImage* image = new LayoutImage(nullptr);
    image->setDocumentForAnonymous(document);
    return image;
}

LayoutImage::~LayoutImage()
{
}

void LayoutImage::willBeDestroyed()
{
    ASSERT(m_imageResource);
    m_imageResource->shutdown();
    LayoutReplaced::willBeDestroyed();
}

void LayoutImage::styleDidChange(StyleDifference diff, const ComputedStyle* oldStyle)
{
    LayoutReplaced::styleDidChange(diff, oldStyle);

    RespectImageOrientationEnum oldOrientation = oldStyle ? oldStyle->respectImageOrientation() : ComputedStyle::initialRespectImageOrientation();
    if (style() && style()->respectImageOrientation() != oldOrientation)
        intrinsicSizeChanged();
}

void LayoutImage::setImageResource(LayoutImageResource* imageResource)
{
    ASSERT(!m_imageResource);
    m_imageResource = imageResource;
    m_imageResource->initialize(this);
}

void LayoutImage::imageChanged(WrappedImagePtr newImage, const IntRect* rect)
{
    ASSERT(view());
    ASSERT(view()->frameView());
    if (documentBeingDestroyed())
        return;

    if (hasBoxDecorationBackground() || hasMask() || hasShapeOutside())
        LayoutReplaced::imageChanged(newImage, rect);

    if (!m_imageResource)
        return;

    if (newImage != m_imageResource->imagePtr())
        return;

    if (isGeneratedContent() && isHTMLImageElement(node()) && m_imageResource->errorOccurred())  {
        toHTMLImageElement(node())->ensureFallbackForGeneratedContent();
        return;
    }

    // Per the spec, we let the server-sent header override srcset/other sources of dpr.
    // https://github.com/igrigorik/http-client-hints/blob/master/draft-grigorik-http-client-hints-01.txt#L255
    if (m_imageResource->cachedImage() && m_imageResource->cachedImage()->hasDevicePixelRatioHeaderValue()) {
        UseCounter::count(&(view()->frameView()->frame()), UseCounter::ClientHintsContentDPR);
        m_imageDevicePixelRatio = 1 / m_imageResource->cachedImage()->devicePixelRatioHeaderValue();
    }

    if (!m_didIncrementVisuallyNonEmptyPixelCount) {
        // At a zoom level of 1 the image is guaranteed to have an integer size.
        view()->frameView()->incrementVisuallyNonEmptyPixelCount(flooredIntSize(m_imageResource->imageSize(1.0f)));
        m_didIncrementVisuallyNonEmptyPixelCount = true;
    }

    invalidatePaintAndMarkForLayoutIfNeeded();
}

void LayoutImage::updateIntrinsicSizeIfNeeded(const LayoutSize& newSize)
{
    if (m_imageResource->errorOccurred() || !m_imageResource->hasImage())
        return;
    setIntrinsicSize(newSize);
}

void LayoutImage::invalidatePaintAndMarkForLayoutIfNeeded()
{
    LayoutSize oldIntrinsicSize = intrinsicSize();
    LayoutSize newIntrinsicSize = m_imageResource->imageSize(style()->effectiveZoom());
    updateIntrinsicSizeIfNeeded(newIntrinsicSize);

    // In the case of generated image content using :before/:after/content, we might not be
    // in the layout tree yet. In that case, we just need to update our intrinsic size.
    // layout() will be called after we are inserted in the tree which will take care of
    // what we are doing here.
    if (!containingBlock())
        return;

    bool imageSourceHasChangedSize = oldIntrinsicSize != newIntrinsicSize;
    if (imageSourceHasChangedSize)
        setPreferredLogicalWidthsDirty();

    // If the actual area occupied by the image has changed and it is not constrained by style then a layout is required.
    bool imageSizeIsConstrained = style()->logicalWidth().isSpecified() && style()->logicalHeight().isSpecified();

    // FIXME: We only need to recompute the containing block's preferred size if the containing block's size
    // depends on the image's size (i.e., the container uses shrink-to-fit sizing).
    // There's no easy way to detect that shrink-to-fit is needed, always force a layout.
    bool containingBlockNeedsToRecomputePreferredSize = style()->logicalWidth().hasPercent() || style()->logicalMaxWidth().hasPercent()  || style()->logicalMinWidth().hasPercent();

    if (imageSourceHasChangedSize && (!imageSizeIsConstrained || containingBlockNeedsToRecomputePreferredSize)) {
        setNeedsLayoutAndFullPaintInvalidation(LayoutInvalidationReason::SizeChanged);
        return;
    }

    if (imageResource() && imageResource()->maybeAnimated())
        setShouldDoFullPaintInvalidation(PaintInvalidationDelayedFull);
    else
        setShouldDoFullPaintInvalidation(PaintInvalidationFull);

    // Tell any potential compositing layers that the image needs updating.
    contentChanged(ImageChanged);
}

void LayoutImage::imageNotifyFinished(ImageResource* newImage)
{
    if (!m_imageResource)
        return;

    if (documentBeingDestroyed())
        return;

    invalidateBackgroundObscurationStatus();

    if (newImage == m_imageResource->cachedImage()) {
        // tell any potential compositing layers
        // that the image is done and they can reference it directly.
        contentChanged(ImageChanged);
    }
}

void LayoutImage::paintReplaced(const PaintInfo& paintInfo, const LayoutPoint& paintOffset) const
{
    ImagePainter(*this).paintReplaced(paintInfo, paintOffset);
}

void LayoutImage::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset) const
{
    ImagePainter(*this).paint(paintInfo, paintOffset);
}

void LayoutImage::areaElementFocusChanged(HTMLAreaElement* areaElement)
{
    ASSERT(areaElement->imageElement() == node());

    if (areaElement->getPath(this).isEmpty())
        return;

    invalidatePaintAndMarkForLayoutIfNeeded();
}

bool LayoutImage::boxShadowShouldBeAppliedToBackground(BackgroundBleedAvoidance bleedAvoidance, const InlineFlowBox*) const
{
    if (!LayoutBoxModelObject::boxShadowShouldBeAppliedToBackground(bleedAvoidance))
        return false;

    return !const_cast<LayoutImage*>(this)->boxDecorationBackgroundIsKnownToBeObscured();
}

bool LayoutImage::foregroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect, unsigned) const
{
    if (!m_imageResource->hasImage() || m_imageResource->errorOccurred())
        return false;
    if (m_imageResource->cachedImage() && !m_imageResource->cachedImage()->isLoaded())
        return false;
    if (!contentBoxRect().contains(localRect))
        return false;
    EFillBox backgroundClip = style()->backgroundClip();
    // Background paints under borders.
    if (backgroundClip == BorderFillBox && style()->hasBorder() && !style()->borderObscuresBackground())
        return false;
    // Background shows in padding area.
    if ((backgroundClip == BorderFillBox || backgroundClip == PaddingFillBox) && style()->hasPadding())
        return false;
    // Object-position may leave parts of the content box empty, regardless of the value of object-fit.
    if (style()->objectPosition() != ComputedStyle::initialObjectPosition())
        return false;
    // Object-fit may leave parts of the content box empty.
    ObjectFit objectFit = style()->getObjectFit();
    if (objectFit != ObjectFitFill && objectFit != ObjectFitCover)
        return false;
    if (!m_imageResource->cachedImage())
        return false;
    // Check for image with alpha.
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage", "data", InspectorPaintImageEvent::data(this, *m_imageResource->cachedImage()));
    return m_imageResource->cachedImage()->getImage()->currentFrameKnownToBeOpaque(Image::PreCacheMetadata);
}

bool LayoutImage::computeBackgroundIsKnownToBeObscured() const
{
    if (!hasBackground())
        return false;

    LayoutRect paintedExtent;
    if (!getBackgroundPaintedExtent(paintedExtent))
        return false;
    return foregroundIsKnownToBeOpaqueInRect(paintedExtent, 0);
}

LayoutUnit LayoutImage::minimumReplacedHeight() const
{
    return m_imageResource->errorOccurred() ? intrinsicSize().height() : LayoutUnit();
}

HTMLMapElement* LayoutImage::imageMap() const
{
    HTMLImageElement* i = isHTMLImageElement(node()) ? toHTMLImageElement(node()) : 0;
    return i ? i->treeScope().getImageMap(i->fastGetAttribute(usemapAttr)) : 0;
}

bool LayoutImage::nodeAtPoint(HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    HitTestResult tempResult(result.hitTestRequest(), result.hitTestLocation());
    bool inside = LayoutReplaced::nodeAtPoint(tempResult, locationInContainer, accumulatedOffset, hitTestAction);

    if (!inside && result.hitTestRequest().listBased())
        result.append(tempResult);
    if (inside)
        result = tempResult;
    return inside;
}

void LayoutImage::computeIntrinsicSizingInfo(IntrinsicSizingInfo& intrinsicSizingInfo) const
{
    LayoutReplaced::computeIntrinsicSizingInfo(intrinsicSizingInfo);

    // Our intrinsicSize is empty if we're laying out generated images with relative width/height. Figure out the right intrinsic size to use.
    if (intrinsicSizingInfo.size.isEmpty() && m_imageResource->imageHasRelativeSize()) {
        LayoutObject* containingBlock = isOutOfFlowPositioned() ? container() : this->containingBlock();
        if (containingBlock->isBox()) {
            LayoutBox* box = toLayoutBox(containingBlock);
            intrinsicSizingInfo.size.setWidth(box->availableLogicalWidth().toFloat());
            intrinsicSizingInfo.size.setHeight(box->availableLogicalHeight(IncludeMarginBorderPadding).toFloat());
        }
    }
    // Don't compute an intrinsic ratio to preserve historical WebKit behavior if we're painting alt text and/or a broken image.
    // Video is excluded from this behavior because video elements have a default aspect ratio that a failed poster image load should not override.
    if (m_imageResource && m_imageResource->errorOccurred() && !isVideo()) {
        intrinsicSizingInfo.aspectRatio = FloatSize(1, 1);
        return;
    }
}

bool LayoutImage::needsPreferredWidthsRecalculation() const
{
    if (LayoutReplaced::needsPreferredWidthsRecalculation())
        return true;
    return embeddedReplacedContent();
}

LayoutReplaced* LayoutImage::embeddedReplacedContent() const
{
    if (!m_imageResource)
        return nullptr;

    ImageResource* cachedImage = m_imageResource->cachedImage();
    if (cachedImage && cachedImage->getImage() && cachedImage->getImage()->isSVGImage())
        return toSVGImage(cachedImage->getImage())->embeddedReplacedContent();

    return nullptr;
}

} // namespace blink
