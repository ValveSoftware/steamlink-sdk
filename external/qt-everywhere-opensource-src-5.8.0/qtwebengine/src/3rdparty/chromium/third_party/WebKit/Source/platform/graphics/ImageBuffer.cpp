/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/ImageBuffer.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageBufferClient.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/gpu/DrawingBuffer.h"
#include "platform/graphics/gpu/Extensions3DUtil.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/image-encoders/JPEGImageEncoder.h"
#include "platform/image-encoders/PNGImageEncoder.h"
#include "platform/image-encoders/WEBPImageEncoder.h"
#include "public/platform/Platform.h"
#include "public/platform/WebExternalTextureMailbox.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "wtf/CheckedNumeric.h"
#include "wtf/MathExtras.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include "wtf/text/Base64.h"
#include "wtf/text/WTFString.h"
#include "wtf/typed_arrays/ArrayBufferContents.h"
#include <memory>

namespace blink {

std::unique_ptr<ImageBuffer> ImageBuffer::create(std::unique_ptr<ImageBufferSurface> surface)
{
    if (!surface->isValid())
        return nullptr;
    return wrapUnique(new ImageBuffer(std::move(surface)));
}

std::unique_ptr<ImageBuffer> ImageBuffer::create(const IntSize& size, OpacityMode opacityMode, ImageInitializationMode initializationMode)
{
    std::unique_ptr<ImageBufferSurface> surface(wrapUnique(new UnacceleratedImageBufferSurface(size, opacityMode, initializationMode)));
    if (!surface->isValid())
        return nullptr;
    return wrapUnique(new ImageBuffer(std::move(surface)));
}

ImageBuffer::ImageBuffer(std::unique_ptr<ImageBufferSurface> surface)
    : m_snapshotState(InitialSnapshotState)
    , m_surface(std::move(surface))
    , m_client(0)
    , m_gpuMemoryUsage(0)
{
    m_surface->setImageBuffer(this);
    updateGPUMemoryUsage();
}

intptr_t ImageBuffer::s_globalGPUMemoryUsage = 0;
unsigned ImageBuffer::s_globalAcceleratedImageBufferCount = 0;

ImageBuffer::~ImageBuffer()
{
    if (m_gpuMemoryUsage) {
        DCHECK_GT(s_globalAcceleratedImageBufferCount, 0u);
        s_globalAcceleratedImageBufferCount--;
    }
    ImageBuffer::s_globalGPUMemoryUsage -= m_gpuMemoryUsage;
}

SkCanvas* ImageBuffer::canvas() const
{
    return m_surface->canvas();
}

void ImageBuffer::disableDeferral(DisableDeferralReason reason) const
{
    return m_surface->disableDeferral(reason);
}

bool ImageBuffer::writePixels(const SkImageInfo& info, const void* pixels, size_t rowBytes, int x, int y)
{
    return m_surface->writePixels(info, pixels, rowBytes, x, y);
}

bool ImageBuffer::isSurfaceValid() const
{
    return m_surface->isValid();
}

bool ImageBuffer::isDirty()
{
    return m_client ? m_client->isDirty() : false;
}

void ImageBuffer::didFinalizeFrame()
{
    if (m_client)
        m_client->didFinalizeFrame();
}

void ImageBuffer::finalizeFrame(const FloatRect &dirtyRect)
{
    m_surface->finalizeFrame(dirtyRect);
    didFinalizeFrame();
}

bool ImageBuffer::restoreSurface() const
{
    return m_surface->isValid() || m_surface->restore();
}

void ImageBuffer::notifySurfaceInvalid()
{
    if (m_client)
        m_client->notifySurfaceInvalid();
}

void ImageBuffer::resetCanvas(SkCanvas* canvas) const
{
    if (m_client)
        m_client->restoreCanvasMatrixClipStack(canvas);
}

PassRefPtr<SkImage> ImageBuffer::newSkImageSnapshot(AccelerationHint hint, SnapshotReason reason) const
{
    if (m_snapshotState == InitialSnapshotState)
        m_snapshotState = DidAcquireSnapshot;

    if (!isSurfaceValid())
        return nullptr;
    return m_surface->newImageSnapshot(hint, reason);
}

PassRefPtr<Image> ImageBuffer::newImageSnapshot(AccelerationHint hint, SnapshotReason reason) const
{
    RefPtr<SkImage> snapshot = newSkImageSnapshot(hint, reason);
    if (!snapshot)
        return nullptr;
    return StaticBitmapImage::create(snapshot);
}

void ImageBuffer::didDraw(const FloatRect& rect) const
{
    if (m_snapshotState == DidAcquireSnapshot)
        m_snapshotState = DrawnToAfterSnapshot;
    m_surface->didDraw(rect);
}

WebLayer* ImageBuffer::platformLayer() const
{
    return m_surface->layer();
}

bool ImageBuffer::copyToPlatformTexture(gpu::gles2::GLES2Interface* gl, GLuint texture, GLenum internalFormat, GLenum destType, GLint level, bool premultiplyAlpha, bool flipY)
{
    if (!Extensions3DUtil::canUseCopyTextureCHROMIUM(GL_TEXTURE_2D, internalFormat, destType, level))
        return false;

    if (!isSurfaceValid())
        return false;

    RefPtr<const SkImage> textureImage = m_surface->newImageSnapshot(PreferAcceleration, SnapshotReasonCopyToWebGLTexture);
    if (!textureImage)
        return false;

    if (!m_surface->isAccelerated())
        return false;


    ASSERT(textureImage->isTextureBacked()); // isAccelerated() check above should guarantee this
    // Get the texture ID, flushing pending operations if needed.
    const GrGLTextureInfo* textureInfo = skia::GrBackendObjectToGrGLTextureInfo(textureImage->getTextureHandle(true));
    if (!textureInfo || !textureInfo->fID)
        return false;

    std::unique_ptr<WebGraphicsContext3DProvider> provider = wrapUnique(Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
    if (!provider)
        return false;
    gpu::gles2::GLES2Interface* sharedGL = provider->contextGL();

    std::unique_ptr<WebExternalTextureMailbox> mailbox = wrapUnique(new WebExternalTextureMailbox);
    mailbox->textureSize = WebSize(textureImage->width(), textureImage->height());

    // Contexts may be in a different share group. We must transfer the texture through a mailbox first
    sharedGL->GenMailboxCHROMIUM(mailbox->name);
    sharedGL->ProduceTextureDirectCHROMIUM(textureInfo->fID, textureInfo->fTarget, mailbox->name);
    const GLuint64 sharedFenceSync = sharedGL->InsertFenceSyncCHROMIUM();
    sharedGL->Flush();

    sharedGL->GenSyncTokenCHROMIUM(sharedFenceSync, mailbox->syncToken);
    mailbox->validSyncToken = true;
    gl->WaitSyncTokenCHROMIUM(mailbox->syncToken);

    GLuint sourceTexture = gl->CreateAndConsumeTextureCHROMIUM(textureInfo->fTarget, mailbox->name);

    // The canvas is stored in a premultiplied format, so unpremultiply if necessary.
    // The canvas is stored in an inverted position, so the flip semantics are reversed.
    gl->CopyTextureCHROMIUM(sourceTexture, texture, internalFormat, destType, flipY ? GL_FALSE : GL_TRUE, GL_FALSE, premultiplyAlpha ? GL_FALSE : GL_TRUE);

    gl->DeleteTextures(1, &sourceTexture);

    const GLuint64 contextFenceSync = gl->InsertFenceSyncCHROMIUM();

    gl->Flush();

    GLbyte syncToken[24];
    gl->GenSyncTokenCHROMIUM(contextFenceSync, syncToken);
    sharedGL->WaitSyncTokenCHROMIUM(syncToken);

    // Undo grContext texture binding changes introduced in this function
    provider->grContext()->resetContext(kTextureBinding_GrGLBackendState);

    return true;
}

bool ImageBuffer::copyRenderingResultsFromDrawingBuffer(DrawingBuffer* drawingBuffer, SourceDrawingBuffer sourceBuffer)
{
    if (!drawingBuffer || !m_surface->isAccelerated())
        return false;
    std::unique_ptr<WebGraphicsContext3DProvider> provider = wrapUnique(Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
    if (!provider)
        return false;
    gpu::gles2::GLES2Interface* gl = provider->contextGL();
    GLuint textureId = m_surface->getBackingTextureHandleForOverwrite();
    if (!textureId)
        return false;

    gl->Flush();

    return drawingBuffer->copyToPlatformTexture(gl, textureId, GL_RGBA,
        GL_UNSIGNED_BYTE, 0, true, false, sourceBuffer);
}

void ImageBuffer::draw(GraphicsContext& context, const FloatRect& destRect, const FloatRect* srcPtr, SkXfermode::Mode op)
{
    if (!isSurfaceValid())
        return;

    FloatRect srcRect = srcPtr ? *srcPtr : FloatRect(FloatPoint(), FloatSize(size()));
    m_surface->draw(context, destRect, srcRect, op);
}

void ImageBuffer::flush(FlushReason reason)
{
    if (m_surface->canvas()) {
        m_surface->flush(reason);
    }
}

void ImageBuffer::flushGpu(FlushReason reason)
{
    if (m_surface->canvas()) {
        m_surface->flushGpu(reason);
    }
}

bool ImageBuffer::getImageData(Multiply multiplied, const IntRect& rect, WTF::ArrayBufferContents& contents) const
{
    CheckedNumeric<int> dataSize = 4;
    dataSize *= rect.width();
    dataSize *= rect.height();
    if (!dataSize.IsValid())
        return false;

    if (!isSurfaceValid()) {
        size_t allocSizeInBytes = rect.width() * rect.height() * 4;
        void* data;
        WTF::ArrayBufferContents::allocateMemoryOrNull(allocSizeInBytes, WTF::ArrayBufferContents::ZeroInitialize, data);
        if (!data)
            return false;
        WTF::ArrayBufferContents result(data, allocSizeInBytes, WTF::ArrayBufferContents::NotShared);
        result.transfer(contents);
        return true;
    }

    DCHECK(canvas());
    RefPtr<SkImage> snapshot = m_surface->newImageSnapshot(PreferNoAcceleration, SnapshotReasonGetImageData);
    if (!snapshot)
        return false;

    const bool mayHaveStrayArea =
        m_surface->isAccelerated() // GPU readback may fail silently
        || rect.x() < 0
        || rect.y() < 0
        || rect.maxX() > m_surface->size().width()
        || rect.maxY() > m_surface->size().height();
    size_t allocSizeInBytes = rect.width() * rect.height() * 4;
    void* data;
    WTF::ArrayBufferContents::InitializationPolicy initializationPolicy = mayHaveStrayArea ? WTF::ArrayBufferContents::ZeroInitialize : WTF::ArrayBufferContents::DontInitialize;
    WTF::ArrayBufferContents::allocateMemoryOrNull(allocSizeInBytes, initializationPolicy, data);
    if (!data)
        return false;
    WTF::ArrayBufferContents result(data, allocSizeInBytes, WTF::ArrayBufferContents::NotShared);

    SkAlphaType alphaType = (multiplied == Premultiplied) ? kPremul_SkAlphaType : kUnpremul_SkAlphaType;
    SkImageInfo info = SkImageInfo::Make(rect.width(), rect.height(), kRGBA_8888_SkColorType, alphaType);

    snapshot->readPixels(info, result.data(), 4 * rect.width(), rect.x(), rect.y());
    result.transfer(contents);
    return true;
}

void ImageBuffer::putByteArray(Multiply multiplied, const unsigned char* source, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint)
{
    if (!isSurfaceValid())
        return;

    ASSERT(sourceRect.width() > 0);
    ASSERT(sourceRect.height() > 0);

    int originX = sourceRect.x();
    int destX = destPoint.x() + sourceRect.x();
    ASSERT(destX >= 0);
    ASSERT(destX < m_surface->size().width());
    ASSERT(originX >= 0);
    ASSERT(originX < sourceRect.maxX());

    int originY = sourceRect.y();
    int destY = destPoint.y() + sourceRect.y();
    ASSERT(destY >= 0);
    ASSERT(destY < m_surface->size().height());
    ASSERT(originY >= 0);
    ASSERT(originY < sourceRect.maxY());

    const size_t srcBytesPerRow = 4 * sourceSize.width();
    const void* srcAddr = source + originY * srcBytesPerRow + originX * 4;
    SkAlphaType alphaType = (multiplied == Premultiplied) ? kPremul_SkAlphaType : kUnpremul_SkAlphaType;
    SkImageInfo info = SkImageInfo::Make(sourceRect.width(), sourceRect.height(), kRGBA_8888_SkColorType, alphaType);
    m_surface->writePixels(info, srcAddr, srcBytesPerRow, destX, destY);
}

void ImageBuffer::updateGPUMemoryUsage() const
{
    if (this->isAccelerated()) {
        // If image buffer is accelerated, we should keep track of GPU memory usage.
        int gpuBufferCount = 2;
        CheckedNumeric<intptr_t> checkedGPUUsage = 4 * gpuBufferCount;
        checkedGPUUsage *= this->size().width();
        checkedGPUUsage *= this->size().height();
        intptr_t gpuMemoryUsage = checkedGPUUsage.ValueOrDefault(std::numeric_limits<intptr_t>::max());

        if (!m_gpuMemoryUsage) // was not accelerated before
            s_globalAcceleratedImageBufferCount++;

        s_globalGPUMemoryUsage += (gpuMemoryUsage - m_gpuMemoryUsage);
        m_gpuMemoryUsage = gpuMemoryUsage;
    } else if (m_gpuMemoryUsage) {
        // In case of switching from accelerated to non-accelerated mode,
        // the GPU memory usage needs to be updated too.
        DCHECK_GT(s_globalAcceleratedImageBufferCount, 0u);
        s_globalAcceleratedImageBufferCount--;
        s_globalGPUMemoryUsage -= m_gpuMemoryUsage;
        m_gpuMemoryUsage = 0;
    }
}

bool ImageDataBuffer::encodeImage(const String& mimeType, const double& quality, Vector<unsigned char>* encodedImage) const
{
    if (mimeType == "image/jpeg") {
        if (!JPEGImageEncoder::encode(*this, quality, encodedImage))
            return false;
    } else if (mimeType == "image/webp") {
        int compressionQuality = WEBPImageEncoder::DefaultCompressionQuality;
        if (quality >= 0.0 && quality <= 1.0)
            compressionQuality = static_cast<int>(quality * 100 + 0.5);
        if (!WEBPImageEncoder::encode(*this, compressionQuality, encodedImage))
            return false;
    } else {
        if (!PNGImageEncoder::encode(*this, encodedImage))
            return false;
        ASSERT(mimeType == "image/png");
    }

    return true;
}

String ImageDataBuffer::toDataURL(const String& mimeType, const double& quality) const
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    Vector<unsigned char> result;
    if (!encodeImage(mimeType, quality, &result))
        return "data:,";

    return "data:" + mimeType + ";base64," + base64Encode(result);
}

} // namespace blink
