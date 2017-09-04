// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/AcceleratedStaticBitmapImage.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "platform/graphics/MailboxTextureHolder.h"
#include "platform/graphics/SkiaTextureHolder.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "third_party/skia/include/core/SkImage.h"
#include "wtf/PtrUtil.h"

#include <memory>
#include <utility>

namespace blink {

PassRefPtr<AcceleratedStaticBitmapImage>
AcceleratedStaticBitmapImage::createFromSharedContextImage(
    sk_sp<SkImage> image) {
  return adoptRef(new AcceleratedStaticBitmapImage(std::move(image)));
}

PassRefPtr<AcceleratedStaticBitmapImage>
AcceleratedStaticBitmapImage::createFromWebGLContextImage(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& syncToken,
    unsigned textureId,
    WeakPtr<WebGraphicsContext3DProviderWrapper> contextProvider,
    IntSize mailboxSize) {
  return adoptRef(new AcceleratedStaticBitmapImage(
      mailbox, syncToken, textureId, contextProvider, mailboxSize));
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(
    sk_sp<SkImage> image) {
  m_textureHolder = wrapUnique(new SkiaTextureHolder(std::move(image)));
  m_threadChecker.DetachFromThread();
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& syncToken,
    unsigned textureId,
    WeakPtr<WebGraphicsContext3DProviderWrapper> contextProvider,
    IntSize mailboxSize) {
  m_textureHolder = wrapUnique(new MailboxTextureHolder(
      mailbox, syncToken, textureId, contextProvider, mailboxSize));
  m_threadChecker.DetachFromThread();
}

AcceleratedStaticBitmapImage::~AcceleratedStaticBitmapImage() {}

IntSize AcceleratedStaticBitmapImage::size() const {
  return m_textureHolder->size();
}

void AcceleratedStaticBitmapImage::updateSyncToken(gpu::SyncToken syncToken) {
  m_textureHolder->updateSyncToken(syncToken);
}

void AcceleratedStaticBitmapImage::copyToTexture(
    WebGraphicsContext3DProvider* destProvider,
    GLuint destTextureId,
    GLenum internalFormat,
    GLenum destType,
    bool flipY) {
  checkThread();
  if (!isValid())
    return;
  // |destProvider| may not be the same context as the one used for |m_image|,
  // so we use a mailbox to generate a texture id for |destProvider| to access.
  ensureMailbox();

  // Get a texture id that |destProvider| knows about and copy from it.
  gpu::gles2::GLES2Interface* destGL = destProvider->contextGL();
  destGL->WaitSyncTokenCHROMIUM(m_textureHolder->syncToken().GetData());
  GLuint sourceTextureId = destGL->CreateAndConsumeTextureCHROMIUM(
      GL_TEXTURE_2D, m_textureHolder->mailbox().name);
  destGL->CopyTextureCHROMIUM(sourceTextureId, destTextureId, internalFormat,
                              destType, flipY, false, false);
  // This drops the |destGL| context's reference on our |m_mailbox|, but it's
  // still held alive by our SkImage.
  destGL->DeleteTextures(1, &sourceTextureId);
}

sk_sp<SkImage> AcceleratedStaticBitmapImage::imageForCurrentFrame() {
  checkThread();
  if (!isValid())
    return nullptr;
  createImageFromMailboxIfNeeded();
  return m_textureHolder->skImage();
}

void AcceleratedStaticBitmapImage::draw(SkCanvas* canvas,
                                        const SkPaint& paint,
                                        const FloatRect& dstRect,
                                        const FloatRect& srcRect,
                                        RespectImageOrientationEnum,
                                        ImageClampingMode imageClampingMode) {
  checkThread();
  if (!isValid())
    return;
  createImageFromMailboxIfNeeded();
  sk_sp<SkImage> image = m_textureHolder->skImage();
  StaticBitmapImage::drawHelper(canvas, paint, dstRect, srcRect,
                                imageClampingMode, image);
}

bool AcceleratedStaticBitmapImage::isValid() {
  if (!m_textureHolder)
    return false;
  if (!SharedGpuContext::isValid())
    return false;  // Gpu context was lost
  unsigned sharedContextId = m_textureHolder->sharedContextId();
  if (sharedContextId != SharedGpuContext::kNoSharedContext &&
      sharedContextId != SharedGpuContext::contextId()) {
    // Gpu context was lost and restored since the resource was created.
    return false;
  }
  return true;
}

void AcceleratedStaticBitmapImage::createImageFromMailboxIfNeeded() {
  if (m_textureHolder->sharedContextId() != SharedGpuContext::kNoSharedContext)
    return;
  if (m_textureHolder->isSkiaTextureHolder())
    return;
  m_textureHolder =
      wrapUnique(new SkiaTextureHolder(std::move(m_textureHolder)));
}

void AcceleratedStaticBitmapImage::ensureMailbox() {
  if (m_textureHolder->isMailboxTextureHolder())
    return;

  m_textureHolder =
      wrapUnique(new MailboxTextureHolder(std::move(m_textureHolder)));
}

void AcceleratedStaticBitmapImage::transfer() {
  checkThread();
  ensureMailbox();
  // If |m_textureThreadTaskRunner| in TextureHolder is set, it means that
  // the |m_texture| in this class has been consumed on the current thread,
  // which may happen when we have chained transfers. When that is the case,
  // we must not reset |m_imageThreadTaskRunner|, so we ensure that
  // releaseImage() or releaseTexture() is called on the right thread.
  if (!m_textureHolder->wasTransferred()) {
    WebThread* currentThread = Platform::current()->currentThread();
    m_textureHolder->setWasTransferred(true);
    m_textureHolder->setTextureThreadTaskRunner(
        currentThread->getWebTaskRunner()->clone());
  }
  m_detachThreadAtNextCheck = true;
}

bool AcceleratedStaticBitmapImage::currentFrameKnownToBeOpaque(
    MetadataMode metadataMode) {
  return m_textureHolder->currentFrameKnownToBeOpaque(metadataMode);
}

void AcceleratedStaticBitmapImage::checkThread() {
  if (m_detachThreadAtNextCheck) {
    m_threadChecker.DetachFromThread();
    m_detachThreadAtNextCheck = false;
  }
  CHECK(m_threadChecker.CalledOnValidThread());
}

}  // namespace blink
