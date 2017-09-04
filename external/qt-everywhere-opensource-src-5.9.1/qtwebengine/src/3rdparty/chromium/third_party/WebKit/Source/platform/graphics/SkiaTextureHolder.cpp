// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/SkiaTextureHolder.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/graphics/MailboxTextureHolder.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "public/platform/Platform.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace blink {

namespace {

void releaseImage(sk_sp<SkImage>&& image,
                  std::unique_ptr<gpu::SyncToken>&& syncToken) {
  if (SharedGpuContext::isValid() && syncToken->HasData())
    SharedGpuContext::gl()->WaitSyncTokenCHROMIUM(syncToken->GetData());
  image.reset();
}

}  // namespace

SkiaTextureHolder::SkiaTextureHolder(sk_sp<SkImage> image)
    : m_image(std::move(image)),
      m_sharedContextId(SharedGpuContext::contextId()) {}

SkiaTextureHolder::SkiaTextureHolder(
    std::unique_ptr<TextureHolder> textureHolder) {
  DCHECK(textureHolder->isMailboxTextureHolder());
  const gpu::Mailbox mailbox = textureHolder->mailbox();
  const gpu::SyncToken syncToken = textureHolder->syncToken();
  const IntSize mailboxSize = textureHolder->size();

  gpu::gles2::GLES2Interface* sharedGL = SharedGpuContext::gl();
  GrContext* sharedGrContext = SharedGpuContext::gr();
  DCHECK(sharedGL &&
         sharedGrContext);  // context isValid already checked in callers

  sharedGL->WaitSyncTokenCHROMIUM(syncToken.GetConstData());
  GLuint sharedContextTextureId =
      sharedGL->CreateAndConsumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
  GrGLTextureInfo textureInfo;
  textureInfo.fTarget = GL_TEXTURE_2D;
  textureInfo.fID = sharedContextTextureId;
  GrBackendTextureDesc backendTexture;
  backendTexture.fOrigin = kBottomLeft_GrSurfaceOrigin;
  backendTexture.fWidth = mailboxSize.width();
  backendTexture.fHeight = mailboxSize.height();
  backendTexture.fConfig = kSkia8888_GrPixelConfig;
  backendTexture.fTextureHandle =
      skia::GrGLTextureInfoToGrBackendObject(textureInfo);

  sk_sp<SkImage> newImage =
      SkImage::MakeFromAdoptedTexture(sharedGrContext, backendTexture);
  releaseImageThreadSafe();
  m_image = newImage;
  m_sharedContextId = SharedGpuContext::contextId();
}

SkiaTextureHolder::~SkiaTextureHolder() {
  releaseImageThreadSafe();
}

unsigned SkiaTextureHolder::sharedContextId() {
  return m_sharedContextId;
}

void SkiaTextureHolder::releaseImageThreadSafe() {
  // If m_image belongs to a GrContext that is on another thread, it
  // must be released on that thread.
  if (textureThreadTaskRunner() && m_image && wasTransferred() &&
      SharedGpuContext::isValid()) {
    gpu::gles2::GLES2Interface* sharedGL = SharedGpuContext::gl();
    std::unique_ptr<gpu::SyncToken> releaseSyncToken(new gpu::SyncToken);
    const GLuint64 fenceSync = sharedGL->InsertFenceSyncCHROMIUM();
    sharedGL->Flush();
    sharedGL->GenSyncTokenCHROMIUM(fenceSync, releaseSyncToken->GetData());
    textureThreadTaskRunner()->postTask(
        BLINK_FROM_HERE,
        crossThreadBind(&releaseImage, passed(std::move(m_image)),
                        passed(std::move(releaseSyncToken))));
  }
  m_image = nullptr;
  setWasTransferred(false);
  setTextureThreadTaskRunner(nullptr);
}

}  // namespace blink
