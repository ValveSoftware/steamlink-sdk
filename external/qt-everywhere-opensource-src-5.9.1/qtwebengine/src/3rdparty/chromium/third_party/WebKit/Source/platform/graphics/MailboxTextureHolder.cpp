// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/MailboxTextureHolder.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/graphics/SkiaTextureHolder.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "public/platform/Platform.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace blink {

namespace {

void releaseTexture(
    bool isConvertedFromSkiaTexture,
    unsigned textureId,
    WeakPtr<WebGraphicsContext3DProviderWrapper> contextProvider,
    std::unique_ptr<gpu::SyncToken> syncToken) {
  if (!isConvertedFromSkiaTexture && textureId && contextProvider) {
    contextProvider->contextProvider()->contextGL()->WaitSyncTokenCHROMIUM(
        syncToken->GetData());
    contextProvider->contextProvider()->contextGL()->DeleteTextures(1,
                                                                    &textureId);
  }
}

}  // namespace

MailboxTextureHolder::MailboxTextureHolder(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& syncToken,
    unsigned textureIdToDeleteAfterMailboxConsumed,
    WeakPtr<WebGraphicsContext3DProviderWrapper> contextProvider,
    IntSize mailboxSize)
    : m_mailbox(mailbox),
      m_syncToken(syncToken),
      m_textureId(textureIdToDeleteAfterMailboxConsumed),
      m_contextProvider(contextProvider),
      m_size(mailboxSize),
      m_isConvertedFromSkiaTexture(false) {}

MailboxTextureHolder::MailboxTextureHolder(
    std::unique_ptr<TextureHolder> textureHolder) {
  DCHECK(textureHolder->isSkiaTextureHolder());
  sk_sp<SkImage> image = textureHolder->skImage();
  DCHECK(image);

  gpu::gles2::GLES2Interface* sharedGL = SharedGpuContext::gl();
  GrContext* sharedGrContext = SharedGpuContext::gr();
  if (!sharedGrContext) {
    // Can happen if the context is lost. The SkImage won't be any good now
    // anyway.
    return;
  }
  GLuint imageTextureId =
      skia::GrBackendObjectToGrGLTextureInfo(image->getTextureHandle(true))
          ->fID;
  sharedGL->BindTexture(GL_TEXTURE_2D, imageTextureId);

  sharedGL->GenMailboxCHROMIUM(m_mailbox.name);
  sharedGL->ProduceTextureCHROMIUM(GL_TEXTURE_2D, m_mailbox.name);
  const GLuint64 fenceSync = sharedGL->InsertFenceSyncCHROMIUM();
  sharedGL->Flush();
  sharedGL->GenSyncTokenCHROMIUM(fenceSync, m_syncToken.GetData());

  sharedGL->BindTexture(GL_TEXTURE_2D, 0);
  // We changed bound textures in this function, so reset the GrContext.
  sharedGrContext->resetContext(kTextureBinding_GrGLBackendState);
  m_size = IntSize(image->width(), image->height());
  m_textureId = imageTextureId;
  m_isConvertedFromSkiaTexture = true;
}

MailboxTextureHolder::~MailboxTextureHolder() {
  // Avoid leaking mailboxes in cases where the texture gets recycled by skia.
  if (SharedGpuContext::isValid()) {
    SharedGpuContext::gl()->ProduceTextureDirectCHROMIUM(0, GL_TEXTURE_2D,
                                                         m_mailbox.name);
  }
  releaseTextureThreadSafe();
}

void MailboxTextureHolder::releaseTextureThreadSafe() {
  // If this member is still null, it means we are still at the thread where
  // the m_texture was created.
  std::unique_ptr<gpu::SyncToken> passedSyncToken(
      new gpu::SyncToken(m_syncToken));
  if (!wasTransferred()) {
    releaseTexture(m_isConvertedFromSkiaTexture, m_textureId, m_contextProvider,
                   std::move(passedSyncToken));
  } else if (wasTransferred() && textureThreadTaskRunner()) {
    textureThreadTaskRunner()->postTask(
        BLINK_FROM_HERE,
        crossThreadBind(&releaseTexture, m_isConvertedFromSkiaTexture,
                        m_textureId, passed(std::move(m_contextProvider)),
                        passed(std::move(passedSyncToken))));
  }
  m_textureId = 0u;  // invalidate the texture.
  setWasTransferred(false);
  setTextureThreadTaskRunner(nullptr);
}

unsigned MailboxTextureHolder::sharedContextId() {
  return SharedGpuContext::kNoSharedContext;
}

}  // namespace blink
