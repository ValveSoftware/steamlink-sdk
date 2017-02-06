/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#include "core/svg/graphics/SVGImage.h"

#include "core/animation/AnimationTimeline.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/shadow/FlatTreeTraversal.h"
#include "core/frame/Deprecation.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/style/ComputedStyle.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/paint/FloatClipRecorder.h"
#include "core/paint/TransformRecorder.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGFEImageElement.h"
#include "core/svg/SVGImageElement.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/animation/SMILTimeContainer.h"
#include "core/svg/graphics/SVGImageChromeClient.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/LengthFunctions.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/TraceEvent.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/ImageObserver.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/CullRect.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/SkPictureBuilder.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "wtf/PassRefPtr.h"

namespace blink {

SVGImage::SVGImage(ImageObserver* observer)
    : Image(observer)
    , m_hasPendingTimelineRewind(false)
{
}

SVGImage::~SVGImage()
{
    if (m_page) {
        // Store m_page in a local variable, clearing m_page, so that SVGImageChromeClient knows we're destructed.
        Page* currentPage = m_page.release();
        // Break both the loader and view references to the frame
        currentPage->willBeDestroyed();
    }

    // Verify that page teardown destroyed the Chrome
    ASSERT(!m_chromeClient || !m_chromeClient->image());
}

bool SVGImage::isInSVGImage(const Node* node)
{
    ASSERT(node);

    Page* page = node->document().page();
    if (!page)
        return false;

    return page->chromeClient().isSVGImageChromeClient();
}

bool SVGImage::currentFrameHasSingleSecurityOrigin() const
{
    if (!m_page)
        return true;

    LocalFrame* frame = toLocalFrame(m_page->mainFrame());

    RELEASE_ASSERT(frame->document()->loadEventFinished());

    SVGSVGElement* rootElement = frame->document()->accessSVGExtensions().rootElement();
    if (!rootElement)
        return true;

    // Don't allow foreignObject elements or images that are not known to be
    // single-origin since these can leak cross-origin information.
    for (Node* node = rootElement; node; node = FlatTreeTraversal::next(*node)) {
        if (isSVGForeignObjectElement(*node))
            return false;
        if (isSVGImageElement(*node)) {
            if (!toSVGImageElement(*node).currentFrameHasSingleSecurityOrigin())
                return false;
        } else if (isSVGFEImageElement(*node)) {
            if (!toSVGFEImageElement(*node).currentFrameHasSingleSecurityOrigin())
                return false;
        }
    }

    // Because SVG image rendering disallows external resources and links, these
    // images effectively are restricted to a single security origin.
    return true;
}

static SVGSVGElement* svgRootElement(Page* page)
{
    if (!page)
        return nullptr;
    LocalFrame* frame = toLocalFrame(page->mainFrame());
    return frame->document()->accessSVGExtensions().rootElement();
}

IntSize SVGImage::containerSize() const
{
    SVGSVGElement* rootElement = svgRootElement(m_page.get());
    if (!rootElement)
        return IntSize();

    LayoutSVGRoot* layoutObject = toLayoutSVGRoot(rootElement->layoutObject());
    if (!layoutObject)
        return IntSize();

    // If a container size is available it has precedence.
    IntSize containerSize = layoutObject->containerSize();
    if (!containerSize.isEmpty())
        return containerSize;

    // Assure that a container size is always given for a non-identity zoom level.
    ASSERT(layoutObject->style()->effectiveZoom() == 1);

    // No set container size; use concrete object size.
    return m_intrinsicSize;
}

static float resolveWidthForRatio(float height, const FloatSize& intrinsicRatio)
{
    return height * intrinsicRatio.width() / intrinsicRatio.height();
}

static float resolveHeightForRatio(float width, const FloatSize& intrinsicRatio)
{
    return width * intrinsicRatio.height() / intrinsicRatio.width();
}

bool SVGImage::hasIntrinsicDimensions() const
{
    return !concreteObjectSize(FloatSize()).isEmpty();
}

FloatSize SVGImage::concreteObjectSize(const FloatSize& defaultObjectSize) const
{
    SVGSVGElement* svg = svgRootElement(m_page.get());
    if (!svg)
        return FloatSize();

    LayoutSVGRoot* layoutObject = toLayoutSVGRoot(svg->layoutObject());
    if (!layoutObject)
        return FloatSize();

    LayoutReplaced::IntrinsicSizingInfo intrinsicSizingInfo;
    layoutObject->computeIntrinsicSizingInfo(intrinsicSizingInfo);

    // https://www.w3.org/TR/css3-images/#default-sizing

    if (intrinsicSizingInfo.hasWidth && intrinsicSizingInfo.hasHeight)
        return intrinsicSizingInfo.size;

    if (svg->preserveAspectRatio()->currentValue()->align() == SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE) {
        // TODO(davve): The intrinsic aspect ratio is not used to resolve a missing intrinsic width
        // or height when preserveAspectRatio is none. It's unclear whether this is correct. See
        // crbug.com/584172.
        return defaultObjectSize;
    }

    if (intrinsicSizingInfo.hasWidth) {
        if (intrinsicSizingInfo.aspectRatio.isEmpty())
            return FloatSize(intrinsicSizingInfo.size.width(), defaultObjectSize.height());

        return FloatSize(intrinsicSizingInfo.size.width(), resolveHeightForRatio(intrinsicSizingInfo.size.width(), intrinsicSizingInfo.aspectRatio));
    }

    if (intrinsicSizingInfo.hasHeight) {
        if (intrinsicSizingInfo.aspectRatio.isEmpty())
            return FloatSize(defaultObjectSize.width(), intrinsicSizingInfo.size.height());

        return FloatSize(resolveWidthForRatio(intrinsicSizingInfo.size.height(), intrinsicSizingInfo.aspectRatio), intrinsicSizingInfo.size.height());
    }

    if (!intrinsicSizingInfo.aspectRatio.isEmpty()) {
        // "A contain constraint is resolved by setting the concrete object size to the largest
        //  rectangle that has the object's intrinsic aspect ratio and additionally has neither
        //  width nor height larger than the constraint rectangle's width and height, respectively."
        float solutionWidth = resolveWidthForRatio(defaultObjectSize.height(), intrinsicSizingInfo.aspectRatio);
        float solutionHeight = resolveHeightForRatio(defaultObjectSize.width(), intrinsicSizingInfo.aspectRatio);
        if (solutionWidth <= defaultObjectSize.width()) {
            if (solutionHeight <= defaultObjectSize.height()) {
                float areaOne = solutionWidth * defaultObjectSize.height();
                float areaTwo = defaultObjectSize.width() * solutionHeight;
                if (areaOne < areaTwo)
                    return FloatSize(defaultObjectSize.width(), solutionHeight);
                return FloatSize(solutionWidth, defaultObjectSize.height());
            }

            return FloatSize(solutionWidth, defaultObjectSize.height());
        }

        ASSERT(solutionHeight <= defaultObjectSize.height());
        return FloatSize(defaultObjectSize.width(), solutionHeight);
    }

    return defaultObjectSize;
}

void SVGImage::drawForContainer(SkCanvas* canvas, const SkPaint& paint, const FloatSize containerSize, float zoom, const FloatRect& dstRect,
    const FloatRect& srcRect, const KURL& url)
{
    if (!m_page)
        return;

    // Temporarily disable the image observer to prevent changeInRect() calls due re-laying out the image.
    ImageObserverDisabler imageObserverDisabler(this);

    IntSize roundedContainerSize = roundedIntSize(containerSize);

    if (SVGSVGElement* rootElement = svgRootElement(m_page.get())) {
        if (LayoutSVGRoot* layoutObject = toLayoutSVGRoot(rootElement->layoutObject()))
            layoutObject->setContainerSize(roundedContainerSize);
    }

    FloatRect scaledSrc = srcRect;
    scaledSrc.scale(1 / zoom);

    // Compensate for the container size rounding by adjusting the source rect.
    FloatSize adjustedSrcSize = scaledSrc.size();
    adjustedSrcSize.scale(roundedContainerSize.width() / containerSize.width(), roundedContainerSize.height() / containerSize.height());
    scaledSrc.setSize(adjustedSrcSize);

    drawInternal(canvas, paint, dstRect, scaledSrc, DoNotRespectImageOrientation, ClampImageToSourceRect, url);
}

PassRefPtr<SkImage> SVGImage::imageForCurrentFrame()
{
    return imageForCurrentFrameForContainer(KURL(), FloatSize(size()));
}

void SVGImage::drawPatternForContainer(GraphicsContext& context, const FloatSize containerSize,
    float zoom, const FloatRect& srcRect, const FloatSize& tileScale, const FloatPoint& phase,
    SkXfermode::Mode compositeOp, const FloatRect& dstRect,
    const FloatSize& repeatSpacing, const KURL& url)
{
    // Tile adjusted for scaling/stretch.
    FloatRect tile(srcRect);
    tile.scale(tileScale.width(), tileScale.height());

    // Expand the tile to account for repeat spacing.
    FloatRect spacedTile(tile);
    spacedTile.expand(FloatSize(repeatSpacing));

    SkPictureBuilder patternPicture(spacedTile, nullptr, &context);
    {
        DrawingRecorder patternPictureRecorder(patternPicture.context(), patternPicture, DisplayItem::Type::SVGImage, spacedTile);
        // When generating an expanded tile, make sure we don't draw into the spacing area.
        if (tile != spacedTile)
            patternPicture.context().clip(tile);
        SkPaint paint;
        drawForContainer(patternPicture.context().canvas(), paint, containerSize, zoom, tile, srcRect, url);
    }
    RefPtr<SkPicture> tilePicture = patternPicture.endRecording();

    SkMatrix patternTransform;
    patternTransform.setTranslate(phase.x() + spacedTile.x(), phase.y() + spacedTile.y());

    SkPaint paint;
    paint.setShader(SkShader::MakePictureShader(toSkSp(tilePicture.release()),
        SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode, &patternTransform, nullptr));
    paint.setXfermodeMode(compositeOp);
    paint.setColorFilter(sk_ref_sp(context.colorFilter()));
    context.drawRect(dstRect, paint);
}

PassRefPtr<SkImage> SVGImage::imageForCurrentFrameForContainer(const KURL& url, const FloatSize& containerSize)
{
    if (!m_page)
        return nullptr;

    SkPictureRecorder recorder;
    SkCanvas* canvas = recorder.beginRecording(width(), height());
    drawForContainer(canvas, SkPaint(), containerSize, 1, rect(), rect(), url);

    return fromSkSp(SkImage::MakeFromPicture(recorder.finishRecordingAsPicture(),
        SkISize::Make(width(), height()), nullptr, nullptr));
}

static bool drawNeedsLayer(const SkPaint& paint)
{
    if (SkColorGetA(paint.getColor()) < 255)
        return true;

    SkXfermode::Mode xfermode;
    if (SkXfermode::AsMode(paint.getXfermode(), &xfermode)) {
        if (xfermode != SkXfermode::kSrcOver_Mode)
            return true;
    }

    return false;
}

void SVGImage::draw(SkCanvas* canvas, const SkPaint& paint, const FloatRect& dstRect, const FloatRect& srcRect,
    RespectImageOrientationEnum shouldRespectImageOrientation, ImageClampingMode clampMode)
{
    if (!m_page)
        return;

    drawInternal(canvas, paint, dstRect, srcRect, shouldRespectImageOrientation, clampMode, KURL());
}

void SVGImage::drawInternal(SkCanvas* canvas, const SkPaint& paint, const FloatRect& dstRect, const FloatRect& srcRect,
    RespectImageOrientationEnum, ImageClampingMode, const KURL& url)
{
    DCHECK(m_page);
    FrameView* view = toLocalFrame(m_page->mainFrame())->view();
    view->resize(containerSize());

    // Always call processUrlFragment, even if the url is empty, because
    // there may have been a previous url/fragment that needs to be reset.
    view->processUrlFragment(url);

    // If the image was reset, we need to rewind the timeline back to 0. This
    // needs to be done before painting, or else we wouldn't get the correct
    // reset semantics (we'd paint the "last" frame rather than the one at
    // time=0.) The reason we do this here and not in resetAnimation() is to
    // avoid setting timers from the latter.
    flushPendingTimelineRewind();

    SkPictureBuilder imagePicture(dstRect);
    {
        ClipRecorder clipRecorder(imagePicture.context(), imagePicture, DisplayItem::ClipNodeImage, enclosingIntRect(dstRect));

        // We can only draw the entire frame, clipped to the rect we want. So compute where the top left
        // of the image would be if we were drawing without clipping, and translate accordingly.
        FloatSize scale(dstRect.width() / srcRect.width(), dstRect.height() / srcRect.height());
        FloatSize topLeftOffset(srcRect.location().x() * scale.width(), srcRect.location().y() * scale.height());
        FloatPoint destOffset = dstRect.location() - topLeftOffset;
        AffineTransform transform = AffineTransform::translation(destOffset.x(), destOffset.y());
        transform.scale(scale.width(), scale.height());
        TransformRecorder transformRecorder(imagePicture.context(), imagePicture, transform);

        view->updateAllLifecyclePhasesExceptPaint();
        view->paint(imagePicture.context(), CullRect(enclosingIntRect(srcRect)));
        ASSERT(!view->needsLayout());
    }

    {
        SkAutoCanvasRestore ar(canvas, false);
        if (drawNeedsLayer(paint)) {
            SkRect layerRect = dstRect;
            canvas->saveLayer(&layerRect, &paint);
        }
        RefPtr<const SkPicture> recording = imagePicture.endRecording();
        canvas->drawPicture(recording.get());
    }

    if (getImageObserver())
        getImageObserver()->didDraw(this);

    // Start any (SMIL) animations if needed. This will restart or continue
    // animations if preceded by calls to resetAnimation or stopAnimation
    // respectively.
    startAnimation();
}

LayoutReplaced* SVGImage::embeddedReplacedContent() const
{
    SVGSVGElement* rootElement = svgRootElement(m_page.get());
    if (!rootElement)
        return nullptr;
    return toLayoutSVGRoot(rootElement->layoutObject());
}

void SVGImage::scheduleTimelineRewind()
{
    m_hasPendingTimelineRewind = true;
}

void SVGImage::flushPendingTimelineRewind()
{
    if (!m_hasPendingTimelineRewind)
        return;
    if (SVGSVGElement* rootElement = svgRootElement(m_page.get()))
        rootElement->setCurrentTime(0);
    m_hasPendingTimelineRewind = false;
}

// FIXME: support CatchUpAnimation = CatchUp.
void SVGImage::startAnimation(CatchUpAnimation)
{
    SVGSVGElement* rootElement = svgRootElement(m_page.get());
    if (!rootElement)
        return;
    m_chromeClient->resumeAnimation();
    if (rootElement->animationsPaused())
        rootElement->unpauseAnimations();
}

void SVGImage::stopAnimation()
{
    SVGSVGElement* rootElement = svgRootElement(m_page.get());
    if (!rootElement)
        return;
    m_chromeClient->suspendAnimation();
    rootElement->pauseAnimations();
}

void SVGImage::resetAnimation()
{
    SVGSVGElement* rootElement = svgRootElement(m_page.get());
    if (!rootElement)
        return;
    m_chromeClient->suspendAnimation();
    rootElement->pauseAnimations();
    scheduleTimelineRewind();
}

bool SVGImage::hasAnimations() const
{
    SVGSVGElement* rootElement = svgRootElement(m_page.get());
    if (!rootElement)
        return false;
    return rootElement->timeContainer()->hasAnimations()
        || toLocalFrame(m_page->mainFrame())->document()->timeline().hasPendingUpdates();
}

void SVGImage::serviceAnimations(double monotonicAnimationStartTime)
{
    // If none of our observers (sic!) are visible, or for some other reason
    // does not want us to keep running animations, stop them until further
    // notice (next paint.)
    if (getImageObserver()->shouldPauseAnimation(this)) {
        stopAnimation();
        return;
    }

    // serviceScriptedAnimations runs requestAnimationFrame callbacks, but SVG
    // images can't have any so we assert there's no script.
    ScriptForbiddenScope forbidScript;

    // The calls below may trigger GCs, so set up the required persistent
    // reference on the ImageResource which owns this SVGImage. By transitivity,
    // that will keep the associated SVGImageChromeClient object alive.
    Persistent<ImageObserver> protect(getImageObserver());
    m_page->animator().serviceScriptedAnimations(monotonicAnimationStartTime);
    m_page->animator().updateAllLifecyclePhases(*toLocalFrame(m_page->mainFrame()));
}

void SVGImage::advanceAnimationForTesting()
{
    if (SVGSVGElement* rootElement = svgRootElement(m_page.get())) {
        rootElement->timeContainer()->advanceFrameForTesting();

        // The following triggers animation updates which can issue a new draw
        // but will not permanently change the animation timeline.
        // TODO(pdr): Actually advance the document timeline so CSS animations
        // can be properly tested.
        m_page->animator().serviceScriptedAnimations(rootElement->getCurrentTime());
        getImageObserver()->animationAdvanced(this);
    }
}

SVGImageChromeClient& SVGImage::chromeClientForTesting()
{
    return *m_chromeClient;
}

void SVGImage::updateUseCounters(Document& document) const
{
    if (SVGSVGElement* rootElement = svgRootElement(m_page.get())) {
        if (rootElement->timeContainer()->hasAnimations())
            Deprecation::countDeprecation(document, UseCounter::SVGSMILAnimationInImageRegardlessOfCache);
    }
}

bool SVGImage::dataChanged(bool allDataReceived)
{
    TRACE_EVENT0("blink", "SVGImage::dataChanged");

    // Don't do anything if is an empty image.
    if (!data()->size())
        return true;

    if (allDataReceived) {
        // SVGImage will fire events (and the default C++ handlers run) but doesn't
        // actually allow script to run so it's fine to call into it. We allow this
        // since it means an SVG data url can synchronously load like other image
        // types.
        EventDispatchForbiddenScope::AllowUserAgentEvents allowUserAgentEvents;

        DEFINE_STATIC_LOCAL(FrameLoaderClient, dummyFrameLoaderClient, (EmptyFrameLoaderClient::create()));

        if (m_page) {
            toLocalFrame(m_page->mainFrame())->loader().load(FrameLoadRequest(0, blankURL(), SubstituteData(data(), AtomicString("image/svg+xml"),
                AtomicString("UTF-8"), KURL(), ForceSynchronousLoad)));
            return true;
        }

        Page::PageClients pageClients;
        fillWithEmptyClients(pageClients);
        m_chromeClient = SVGImageChromeClient::create(this);
        pageClients.chromeClient = m_chromeClient.get();

        // FIXME: If this SVG ends up loading itself, we might leak the world.
        // The Cache code does not know about ImageResources holding Frames and
        // won't know to break the cycle.
        // This will become an issue when SVGImage will be able to load other
        // SVGImage objects, but we're safe now, because SVGImage can only be
        // loaded by a top-level document.
        Page* page;
        {
            TRACE_EVENT0("blink", "SVGImage::dataChanged::createPage");
            page = Page::create(pageClients);
            page->settings().setScriptEnabled(false);
            page->settings().setPluginsEnabled(false);
            page->settings().setAcceleratedCompositingEnabled(false);

            // Because this page is detached, it can't get default font settings
            // from the embedder. Copy over font settings so we have sensible
            // defaults. These settings are fixed and will not update if changed.
            if (!Page::ordinaryPages().isEmpty()) {
                Settings& defaultSettings = (*Page::ordinaryPages().begin())->settings();
                page->settings().genericFontFamilySettings() = defaultSettings.genericFontFamilySettings();
                page->settings().setMinimumFontSize(defaultSettings.minimumFontSize());
                page->settings().setMinimumLogicalFontSize(defaultSettings.minimumLogicalFontSize());
                page->settings().setDefaultFontSize(defaultSettings.defaultFontSize());
                page->settings().setDefaultFixedFontSize(defaultSettings.defaultFixedFontSize());
            }
        }

        LocalFrame* frame = nullptr;
        {
            TRACE_EVENT0("blink", "SVGImage::dataChanged::createFrame");
            frame = LocalFrame::create(&dummyFrameLoaderClient, &page->frameHost(), 0);
            frame->setView(FrameView::create(frame));
            frame->init();
        }

        FrameLoader& loader = frame->loader();
        loader.forceSandboxFlags(SandboxAll);

        frame->view()->setScrollbarsSuppressed(true);
        frame->view()->setCanHaveScrollbars(false); // SVG Images will always synthesize a viewBox, if it's not available, and thus never see scrollbars.
        frame->view()->setTransparent(true); // SVG Images are transparent.

        m_page = page;

        TRACE_EVENT0("blink", "SVGImage::dataChanged::load");
        loader.load(FrameLoadRequest(0, blankURL(), SubstituteData(data(), AtomicString("image/svg+xml"),
            AtomicString("UTF-8"), KURL(), ForceSynchronousLoad)));

        // Set the concrete object size before a container size is available.
        m_intrinsicSize = roundedIntSize(concreteObjectSize(FloatSize(LayoutReplaced::defaultWidth, LayoutReplaced::defaultHeight)));
    }

    return m_page;
}

String SVGImage::filenameExtension() const
{
    return "svg";
}

} // namespace blink
