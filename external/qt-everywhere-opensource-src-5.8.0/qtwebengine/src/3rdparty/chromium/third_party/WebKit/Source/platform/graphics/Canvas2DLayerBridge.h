/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Canvas2DLayerBridge_h
#define Canvas2DLayerBridge_h

#include "platform/PlatformExport.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/ImageBufferSurface.h"
#include "public/platform/WebExternalTextureLayer.h"
#include "public/platform/WebExternalTextureLayerClient.h"
#include "public/platform/WebExternalTextureMailbox.h"
#include "public/platform/WebThread.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "wtf/Allocator.h"
#include "wtf/Deque.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/WeakPtr.h"
#include <memory>

class SkImage;
struct SkImageInfo;
class SkPictureRecorder;

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace blink {

class Canvas2DLayerBridgeHistogramLogger;
class Canvas2DLayerBridgeTest;
class ImageBuffer;
class WebGraphicsContext3DProvider;
class SharedContextRateLimiter;

#if OS(MACOSX)
// Canvas hibernation is currently disabled on MacOS X due to a bug that causes content loss
// TODO: Find a better fix for crbug.com/588434
#define CANVAS2D_HIBERNATION_ENABLED 0

// IOSurfaces are a primitive only present on OS X.
#define USE_IOSURFACE_FOR_2D_CANVAS 1
#else
#define CANVAS2D_HIBERNATION_ENABLED 1
#define USE_IOSURFACE_FOR_2D_CANVAS 0
#endif

// TODO: Fix background rendering and remove this workaround. crbug.com/600386
#define CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU 0

class PLATFORM_EXPORT Canvas2DLayerBridge : public WebExternalTextureLayerClient, public WebThread::TaskObserver, public RefCounted<Canvas2DLayerBridge> {
    WTF_MAKE_NONCOPYABLE(Canvas2DLayerBridge);
public:
    enum AccelerationMode {
        DisableAcceleration,
        EnableAcceleration,
        ForceAccelerationForTesting,
    };

    static PassRefPtr<Canvas2DLayerBridge> create(const IntSize&, int msaaSampleCount, OpacityMode, AccelerationMode);

    ~Canvas2DLayerBridge() override;

    // WebExternalTextureLayerClient implementation.
    bool prepareMailbox(WebExternalTextureMailbox*, WebExternalBitmap*) override;
    void mailboxReleased(const WebExternalTextureMailbox&, bool lostResource) override;

    // ImageBufferSurface implementation
    void finalizeFrame(const FloatRect &dirtyRect);
    void willWritePixels();
    void willOverwriteAllPixels();
    void willOverwriteCanvas();
    SkCanvas* canvas();
    void disableDeferral(DisableDeferralReason);
    bool checkSurfaceValid();
    bool restoreSurface();
    WebLayer* layer() const;
    bool isAccelerated() const;
    void setFilterQuality(SkFilterQuality);
    void setIsHidden(bool);
    void setImageBuffer(ImageBuffer*);
    void didDraw(const FloatRect&);
    bool writePixels(const SkImageInfo&, const void* pixels, size_t rowBytes, int x, int y);
    void flush();
    void flushGpu();
    void prepareSurfaceForPaintingIfNeeded();
    bool isHidden() { return m_isHidden; }

    void beginDestruction();
    void hibernate();
    bool isHibernating() const { return m_hibernationImage.get(); }

    PassRefPtr<SkImage> newImageSnapshot(AccelerationHint, SnapshotReason);

    // The values of the enum entries must not change because they are used for
    // usage metrics histograms. New values can be added to the end.
    enum HibernationEvent {
        HibernationScheduled = 0,
        HibernationAbortedDueToDestructionWhileHibernatePending = 1,
        HibernationAbortedDueToPendingDestruction = 2,
        HibernationAbortedDueToVisibilityChange = 3,
        HibernationAbortedDueGpuContextLoss = 4,
        HibernationAbortedDueToSwitchToUnacceleratedRendering = 5,
        HibernationAbortedDueToAllocationFailure = 6,
        HibernationEndedNormally = 7,
        HibernationEndedWithSwitchToBackgroundRendering = 8,
        HibernationEndedWithFallbackToSW = 9,
        HibernationEndedWithTeardown = 10,
        HibernationAbortedBecauseNoSurface = 11,

        HibernationEventCount = 12,
    };

    class PLATFORM_EXPORT Logger {
    public:
        virtual void reportHibernationEvent(HibernationEvent);
        virtual void didStartHibernating() { }
        virtual ~Logger() { }
    };

    void setLoggerForTesting(std::unique_ptr<Logger>);

private:
#if USE_IOSURFACE_FOR_2D_CANVAS
    // All information associated with a CHROMIUM image.
    struct ImageInfo {
        ImageInfo() {}
        ImageInfo(GLuint imageId, GLuint textureId, GLint gpuMemoryBufferId);

        // Whether this structure holds references to a CHROMIUM image.
        bool empty();

        // The id of the CHROMIUM image.
        GLuint m_imageId = 0;

        // The id of the texture bound to the CHROMIUM image.
        GLuint m_textureId = 0;

        // The id of the GpuMemoryBuffer backing the texture and CHROMIUM image.
        GLint m_gpuMemoryBufferId = -1;
    };
#endif // USE_IOSURFACE_FOR_2D_CANVAS

    struct MailboxInfo {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
        WebExternalTextureMailbox m_mailbox;
        RefPtr<SkImage> m_image;
        RefPtr<Canvas2DLayerBridge> m_parentLayerBridge;

#if USE_IOSURFACE_FOR_2D_CANVAS
        // If this mailbox wraps an IOSurface-backed texture, the ids of the
        // CHROMIUM image and the texture.
        ImageInfo m_imageInfo;
#endif // USE_IOSURFACE_FOR_2D_CANVAS

        MailboxInfo(const MailboxInfo&);
        MailboxInfo() {}
    };

    Canvas2DLayerBridge(std::unique_ptr<WebGraphicsContext3DProvider>, const IntSize&, int msaaSampleCount, OpacityMode, AccelerationMode);
    gpu::gles2::GLES2Interface* contextGL();
    void startRecording();
    void skipQueuedDrawCommands();
    void flushRecordingOnly();
    void unregisterTaskObserver();
    void reportSurfaceCreationFailure();

    // WebThread::TaskOberver implementation
    void willProcessTask() override;
    void didProcessTask() override;

    SkSurface* getOrCreateSurface(AccelerationHint = PreferAcceleration);
    bool shouldAccelerate(AccelerationHint) const;

    // Returns the GL filter associated with |m_filterQuality|.
    GLenum getGLFilter();

#if USE_IOSURFACE_FOR_2D_CANVAS
    // Creates an IOSurface-backed texture. Copies |image| into the texture.
    // Prepares a mailbox from the texture. The caller must have created a new
    // MailboxInfo, and prepended it to |m_mailboxs|. Returns whether the
    // mailbox was successfully prepared. |mailbox| is an out parameter only
    // populated on success.
    bool prepareIOSurfaceMailboxFromImage(SkImage*, WebExternalTextureMailbox*);

    // Creates an IOSurface-backed texture. Returns an ImageInfo, which is empty
    // on failure. The caller takes ownership of both the texture and the image.
    ImageInfo createIOSurfaceBackedTexture();

    // Releases all resources associated with a CHROMIUM image.
    void deleteCHROMIUMImage(ImageInfo);

    // Releases all resources in the CHROMIUM image cache.
    void clearCHROMIUMImageCache();
#endif // USE_IOSURFACE_FOR_2D_CANVAS

    // Prepends a new MailboxInfo object to |m_mailboxes|.
    void createMailboxInfo();

    // Returns whether the mailbox was successfully prepared from the SkImage.
    // The mailbox is an out parameter only populated on success.
    bool prepareMailboxFromImage(PassRefPtr<SkImage>, WebExternalTextureMailbox*);

    // Resets Skia's texture bindings. This method should be called after
    // changing texture bindings.
    void resetSkiaTextureBinding();

    std::unique_ptr<SkPictureRecorder> m_recorder;
    RefPtr<SkSurface> m_surface;
    RefPtr<SkImage> m_hibernationImage;
    int m_initialSurfaceSaveCount;
    std::unique_ptr<WebExternalTextureLayer> m_layer;
    std::unique_ptr<WebGraphicsContext3DProvider> m_contextProvider;
    std::unique_ptr<SharedContextRateLimiter> m_rateLimiter;
    std::unique_ptr<Logger> m_logger;
    WeakPtrFactory<Canvas2DLayerBridge> m_weakPtrFactory;
    ImageBuffer* m_imageBuffer;
    int m_msaaSampleCount;
    size_t m_bytesAllocated;
    bool m_haveRecordedDrawCommands;
    bool m_destructionInProgress;
    SkFilterQuality m_filterQuality;
    bool m_isHidden;
    bool m_isDeferralEnabled;
    bool m_isRegisteredTaskObserver;
    bool m_renderingTaskCompletedForCurrentFrame;
    bool m_softwareRenderingWhileHidden;
    bool m_surfaceCreationFailedAtLeastOnce = false;
    bool m_hibernationScheduled = false;

    friend class Canvas2DLayerBridgeTest;

    uint32_t m_lastImageId;

    enum {
        // We should normally not have more that two active mailboxes at a time,
        // but sometime we may have three due to the async nature of mailbox handling.
        MaxActiveMailboxes = 3,
    };

    Deque<MailboxInfo, MaxActiveMailboxes> m_mailboxes;
    GLenum m_lastFilter;
    AccelerationMode m_accelerationMode;
    OpacityMode m_opacityMode;
    const IntSize m_size;
    int m_recordingPixelCount;

#if USE_IOSURFACE_FOR_2D_CANVAS
    // Each element in this vector represents an IOSurface backed texture that
    // is ready to be reused.
    // Elements in this vector can safely be purged in low memory conditions.
    Vector<ImageInfo> m_imageInfoCache;
#endif // USE_IOSURFACE_FOR_2D_CANVAS
};

} // namespace blink

#endif
