/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/BitmapImage.h"

#include "platform/PlatformInstrumentation.h"
#include "platform/Timer.h"
#include "platform/TraceEvent.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/BitmapImageMetrics.h"
#include "platform/graphics/DeferredImageDecoder.h"
#include "platform/graphics/ImageObserver.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/WTFString.h"

namespace blink {

PassRefPtr<BitmapImage> BitmapImage::createWithOrientationForTesting(const SkBitmap& bitmap, ImageOrientation orientation)
{
    if (bitmap.isNull()) {
        return BitmapImage::create();
    }

    RefPtr<BitmapImage> result = adoptRef(new BitmapImage(bitmap));
    result->m_frames[0].m_orientation = orientation;
    if (orientation.usesWidthAsHeight())
        result->m_sizeRespectingOrientation = result->m_size.transposedSize();
    return result.release();
}

BitmapImage::BitmapImage(ImageObserver* observer)
    : Image(observer)
    , m_currentFrame(0)
    , m_cachedFrameIndex(0)
    , m_repetitionCount(cAnimationNone)
    , m_repetitionCountStatus(Unknown)
    , m_repetitionsComplete(0)
    , m_desiredFrameStartTime(0)
    , m_frameCount(0)
    , m_animationPolicy(ImageAnimationPolicyAllowed)
    , m_animationFinished(false)
    , m_allDataReceived(false)
    , m_haveSize(false)
    , m_sizeAvailable(false)
    , m_haveFrameCount(false)
{
}

BitmapImage::BitmapImage(const SkBitmap& bitmap, ImageObserver* observer)
    : Image(observer)
    , m_size(bitmap.width(), bitmap.height())
    , m_currentFrame(0)
    , m_cachedFrame(fromSkSp(SkImage::MakeFromBitmap(bitmap)))
    , m_cachedFrameIndex(0)
    , m_repetitionCount(cAnimationNone)
    , m_repetitionCountStatus(Unknown)
    , m_repetitionsComplete(0)
    , m_frameCount(1)
    , m_animationPolicy(ImageAnimationPolicyAllowed)
    , m_animationFinished(true)
    , m_allDataReceived(true)
    , m_haveSize(true)
    , m_sizeAvailable(true)
    , m_haveFrameCount(true)
{
    // Since we don't have a decoder, we can't figure out the image orientation.
    // Set m_sizeRespectingOrientation to be the same as m_size so it's not 0x0.
    m_sizeRespectingOrientation = m_size;

    m_frames.grow(1);
    m_frames[0].m_hasAlpha = !bitmap.isOpaque();
    m_frames[0].m_haveMetadata = true;
}

BitmapImage::~BitmapImage()
{
    stopAnimation();
}

bool BitmapImage::currentFrameHasSingleSecurityOrigin() const
{
    return true;
}

void BitmapImage::destroyDecodedData()
{
    m_cachedFrame.clear();
    for (size_t i = 0; i < m_frames.size(); ++i)
        m_frames[i].clear(true);
    m_source.clearCacheExceptFrame(kNotFound);
    notifyMemoryChanged();
}

void BitmapImage::notifyMemoryChanged()
{
    if (getImageObserver())
        getImageObserver()->decodedSizeChangedTo(this, totalFrameBytes());
}

size_t BitmapImage::totalFrameBytes()
{
    const size_t numFrames = frameCount();
    size_t totalBytes = 0;
    for (size_t i = 0; i < numFrames; ++i)
        totalBytes += m_source.frameBytesAtIndex(i);
    return totalBytes;
}

PassRefPtr<SkImage> BitmapImage::decodeAndCacheFrame(size_t index)
{
    size_t numFrames = frameCount();
    if (m_frames.size() < numFrames)
        m_frames.grow(numFrames);

    // We are caching frame snapshots.  This is OK even for partially decoded frames,
    // as they are cleared by dataChanged() when new data arrives.
    RefPtr<SkImage> image = m_source.createFrameAtIndex(index);
    m_cachedFrame = image;
    m_cachedFrameIndex = index;

    m_frames[index].m_orientation = m_source.orientationAtIndex(index);
    m_frames[index].m_haveMetadata = true;
    m_frames[index].m_isComplete = m_source.frameIsCompleteAtIndex(index);
    if (repetitionCount(false) != cAnimationNone)
        m_frames[index].m_duration = m_source.frameDurationAtIndex(index);
    m_frames[index].m_hasAlpha = m_source.frameHasAlphaAtIndex(index);
    m_frames[index].m_frameBytes = m_source.frameBytesAtIndex(index);

    notifyMemoryChanged();
    return image.release();
}

void BitmapImage::updateSize() const
{
    if (!m_sizeAvailable || m_haveSize)
        return;

    m_size = m_source.size();
    m_sizeRespectingOrientation = m_source.size(RespectImageOrientation);
    m_haveSize = true;
}

IntSize BitmapImage::size() const
{
    updateSize();
    return m_size;
}

IntSize BitmapImage::sizeRespectingOrientation() const
{
    updateSize();
    return m_sizeRespectingOrientation;
}

bool BitmapImage::getHotSpot(IntPoint& hotSpot) const
{
    return m_source.getHotSpot(hotSpot);
}

bool BitmapImage::dataChanged(bool allDataReceived)
{
    TRACE_EVENT0("blink", "BitmapImage::dataChanged");

    // Clear all partially-decoded frames. For most image formats, there is only
    // one frame, but at least GIF and ICO can have more. With GIFs, the frames
    // come in order and we ask to decode them in order, waiting to request a
    // subsequent frame until the prior one is complete. Given that we clear
    // incomplete frames here, this means there is at most one incomplete frame
    // (even if we use destroyDecodedData() -- since it doesn't reset the
    // metadata), and it is after all the complete frames.
    //
    // With ICOs, on the other hand, we may ask for arbitrary frames at
    // different times (e.g. because we're displaying a higher-resolution image
    // in the content area and using a lower-resolution one for the favicon),
    // and the frames aren't even guaranteed to appear in the file in the same
    // order as in the directory, so an arbitrary number of the frames might be
    // incomplete (if we ask for frames for which we've not yet reached the
    // start of the frame data), and any or none of them might be the particular
    // frame affected by appending new data here. Thus we have to clear all the
    // incomplete frames to be safe.
    for (size_t i = 0; i < m_frames.size(); ++i) {
        // NOTE: Don't call frameIsCompleteAtIndex() here, that will try to
        // decode any uncached (i.e. never-decoded or
        // cleared-on-a-previous-pass) frames!
        if (m_frames[i].m_haveMetadata && !m_frames[i].m_isComplete) {
            m_frames[i].clear(true);
            if (i == m_cachedFrameIndex)
                m_cachedFrame.clear();
        }
    }

    // Feed all the data we've seen so far to the image decoder.
    m_allDataReceived = allDataReceived;
    ASSERT(data());
    m_source.setData(*data(), allDataReceived);

    m_haveFrameCount = false;
    return isSizeAvailable();
}

bool BitmapImage::hasColorProfile() const
{
    return m_source.hasColorProfile();
}

String BitmapImage::filenameExtension() const
{
    return m_source.filenameExtension();
}

void BitmapImage::draw(SkCanvas* canvas, const SkPaint& paint, const FloatRect& dstRect, const FloatRect& srcRect, RespectImageOrientationEnum shouldRespectImageOrientation, ImageClampingMode clampMode)
{
    TRACE_EVENT0("skia", "BitmapImage::draw");

    RefPtr<SkImage> image = imageForCurrentFrame();
    if (!image)
        return; // It's too early and we don't have an image yet.

    FloatRect adjustedSrcRect = srcRect;
    adjustedSrcRect.intersect(SkRect::Make(image->bounds()));

    if (adjustedSrcRect.isEmpty() || dstRect.isEmpty())
        return; // Nothing to draw.

    ImageOrientation orientation = DefaultImageOrientation;
    if (shouldRespectImageOrientation == RespectImageOrientation)
        orientation = frameOrientationAtIndex(m_currentFrame);

    SkAutoCanvasRestore autoRestore(canvas, false);
    FloatRect adjustedDstRect = dstRect;
    if (orientation != DefaultImageOrientation) {
        canvas->save();

        // ImageOrientation expects the origin to be at (0, 0)
        canvas->translate(adjustedDstRect.x(), adjustedDstRect.y());
        adjustedDstRect.setLocation(FloatPoint());

        canvas->concat(affineTransformToSkMatrix(orientation.transformFromDefault(adjustedDstRect.size())));

        if (orientation.usesWidthAsHeight()) {
            // The destination rect will have it's width and height already reversed for the orientation of
            // the image, as it was needed for page layout, so we need to reverse it back here.
            adjustedDstRect = FloatRect(adjustedDstRect.x(), adjustedDstRect.y(), adjustedDstRect.height(), adjustedDstRect.width());
        }
    }

    canvas->drawImageRect(image.get(), adjustedSrcRect, adjustedDstRect, &paint,
        WebCoreClampingModeToSkiaRectConstraint(clampMode));

    if (image->isLazyGenerated())
        PlatformInstrumentation::didDrawLazyPixelRef(image->uniqueID());

    if (ImageObserver* observer = getImageObserver())
        observer->didDraw(this);

    startAnimation();
}

size_t BitmapImage::frameCount()
{
    if (!m_haveFrameCount) {
        m_frameCount = m_source.frameCount();
        // If decoder is not initialized yet, m_source.frameCount() returns 0.
        if (m_frameCount)
            m_haveFrameCount = true;
    }

    return m_frameCount;
}

static inline bool hasVisibleImageSize(IntSize size)
{
    return (size.width() > 1 || size.height() > 1);
}

bool BitmapImage::isSizeAvailable()
{
    if (m_sizeAvailable)
        return true;

    m_sizeAvailable = m_source.isSizeAvailable();

    if (m_sizeAvailable && hasVisibleImageSize(size())) {
        BitmapImageMetrics::countDecodedImageType(m_source.filenameExtension());
        if (m_source.filenameExtension() == "jpg")
            BitmapImageMetrics::countImageOrientation(m_source.orientationAtIndex(0).orientation());
    }

    return m_sizeAvailable;
}

PassRefPtr<SkImage> BitmapImage::frameAtIndex(size_t index)
{
    if (index >= frameCount())
        return nullptr;

    if (index == m_cachedFrameIndex && m_cachedFrame)
        return m_cachedFrame;

    return decodeAndCacheFrame(index);
}

bool BitmapImage::frameIsCompleteAtIndex(size_t index)
{
    if (index < m_frames.size() && m_frames[index].m_haveMetadata && m_frames[index].m_isComplete)
        return true;

    return m_source.frameIsCompleteAtIndex(index);
}

float BitmapImage::frameDurationAtIndex(size_t index)
{
    if (index < m_frames.size() && m_frames[index].m_haveMetadata)
        return m_frames[index].m_duration;

    return m_source.frameDurationAtIndex(index);
}

PassRefPtr<SkImage> BitmapImage::imageForCurrentFrame()
{
    return frameAtIndex(currentFrame());
}

PassRefPtr<Image> BitmapImage::imageForDefaultFrame()
{
    if (frameCount() > 1) {
        RefPtr<SkImage> firstFrame = frameAtIndex(0);
        if (firstFrame)
            return StaticBitmapImage::create(firstFrame);
    }

    return Image::imageForDefaultFrame();
}

bool BitmapImage::frameHasAlphaAtIndex(size_t index)
{
    if (m_frames.size() <= index)
        return true;

    if (m_frames[index].m_haveMetadata)
        return m_frames[index].m_hasAlpha;

    return m_source.frameHasAlphaAtIndex(index);
}

bool BitmapImage::currentFrameKnownToBeOpaque(MetadataMode metadataMode)
{
    if (metadataMode == PreCacheMetadata) {
        // frameHasAlphaAtIndex() conservatively returns false for uncached frames. To increase the
        // chance of an accurate answer, pre-cache the current frame metadata.
        frameAtIndex(currentFrame());
    }
    return !frameHasAlphaAtIndex(currentFrame());
}

bool BitmapImage::currentFrameIsComplete()
{
    return frameIsCompleteAtIndex(currentFrame());
}

bool BitmapImage::currentFrameIsLazyDecoded()
{
    RefPtr<SkImage> image = frameAtIndex(currentFrame());
    return image && image->isLazyGenerated();
}

ImageOrientation BitmapImage::currentFrameOrientation()
{
    return frameOrientationAtIndex(currentFrame());
}

ImageOrientation BitmapImage::frameOrientationAtIndex(size_t index)
{
    if (m_frames.size() <= index)
        return DefaultImageOrientation;

    if (m_frames[index].m_haveMetadata)
        return m_frames[index].m_orientation;

    return m_source.orientationAtIndex(index);
}

int BitmapImage::repetitionCount(bool imageKnownToBeComplete)
{
    if ((m_repetitionCountStatus == Unknown) || ((m_repetitionCountStatus == Uncertain) && imageKnownToBeComplete)) {
        // Snag the repetition count.  If |imageKnownToBeComplete| is false, the
        // repetition count may not be accurate yet for GIFs; in this case the
        // decoder will default to cAnimationLoopOnce, and we'll try and read
        // the count again once the whole image is decoded.
        m_repetitionCount = m_source.repetitionCount();
        m_repetitionCountStatus = (imageKnownToBeComplete || m_repetitionCount == cAnimationNone) ? Certain : Uncertain;
    }
    return m_repetitionCount;
}

bool BitmapImage::shouldAnimate()
{
    bool animated = repetitionCount(false) != cAnimationNone && !m_animationFinished && getImageObserver();
    if (animated && m_animationPolicy == ImageAnimationPolicyNoAnimation)
        animated = false;
    return animated;
}

void BitmapImage::startAnimation(CatchUpAnimation catchUpIfNecessary)
{
    if (m_frameTimer || !shouldAnimate() || frameCount() <= 1)
        return;

    // If we aren't already animating, set now as the animation start time.
    const double time = monotonicallyIncreasingTime();
    if (!m_desiredFrameStartTime)
        m_desiredFrameStartTime = time;

    // Don't advance the animation to an incomplete frame.
    size_t nextFrame = (m_currentFrame + 1) % frameCount();
    if (!m_allDataReceived && !frameIsCompleteAtIndex(nextFrame))
        return;

    // Don't advance past the last frame if we haven't decoded the whole image
    // yet and our repetition count is potentially unset.  The repetition count
    // in a GIF can potentially come after all the rest of the image data, so
    // wait on it.
    if (!m_allDataReceived
        && (repetitionCount(false) == cAnimationLoopOnce || m_animationPolicy == ImageAnimationPolicyAnimateOnce)
        && m_currentFrame >= (frameCount() - 1))
        return;

    // Determine time for next frame to start.  By ignoring paint and timer lag
    // in this calculation, we make the animation appear to run at its desired
    // rate regardless of how fast it's being repainted.
    const double currentDuration = frameDurationAtIndex(m_currentFrame);
    m_desiredFrameStartTime += currentDuration;

    // When an animated image is more than five minutes out of date, the
    // user probably doesn't care about resyncing and we could burn a lot of
    // time looping through frames below.  Just reset the timings.
    const double cAnimationResyncCutoff = 5 * 60;
    if ((time - m_desiredFrameStartTime) > cAnimationResyncCutoff)
        m_desiredFrameStartTime = time + currentDuration;

    // The image may load more slowly than it's supposed to animate, so that by
    // the time we reach the end of the first repetition, we're well behind.
    // Clamp the desired frame start time in this case, so that we don't skip
    // frames (or whole iterations) trying to "catch up".  This is a tradeoff:
    // It guarantees users see the whole animation the second time through and
    // don't miss any repetitions, and is closer to what other browsers do; on
    // the other hand, it makes animations "less accurate" for pages that try to
    // sync an image and some other resource (e.g. audio), especially if users
    // switch tabs (and thus stop drawing the animation, which will pause it)
    // during that initial loop, then switch back later.
    if (nextFrame == 0 && m_repetitionsComplete == 0 && m_desiredFrameStartTime < time)
        m_desiredFrameStartTime = time;

    if (catchUpIfNecessary == DoNotCatchUp || time < m_desiredFrameStartTime) {
        // Haven't yet reached time for next frame to start; delay until then.
        m_frameTimer = wrapUnique(new Timer<BitmapImage>(this, &BitmapImage::advanceAnimation));
        m_frameTimer->startOneShot(std::max(m_desiredFrameStartTime - time, 0.), BLINK_FROM_HERE);
    } else {
        // We've already reached or passed the time for the next frame to start.
        // See if we've also passed the time for frames after that to start, in
        // case we need to skip some frames entirely.  Remember not to advance
        // to an incomplete frame.
        for (size_t frameAfterNext = (nextFrame + 1) % frameCount(); frameIsCompleteAtIndex(frameAfterNext); frameAfterNext = (nextFrame + 1) % frameCount()) {
            // Should we skip the next frame?
            double frameAfterNextStartTime = m_desiredFrameStartTime + frameDurationAtIndex(nextFrame);
            if (time < frameAfterNextStartTime)
                break;

            // Yes; skip over it without notifying our observers.
            if (!internalAdvanceAnimation(true))
                return;
            m_desiredFrameStartTime = frameAfterNextStartTime;
            nextFrame = frameAfterNext;
        }

        // Post a task to advance the frame immediately. m_desiredFrameStartTime
        // may be in the past, meaning the next time through this function we'll
        // kick off the next advancement sooner than this frame's duration would
        // suggest.
        m_frameTimer = wrapUnique(new Timer<BitmapImage>(this, &BitmapImage::advanceAnimationWithoutCatchUp));
        m_frameTimer->startOneShot(0, BLINK_FROM_HERE);
    }
}

void BitmapImage::stopAnimation()
{
    // This timer is used to animate all occurrences of this image.  Don't invalidate
    // the timer unless all renderers have stopped drawing.
    m_frameTimer.reset();
}

void BitmapImage::resetAnimation()
{
    stopAnimation();
    m_currentFrame = 0;
    m_repetitionsComplete = 0;
    m_desiredFrameStartTime = 0;
    m_animationFinished = false;
    m_cachedFrame.clear();
}

bool BitmapImage::maybeAnimated()
{
    if (m_animationFinished)
        return false;
    if (frameCount() > 1)
        return true;

    return m_source.repetitionCount() != cAnimationNone;
}

void BitmapImage::advanceTime(double deltaTimeInSeconds)
{
    if (m_desiredFrameStartTime)
        m_desiredFrameStartTime -= deltaTimeInSeconds;
    else
        m_desiredFrameStartTime = monotonicallyIncreasingTime() - deltaTimeInSeconds;
}

void BitmapImage::advanceAnimation(Timer<BitmapImage>*)
{
    internalAdvanceAnimation(false);
    // At this point the image region has been marked dirty, and if it's
    // onscreen, we'll soon make a call to draw(), which will call
    // startAnimation() again to keep the animation moving.
}

void BitmapImage::advanceAnimationWithoutCatchUp(Timer<BitmapImage>*)
{
    if (internalAdvanceAnimation(false))
        startAnimation(DoNotCatchUp);
}

bool BitmapImage::internalAdvanceAnimation(bool skippingFrames)
{
    // Stop the animation.
    stopAnimation();

    // See if anyone is still paying attention to this animation.  If not, we don't
    // advance and will remain suspended at the current frame until the animation is resumed.
    if (!skippingFrames && getImageObserver()->shouldPauseAnimation(this))
        return false;

    ++m_currentFrame;
    bool advancedAnimation = true;
    if (m_currentFrame >= frameCount()) {
        ++m_repetitionsComplete;

        // Get the repetition count again.  If we weren't able to get a
        // repetition count before, we should have decoded the whole image by
        // now, so it should now be available.
        // Note that we don't need to special-case cAnimationLoopOnce here
        // because it is 0 (see comments on its declaration in ImageAnimation.h).
        if ((repetitionCount(true) != cAnimationLoopInfinite && m_repetitionsComplete > m_repetitionCount)
            || (m_animationPolicy == ImageAnimationPolicyAnimateOnce && m_repetitionsComplete > 0)) {
            m_animationFinished = true;
            m_desiredFrameStartTime = 0;
            --m_currentFrame;
            advancedAnimation = false;
        } else
            m_currentFrame = 0;
    }

    // We need to draw this frame if we advanced to it while not skipping, or if
    // while trying to skip frames we hit the last frame and thus had to stop.
    if (skippingFrames != advancedAnimation)
        getImageObserver()->animationAdvanced(this);

    return advancedAnimation;
}

} // namespace blink
