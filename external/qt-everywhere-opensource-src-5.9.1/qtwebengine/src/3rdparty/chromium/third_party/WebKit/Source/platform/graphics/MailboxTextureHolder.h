// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MailboxTextureHolder_h
#define MailboxTextureHolder_h

#include "platform/PlatformExport.h"
#include "platform/graphics/TextureHolder.h"
#include "platform/graphics/WebGraphicsContext3DProviderWrapper.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "wtf/WeakPtr.h"

namespace blink {

class PLATFORM_EXPORT MailboxTextureHolder final : public TextureHolder {
 public:
  ~MailboxTextureHolder() override;

  bool isSkiaTextureHolder() final { return false; }
  bool isMailboxTextureHolder() final { return true; }
  unsigned sharedContextId() final;
  IntSize size() const final { return m_size; }
  bool currentFrameKnownToBeOpaque(Image::MetadataMode) final { return false; }

  gpu::Mailbox mailbox() final { return m_mailbox; }
  gpu::SyncToken syncToken() final { return m_syncToken; }
  void updateSyncToken(gpu::SyncToken syncToken) final {
    m_syncToken = syncToken;
  }

  // In WebGL's commit or transferToImageBitmap calls, it will call the
  // DrawingBuffer::transferToStaticBitmapImage function, which produces the
  // input parameters for this method.
  MailboxTextureHolder(const gpu::Mailbox&,
                       const gpu::SyncToken&,
                       unsigned textureIdToDeleteAfterMailboxConsumed,
                       WeakPtr<WebGraphicsContext3DProviderWrapper>,
                       IntSize mailboxSize);
  // This function turns a texture-backed SkImage into a mailbox and a
  // syncToken.
  MailboxTextureHolder(std::unique_ptr<TextureHolder>);

 private:
  void releaseTextureThreadSafe();

  gpu::Mailbox m_mailbox;
  gpu::SyncToken m_syncToken;
  unsigned m_textureId;
  WeakPtr<WebGraphicsContext3DProviderWrapper> m_contextProvider;
  IntSize m_size;
  bool m_isConvertedFromSkiaTexture;
};

}  // namespace blink

#endif  // MailboxTextureHolder_h
