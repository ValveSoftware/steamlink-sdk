// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/RecordingImageBufferSurface.h"

#include "platform/Histogram.h"
#include "platform/graphics/CanvasMetrics.h"
#include "platform/graphics/ExpensiveCanvasHeuristicParameters.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageBuffer.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

RecordingImageBufferSurface::RecordingImageBufferSurface(const IntSize& size, std::unique_ptr<RecordingImageBufferFallbackSurfaceFactory> fallbackFactory, OpacityMode opacityMode)
    : ImageBufferSurface(size, opacityMode)
    , m_imageBuffer(0)
    , m_currentFramePixelCount(0)
    , m_previousFramePixelCount(0)
    , m_frameWasCleared(true)
    , m_didRecordDrawCommandsInCurrentFrame(false)
    , m_currentFrameHasExpensiveOp(false)
    , m_previousFrameHasExpensiveOp(false)
    , m_fallbackFactory(std::move(fallbackFactory))
{
    initializeCurrentFrame();
}

RecordingImageBufferSurface::~RecordingImageBufferSurface()
{ }

void RecordingImageBufferSurface::initializeCurrentFrame()
{
    static SkRTreeFactory rTreeFactory;
    m_currentFrame = wrapUnique(new SkPictureRecorder);
    m_currentFrame->beginRecording(size().width(), size().height(), &rTreeFactory);
    if (m_imageBuffer) {
        m_imageBuffer->resetCanvas(m_currentFrame->getRecordingCanvas());
    }
    m_didRecordDrawCommandsInCurrentFrame = false;
    m_currentFrameHasExpensiveOp = false;
    m_currentFramePixelCount = 0;
}

void RecordingImageBufferSurface::setImageBuffer(ImageBuffer* imageBuffer)
{
    m_imageBuffer = imageBuffer;
    if (m_currentFrame && m_imageBuffer) {
        m_imageBuffer->resetCanvas(m_currentFrame->getRecordingCanvas());
    }
    if (m_fallbackSurface) {
        m_fallbackSurface->setImageBuffer(imageBuffer);
    }
}

bool RecordingImageBufferSurface::writePixels(const SkImageInfo& origInfo, const void* pixels, size_t rowBytes, int x, int y)
{
    if (!m_fallbackSurface) {
        if (x <= 0 && y <= 0 && x + origInfo.width() >= size().width() && y + origInfo.height() >= size().height()) {
            willOverwriteCanvas();
        }
        fallBackToRasterCanvas(FallbackReasonWritePixels);
    }
    return m_fallbackSurface->writePixels(origInfo, pixels, rowBytes, x, y);
}

void RecordingImageBufferSurface::fallBackToRasterCanvas(FallbackReason reason)
{
    ASSERT(m_fallbackFactory);
    DCHECK(reason != FallbackReasonUnknown);

    if (m_fallbackSurface) {
        ASSERT(!m_currentFrame);
        return;
    }

    DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, canvasFallbackHistogram, new EnumerationHistogram("Canvas.DisplayListFallbackReason", FallbackReasonCount));
    canvasFallbackHistogram.count(reason);

    m_fallbackSurface = m_fallbackFactory->createSurface(size(), getOpacityMode());
    m_fallbackSurface->setImageBuffer(m_imageBuffer);

    if (m_previousFrame) {
        m_previousFrame->playback(m_fallbackSurface->canvas());
        m_previousFrame.clear();
    }

    if (m_currentFrame) {
        m_currentFrame->finishRecordingAsPicture()->playback(m_fallbackSurface->canvas());
        m_currentFrame.reset();
    }

    if (m_imageBuffer) {
        m_imageBuffer->resetCanvas(m_fallbackSurface->canvas());
    }

    CanvasMetrics::countCanvasContextUsage(CanvasMetrics::DisplayList2DCanvasFallbackToRaster);
}

static RecordingImageBufferSurface::FallbackReason snapshotReasonToFallbackReason(SnapshotReason reason)
{
    switch (reason) {
    case SnapshotReasonUnknown:
        return RecordingImageBufferSurface::FallbackReasonUnknown;
    case SnapshotReasonGetImageData:
        return RecordingImageBufferSurface::FallbackReasonSnapshotForGetImageData;
    case SnapshotReasonCopyToWebGLTexture:
        return RecordingImageBufferSurface::FallbackReasonSnapshotForCopyToWebGLTexture;
    case SnapshotReasonPaint:
        return RecordingImageBufferSurface::FallbackReasonSnapshotForPaint;
    case SnapshotReasonToDataURL:
        return RecordingImageBufferSurface::FallbackReasonSnapshotForToDataURL;
    case SnapshotReasonToBlob:
        return RecordingImageBufferSurface::FallbackReasonSnapshotForToBlob;
    case SnapshotReasonCanvasListenerCapture:
        return RecordingImageBufferSurface::FallbackReasonSnapshotForCanvasListenerCapture;
    case SnapshotReasonDrawImage:
        return RecordingImageBufferSurface::FallbackReasonSnapshotForDrawImage;
    case SnapshotReasonCreatePattern:
        return RecordingImageBufferSurface::FallbackReasonSnapshotForCreatePattern;
    case SnapshotReasonTransferToImageBitmap:
        return RecordingImageBufferSurface::FallbackReasonSnapshotForTransferToImageBitmap;
    case SnapshotReasonUnitTests:
        return RecordingImageBufferSurface::FallbackReasonSnapshotForUnitTests;
    case SnapshotReasonGetCopiedImage:
        return RecordingImageBufferSurface::FallbackReasonSnapshotGetCopiedImage;
    case SnapshotReasonWebGLDrawImageIntoBuffer:
        return RecordingImageBufferSurface::FallbackReasonSnapshotWebGLDrawImageIntoBuffer;
    }
    ASSERT_NOT_REACHED();
    return RecordingImageBufferSurface::FallbackReasonUnknown;
}

PassRefPtr<SkImage> RecordingImageBufferSurface::newImageSnapshot(AccelerationHint hint, SnapshotReason reason)
{
    if (!m_fallbackSurface)
        fallBackToRasterCanvas(snapshotReasonToFallbackReason(reason));
    return m_fallbackSurface->newImageSnapshot(hint, reason);
}

SkCanvas* RecordingImageBufferSurface::canvas()
{
    if (m_fallbackSurface)
        return m_fallbackSurface->canvas();

    ASSERT(m_currentFrame->getRecordingCanvas());
    return m_currentFrame->getRecordingCanvas();
}

static RecordingImageBufferSurface::FallbackReason disableDeferralReasonToFallbackReason(DisableDeferralReason reason)
{
    switch (reason) {
    case DisableDeferralReasonUnknown:
        return RecordingImageBufferSurface::FallbackReasonUnknown;
    case DisableDeferralReasonExpensiveOverdrawHeuristic:
        return RecordingImageBufferSurface::FallbackReasonExpensiveOverdrawHeuristic;
    case DisableDeferralReasonUsingTextureBackedPattern:
        return RecordingImageBufferSurface::FallbackReasonTextureBackedPattern;
    case DisableDeferralReasonDrawImageOfVideo:
        return RecordingImageBufferSurface::FallbackReasonDrawImageOfVideo;
    case DisableDeferralReasonDrawImageOfAnimated2dCanvas:
        return RecordingImageBufferSurface::FallbackReasonDrawImageOfAnimated2dCanvas;
    case DisableDeferralReasonSubPixelTextAntiAliasingSupport:
        return RecordingImageBufferSurface::FallbackReasonSubPixelTextAntiAliasingSupport;
    case DisableDeferralDrawImageWithTextureBackedSourceImage:
        return RecordingImageBufferSurface::FallbackReasonDrawImageWithTextureBackedSourceImage;
    case DisableDeferralReasonCount:
        ASSERT_NOT_REACHED();
        break;
    }
    ASSERT_NOT_REACHED();
    return RecordingImageBufferSurface::FallbackReasonUnknown;
}

void RecordingImageBufferSurface::disableDeferral(DisableDeferralReason reason)
{
    if (!m_fallbackSurface)
        fallBackToRasterCanvas(disableDeferralReasonToFallbackReason(reason));
}

PassRefPtr<SkPicture> RecordingImageBufferSurface::getPicture()
{
    if (m_fallbackSurface)
        return nullptr;

    FallbackReason fallbackReason = FallbackReasonUnknown;
    bool canUsePicture = finalizeFrameInternal(&fallbackReason);
    m_imageBuffer->didFinalizeFrame();

    ASSERT(canUsePicture || m_fallbackFactory);

    if (canUsePicture) {
        return m_previousFrame;
    }

    if (!m_fallbackSurface)
        fallBackToRasterCanvas(fallbackReason);
    return nullptr;
}

void RecordingImageBufferSurface::finalizeFrame(const FloatRect &dirtyRect)
{
    if (m_fallbackSurface) {
        m_fallbackSurface->finalizeFrame(dirtyRect);
        return;
    }

    FallbackReason fallbackReason = FallbackReasonUnknown;
    if (!finalizeFrameInternal(&fallbackReason))
        fallBackToRasterCanvas(fallbackReason);
}

static RecordingImageBufferSurface::FallbackReason flushReasonToFallbackReason(FlushReason reason)
{
    switch (reason) {
    case FlushReasonUnknown:
        return RecordingImageBufferSurface::FallbackReasonUnknown;
    case FlushReasonInitialClear:
        return RecordingImageBufferSurface::FallbackReasonFlushInitialClear;
    case FlushReasonDrawImageOfWebGL:
        return RecordingImageBufferSurface::FallbackReasonFlushForDrawImageOfWebGL;
    }
    ASSERT_NOT_REACHED();
    return RecordingImageBufferSurface::FallbackReasonUnknown;
}

void RecordingImageBufferSurface::flush(FlushReason reason)
{
    if (!m_fallbackSurface)
        fallBackToRasterCanvas(flushReasonToFallbackReason(reason));
    m_fallbackSurface->flush(reason);
}

void RecordingImageBufferSurface::willOverwriteCanvas()
{
    m_frameWasCleared = true;
    m_previousFrame.clear();
    m_previousFrameHasExpensiveOp = false;
    m_previousFramePixelCount = 0;
    if (m_didRecordDrawCommandsInCurrentFrame) {
        // Discard previous draw commands
        m_currentFrame->finishRecordingAsPicture();
        initializeCurrentFrame();
    }
}

void RecordingImageBufferSurface::didDraw(const FloatRect& rect)
{
    m_didRecordDrawCommandsInCurrentFrame = true;
    IntRect pixelBounds = enclosingIntRect(rect);
    m_currentFramePixelCount += pixelBounds.width() * pixelBounds.height();
}

bool RecordingImageBufferSurface::finalizeFrameInternal(FallbackReason* fallbackReason)
{
    ASSERT(!m_fallbackSurface);
    ASSERT(m_currentFrame);
    ASSERT(m_currentFrame->getRecordingCanvas());
    ASSERT(fallbackReason);
    ASSERT(*fallbackReason == FallbackReasonUnknown);

    if (!m_imageBuffer->isDirty()) {
        if (!m_previousFrame) {
            // Create an initial blank frame
            m_previousFrame = fromSkSp(m_currentFrame->finishRecordingAsPicture());
            initializeCurrentFrame();
        }
        return m_currentFrame.get();
    }

    if (!m_frameWasCleared) {
        *fallbackReason = FallbackReasonCanvasNotClearedBetweenFrames;
        return false;
    }

    if (m_fallbackFactory && m_currentFrame->getRecordingCanvas()->getSaveCount() > ExpensiveCanvasHeuristicParameters::ExpensiveRecordingStackDepth) {
        *fallbackReason = FallbackReasonRunawayStateStack;
        return false;
    }

    m_previousFrame = fromSkSp(m_currentFrame->finishRecordingAsPicture());
    m_previousFrameHasExpensiveOp = m_currentFrameHasExpensiveOp;
    m_previousFramePixelCount = m_currentFramePixelCount;
    initializeCurrentFrame();

    m_frameWasCleared = false;
    return true;
}

void RecordingImageBufferSurface::draw(GraphicsContext& context, const FloatRect& destRect, const FloatRect& srcRect, SkXfermode::Mode op)
{
    if (m_fallbackSurface) {
        m_fallbackSurface->draw(context, destRect, srcRect, op);
        return;
    }

    RefPtr<SkPicture> picture = getPicture();
    if (picture) {
        context.compositePicture(picture.get(), destRect, srcRect, op);
    } else {
        ImageBufferSurface::draw(context, destRect, srcRect, op);
    }
}

bool RecordingImageBufferSurface::isExpensiveToPaint()
{
    if (m_fallbackSurface)
        return m_fallbackSurface->isExpensiveToPaint();

    if (m_didRecordDrawCommandsInCurrentFrame) {
        if (m_currentFrameHasExpensiveOp)
            return true;

        if (m_currentFramePixelCount >= (size().width() * size().height() * ExpensiveCanvasHeuristicParameters::ExpensiveOverdrawThreshold))
            return true;

        if (m_frameWasCleared)
            return false; // early exit because previous frame is overdrawn
    }

    if (m_previousFrame) {
        if (m_previousFrameHasExpensiveOp)
            return true;

        if (m_previousFramePixelCount >= (size().width() * size().height() * ExpensiveCanvasHeuristicParameters::ExpensiveOverdrawThreshold))
            return true;
    }

    return false;
}

// Fallback passthroughs

bool RecordingImageBufferSurface::restore()
{
    if (m_fallbackSurface)
        return m_fallbackSurface->restore();
    return ImageBufferSurface::restore();
}

WebLayer* RecordingImageBufferSurface::layer() const
{
    if (m_fallbackSurface)
        return m_fallbackSurface->layer();
    return ImageBufferSurface::layer();
}

bool RecordingImageBufferSurface::isAccelerated() const
{
    if (m_fallbackSurface)
        return m_fallbackSurface->isAccelerated();
    return ImageBufferSurface::isAccelerated();
}

void RecordingImageBufferSurface::setIsHidden(bool hidden)
{
    if (m_fallbackSurface)
        m_fallbackSurface->setIsHidden(hidden);
    else
        ImageBufferSurface::setIsHidden(hidden);
}

} // namespace blink
