// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SkiaTextureHolder_h
#define SkiaTextureHolder_h

#include "platform/PlatformExport.h"
#include "platform/graphics/TextureHolder.h"

namespace blink {

class PLATFORM_EXPORT SkiaTextureHolder final : public TextureHolder {
 public:
  ~SkiaTextureHolder() override;

  bool isSkiaTextureHolder() final { return true; }
  bool isMailboxTextureHolder() final { return false; }
  unsigned sharedContextId() final;
  IntSize size() const final {
    return IntSize(m_image->width(), m_image->height());
  }
  bool currentFrameKnownToBeOpaque(Image::MetadataMode) final {
    return m_image->isOpaque();
  }

  sk_sp<SkImage> skImage() final { return m_image; }
  void setSharedContextId(unsigned contextId) final {
    m_sharedContextId = contextId;
  }

  // When creating a AcceleratedStaticBitmap from a texture-backed SkImage, this
  // function will be called to create a TextureHolder object.
  SkiaTextureHolder(sk_sp<SkImage>);
  // This function consumes the mailbox in the input parameter and turn it into
  // a texture-backed SkImage.
  SkiaTextureHolder(std::unique_ptr<TextureHolder>);

 private:
  void releaseImageThreadSafe();

  // The m_image should always be texture-backed
  sk_sp<SkImage> m_image;
  // Id of the shared context where m_image was created
  unsigned m_sharedContextId = 0;
};

}  // namespace blink

#endif  // SkiaTextureHolder_h
