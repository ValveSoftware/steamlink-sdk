// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageBitmap_h
#define ImageBitmap_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "core/imagebitmap/ImageBitmapOptions.h"
#include "core/imagebitmap/ImageBitmapSource.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include <memory>

namespace blink {

class HTMLCanvasElement;
class HTMLVideoElement;
class ImageData;
class ImageDecoder;

enum AlphaDisposition {
    PremultiplyAlpha,
    DontPremultiplyAlpha,
};

class CORE_EXPORT ImageBitmap final : public GarbageCollectedFinalized<ImageBitmap>, public ScriptWrappable, public CanvasImageSource, public ImageBitmapSource {
    DEFINE_WRAPPERTYPEINFO();
public:
    static ImageBitmap* create(HTMLImageElement*, const IntRect&, Document*, const ImageBitmapOptions& = ImageBitmapOptions());
    static ImageBitmap* create(HTMLVideoElement*, const IntRect&, Document*, const ImageBitmapOptions& = ImageBitmapOptions());
    static ImageBitmap* create(HTMLCanvasElement*, const IntRect&, const ImageBitmapOptions& = ImageBitmapOptions());
    static ImageBitmap* create(ImageData*, const IntRect&, const ImageBitmapOptions& = ImageBitmapOptions(), const bool& isImageDataPremultiplied = false, const bool& isImageDataOriginClean = true);
    static ImageBitmap* create(ImageBitmap*, const IntRect&, const ImageBitmapOptions& = ImageBitmapOptions());
    static ImageBitmap* create(PassRefPtr<StaticBitmapImage>);
    static ImageBitmap* create(PassRefPtr<StaticBitmapImage>, const IntRect&, const ImageBitmapOptions& = ImageBitmapOptions());
    static ImageBitmap* create(WebExternalTextureMailbox&);
    static PassRefPtr<SkImage> getSkImageFromDecoder(std::unique_ptr<ImageDecoder>);

    // Type and helper function required by CallbackPromiseAdapter:
    using WebType = sk_sp<SkImage>;
    static ImageBitmap* take(ScriptPromiseResolver*, sk_sp<SkImage>);

    StaticBitmapImage* bitmapImage() const { return (m_image) ? m_image.get() : nullptr; }
    std::unique_ptr<uint8_t[]> copyBitmapData(AlphaDisposition alphaOp = DontPremultiplyAlpha);
    unsigned long width() const;
    unsigned long height() const;
    IntSize size() const;

    bool isNeutered() const { return m_isNeutered; }
    bool originClean() const { return m_image->originClean(); }
    bool isPremultiplied() const { return m_image->isPremultiplied(); }
    PassRefPtr<StaticBitmapImage> transfer();
    void close();
    bool isTextureBacked() const;

    ~ImageBitmap() override;

    // CanvasImageSource implementation
    PassRefPtr<Image> getSourceImageForCanvas(SourceImageStatus*, AccelerationHint, SnapshotReason, const FloatSize&) const override;
    bool wouldTaintOrigin(SecurityOrigin*) const override { return !m_image->originClean(); }
    void adjustDrawRects(FloatRect* srcRect, FloatRect* dstRect) const override;
    FloatSize elementSize(const FloatSize&) const override;
    bool isImageBitmap() const override { return true; }
    int sourceWidth() override { return m_image ? m_image->width() : 0; }
    int sourceHeight() override { return m_image ? m_image->height() : 0; }

    // ImageBitmapSource implementation
    IntSize bitmapSourceSize() const override { return size(); }
    ScriptPromise createImageBitmap(ScriptState*, EventTarget&, int sx, int sy, int sw, int sh, const ImageBitmapOptions&, ExceptionState&) override;

    DECLARE_VIRTUAL_TRACE();

private:
    ImageBitmap(HTMLImageElement*, const IntRect&, Document*, const ImageBitmapOptions&);
    ImageBitmap(HTMLVideoElement*, const IntRect&, Document*, const ImageBitmapOptions&);
    ImageBitmap(HTMLCanvasElement*, const IntRect&, const ImageBitmapOptions&);
    ImageBitmap(ImageData*, const IntRect&, const ImageBitmapOptions&, const bool&, const bool&);
    ImageBitmap(ImageBitmap*, const IntRect&, const ImageBitmapOptions&);
    ImageBitmap(PassRefPtr<StaticBitmapImage>);
    ImageBitmap(PassRefPtr<StaticBitmapImage>, const IntRect&, const ImageBitmapOptions&);
    ImageBitmap(WebExternalTextureMailbox&);

    void parseOptions(const ImageBitmapOptions&, bool&, bool&);

    RefPtr<StaticBitmapImage> m_image;
    bool m_isNeutered = false;
};

} // namespace blink

#endif // ImageBitmap_h
