// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RecordingImageBufferSurface_h
#define RecordingImageBufferSurface_h

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageBufferSurface.h"
#include "public/platform/WebThread.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"
#include <memory>

class SkCanvas;
class SkPicture;
class SkPictureRecorder;

namespace blink {

class ImageBuffer;
class RecordingImageBufferSurfaceTest;

class RecordingImageBufferFallbackSurfaceFactory {
    USING_FAST_MALLOC(RecordingImageBufferFallbackSurfaceFactory);
    WTF_MAKE_NONCOPYABLE(RecordingImageBufferFallbackSurfaceFactory);
public:
    virtual std::unique_ptr<ImageBufferSurface> createSurface(const IntSize&, OpacityMode) = 0;
    virtual ~RecordingImageBufferFallbackSurfaceFactory() { }
protected:
    RecordingImageBufferFallbackSurfaceFactory() { }
};

class PLATFORM_EXPORT RecordingImageBufferSurface : public ImageBufferSurface {
    WTF_MAKE_NONCOPYABLE(RecordingImageBufferSurface); USING_FAST_MALLOC(RecordingImageBufferSurface);
public:
    // If the fallbackFactory is null the buffer surface should only be used
    // for one frame and should not be used for any operations which need a
    // raster surface, (i.e. writePixels).
    // Only #getPicture should be used to access the resulting frame.
    RecordingImageBufferSurface(const IntSize&, std::unique_ptr<RecordingImageBufferFallbackSurfaceFactory> fallbackFactory = nullptr, OpacityMode = NonOpaque);
    ~RecordingImageBufferSurface() override;

    // Implementation of ImageBufferSurface interfaces
    SkCanvas* canvas() override;
    void disableDeferral(DisableDeferralReason) override;
    PassRefPtr<SkPicture> getPicture() override;
    void flush(FlushReason) override;
    void didDraw(const FloatRect&) override;
    bool isValid() const override { return true; }
    bool isRecording() const override { return !m_fallbackSurface; }
    bool writePixels(const SkImageInfo& origInfo, const void* pixels, size_t rowBytes, int x, int y) override;
    void willOverwriteCanvas() override;
    virtual void finalizeFrame(const FloatRect&);
    void setImageBuffer(ImageBuffer*) override;
    PassRefPtr<SkImage> newImageSnapshot(AccelerationHint, SnapshotReason) override;
    void draw(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect, SkXfermode::Mode) override;
    bool isExpensiveToPaint() override;
    void setHasExpensiveOp() override { m_currentFrameHasExpensiveOp = true; }

    // Passthroughs to fallback surface
    bool restore() override;
    WebLayer* layer() const override;
    bool isAccelerated() const override;
    void setIsHidden(bool) override;

    // This enum is used in a UMA histogram.
    enum FallbackReason {
        FallbackReasonUnknown = 0, // This value should never appear in production histograms
        FallbackReasonCanvasNotClearedBetweenFrames = 1,
        FallbackReasonRunawayStateStack = 2,
        FallbackReasonWritePixels = 3,
        FallbackReasonFlushInitialClear = 4,
        FallbackReasonFlushForDrawImageOfWebGL = 5,
        FallbackReasonSnapshotForGetImageData = 6,
        FallbackReasonSnapshotForCopyToWebGLTexture = 7,
        FallbackReasonSnapshotForPaint = 8,
        FallbackReasonSnapshotForToDataURL = 9,
        FallbackReasonSnapshotForToBlob = 10,
        FallbackReasonSnapshotForCanvasListenerCapture = 11,
        FallbackReasonSnapshotForDrawImage = 12,
        FallbackReasonSnapshotForCreatePattern = 13,
        FallbackReasonExpensiveOverdrawHeuristic = 14,
        FallbackReasonTextureBackedPattern = 15,
        FallbackReasonDrawImageOfVideo = 16,
        FallbackReasonDrawImageOfAnimated2dCanvas = 17,
        FallbackReasonSubPixelTextAntiAliasingSupport = 18,
        FallbackReasonDrawImageWithTextureBackedSourceImage = 19,
        FallbackReasonSnapshotForTransferToImageBitmap = 20,
        FallbackReasonSnapshotForUnitTests = 21, // This value should never appear in production histograms
        FallbackReasonSnapshotGetCopiedImage = 22,
        FallbackReasonSnapshotWebGLDrawImageIntoBuffer = 23,
        FallbackReasonCount,
    };
private:
    friend class RecordingImageBufferSurfaceTest; // for unit testing
    void fallBackToRasterCanvas(FallbackReason);
    void initializeCurrentFrame();
    bool finalizeFrameInternal(FallbackReason*);
    int approximateOpCount();

    std::unique_ptr<SkPictureRecorder> m_currentFrame;
    RefPtr<SkPicture> m_previousFrame;
    std::unique_ptr<ImageBufferSurface> m_fallbackSurface;
    ImageBuffer* m_imageBuffer;
    int m_initialSaveCount;
    int m_currentFramePixelCount;
    int m_previousFramePixelCount;
    bool m_frameWasCleared;
    bool m_didRecordDrawCommandsInCurrentFrame;
    bool m_currentFrameHasExpensiveOp;
    bool m_previousFrameHasExpensiveOp;
    std::unique_ptr<RecordingImageBufferFallbackSurfaceFactory> m_fallbackFactory;
};

} // namespace blink

#endif
