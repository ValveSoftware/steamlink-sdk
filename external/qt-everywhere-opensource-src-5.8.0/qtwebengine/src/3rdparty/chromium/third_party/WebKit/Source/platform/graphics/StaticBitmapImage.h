// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StaticBitmapImage_h
#define StaticBitmapImage_h

#include "platform/graphics/Image.h"
#include "public/platform/WebExternalTextureMailbox.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace blink {

class WebGraphicsContext3DProvider;

class PLATFORM_EXPORT StaticBitmapImage final : public Image {
public:
    ~StaticBitmapImage() override;

    bool currentFrameIsComplete() override { return true; }

    static PassRefPtr<StaticBitmapImage> create(PassRefPtr<SkImage>);
    static PassRefPtr<StaticBitmapImage> create(WebExternalTextureMailbox&);
    virtual void destroyDecodedData() { }
    virtual bool currentFrameKnownToBeOpaque(MetadataMode = UseCurrentMetadata);
    virtual IntSize size() const;
    void draw(SkCanvas*, const SkPaint&, const FloatRect& dstRect, const FloatRect& srcRect, RespectImageOrientationEnum, ImageClampingMode) override;

    PassRefPtr<SkImage> imageForCurrentFrame() override;

    bool originClean() const { return m_isOriginClean; }
    void setOriginClean(bool flag) { m_isOriginClean = flag; }
    bool isPremultiplied() const { return m_isPremultiplied; }
    void setPremultiplied(bool flag) { m_isPremultiplied = flag; }
    void copyToTexture(WebGraphicsContext3DProvider*, GLuint, GLenum, GLenum, bool);
    bool isTextureBacked() override;
    bool hasMailbox() { return m_mailbox.textureSize.width != 0 && m_mailbox.textureSize.height != 0; }

protected:
    StaticBitmapImage(PassRefPtr<SkImage>);
    StaticBitmapImage(WebExternalTextureMailbox&);

private:
    GLuint switchStorageToSkImage(WebGraphicsContext3DProvider*);
    bool switchStorageToMailbox(WebGraphicsContext3DProvider*);
    GLuint textureIdForWebGL(WebGraphicsContext3DProvider*);

    RefPtr<SkImage> m_image;
    WebExternalTextureMailbox m_mailbox;
    bool m_isOriginClean = true;
    // The premultiply info is stored here because the SkImage API
    // doesn't expose this info.
    bool m_isPremultiplied = true;
};

} // namespace blink

#endif
