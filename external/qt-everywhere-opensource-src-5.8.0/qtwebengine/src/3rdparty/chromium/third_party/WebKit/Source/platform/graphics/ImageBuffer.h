/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#ifndef ImageBuffer_h
#define ImageBuffer_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/Canvas2DLayerBridge.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/GraphicsTypes3D.h"
#include "platform/graphics/ImageBufferSurface.h"
#include "platform/transforms/AffineTransform.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include "wtf/typed_arrays/Uint8ClampedArray.h"
#include <memory>

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace WTF {

class ArrayBufferContents;

} // namespace WTF

namespace blink {

class DrawingBuffer;
class GraphicsContext;
class Image;
class ImageBufferClient;
class IntPoint;
class IntRect;

enum Multiply {
    Premultiplied,
    Unmultiplied
};

class PLATFORM_EXPORT ImageBuffer {
    WTF_MAKE_NONCOPYABLE(ImageBuffer);
    USING_FAST_MALLOC(ImageBuffer);
public:
    static std::unique_ptr<ImageBuffer> create(const IntSize&, OpacityMode = NonOpaque, ImageInitializationMode = InitializeImagePixels);
    static std::unique_ptr<ImageBuffer> create(std::unique_ptr<ImageBufferSurface>);

    virtual ~ImageBuffer();

    void setClient(ImageBufferClient* client) { m_client = client; }

    const IntSize& size() const { return m_surface->size(); }
    bool isAccelerated() const { return m_surface->isAccelerated(); }
    bool isRecording() const { return m_surface->isRecording(); }
    void setHasExpensiveOp() { m_surface->setHasExpensiveOp(); }
    bool isExpensiveToPaint() const { return m_surface->isExpensiveToPaint(); }
    void prepareSurfaceForPaintingIfNeeded() { m_surface->prepareSurfaceForPaintingIfNeeded(); }
    bool isSurfaceValid() const;
    bool restoreSurface() const;
    void didDraw(const FloatRect&) const;
    bool wasDrawnToAfterSnapshot() const { return m_snapshotState == DrawnToAfterSnapshot; }

    void setFilterQuality(SkFilterQuality filterQuality) { m_surface->setFilterQuality(filterQuality); }
    void setIsHidden(bool hidden) { m_surface->setIsHidden(hidden); }

    // Called by subclasses of ImageBufferSurface to install a new canvas object.
    // Virtual for mocking
    virtual void resetCanvas(SkCanvas*) const;

    SkCanvas* canvas() const;
    void disableDeferral(DisableDeferralReason) const;

    // Called at the end of a task that rendered a whole frame
    void finalizeFrame(const FloatRect &dirtyRect);
    void didFinalizeFrame();

    bool isDirty();

    bool writePixels(const SkImageInfo&, const void* pixels, size_t rowBytes, int x, int y);

    void willOverwriteCanvas() { m_surface->willOverwriteCanvas(); }

    bool getImageData(Multiply, const IntRect&, WTF::ArrayBufferContents&) const;

    void putByteArray(Multiply, const unsigned char* source, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint);

    AffineTransform baseTransform() const { return AffineTransform(); }
    WebLayer* platformLayer() const;

    // FIXME: current implementations of this method have the restriction that they only work
    // with textures that are RGB or RGBA format, UNSIGNED_BYTE type and level 0, as specified in
    // Extensions3D::canUseCopyTextureCHROMIUM().
    // Destroys the TEXTURE_2D binding for the active texture unit of the passed context
    bool copyToPlatformTexture(gpu::gles2::GLES2Interface*, GLuint texture, GLenum internalFormat, GLenum destType, GLint level, bool premultiplyAlpha, bool flipY);

    bool copyRenderingResultsFromDrawingBuffer(DrawingBuffer*, SourceDrawingBuffer);

    void flush(FlushReason); // process deferred draw commands immediately
    void flushGpu(FlushReason); // Like flush(), but flushes all the way down to the Gpu context if the surface is accelerated

    void notifySurfaceInvalid();

    PassRefPtr<SkImage> newSkImageSnapshot(AccelerationHint, SnapshotReason) const;
    PassRefPtr<Image> newImageSnapshot(AccelerationHint = PreferNoAcceleration, SnapshotReason = SnapshotReasonUnknown) const;

    PassRefPtr<SkPicture> getPicture() { return m_surface->getPicture(); }

    void draw(GraphicsContext&, const FloatRect&, const FloatRect*, SkXfermode::Mode);

    void updateGPUMemoryUsage() const;
    static intptr_t getGlobalGPUMemoryUsage() { return s_globalGPUMemoryUsage; }
    static unsigned getGlobalAcceleratedImageBufferCount() { return s_globalAcceleratedImageBufferCount; }
    intptr_t getGPUMemoryUsage() { return m_gpuMemoryUsage; }

protected:
    ImageBuffer(std::unique_ptr<ImageBufferSurface>);

private:
    enum SnapshotState {
        InitialSnapshotState,
        DidAcquireSnapshot,
        DrawnToAfterSnapshot,
    };
    mutable SnapshotState m_snapshotState;
    std::unique_ptr<ImageBufferSurface> m_surface;
    ImageBufferClient* m_client;

    mutable intptr_t m_gpuMemoryUsage;
    static intptr_t s_globalGPUMemoryUsage;
    static unsigned s_globalAcceleratedImageBufferCount;
};

struct ImageDataBuffer {
    STACK_ALLOCATED();
    ImageDataBuffer(const IntSize& size, const unsigned char* data) : m_data(data), m_size(size) { }
    String PLATFORM_EXPORT toDataURL(const String& mimeType, const double& quality) const;
    bool PLATFORM_EXPORT encodeImage(const String& mimeType, const double& quality, Vector<unsigned char>* encodedImage) const;
    const unsigned char* pixels() const { return m_data; }
    const IntSize& size() const { return m_size; }
    int height() const { return m_size.height(); }
    int width() const { return m_size.width(); }

    const unsigned char* m_data;
    const IntSize m_size;
};

} // namespace blink

#endif // ImageBuffer_h
