// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/ImageBitmap.h"

#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "wtf/CheckedNumeric.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

static const char* imageOrientationFlipY = "flipY";
static const char* imageBitmapOptionNone = "none";

namespace {

struct ParsedOptions {
  bool flipY = false;
  bool premultiplyAlpha = true;
  bool shouldScaleInput = false;
  unsigned resizeWidth = 0;
  unsigned resizeHeight = 0;
  IntRect cropRect;
  SkFilterQuality resizeQuality = kLow_SkFilterQuality;
  // This value should be changed in the future when we support
  // createImageBitmap with higher bit depth, in the parseOptions() function.
  // For now, it is always 4.
  int bytesPerPixel = 4;
};

// The following two functions are helpers used in cropImage
static inline IntRect normalizeRect(const IntRect& rect) {
  return IntRect(std::min(rect.x(), rect.maxX()),
                 std::min(rect.y(), rect.maxY()),
                 std::max(rect.width(), -rect.width()),
                 std::max(rect.height(), -rect.height()));
}

ParsedOptions parseOptions(const ImageBitmapOptions& options,
                           Optional<IntRect> cropRect,
                           IntSize sourceSize) {
  ParsedOptions parsedOptions;
  if (options.imageOrientation() == imageOrientationFlipY) {
    parsedOptions.flipY = true;
  } else {
    parsedOptions.flipY = false;
    DCHECK(options.imageOrientation() == imageBitmapOptionNone);
  }
  if (options.premultiplyAlpha() == imageBitmapOptionNone) {
    parsedOptions.premultiplyAlpha = false;
  } else {
    parsedOptions.premultiplyAlpha = true;
    DCHECK(options.premultiplyAlpha() == "default" ||
           options.premultiplyAlpha() == "premultiply");
  }

  int sourceWidth = sourceSize.width();
  int sourceHeight = sourceSize.height();
  if (!cropRect) {
    parsedOptions.cropRect = IntRect(0, 0, sourceWidth, sourceHeight);
  } else {
    parsedOptions.cropRect = normalizeRect(*cropRect);
  }
  if (!options.hasResizeWidth() && !options.hasResizeHeight()) {
    parsedOptions.resizeWidth = parsedOptions.cropRect.width();
    parsedOptions.resizeHeight = parsedOptions.cropRect.height();
  } else if (options.hasResizeWidth() && options.hasResizeHeight()) {
    parsedOptions.resizeWidth = options.resizeWidth();
    parsedOptions.resizeHeight = options.resizeHeight();
  } else if (options.hasResizeWidth() && !options.hasResizeHeight()) {
    parsedOptions.resizeWidth = options.resizeWidth();
    parsedOptions.resizeHeight =
        ceil(static_cast<float>(options.resizeWidth()) /
             parsedOptions.cropRect.width() * parsedOptions.cropRect.height());
  } else {
    parsedOptions.resizeHeight = options.resizeHeight();
    parsedOptions.resizeWidth =
        ceil(static_cast<float>(options.resizeHeight()) /
             parsedOptions.cropRect.height() * parsedOptions.cropRect.width());
  }
  if (static_cast<int>(parsedOptions.resizeWidth) ==
          parsedOptions.cropRect.width() &&
      static_cast<int>(parsedOptions.resizeHeight) ==
          parsedOptions.cropRect.height()) {
    parsedOptions.shouldScaleInput = false;
    return parsedOptions;
  }
  parsedOptions.shouldScaleInput = true;

  if (options.resizeQuality() == "high")
    parsedOptions.resizeQuality = kHigh_SkFilterQuality;
  else if (options.resizeQuality() == "medium")
    parsedOptions.resizeQuality = kMedium_SkFilterQuality;
  else if (options.resizeQuality() == "pixelated")
    parsedOptions.resizeQuality = kNone_SkFilterQuality;
  else
    parsedOptions.resizeQuality = kLow_SkFilterQuality;
  return parsedOptions;
}

bool dstBufferSizeHasOverflow(ParsedOptions options) {
  CheckedNumeric<unsigned> totalBytes = options.cropRect.width();
  totalBytes *= options.cropRect.height();
  totalBytes *= options.bytesPerPixel;
  if (!totalBytes.IsValid())
    return true;

  if (!options.shouldScaleInput)
    return false;
  totalBytes = options.resizeWidth;
  totalBytes *= options.resizeHeight;
  totalBytes *= options.bytesPerPixel;
  if (!totalBytes.IsValid())
    return true;

  return false;
}

}  // namespace

static PassRefPtr<Uint8Array> copySkImageData(SkImage* input,
                                              const SkImageInfo& info) {
  // The function dstBufferSizeHasOverflow() is being called at the beginning of
  // each ImageBitmap() constructor, which makes sure that doing
  // width * height * bytesPerPixel will never overflow unsigned.
  unsigned width = static_cast<unsigned>(input->width());
  RefPtr<ArrayBuffer> dstBuffer =
      ArrayBuffer::createOrNull(width * input->height(), info.bytesPerPixel());
  if (!dstBuffer)
    return nullptr;
  RefPtr<Uint8Array> dstPixels =
      Uint8Array::create(dstBuffer, 0, dstBuffer->byteLength());
  input->readPixels(info, dstPixels->data(), width * info.bytesPerPixel(), 0,
                    0);
  return dstPixels;
}

static sk_sp<SkImage> newSkImageFromRaster(const SkImageInfo& info,
                                           PassRefPtr<Uint8Array> imagePixels,
                                           unsigned imageRowBytes) {
  SkPixmap pixmap(info, imagePixels->data(), imageRowBytes);
  return SkImage::MakeFromRaster(pixmap,
                                 [](const void*, void* pixels) {
                                   static_cast<Uint8Array*>(pixels)->deref();
                                 },
                                 imagePixels.leakRef());
}

static void swizzleImageData(unsigned char* srcAddr,
                             unsigned height,
                             unsigned bytesPerRow,
                             bool flipY) {
  if (flipY) {
    for (unsigned i = 0; i < height / 2; i++) {
      unsigned topRowStartPosition = i * bytesPerRow;
      unsigned bottomRowStartPosition = (height - 1 - i) * bytesPerRow;
      if (kN32_SkColorType == kBGRA_8888_SkColorType) {  // needs to swizzle
        for (unsigned j = 0; j < bytesPerRow; j += 4) {
          std::swap(srcAddr[topRowStartPosition + j],
                    srcAddr[bottomRowStartPosition + j + 2]);
          std::swap(srcAddr[topRowStartPosition + j + 1],
                    srcAddr[bottomRowStartPosition + j + 1]);
          std::swap(srcAddr[topRowStartPosition + j + 2],
                    srcAddr[bottomRowStartPosition + j]);
          std::swap(srcAddr[topRowStartPosition + j + 3],
                    srcAddr[bottomRowStartPosition + j + 3]);
        }
      } else {
        std::swap_ranges(srcAddr + topRowStartPosition,
                         srcAddr + topRowStartPosition + bytesPerRow,
                         srcAddr + bottomRowStartPosition);
      }
    }
  } else {
    if (kN32_SkColorType == kBGRA_8888_SkColorType)  // needs to swizzle
      for (unsigned i = 0; i < height * bytesPerRow; i += 4)
        std::swap(srcAddr[i], srcAddr[i + 2]);
  }
}

static sk_sp<SkImage> flipSkImageVertically(SkImage* input,
                                            AlphaDisposition alphaOp) {
  unsigned width = static_cast<unsigned>(input->width());
  unsigned height = static_cast<unsigned>(input->height());
  SkImageInfo info = SkImageInfo::MakeN32(input->width(), input->height(),
                                          (alphaOp == PremultiplyAlpha)
                                              ? kPremul_SkAlphaType
                                              : kUnpremul_SkAlphaType);
  unsigned imageRowBytes = width * info.bytesPerPixel();
  RefPtr<Uint8Array> imagePixels = copySkImageData(input, info);
  if (!imagePixels)
    return nullptr;
  for (unsigned i = 0; i < height / 2; i++) {
    unsigned topFirstElement = i * imageRowBytes;
    unsigned topLastElement = (i + 1) * imageRowBytes;
    unsigned bottomFirstElement = (height - 1 - i) * imageRowBytes;
    std::swap_ranges(imagePixels->data() + topFirstElement,
                     imagePixels->data() + topLastElement,
                     imagePixels->data() + bottomFirstElement);
  }
  return newSkImageFromRaster(info, std::move(imagePixels), imageRowBytes);
}

static sk_sp<SkImage> premulSkImageToUnPremul(SkImage* input) {
  SkImageInfo info = SkImageInfo::Make(input->width(), input->height(),
                                       kN32_SkColorType, kUnpremul_SkAlphaType);
  RefPtr<Uint8Array> dstPixels = copySkImageData(input, info);
  if (!dstPixels)
    return nullptr;
  return newSkImageFromRaster(
      info, std::move(dstPixels),
      static_cast<unsigned>(input->width()) * info.bytesPerPixel());
}

static sk_sp<SkImage> unPremulSkImageToPremul(SkImage* input) {
  SkImageInfo info = SkImageInfo::Make(input->width(), input->height(),
                                       kN32_SkColorType, kPremul_SkAlphaType);
  RefPtr<Uint8Array> dstPixels = copySkImageData(input, info);
  if (!dstPixels)
    return nullptr;
  return newSkImageFromRaster(
      info, std::move(dstPixels),
      static_cast<unsigned>(input->width()) * info.bytesPerPixel());
}

sk_sp<SkImage> ImageBitmap::getSkImageFromDecoder(
    std::unique_ptr<ImageDecoder> decoder) {
  if (!decoder->frameCount())
    return nullptr;
  ImageFrame* frame = decoder->frameBufferAtIndex(0);
  if (!frame || frame->getStatus() != ImageFrame::FrameComplete)
    return nullptr;
  DCHECK(!frame->bitmap().isNull() && !frame->bitmap().empty());
  return frame->finalizePixelsAndGetImage();
}

bool ImageBitmap::isResizeOptionValid(const ImageBitmapOptions& options,
                                      ExceptionState& exceptionState) {
  if ((options.hasResizeWidth() && options.resizeWidth() == 0) ||
      (options.hasResizeHeight() && options.resizeHeight() == 0)) {
    exceptionState.throwDOMException(
        InvalidStateError,
        "The resizeWidth or/and resizeHeight is equal to 0.");
    return false;
  }
  return true;
}

bool ImageBitmap::isSourceSizeValid(int sourceWidth,
                                    int sourceHeight,
                                    ExceptionState& exceptionState) {
  if (!sourceWidth || !sourceHeight) {
    exceptionState.throwDOMException(
        IndexSizeError, String::format("The source %s provided is 0.",
                                       sourceWidth ? "height" : "width"));
    return false;
  }
  return true;
}

// The parameter imageFormat indicates whether the first parameter "image" is
// unpremultiplied or not.  imageFormat = PremultiplyAlpha means the image is in
// premuliplied format For example, if the image is already in unpremultiplied
// format and we want the created ImageBitmap in the same format, then we don't
// need to use the ImageDecoder to decode the image.
static PassRefPtr<StaticBitmapImage> cropImage(
    Image* image,
    const ParsedOptions& parsedOptions,
    AlphaDisposition imageFormat = PremultiplyAlpha,
    ImageDecoder::ColorSpaceOption colorSpaceOp =
        ImageDecoder::ColorSpaceApplied) {
  ASSERT(image);
  IntRect imgRect(IntPoint(), IntSize(image->width(), image->height()));
  const IntRect srcRect = intersection(imgRect, parsedOptions.cropRect);

  // In the case when cropRect doesn't intersect the source image and it
  // requires a umpremul image We immediately return a transparent black image
  // with cropRect.size()
  if (srcRect.isEmpty() && !parsedOptions.premultiplyAlpha) {
    SkImageInfo info =
        SkImageInfo::Make(parsedOptions.resizeWidth, parsedOptions.resizeHeight,
                          kN32_SkColorType, kUnpremul_SkAlphaType);
    RefPtr<ArrayBuffer> dstBuffer = ArrayBuffer::createOrNull(
        static_cast<unsigned>(info.width()) * info.height(),
        info.bytesPerPixel());
    if (!dstBuffer)
      return nullptr;
    RefPtr<Uint8Array> dstPixels =
        Uint8Array::create(dstBuffer, 0, dstBuffer->byteLength());
    return StaticBitmapImage::create(newSkImageFromRaster(
        info, std::move(dstPixels),
        static_cast<unsigned>(info.width()) * info.bytesPerPixel()));
  }

  sk_sp<SkImage> skiaImage = image->imageForCurrentFrame();
  // Attempt to get raw unpremultiplied image data, executed only when skiaImage
  // is premultiplied.
  if ((((!parsedOptions.premultiplyAlpha && !skiaImage->isOpaque()) ||
        !skiaImage) &&
       image->data() && imageFormat == PremultiplyAlpha) ||
      colorSpaceOp == ImageDecoder::ColorSpaceIgnored) {
    std::unique_ptr<ImageDecoder> decoder(ImageDecoder::create(
        image->data(), true,
        parsedOptions.premultiplyAlpha ? ImageDecoder::AlphaPremultiplied
                                       : ImageDecoder::AlphaNotPremultiplied,
        colorSpaceOp));
    if (!decoder)
      return nullptr;
    skiaImage = ImageBitmap::getSkImageFromDecoder(std::move(decoder));
    if (!skiaImage)
      return nullptr;
  }

  if (parsedOptions.cropRect == srcRect && !parsedOptions.shouldScaleInput) {
    sk_sp<SkImage> croppedSkImage = skiaImage->makeSubset(srcRect);
    if (parsedOptions.flipY)
      return StaticBitmapImage::create(flipSkImageVertically(
          croppedSkImage.get(), parsedOptions.premultiplyAlpha
                                    ? PremultiplyAlpha
                                    : DontPremultiplyAlpha));
    // Special case: The first parameter image is unpremul but we need to turn
    // it into premul.
    if (parsedOptions.premultiplyAlpha && imageFormat == DontPremultiplyAlpha)
      return StaticBitmapImage::create(
          unPremulSkImageToPremul(croppedSkImage.get()));
    // Call preroll to trigger image decoding.
    croppedSkImage->preroll();
    return StaticBitmapImage::create(std::move(croppedSkImage));
  }

  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(
      parsedOptions.resizeWidth, parsedOptions.resizeHeight);
  if (!surface)
    return nullptr;
  if (srcRect.isEmpty())
    return StaticBitmapImage::create(surface->makeImageSnapshot());

  SkScalar dstLeft = std::min(0, -parsedOptions.cropRect.x());
  SkScalar dstTop = std::min(0, -parsedOptions.cropRect.y());
  if (parsedOptions.cropRect.x() < 0)
    dstLeft = -parsedOptions.cropRect.x();
  if (parsedOptions.cropRect.y() < 0)
    dstTop = -parsedOptions.cropRect.y();
  if (parsedOptions.flipY) {
    surface->getCanvas()->translate(0, surface->height());
    surface->getCanvas()->scale(1, -1);
  }
  if (parsedOptions.shouldScaleInput) {
    SkRect drawSrcRect = SkRect::MakeXYWH(
        parsedOptions.cropRect.x(), parsedOptions.cropRect.y(),
        parsedOptions.cropRect.width(), parsedOptions.cropRect.height());
    SkRect drawDstRect = SkRect::MakeXYWH(0, 0, parsedOptions.resizeWidth,
                                          parsedOptions.resizeHeight);
    SkPaint paint;
    paint.setFilterQuality(parsedOptions.resizeQuality);
    surface->getCanvas()->drawImageRect(skiaImage, drawSrcRect, drawDstRect,
                                        &paint);
  } else {
    surface->getCanvas()->drawImage(skiaImage, dstLeft, dstTop);
  }
  skiaImage = surface->makeImageSnapshot();

  if (parsedOptions.premultiplyAlpha) {
    if (imageFormat == DontPremultiplyAlpha)
      return StaticBitmapImage::create(
          unPremulSkImageToPremul(skiaImage.get()));
    return StaticBitmapImage::create(std::move(skiaImage));
  }
  return StaticBitmapImage::create(premulSkImageToUnPremul(skiaImage.get()));
}

ImageBitmap::ImageBitmap(HTMLImageElement* image,
                         Optional<IntRect> cropRect,
                         Document* document,
                         const ImageBitmapOptions& options) {
  RefPtr<Image> input = image->cachedImage()->getImage();
  ParsedOptions parsedOptions =
      parseOptions(options, cropRect, image->bitmapSourceSize());
  if (dstBufferSizeHasOverflow(parsedOptions))
    return;

  if (options.colorSpaceConversion() == "none") {
    m_image = cropImage(input.get(), parsedOptions, PremultiplyAlpha,
                        ImageDecoder::ColorSpaceIgnored);
  } else {
    m_image = cropImage(input.get(), parsedOptions, PremultiplyAlpha,
                        ImageDecoder::ColorSpaceApplied);
  }
  if (!m_image)
    return;
  // In the case where the source image is lazy-decoded, m_image may not be in
  // a decoded state, we trigger it here.
  sk_sp<SkImage> skImage = m_image->imageForCurrentFrame();
  SkPixmap pixmap;
  if (!skImage->isTextureBacked() && !skImage->peekPixels(&pixmap)) {
    sk_sp<SkSurface> surface =
        SkSurface::MakeRasterN32Premul(skImage->width(), skImage->height());
    surface->getCanvas()->drawImage(skImage, 0, 0);
    m_image = StaticBitmapImage::create(surface->makeImageSnapshot());
  }
  if (!m_image)
    return;
  m_image->setOriginClean(
      !image->wouldTaintOrigin(document->getSecurityOrigin()));
  m_image->setPremultiplied(parsedOptions.premultiplyAlpha);
}

ImageBitmap::ImageBitmap(HTMLVideoElement* video,
                         Optional<IntRect> cropRect,
                         Document* document,
                         const ImageBitmapOptions& options) {
  IntSize playerSize;
  if (video->webMediaPlayer())
    playerSize = video->webMediaPlayer()->naturalSize();
  ParsedOptions parsedOptions =
      parseOptions(options, cropRect, video->bitmapSourceSize());
  if (dstBufferSizeHasOverflow(parsedOptions))
    return;

  std::unique_ptr<ImageBuffer> buffer = ImageBuffer::create(
      IntSize(parsedOptions.resizeWidth, parsedOptions.resizeHeight), NonOpaque,
      DoNotInitializeImagePixels);
  if (!buffer)
    return;

  IntPoint dstPoint =
      IntPoint(-parsedOptions.cropRect.x(), -parsedOptions.cropRect.y());
  if (parsedOptions.flipY) {
    buffer->canvas()->translate(0, buffer->size().height());
    buffer->canvas()->scale(1, -1);
  }
  SkPaint paint;
  if (parsedOptions.shouldScaleInput) {
    float scaleRatioX = static_cast<float>(parsedOptions.resizeWidth) /
                        parsedOptions.cropRect.width();
    float scaleRatioY = static_cast<float>(parsedOptions.resizeHeight) /
                        parsedOptions.cropRect.height();
    buffer->canvas()->scale(scaleRatioX, scaleRatioY);
    paint.setFilterQuality(parsedOptions.resizeQuality);
  }
  buffer->canvas()->translate(dstPoint.x(), dstPoint.y());
  video->paintCurrentFrame(
      buffer->canvas(),
      IntRect(IntPoint(), IntSize(video->videoWidth(), video->videoHeight())),
      parsedOptions.shouldScaleInput ? &paint : nullptr);

  sk_sp<SkImage> skiaImage =
      buffer->newSkImageSnapshot(PreferNoAcceleration, SnapshotReasonUnknown);
  if (!parsedOptions.premultiplyAlpha)
    skiaImage = premulSkImageToUnPremul(skiaImage.get());
  if (!skiaImage)
    return;
  m_image = StaticBitmapImage::create(std::move(skiaImage));
  m_image->setOriginClean(
      !video->wouldTaintOrigin(document->getSecurityOrigin()));
  m_image->setPremultiplied(parsedOptions.premultiplyAlpha);
}

ImageBitmap::ImageBitmap(HTMLCanvasElement* canvas,
                         Optional<IntRect> cropRect,
                         const ImageBitmapOptions& options) {
  ASSERT(canvas->isPaintable());
  RefPtr<Image> input;
  if (canvas->placeholderFrame())
    input = canvas->placeholderFrame();
  else
    input = canvas->copiedImage(BackBuffer, PreferAcceleration);
  ParsedOptions parsedOptions =
      parseOptions(options, cropRect, IntSize(input->width(), input->height()));
  if (dstBufferSizeHasOverflow(parsedOptions))
    return;

  bool isPremultiplyAlphaReverted = false;
  if (!parsedOptions.premultiplyAlpha) {
    parsedOptions.premultiplyAlpha = true;
    isPremultiplyAlphaReverted = true;
  }
  m_image = cropImage(input.get(), parsedOptions);
  if (!m_image)
    return;
  if (isPremultiplyAlphaReverted) {
    parsedOptions.premultiplyAlpha = false;
    m_image = StaticBitmapImage::create(
        premulSkImageToUnPremul(m_image->imageForCurrentFrame().get()));
  }
  if (!m_image)
    return;
  m_image->setOriginClean(canvas->originClean());
  m_image->setPremultiplied(parsedOptions.premultiplyAlpha);
}

ImageBitmap::ImageBitmap(const void* pixelData,
                         uint32_t width,
                         uint32_t height,
                         bool isImageBitmapPremultiplied,
                         bool isImageBitmapOriginClean) {
  SkImageInfo info = SkImageInfo::MakeN32(
      width, height,
      isImageBitmapPremultiplied ? kPremul_SkAlphaType : kUnpremul_SkAlphaType);
  SkPixmap pixmap(info, pixelData, info.bytesPerPixel() * width);
  m_image = StaticBitmapImage::create(SkImage::MakeRasterCopy(pixmap));
  if (!m_image)
    return;
  m_image->setPremultiplied(isImageBitmapPremultiplied);
  m_image->setOriginClean(isImageBitmapOriginClean);
}

static sk_sp<SkImage> scaleSkImage(sk_sp<SkImage> skImage,
                                   unsigned resizeWidth,
                                   unsigned resizeHeight,
                                   SkFilterQuality resizeQuality) {
  SkImageInfo resizedInfo = SkImageInfo::Make(
      resizeWidth, resizeHeight, kN32_SkColorType, kUnpremul_SkAlphaType);
  RefPtr<ArrayBuffer> dstBuffer = ArrayBuffer::createOrNull(
      resizeWidth * resizeHeight, resizedInfo.bytesPerPixel());
  if (!dstBuffer)
    return nullptr;
  RefPtr<Uint8Array> resizedPixels =
      Uint8Array::create(dstBuffer, 0, dstBuffer->byteLength());
  SkPixmap pixmap(
      resizedInfo, resizedPixels->data(),
      static_cast<unsigned>(resizeWidth) * resizedInfo.bytesPerPixel());
  skImage->scalePixels(pixmap, resizeQuality);
  return SkImage::MakeFromRaster(pixmap,
                                 [](const void*, void* pixels) {
                                   static_cast<Uint8Array*>(pixels)->deref();
                                 },
                                 resizedPixels.release().leakRef());
}

ImageBitmap::ImageBitmap(ImageData* data,
                         Optional<IntRect> cropRect,
                         const ImageBitmapOptions& options) {
  // TODO(xidachen): implement the resize option
  IntRect dataSrcRect = IntRect(IntPoint(), data->size());
  ParsedOptions parsedOptions =
      parseOptions(options, cropRect, data->bitmapSourceSize());
  if (dstBufferSizeHasOverflow(parsedOptions))
    return;
  IntRect srcRect = cropRect ? intersection(parsedOptions.cropRect, dataSrcRect)
                             : dataSrcRect;

  // treat non-premultiplyAlpha as a special case
  if (!parsedOptions.premultiplyAlpha) {
    unsigned char* srcAddr = data->data()->data();

    // Using kN32 type, swizzle input if necessary.
    SkImageInfo info = SkImageInfo::Make(
        parsedOptions.cropRect.width(), parsedOptions.cropRect.height(),
        kN32_SkColorType, kUnpremul_SkAlphaType);
    unsigned bytesPerPixel = static_cast<unsigned>(info.bytesPerPixel());
    unsigned srcPixelBytesPerRow = bytesPerPixel * data->size().width();
    unsigned dstPixelBytesPerRow =
        bytesPerPixel * parsedOptions.cropRect.width();
    sk_sp<SkImage> skImage;
    if (parsedOptions.cropRect == IntRect(IntPoint(), data->size())) {
      swizzleImageData(srcAddr, data->size().height(), srcPixelBytesPerRow,
                       parsedOptions.flipY);
      skImage =
          SkImage::MakeRasterCopy(SkPixmap(info, srcAddr, dstPixelBytesPerRow));
      // restore the original ImageData
      swizzleImageData(srcAddr, data->size().height(), srcPixelBytesPerRow,
                       parsedOptions.flipY);
    } else {
      RefPtr<ArrayBuffer> dstBuffer = ArrayBuffer::createOrNull(
          static_cast<unsigned>(parsedOptions.cropRect.height()) *
              parsedOptions.cropRect.width(),
          bytesPerPixel);
      if (!dstBuffer)
        return;
      RefPtr<Uint8Array> copiedDataBuffer =
          Uint8Array::create(dstBuffer, 0, dstBuffer->byteLength());
      if (!srcRect.isEmpty()) {
        IntPoint srcPoint = IntPoint(
            (parsedOptions.cropRect.x() > 0) ? parsedOptions.cropRect.x() : 0,
            (parsedOptions.cropRect.y() > 0) ? parsedOptions.cropRect.y() : 0);
        IntPoint dstPoint = IntPoint(
            (parsedOptions.cropRect.x() >= 0) ? 0 : -parsedOptions.cropRect.x(),
            (parsedOptions.cropRect.y() >= 0) ? 0
                                              : -parsedOptions.cropRect.y());
        int copyHeight = data->size().height() - srcPoint.y();
        if (parsedOptions.cropRect.height() < copyHeight)
          copyHeight = parsedOptions.cropRect.height();
        int copyWidth = data->size().width() - srcPoint.x();
        if (parsedOptions.cropRect.width() < copyWidth)
          copyWidth = parsedOptions.cropRect.width();
        for (int i = 0; i < copyHeight; i++) {
          unsigned srcStartCopyPosition =
              (i + srcPoint.y()) * srcPixelBytesPerRow +
              srcPoint.x() * bytesPerPixel;
          unsigned srcEndCopyPosition =
              srcStartCopyPosition + copyWidth * bytesPerPixel;
          unsigned dstStartCopyPosition;
          if (parsedOptions.flipY)
            dstStartCopyPosition =
                (parsedOptions.cropRect.height() - 1 - dstPoint.y() - i) *
                    dstPixelBytesPerRow +
                dstPoint.x() * bytesPerPixel;
          else
            dstStartCopyPosition = (dstPoint.y() + i) * dstPixelBytesPerRow +
                                   dstPoint.x() * bytesPerPixel;
          for (unsigned j = 0; j < srcEndCopyPosition - srcStartCopyPosition;
               j++) {
            // swizzle when necessary
            if (kN32_SkColorType == kBGRA_8888_SkColorType) {
              if (j % 4 == 0)
                copiedDataBuffer->data()[dstStartCopyPosition + j] =
                    srcAddr[srcStartCopyPosition + j + 2];
              else if (j % 4 == 2)
                copiedDataBuffer->data()[dstStartCopyPosition + j] =
                    srcAddr[srcStartCopyPosition + j - 2];
              else
                copiedDataBuffer->data()[dstStartCopyPosition + j] =
                    srcAddr[srcStartCopyPosition + j];
            } else {
              copiedDataBuffer->data()[dstStartCopyPosition + j] =
                  srcAddr[srcStartCopyPosition + j];
            }
          }
        }
      }
      skImage = newSkImageFromRaster(info, std::move(copiedDataBuffer),
                                     dstPixelBytesPerRow);
    }
    if (!skImage)
      return;
    if (parsedOptions.shouldScaleInput)
      m_image = StaticBitmapImage::create(scaleSkImage(
          skImage, parsedOptions.resizeWidth, parsedOptions.resizeHeight,
          parsedOptions.resizeQuality));
    else
      m_image = StaticBitmapImage::create(skImage);
    if (!m_image)
      return;
    m_image->setPremultiplied(parsedOptions.premultiplyAlpha);
    return;
  }

  std::unique_ptr<ImageBuffer> buffer = ImageBuffer::create(
      parsedOptions.cropRect.size(), NonOpaque, DoNotInitializeImagePixels);
  if (!buffer)
    return;

  if (srcRect.isEmpty()) {
    m_image = StaticBitmapImage::create(buffer->newSkImageSnapshot(
        PreferNoAcceleration, SnapshotReasonUnknown));
    return;
  }

  IntPoint dstPoint = IntPoint(std::min(0, -parsedOptions.cropRect.x()),
                               std::min(0, -parsedOptions.cropRect.y()));
  if (parsedOptions.cropRect.x() < 0)
    dstPoint.setX(-parsedOptions.cropRect.x());
  if (parsedOptions.cropRect.y() < 0)
    dstPoint.setY(-parsedOptions.cropRect.y());
  buffer->putByteArray(Unmultiplied, data->data()->data(), data->size(),
                       srcRect, dstPoint);
  sk_sp<SkImage> skImage =
      buffer->newSkImageSnapshot(PreferNoAcceleration, SnapshotReasonUnknown);
  if (parsedOptions.flipY)
    skImage = flipSkImageVertically(skImage.get(), PremultiplyAlpha);
  if (!skImage)
    return;
  if (parsedOptions.shouldScaleInput) {
    sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(
        parsedOptions.resizeWidth, parsedOptions.resizeHeight);
    if (!surface)
      return;
    SkPaint paint;
    paint.setFilterQuality(parsedOptions.resizeQuality);
    SkRect dstDrawRect =
        SkRect::MakeWH(parsedOptions.resizeWidth, parsedOptions.resizeHeight);
    surface->getCanvas()->drawImageRect(skImage, dstDrawRect, &paint);
    skImage = surface->makeImageSnapshot();
  }
  m_image = StaticBitmapImage::create(std::move(skImage));
}

ImageBitmap::ImageBitmap(ImageBitmap* bitmap,
                         Optional<IntRect> cropRect,
                         const ImageBitmapOptions& options) {
  RefPtr<Image> input = bitmap->bitmapImage();
  if (!input)
    return;
  ParsedOptions parsedOptions = parseOptions(options, cropRect, input->size());
  if (dstBufferSizeHasOverflow(parsedOptions))
    return;

  m_image = cropImage(input.get(), parsedOptions, bitmap->isPremultiplied()
                                                      ? PremultiplyAlpha
                                                      : DontPremultiplyAlpha);
  if (!m_image)
    return;
  m_image->setOriginClean(bitmap->originClean());
  m_image->setPremultiplied(parsedOptions.premultiplyAlpha);
}

ImageBitmap::ImageBitmap(PassRefPtr<StaticBitmapImage> image,
                         Optional<IntRect> cropRect,
                         const ImageBitmapOptions& options) {
  bool originClean = image->originClean();
  RefPtr<Image> input = image;
  ParsedOptions parsedOptions = parseOptions(options, cropRect, input->size());
  if (dstBufferSizeHasOverflow(parsedOptions))
    return;

  m_image = cropImage(input.get(), parsedOptions);
  if (!m_image)
    return;

  m_image->setOriginClean(originClean);
  m_image->setPremultiplied(parsedOptions.premultiplyAlpha);
}

ImageBitmap::ImageBitmap(PassRefPtr<StaticBitmapImage> image) {
  m_image = image;
}

PassRefPtr<StaticBitmapImage> ImageBitmap::transfer() {
  ASSERT(!isNeutered());
  m_isNeutered = true;
  m_image->transfer();
  return m_image.release();
}

ImageBitmap::~ImageBitmap() {}

ImageBitmap* ImageBitmap::create(HTMLImageElement* image,
                                 Optional<IntRect> cropRect,
                                 Document* document,
                                 const ImageBitmapOptions& options) {
  return new ImageBitmap(image, cropRect, document, options);
}

ImageBitmap* ImageBitmap::create(HTMLVideoElement* video,
                                 Optional<IntRect> cropRect,
                                 Document* document,
                                 const ImageBitmapOptions& options) {
  return new ImageBitmap(video, cropRect, document, options);
}

ImageBitmap* ImageBitmap::create(HTMLCanvasElement* canvas,
                                 Optional<IntRect> cropRect,
                                 const ImageBitmapOptions& options) {
  return new ImageBitmap(canvas, cropRect, options);
}

ImageBitmap* ImageBitmap::create(ImageData* data,
                                 Optional<IntRect> cropRect,
                                 const ImageBitmapOptions& options) {
  return new ImageBitmap(data, cropRect, options);
}

ImageBitmap* ImageBitmap::create(ImageBitmap* bitmap,
                                 Optional<IntRect> cropRect,
                                 const ImageBitmapOptions& options) {
  return new ImageBitmap(bitmap, cropRect, options);
}

ImageBitmap* ImageBitmap::create(PassRefPtr<StaticBitmapImage> image,
                                 Optional<IntRect> cropRect,
                                 const ImageBitmapOptions& options) {
  return new ImageBitmap(std::move(image), cropRect, options);
}

ImageBitmap* ImageBitmap::create(PassRefPtr<StaticBitmapImage> image) {
  return new ImageBitmap(std::move(image));
}

ImageBitmap* ImageBitmap::create(const void* pixelData,
                                 uint32_t width,
                                 uint32_t height,
                                 bool isImageBitmapPremultiplied,
                                 bool isImageBitmapOriginClean) {
  return new ImageBitmap(pixelData, width, height, isImageBitmapPremultiplied,
                         isImageBitmapOriginClean);
}

void ImageBitmap::close() {
  if (!m_image || m_isNeutered)
    return;
  m_image.clear();
  m_isNeutered = true;
}

// static
ImageBitmap* ImageBitmap::take(ScriptPromiseResolver*, sk_sp<SkImage> image) {
  return ImageBitmap::create(StaticBitmapImage::create(std::move(image)));
}

PassRefPtr<Uint8Array> ImageBitmap::copyBitmapData(AlphaDisposition alphaOp,
                                                   DataColorFormat format) {
  SkImageInfo info = SkImageInfo::Make(
      width(), height(),
      (format == RGBAColorType) ? kRGBA_8888_SkColorType : kN32_SkColorType,
      (alphaOp == PremultiplyAlpha) ? kPremul_SkAlphaType
                                    : kUnpremul_SkAlphaType);
  RefPtr<Uint8Array> dstPixels =
      copySkImageData(m_image->imageForCurrentFrame().get(), info);
  return dstPixels.release();
}

unsigned long ImageBitmap::width() const {
  if (!m_image)
    return 0;
  ASSERT(m_image->width() > 0);
  return m_image->width();
}

unsigned long ImageBitmap::height() const {
  if (!m_image)
    return 0;
  ASSERT(m_image->height() > 0);
  return m_image->height();
}

bool ImageBitmap::isAccelerated() const {
  return m_image && (m_image->isTextureBacked() || m_image->hasMailbox());
}

IntSize ImageBitmap::size() const {
  if (!m_image)
    return IntSize();
  ASSERT(m_image->width() > 0 && m_image->height() > 0);
  return IntSize(m_image->width(), m_image->height());
}

ScriptPromise ImageBitmap::createImageBitmap(ScriptState* scriptState,
                                             EventTarget& eventTarget,
                                             Optional<IntRect> cropRect,
                                             const ImageBitmapOptions& options,
                                             ExceptionState& exceptionState) {
  if ((cropRect &&
       !isSourceSizeValid(cropRect->width(), cropRect->height(),
                          exceptionState)) ||
      !isSourceSizeValid(width(), height(), exceptionState))
    return ScriptPromise();
  if (!isResizeOptionValid(options, exceptionState))
    return ScriptPromise();
  return ImageBitmapSource::fulfillImageBitmap(scriptState,
                                               create(this, cropRect, options));
}

PassRefPtr<Image> ImageBitmap::getSourceImageForCanvas(
    SourceImageStatus* status,
    AccelerationHint,
    SnapshotReason,
    const FloatSize&) const {
  *status = NormalSourceImageStatus;
  return m_image ? m_image : nullptr;
}

void ImageBitmap::adjustDrawRects(FloatRect* srcRect,
                                  FloatRect* dstRect) const {}

FloatSize ImageBitmap::elementSize(const FloatSize&) const {
  return FloatSize(width(), height());
}

DEFINE_TRACE(ImageBitmap) {}

}  // namespace blink
