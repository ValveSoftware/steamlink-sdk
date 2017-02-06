// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintGeneratedImage_h
#define PaintGeneratedImage_h

#include "platform/geometry/IntSize.h"
#include "platform/graphics/GeneratedImage.h"

class SkPicture;

namespace blink {

class PLATFORM_EXPORT PaintGeneratedImage : public GeneratedImage {
public:
    static PassRefPtr<PaintGeneratedImage> create(PassRefPtr<SkPicture> picture, const IntSize& size)
    {
        return adoptRef(new PaintGeneratedImage(picture, size));
    }
    ~PaintGeneratedImage() override { }

protected:
    void draw(SkCanvas*, const SkPaint&, const FloatRect&, const FloatRect&, RespectImageOrientationEnum, ImageClampingMode) override;
    void drawTile(GraphicsContext&, const FloatRect&) final;

    PaintGeneratedImage(PassRefPtr<SkPicture> picture, const IntSize& size)
        : GeneratedImage(size)
        , m_picture(picture)
    {
    }

    RefPtr<SkPicture> m_picture;
};

} // namespace blink

#endif // PaintGeneratedImage_h
