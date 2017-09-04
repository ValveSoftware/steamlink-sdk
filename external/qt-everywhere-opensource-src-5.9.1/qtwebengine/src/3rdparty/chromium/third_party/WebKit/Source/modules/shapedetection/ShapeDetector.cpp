// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/ShapeDetector.h"

#include "core/dom/DOMException.h"
#include "core/dom/DOMRect.h"
#include "core/dom/Document.h"
#include "core/fetch/ImageResource.h"
#include "core/frame/ImageBitmap.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "modules/shapedetection/DetectedBarcode.h"
#include "platform/graphics/Image.h"
#include "public/platform/InterfaceProvider.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "wtf/CheckedNumeric.h"

namespace blink {

namespace {

static CanvasImageSource* toImageSourceInternal(
    const CanvasImageSourceUnion& value) {
  if (value.isHTMLImageElement())
    return value.getAsHTMLImageElement();

  if (value.isImageBitmap() &&
      !static_cast<ImageBitmap*>(value.getAsImageBitmap())->isNeutered()) {
    return value.getAsImageBitmap();
  }

  if (value.isHTMLVideoElement())
    return value.getAsHTMLVideoElement();

  return nullptr;
}

}  // anonymous namespace

ShapeDetector::ShapeDetector(LocalFrame& frame) {
  DCHECK(!m_service.is_bound());
  DCHECK(frame.interfaceProvider());
  frame.interfaceProvider()->getInterface(mojo::GetProxy(&m_service));
}

ScriptPromise ShapeDetector::detectShapes(
    ScriptState* scriptState,
    DetectorType detectorType,
    const CanvasImageSourceUnion& imageSource) {
  CanvasImageSource* imageSourceInternal = toImageSourceInternal(imageSource);

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  if (!imageSourceInternal) {
    // TODO(mcasas): Implement more CanvasImageSources, https://crbug.com/659138
    NOTIMPLEMENTED() << "Unsupported CanvasImageSource";
    resolver->reject(
        DOMException::create(NotFoundError, "Unsupported source."));
    return promise;
  }

  if (imageSourceInternal->wouldTaintOrigin(
          scriptState->getExecutionContext()->getSecurityOrigin())) {
    resolver->reject(
        DOMException::create(SecurityError, "Source would taint origin."));
    return promise;
  }

  if (imageSource.isHTMLImageElement()) {
    return detectShapesOnImageElement(
        detectorType, resolver,
        static_cast<HTMLImageElement*>(imageSourceInternal));
  }
  if (imageSourceInternal->isImageBitmap()) {
    return detectShapesOnImageBitmap(
        detectorType, resolver, static_cast<ImageBitmap*>(imageSourceInternal));
  }
  if (imageSourceInternal->isVideoElement()) {
    return detectShapesOnVideoElement(
        detectorType, resolver,
        static_cast<HTMLVideoElement*>(imageSourceInternal));
  }

  NOTREACHED();
  return promise;
}

ScriptPromise ShapeDetector::detectShapesOnImageElement(
    DetectorType detectorType,
    ScriptPromiseResolver* resolver,
    const HTMLImageElement* img) {
  ScriptPromise promise = resolver->promise();
  if (img->bitmapSourceSize().isZero()) {
    resolver->resolve(HeapVector<Member<DOMRect>>());
    return promise;
  }

  ImageResource* const imageResource = img->cachedImage();
  if (!imageResource || imageResource->errorOccurred()) {
    resolver->reject(DOMException::create(
        InvalidStateError, "Failed to load or decode HTMLImageElement."));
    return promise;
  }

  Image* const blinkImage = imageResource->getImage();
  if (!blinkImage) {
    resolver->reject(DOMException::create(
        InvalidStateError, "Failed to get image from resource."));
    return promise;
  }

  const sk_sp<SkImage> image = blinkImage->imageForCurrentFrame();
  DCHECK_EQ(img->naturalWidth(), static_cast<unsigned>(image->width()));
  DCHECK_EQ(img->naturalHeight(), static_cast<unsigned>(image->height()));

  if (!image) {
    resolver->reject(DOMException::create(
        InvalidStateError, "Failed to get image from current frame."));
    return promise;
  }

  const SkImageInfo skiaInfo =
      SkImageInfo::MakeN32(image->width(), image->height(), image->alphaType());

  const uint32_t allocationSize = skiaInfo.getSafeSize(skiaInfo.minRowBytes());

  mojo::ScopedSharedBufferHandle sharedBufferHandle =
      mojo::SharedBufferHandle::Create(allocationSize);
  if (!sharedBufferHandle.is_valid()) {
    DLOG(ERROR) << "Requested allocation : " << allocationSize
                << "B, larger than |mojo::edk::kMaxSharedBufferSize| == 16MB ";
    // TODO(xianglu): For now we reject the promise if the image is too large.
    // But consider resizing the image to remove restriction on the user side.
    // Also, add LayoutTests for this case later.
    resolver->reject(
        DOMException::create(InvalidStateError, "Image exceeds size limit."));
    return promise;
  }

  const mojo::ScopedSharedBufferMapping mappedBuffer =
      sharedBufferHandle->Map(allocationSize);

  const SkPixmap pixmap(skiaInfo, mappedBuffer.get(), skiaInfo.minRowBytes());
  if (!image->readPixels(pixmap, 0, 0)) {
    resolver->reject(DOMException::create(
        InvalidStateError,
        "Failed to read pixels: Unable to decompress or unsupported format."));
    return promise;
  }

  if (!m_service) {
    resolver->reject(DOMException::create(
        NotSupportedError, "Shape detection service unavailable."));
    return promise;
  }

  m_serviceRequests.add(resolver);
  DCHECK(m_service.is_bound());
  if (detectorType == DetectorType::Face) {
    m_service->DetectFaces(
        std::move(sharedBufferHandle), img->naturalWidth(),
        img->naturalHeight(),
        convertToBaseCallback(WTF::bind(&ShapeDetector::onDetectFaces,
                                        wrapPersistent(this),
                                        wrapPersistent(resolver))));
  } else if (detectorType == DetectorType::Barcode) {
    m_service->DetectBarcodes(
        std::move(sharedBufferHandle), img->naturalWidth(),
        img->naturalHeight(),
        convertToBaseCallback(WTF::bind(&ShapeDetector::onDetectBarcodes,
                                        wrapPersistent(this),
                                        wrapPersistent(resolver))));
  } else {
    NOTREACHED() << "Unsupported detector type";
  }

  return promise;
}

ScriptPromise ShapeDetector::detectShapesOnImageBitmap(
    DetectorType detectorType,
    ScriptPromiseResolver* resolver,
    ImageBitmap* imageBitmap) {
  ScriptPromise promise = resolver->promise();
  if (!imageBitmap->originClean()) {
    resolver->reject(
        DOMException::create(SecurityError, "ImageBitmap is not origin clean"));
    return promise;
  }

  if (imageBitmap->size().area() == 0) {
    resolver->resolve(HeapVector<Member<DOMRect>>());
    return promise;
  }

  SkPixmap pixmap;
  RefPtr<Uint8Array> pixelData;
  uint8_t* pixelDataPtr = nullptr;
  WTF::CheckedNumeric<int> allocationSize = 0;
  // Use |skImage|'s pixels if it has direct access to them, otherwise retrieve
  // them from elsewhere via copyBitmapData().
  sk_sp<SkImage> skImage = imageBitmap->bitmapImage()->imageForCurrentFrame();
  if (skImage->peekPixels(&pixmap)) {
    pixelDataPtr = static_cast<uint8_t*>(pixmap.writable_addr());
    allocationSize = pixmap.getSafeSize();
  } else {
    pixelData = imageBitmap->copyBitmapData(imageBitmap->isPremultiplied()
                                                ? PremultiplyAlpha
                                                : DontPremultiplyAlpha,
                                            N32ColorType);
    pixelDataPtr = pixelData->data();
    allocationSize = imageBitmap->size().area() * 4 /* bytes per pixel */;
  }

  return detectShapesOnData(detectorType, resolver, pixelDataPtr,
                            allocationSize.ValueOrDefault(0),
                            imageBitmap->width(), imageBitmap->height());
}

ScriptPromise ShapeDetector::detectShapesOnVideoElement(
    DetectorType detectorType,
    ScriptPromiseResolver* resolver,
    const HTMLVideoElement* video) {
  ScriptPromise promise = resolver->promise();

  // TODO(mcasas): Check if |video| is actually playing a MediaStream by using
  // HTMLMediaElement::isMediaStreamURL(video->currentSrc().getString()); if
  // there is a local WebCam associated, there might be sophisticated ways to
  // detect faces on it. Until then, treat as a normal <video> element.

  // !hasAvailableVideoFrame() is a bundle of invalid states.
  if (!video->hasAvailableVideoFrame()) {
    resolver->reject(DOMException::create(
        InvalidStateError, "Invalid HTMLVideoElement or state."));
    return promise;
  }

  const FloatSize videoSize(video->videoWidth(), video->videoHeight());
  SourceImageStatus sourceImageStatus = InvalidSourceImageStatus;
  RefPtr<Image> image =
      video->getSourceImageForCanvas(&sourceImageStatus, PreferNoAcceleration,
                                     SnapshotReasonDrawImage, videoSize);

  DCHECK_EQ(NormalSourceImageStatus, sourceImageStatus);

  SkPixmap pixmap;
  RefPtr<Uint8Array> pixelData;
  uint8_t* pixelDataPtr = nullptr;
  WTF::CheckedNumeric<int> allocationSize = 0;
  // Use |skImage|'s pixels if it has direct access to them.
  sk_sp<SkImage> skImage = image->imageForCurrentFrame();
  if (skImage->peekPixels(&pixmap)) {
    pixelDataPtr = static_cast<uint8_t*>(pixmap.writable_addr());
    allocationSize = pixmap.getSafeSize();
  } else {
    // TODO(mcasas): retrieve the pixels from elsewhere.
    NOTREACHED();
    resolver->reject(DOMException::create(
        InvalidStateError, "Failed to get pixels for current frame."));
    return promise;
  }

  return detectShapesOnData(detectorType, resolver, pixelDataPtr,
                            allocationSize.ValueOrDefault(0), image->width(),
                            image->height());
}

ScriptPromise ShapeDetector::detectShapesOnData(DetectorType detectorType,
                                                ScriptPromiseResolver* resolver,
                                                uint8_t* data,
                                                int size,
                                                int width,
                                                int height) {
  DCHECK(data);
  DCHECK(size);
  ScriptPromise promise = resolver->promise();

  mojo::ScopedSharedBufferHandle sharedBufferHandle =
      mojo::SharedBufferHandle::Create(size);
  if (!sharedBufferHandle->is_valid()) {
    resolver->reject(
        DOMException::create(InvalidStateError, "Internal allocation error"));
    return promise;
  }

  if (!m_service) {
    resolver->reject(DOMException::create(
        NotSupportedError, "Shape detection service unavailable."));
    return promise;
  }

  const mojo::ScopedSharedBufferMapping mappedBuffer =
      sharedBufferHandle->Map(size);
  DCHECK(mappedBuffer.get());

  memcpy(mappedBuffer.get(), data, size);

  m_serviceRequests.add(resolver);
  DCHECK(m_service.is_bound());
  if (detectorType == DetectorType::Face) {
    m_service->DetectFaces(
        std::move(sharedBufferHandle), width, height,
        convertToBaseCallback(WTF::bind(&ShapeDetector::onDetectFaces,
                                        wrapPersistent(this),
                                        wrapPersistent(resolver))));
  } else if (detectorType == DetectorType::Barcode) {
    m_service->DetectBarcodes(
        std::move(sharedBufferHandle), width, height,
        convertToBaseCallback(WTF::bind(&ShapeDetector::onDetectBarcodes,
                                        wrapPersistent(this),
                                        wrapPersistent(resolver))));
  } else {
    NOTREACHED() << "Unsupported detector type";
  }
  sharedBufferHandle.reset();
  return promise;
}

void ShapeDetector::onDetectFaces(
    ScriptPromiseResolver* resolver,
    mojom::blink::FaceDetectionResultPtr faceDetectionResult) {
  if (!m_serviceRequests.contains(resolver))
    return;

  HeapVector<Member<DOMRect>> detectedFaces;
  for (const auto& boundingBox : faceDetectionResult->boundingBoxes) {
    detectedFaces.append(DOMRect::create(boundingBox->x, boundingBox->y,
                                         boundingBox->width,
                                         boundingBox->height));
  }

  resolver->resolve(detectedFaces);
  m_serviceRequests.remove(resolver);
}

void ShapeDetector::onDetectBarcodes(
    ScriptPromiseResolver* resolver,
    Vector<mojom::blink::BarcodeDetectionResultPtr> barcodeDetectionResults) {
  if (!m_serviceRequests.contains(resolver))
    return;
  m_serviceRequests.remove(resolver);

  HeapVector<Member<DetectedBarcode>> detectedBarcodes;
  for (const auto& barcode : barcodeDetectionResults) {
    detectedBarcodes.append(DetectedBarcode::create(
        barcode->raw_value,
        DOMRect::create(barcode->bounding_box->x, barcode->bounding_box->y,
                        barcode->bounding_box->width,
                        barcode->bounding_box->height)));
  }

  resolver->resolve(detectedBarcodes);
}

DEFINE_TRACE(ShapeDetector) {
  visitor->trace(m_serviceRequests);
}

}  // namespace blink
