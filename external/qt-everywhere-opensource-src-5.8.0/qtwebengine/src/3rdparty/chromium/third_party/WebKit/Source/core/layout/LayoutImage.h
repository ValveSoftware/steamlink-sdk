/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2006 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef LayoutImage_h
#define LayoutImage_h

#include "core/CoreExport.h"
#include "core/fetch/ResourceClient.h"
#include "core/layout/LayoutImageResource.h"
#include "core/layout/LayoutReplaced.h"

namespace blink {

class HTMLAreaElement;
class HTMLMapElement;

// LayoutImage is used to display any image type.
//
// There is 2 types of images:
// * normal images, e.g. <image>, <picture>.
// * content images with "content: url(path/to/image.png)".
// We store the type inside  m_isGeneratedContent.
//
// The class is image type agnostic as it only manipulates decoded images.
// See LayoutImageResource that holds this image.
class CORE_EXPORT LayoutImage : public LayoutReplaced {
public:
    // These are the paddings to use when displaying either alt text or an image.
    static const unsigned short paddingWidth = 4;
    static const unsigned short paddingHeight = 4;

    LayoutImage(Element*);
    ~LayoutImage() override;

    static LayoutImage* createAnonymous(Document*);

    void setImageResource(LayoutImageResource*);

    LayoutImageResource* imageResource() { return m_imageResource.get(); }
    const LayoutImageResource* imageResource() const { return m_imageResource.get(); }
    ImageResource* cachedImage() const { return m_imageResource ? m_imageResource->cachedImage() : 0; }

    HTMLMapElement* imageMap() const;
    void areaElementFocusChanged(HTMLAreaElement*);

    void setIsGeneratedContent(bool generated = true) { m_isGeneratedContent = generated; }

    bool isGeneratedContent() const { return m_isGeneratedContent; }

    inline void setImageDevicePixelRatio(float factor) { m_imageDevicePixelRatio = factor; }
    float imageDevicePixelRatio() const { return m_imageDevicePixelRatio; }

    void intrinsicSizeChanged() override
    {
        if (m_imageResource)
            imageChanged(m_imageResource->imagePtr());
    }

    const char* name() const override { return "LayoutImage"; }

protected:
    bool needsPreferredWidthsRecalculation() const final;
    LayoutReplaced* embeddedReplacedContent() const final;
    void computeIntrinsicSizingInfo(IntrinsicSizingInfo&) const final;

    void imageChanged(WrappedImagePtr, const IntRect* = nullptr) override;

    void paint(const PaintInfo&, const LayoutPoint&) const final;

    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectLayoutImage || LayoutReplaced::isOfType(type); }

    void willBeDestroyed() override;

    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;

private:
    bool isImage() const override { return true; }

    void paintReplaced(const PaintInfo&, const LayoutPoint&) const override;

    bool foregroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect, unsigned maxDepthToTest) const final;
    bool computeBackgroundIsKnownToBeObscured() const final;

    bool backgroundShouldAlwaysBeClipped() const override { return true; }

    LayoutUnit minimumReplacedHeight() const override;

    void imageNotifyFinished(ImageResource*) final;
    bool nodeAtPoint(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) final;

    bool boxShadowShouldBeAppliedToBackground(BackgroundBleedAvoidance, const InlineFlowBox*) const final;

    void invalidatePaintAndMarkForLayoutIfNeeded();
    void updateIntrinsicSizeIfNeeded(const LayoutSize&);

    // This member wraps the associated decoded image.
    //
    // This field is set using setImageResource above which can be called in
    // several ways:
    // * For normal images, from the network stack (ImageLoader) once we have
    // some image data.
    // * For generated content, the resource is loaded during style resolution
    // and thus is stored in ComputedStyle (see ContentData::image) that gets
    // propagated to the anonymous LayoutImage in LayoutObject::createObject.
    Persistent<LayoutImageResource> m_imageResource;
    bool m_didIncrementVisuallyNonEmptyPixelCount;

    // This field stores whether this image is generated with 'content'.
    bool m_isGeneratedContent;
    float m_imageDevicePixelRatio;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutImage, isLayoutImage());

} // namespace blink

#endif // LayoutImage_h
