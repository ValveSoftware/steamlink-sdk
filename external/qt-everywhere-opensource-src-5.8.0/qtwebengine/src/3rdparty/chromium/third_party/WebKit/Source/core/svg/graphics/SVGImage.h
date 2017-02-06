/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SVGImage_h
#define SVGImage_h

#include "core/CoreExport.h"
#include "platform/graphics/Image.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "wtf/Allocator.h"

namespace blink {

class Document;
class Page;
class LayoutReplaced;
class SVGImageChromeClient;
class SVGImageForContainer;

class CORE_EXPORT SVGImage final : public Image {
public:
    static PassRefPtr<SVGImage> create(ImageObserver* observer)
    {
        return adoptRef(new SVGImage(observer));
    }

    static bool isInSVGImage(const Node*);

    LayoutReplaced* embeddedReplacedContent() const;

    bool isSVGImage() const override { return true; }
    bool isTextureBacked() override { return false; }
    IntSize size() const override { return m_intrinsicSize; }

    bool currentFrameHasSingleSecurityOrigin() const override;

    void startAnimation(CatchUpAnimation = CatchUp) override;
    void resetAnimation() override;

    // Advances an animated image. This will trigger an animation update for CSS
    // and advance the SMIL timeline by one frame.
    void advanceAnimationForTesting() override;
    SVGImageChromeClient& chromeClientForTesting();

    PassRefPtr<SkImage> imageForCurrentFrame() override;

    // Does the SVG image/document contain any animations?
    bool hasAnimations() const;
    // Service CSS and SMIL animations.
    void serviceAnimations(double monotonicAnimationStartTime);

    void updateUseCounters(Document&) const;

    // The defaultObjectSize is assumed to be unzoomed, i.e. it should
    // not have the effective zoom level applied. The returned size is
    // thus also independent of current zoom level.
    FloatSize concreteObjectSize(const FloatSize& defaultObjectSize) const;

    bool hasIntrinsicDimensions() const;

private:
    // Accesses m_page.
    friend class SVGImageChromeClient;
    // Forwards calls to the various *ForContainer methods and other parts of
    // the the Image interface.
    friend class SVGImageForContainer;

    ~SVGImage() override;

    String filenameExtension() const override;

    IntSize containerSize() const;
    bool usesContainerSize() const override { return true; }

    bool dataChanged(bool allDataReceived) override;

    // FIXME: SVGImages are underreporting decoded sizes and will be unable
    // to prune because these functions are not implemented yet.
    void destroyDecodedData() override { }

    // FIXME: Implement this to be less conservative.
    bool currentFrameKnownToBeOpaque(MetadataMode = UseCurrentMetadata) override { return false; }

    SVGImage(ImageObserver*);
    void draw(SkCanvas*, const SkPaint&, const FloatRect& fromRect, const FloatRect& toRect, RespectImageOrientationEnum, ImageClampingMode) override;
    void drawForContainer(SkCanvas*, const SkPaint&, const FloatSize, float, const FloatRect&, const FloatRect&, const KURL&);
    void drawPatternForContainer(GraphicsContext&, const FloatSize, float, const FloatRect&, const FloatSize&, const FloatPoint&,
        SkXfermode::Mode, const FloatRect&, const FloatSize& repeatSpacing, const KURL&);
    PassRefPtr<SkImage> imageForCurrentFrameForContainer(const KURL&, const FloatSize& containerSize);
    void drawInternal(SkCanvas*, const SkPaint&, const FloatRect& fromRect, const FloatRect& toRect, RespectImageOrientationEnum,
        ImageClampingMode, const KURL&);

    void stopAnimation();
    void scheduleTimelineRewind();
    void flushPendingTimelineRewind();

    Persistent<SVGImageChromeClient> m_chromeClient;
    Persistent<Page> m_page;

    // When an SVG image has no intrinsic size the size depends on the
    // default object size, which in turn depends on the
    // container. SVGImage may belong to multiple containers so the
    // final image size can't be known in
    // SVGImage. SVGImageForContainer carried the final image size,
    // also called concrete object size.
    IntSize m_intrinsicSize;
    bool m_hasPendingTimelineRewind;
};

DEFINE_IMAGE_TYPE_CASTS(SVGImage);

class ImageObserverDisabler {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(ImageObserverDisabler);
public:
    ImageObserverDisabler(Image* image)
        : m_image(image)
    {
        m_image->setImageObserverDisabled(true);
    }

    ~ImageObserverDisabler()
    {
        m_image->setImageObserverDisabled(false);
    }
private:
    Image* m_image;
};

} // namespace blink

#endif // SVGImage_h
