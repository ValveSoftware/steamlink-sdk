// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AcceleratedStaticBitmapImage_h
#define AcceleratedStaticBitmapImage_h

#include "base/threading/thread_checker.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/TextureHolder.h"
#include "wtf/WeakPtr.h"

#include <memory>

class GrContext;

namespace blink {
class WebGraphicsContext3DProviderWrapper;
class TextureHolder;

class PLATFORM_EXPORT AcceleratedStaticBitmapImage final
    : public StaticBitmapImage {
 public:
  ~AcceleratedStaticBitmapImage() override;
  // SkImage with a texture backing that is assumed to be from the shared
  // context of the current thread.
  static PassRefPtr<AcceleratedStaticBitmapImage> createFromSharedContextImage(
      sk_sp<SkImage>);
  // Can specify the GrContext that created the texture backing. Ideally all
  // callers would use this option. The |mailbox| is a name for the texture
  // backing, allowing other contexts to use the same backing.
  static PassRefPtr<AcceleratedStaticBitmapImage> createFromWebGLContextImage(
      const gpu::Mailbox&,
      const gpu::SyncToken&,
      unsigned textureId,
      WeakPtr<WebGraphicsContext3DProviderWrapper>,
      IntSize mailboxSize);

  bool currentFrameKnownToBeOpaque(MetadataMode = UseCurrentMetadata) override;
  IntSize size() const override;
  sk_sp<SkImage> imageForCurrentFrame() override;
  bool isTextureBacked() final { return true; }

  void draw(SkCanvas*,
            const SkPaint&,
            const FloatRect& dstRect,
            const FloatRect& srcRect,
            RespectImageOrientationEnum,
            ImageClampingMode) override;

  void copyToTexture(WebGraphicsContext3DProvider*,
                     GLuint destTextureId,
                     GLenum destInternalFormat,
                     GLenum destType,
                     bool flipY) override;

  bool hasMailbox() final { return m_textureHolder->isMailboxTextureHolder(); }
  // To be called on sender thread before performing a transfer
  void transfer() final;

  void ensureMailbox() final;
  gpu::Mailbox mailbox() final { return m_textureHolder->mailbox(); }
  gpu::SyncToken syncToken() final { return m_textureHolder->syncToken(); }
  void updateSyncToken(gpu::SyncToken) final;

 private:
  AcceleratedStaticBitmapImage(sk_sp<SkImage>);
  AcceleratedStaticBitmapImage(const gpu::Mailbox&,
                               const gpu::SyncToken&,
                               unsigned textureId,
                               WeakPtr<WebGraphicsContext3DProviderWrapper>,
                               IntSize mailboxSize);

  void createImageFromMailboxIfNeeded();
  void checkThread();
  void waitSyncTokenIfNeeded();
  bool isValid();

  std::unique_ptr<TextureHolder> m_textureHolder;

  base::ThreadChecker m_threadChecker;
  bool m_detachThreadAtNextCheck = false;
};

}  // namespace blink

#endif
