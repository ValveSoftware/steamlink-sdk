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
#include "third_party/skia/include/core/SkRefCnt.h"
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
enum DataColorFormat {
  RGBAColorType,
  N32ColorType,
};

class CORE_EXPORT ImageBitmap final
    : public GarbageCollectedFinalized<ImageBitmap>,
      public ScriptWrappable,
      public CanvasImageSource,
      public ImageBitmapSource {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ImageBitmap* create(HTMLImageElement*,
                             Optional<IntRect>,
                             Document*,
                             const ImageBitmapOptions& = ImageBitmapOptions());
  static ImageBitmap* create(HTMLVideoElement*,
                             Optional<IntRect>,
                             Document*,
                             const ImageBitmapOptions& = ImageBitmapOptions());
  static ImageBitmap* create(HTMLCanvasElement*,
                             Optional<IntRect>,
                             const ImageBitmapOptions& = ImageBitmapOptions());
  static ImageBitmap* create(ImageData*,
                             Optional<IntRect>,
                             const ImageBitmapOptions& = ImageBitmapOptions());
  static ImageBitmap* create(ImageBitmap*,
                             Optional<IntRect>,
                             const ImageBitmapOptions& = ImageBitmapOptions());
  static ImageBitmap* create(PassRefPtr<StaticBitmapImage>);
  static ImageBitmap* create(PassRefPtr<StaticBitmapImage>,
                             Optional<IntRect>,
                             const ImageBitmapOptions& = ImageBitmapOptions());
  // This function is called by structured-cloning an ImageBitmap.
  // isImageBitmapPremultiplied indicates whether the original ImageBitmap is
  // premultiplied or not.
  // isImageBitmapOriginClean indicates whether the original ImageBitmap is
  // origin clean or not.
  static ImageBitmap* create(const void* pixelData,
                             uint32_t width,
                             uint32_t height,
                             bool isImageBitmapPremultiplied,
                             bool isImageBitmapOriginClean);
  static sk_sp<SkImage> getSkImageFromDecoder(std::unique_ptr<ImageDecoder>);
  static bool isResizeOptionValid(const ImageBitmapOptions&, ExceptionState&);
  static bool isSourceSizeValid(int sourceWidth,
                                int sourceHeight,
                                ExceptionState&);

  // Type and helper function required by CallbackPromiseAdapter:
  using WebType = sk_sp<SkImage>;
  static ImageBitmap* take(ScriptPromiseResolver*, sk_sp<SkImage>);

  StaticBitmapImage* bitmapImage() const {
    return (m_image) ? m_image.get() : nullptr;
  }
  PassRefPtr<Uint8Array> copyBitmapData(AlphaDisposition = DontPremultiplyAlpha,
                                        DataColorFormat = RGBAColorType);
  unsigned long width() const;
  unsigned long height() const;
  IntSize size() const;

  bool isNeutered() const { return m_isNeutered; }
  bool originClean() const { return m_image->originClean(); }
  bool isPremultiplied() const { return m_image->isPremultiplied(); }
  PassRefPtr<StaticBitmapImage> transfer();
  void close();

  ~ImageBitmap() override;

  // CanvasImageSource implementation
  PassRefPtr<Image> getSourceImageForCanvas(SourceImageStatus*,
                                            AccelerationHint,
                                            SnapshotReason,
                                            const FloatSize&) const override;
  bool wouldTaintOrigin(SecurityOrigin*) const override {
    return !m_image->originClean();
  }
  void adjustDrawRects(FloatRect* srcRect, FloatRect* dstRect) const override;
  FloatSize elementSize(const FloatSize&) const override;
  bool isImageBitmap() const override { return true; }
  int sourceWidth() override { return m_image ? m_image->width() : 0; }
  int sourceHeight() override { return m_image ? m_image->height() : 0; }
  bool isAccelerated() const override;

  // ImageBitmapSource implementation
  IntSize bitmapSourceSize() const override { return size(); }
  ScriptPromise createImageBitmap(ScriptState*,
                                  EventTarget&,
                                  Optional<IntRect>,
                                  const ImageBitmapOptions&,
                                  ExceptionState&) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  ImageBitmap(HTMLImageElement*,
              Optional<IntRect>,
              Document*,
              const ImageBitmapOptions&);
  ImageBitmap(HTMLVideoElement*,
              Optional<IntRect>,
              Document*,
              const ImageBitmapOptions&);
  ImageBitmap(HTMLCanvasElement*, Optional<IntRect>, const ImageBitmapOptions&);
  ImageBitmap(ImageData*, Optional<IntRect>, const ImageBitmapOptions&);
  ImageBitmap(ImageBitmap*, Optional<IntRect>, const ImageBitmapOptions&);
  ImageBitmap(PassRefPtr<StaticBitmapImage>);
  ImageBitmap(PassRefPtr<StaticBitmapImage>,
              Optional<IntRect>,
              const ImageBitmapOptions&);
  ImageBitmap(const void* pixelData,
              uint32_t width,
              uint32_t height,
              bool isImageBitmapPremultiplied,
              bool isImageBitmapOriginClean);

  RefPtr<StaticBitmapImage> m_image;
  bool m_isNeutered = false;
};

}  // namespace blink

#endif  // ImageBitmap_h
