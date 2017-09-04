// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StaticBitmapImage_h
#define StaticBitmapImage_h

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "platform/graphics/Image.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class WebGraphicsContext3DProvider;

class PLATFORM_EXPORT StaticBitmapImage : public Image {
 public:
  static PassRefPtr<StaticBitmapImage> create(sk_sp<SkImage>);

  // Methods overrided by all sub-classes
  virtual ~StaticBitmapImage() {}
  bool currentFrameKnownToBeOpaque(MetadataMode = UseCurrentMetadata) = 0;
  sk_sp<SkImage> imageForCurrentFrame() = 0;
  bool isTextureBacked() = 0;
  void draw(SkCanvas*,
            const SkPaint&,
            const FloatRect& dstRect,
            const FloatRect& srcRect,
            RespectImageOrientationEnum,
            ImageClampingMode) = 0;

  // Methods have common implementation for all sub-classes
  bool currentFrameIsComplete() override { return true; }
  void destroyDecodedData() {}

  // Methods that have a default implementation, and overrided by only one
  // sub-class
  virtual bool hasMailbox() { return false; }

  // Methods overrided by AcceleratedStaticBitmapImage only
  virtual void copyToTexture(WebGraphicsContext3DProvider*,
                             GLuint,
                             GLenum,
                             GLenum,
                             bool) {
    NOTREACHED();
  }
  virtual void ensureMailbox() { NOTREACHED(); }
  virtual gpu::Mailbox mailbox() {
    NOTREACHED();
    return gpu::Mailbox();
  }
  virtual gpu::SyncToken syncToken() {
    NOTREACHED();
    return gpu::SyncToken();
  }
  virtual void updateSyncToken(gpu::SyncToken) { NOTREACHED(); }

  // Methods have exactly the same implementation for all sub-classes
  bool originClean() const { return m_isOriginClean; }
  void setOriginClean(bool flag) { m_isOriginClean = flag; }
  bool isPremultiplied() const { return m_isPremultiplied; }
  void setPremultiplied(bool flag) { m_isPremultiplied = flag; }

 protected:
  // Helper for sub-classes
  void drawHelper(SkCanvas*,
                  const SkPaint&,
                  const FloatRect&,
                  const FloatRect&,
                  ImageClampingMode,
                  sk_sp<SkImage>);

  // These two properties are here because the SkImage API doesn't expose the
  // info. They applied to both UnacceleratedStaticBitmapImage and
  // AcceleratedStaticBitmapImage. To change these two properties, the call
  // site would have to call the API setOriginClean() and setPremultiplied().
  bool m_isOriginClean = true;
  bool m_isPremultiplied = true;
};

}  // namespace blink

#endif
