// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/StaticBitmapImage.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageObserver.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

PassRefPtr<StaticBitmapImage> StaticBitmapImage::create(PassRefPtr<SkImage> image)
{
    if (!image)
        return nullptr;
    return adoptRef(new StaticBitmapImage(image));
}

PassRefPtr<StaticBitmapImage> StaticBitmapImage::create(WebExternalTextureMailbox& mailbox)
{
    return adoptRef(new StaticBitmapImage(mailbox));
}

StaticBitmapImage::StaticBitmapImage(PassRefPtr<SkImage> image) : m_image(image)
{
    ASSERT(m_image);
}

StaticBitmapImage::StaticBitmapImage(WebExternalTextureMailbox& mailbox) : m_mailbox(mailbox)
{
}

StaticBitmapImage::~StaticBitmapImage() { }

IntSize StaticBitmapImage::size() const
{
    if (!m_image)
        return IntSize(m_mailbox.textureSize.width, m_mailbox.textureSize.height);
    return IntSize(m_image->width(), m_image->height());
}

bool StaticBitmapImage::isTextureBacked()
{
    return m_image && m_image->isTextureBacked();
}

bool StaticBitmapImage::currentFrameKnownToBeOpaque(MetadataMode)
{
    return m_image->isOpaque();
}

void StaticBitmapImage::draw(SkCanvas* canvas, const SkPaint& paint, const FloatRect& dstRect,
    const FloatRect& srcRect, RespectImageOrientationEnum, ImageClampingMode clampMode)
{
    FloatRect adjustedSrcRect = srcRect;
    adjustedSrcRect.intersect(SkRect::Make(m_image->bounds()));

    if (dstRect.isEmpty() || adjustedSrcRect.isEmpty())
        return; // Nothing to draw.

    canvas->drawImageRect(m_image.get(), adjustedSrcRect, dstRect, &paint,
        WebCoreClampingModeToSkiaRectConstraint(clampMode));

    if (ImageObserver* observer = getImageObserver())
        observer->didDraw(this);
}

GLuint StaticBitmapImage::switchStorageToSkImage(WebGraphicsContext3DProvider* provider)
{
    if (!provider)
        return 0;
    GrContext* grContext = provider->grContext();
    if (!grContext)
        return 0;
    gpu::gles2::GLES2Interface* gl = provider->contextGL();
    if (!gl)
        return 0;
    gl->WaitSyncTokenCHROMIUM(m_mailbox.syncToken);
    GLuint textureId = gl->CreateAndConsumeTextureCHROMIUM(GL_TEXTURE_2D, m_mailbox.name);
    GrGLTextureInfo textureInfo;
    textureInfo.fTarget = GL_TEXTURE_2D;
    textureInfo.fID = textureId;
    GrBackendTextureDesc backendTexture;
    backendTexture.fOrigin = kBottomLeft_GrSurfaceOrigin;
    backendTexture.fWidth = m_mailbox.textureSize.width;
    backendTexture.fHeight = m_mailbox.textureSize.height;
    backendTexture.fConfig = kSkia8888_GrPixelConfig;
    backendTexture.fTextureHandle = skia::GrGLTextureInfoToGrBackendObject(textureInfo);
    sk_sp<SkImage> skImage = SkImage::MakeFromAdoptedTexture(grContext, backendTexture);
    m_image = fromSkSp(skImage);
    return textureId;
}

void StaticBitmapImage::copyToTexture(WebGraphicsContext3DProvider* provider, GLuint destinationTexture, GLenum internalFormat, GLenum destType, bool flipY)
{
    GLuint textureId = textureIdForWebGL(provider);
    gpu::gles2::GLES2Interface* gl = provider->contextGL();
    if (!gl)
        return;
    gl->CopyTextureCHROMIUM(textureId, destinationTexture, internalFormat, destType, flipY, false, false);
    const GLuint64 fenceSync = gl->InsertFenceSyncCHROMIUM();
    gl->Flush();
    GLbyte syncToken[24];
    gl->GenSyncTokenCHROMIUM(fenceSync, syncToken);
}

bool StaticBitmapImage::switchStorageToMailbox(WebGraphicsContext3DProvider* provider)
{
    m_mailbox.textureSize = WebSize(m_image->width(), m_image->height());
    GrContext* grContext = provider->grContext();
    if (!grContext)
        return false;
    grContext->flush();
    m_mailbox.textureTarget = GL_TEXTURE_2D;
    gpu::gles2::GLES2Interface* gl = provider->contextGL();
    if (!gl)
        return false;
    GLuint textureID = skia::GrBackendObjectToGrGLTextureInfo(m_image->getTextureHandle(true))->fID;
    gl->BindTexture(GL_TEXTURE_2D, textureID);

    gl->GenMailboxCHROMIUM(m_mailbox.name);
    gl->ProduceTextureCHROMIUM(GL_TEXTURE_2D, m_mailbox.name);
    const GLuint64 fenceSync = gl->InsertFenceSyncCHROMIUM();
    gl->Flush();
    gl->GenSyncTokenCHROMIUM(fenceSync, m_mailbox.syncToken);
    m_mailbox.validSyncToken = true;
    gl->BindTexture(GL_TEXTURE_2D, 0);
    grContext->resetContext(kTextureBinding_GrGLBackendState);
    return true;
}

// This function is called only in the case that m_image is texture backed.
GLuint StaticBitmapImage::textureIdForWebGL(WebGraphicsContext3DProvider* contextProvider)
{
    DCHECK(!m_image || m_image->isTextureBacked());
    GLuint textureId = 0;
    if (m_image) {
        // SkImage is texture-backed on the shared context
        if (!hasMailbox()) {
            std::unique_ptr<WebGraphicsContext3DProvider> sharedProvider = wrapUnique(Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
            if (!switchStorageToMailbox(sharedProvider.get()))
                return 0;
            textureId = switchStorageToSkImage(contextProvider);
            // Get a new mailbox because we cannot retain a texture in the WebGL context.
            if (!switchStorageToMailbox(contextProvider))
                return 0;
            return textureId;
        }
    }
    DCHECK(hasMailbox());
    textureId = switchStorageToSkImage(contextProvider);
    if (!switchStorageToMailbox(contextProvider))
        return 0;
    return textureId;
}

PassRefPtr<SkImage> StaticBitmapImage::imageForCurrentFrame()
{
    if (m_image)
        return m_image;
    // No mailbox, return null;
    if (!hasMailbox())
        return nullptr;
    // Has mailbox, consume mailbox, prepare a new mailbox if contextProvider is not null (3D).
    DCHECK(isMainThread());
    // TODO(xidachen): make this work on a worker thread.
    std::unique_ptr<WebGraphicsContext3DProvider> sharedProvider = wrapUnique(Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
    if (!switchStorageToSkImage(sharedProvider.get()))
        return nullptr;
    return m_image;
}

} // namespace blink
