/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/imagebitmap/ImageBitmapFactories.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/frame/ImageBitmap.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "core/imagebitmap/ImageBitmapOptions.h"
#include "core/svg/graphics/SVGImage.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/SharedBuffer.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/threading/BackgroundTaskRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include <memory>
#include <v8.h>

namespace blink {

static inline ImageBitmapSource* toImageBitmapSourceInternal(
    const ImageBitmapSourceUnion& value,
    ExceptionState& exceptionState,
    const ImageBitmapOptions& options,
    bool hasCropRect) {
  if (value.isHTMLImageElement()) {
    HTMLImageElement* imageElement = value.getAsHTMLImageElement();
    if (!imageElement || !imageElement->cachedImage()) {
      exceptionState.throwDOMException(
          InvalidStateError,
          "No image can be retrieved from the provided element.");
      return nullptr;
    }
    if (imageElement->cachedImage()->getImage()->isSVGImage()) {
      SVGImage* image = toSVGImage(imageElement->cachedImage()->getImage());
      if (!image->hasIntrinsicDimensions() &&
          (!hasCropRect &&
           (!options.hasResizeWidth() || !options.hasResizeHeight()))) {
        exceptionState.throwDOMException(
            InvalidStateError,
            "The image element contains an SVG image without intrinsic "
            "dimensions, and no resize options or crop region are specified.");
        return nullptr;
      }
    }
    return imageElement;
  }
  if (value.isHTMLVideoElement())
    return value.getAsHTMLVideoElement();
  if (value.isHTMLCanvasElement())
    return value.getAsHTMLCanvasElement();
  if (value.isBlob())
    return value.getAsBlob();
  if (value.isImageData())
    return value.getAsImageData();
  if (value.isImageBitmap())
    return value.getAsImageBitmap();
  ASSERT_NOT_REACHED();
  return nullptr;
}

ScriptPromise ImageBitmapFactories::createImageBitmapFromBlob(
    ScriptState* scriptState,
    EventTarget& eventTarget,
    ImageBitmapSource* bitmapSource,
    Optional<IntRect> cropRect,
    const ImageBitmapOptions& options,
    ExceptionState& exceptionState) {
  if (cropRect &&
      !ImageBitmap::isSourceSizeValid(cropRect->width(), cropRect->height(),
                                      exceptionState))
    return ScriptPromise();
  if (!ImageBitmap::isResizeOptionValid(options, exceptionState))
    return ScriptPromise();
  Blob* blob = static_cast<Blob*>(bitmapSource);
  ImageBitmapLoader* loader = ImageBitmapFactories::ImageBitmapLoader::create(
      from(eventTarget), cropRect, options, scriptState);
  ScriptPromise promise = loader->promise();
  from(eventTarget).addLoader(loader);
  loader->loadBlobAsync(eventTarget.getExecutionContext(), blob);
  return promise;
}

ScriptPromise ImageBitmapFactories::createImageBitmap(
    ScriptState* scriptState,
    EventTarget& eventTarget,
    const ImageBitmapSourceUnion& bitmapSource,
    const ImageBitmapOptions& options,
    ExceptionState& exceptionState) {
  UseCounter::Feature feature = UseCounter::CreateImageBitmap;
  UseCounter::count(scriptState->getExecutionContext(), feature);
  ImageBitmapSource* bitmapSourceInternal =
      toImageBitmapSourceInternal(bitmapSource, exceptionState, options, false);
  if (!bitmapSourceInternal)
    return ScriptPromise();
  return createImageBitmap(scriptState, eventTarget, bitmapSourceInternal,
                           Optional<IntRect>(), options, exceptionState);
}

ScriptPromise ImageBitmapFactories::createImageBitmap(
    ScriptState* scriptState,
    EventTarget& eventTarget,
    const ImageBitmapSourceUnion& bitmapSource,
    int sx,
    int sy,
    int sw,
    int sh,
    const ImageBitmapOptions& options,
    ExceptionState& exceptionState) {
  UseCounter::Feature feature = UseCounter::CreateImageBitmap;
  UseCounter::count(scriptState->getExecutionContext(), feature);
  ImageBitmapSource* bitmapSourceInternal =
      toImageBitmapSourceInternal(bitmapSource, exceptionState, options, true);
  if (!bitmapSourceInternal)
    return ScriptPromise();
  Optional<IntRect> cropRect = IntRect(sx, sy, sw, sh);
  return createImageBitmap(scriptState, eventTarget, bitmapSourceInternal,
                           cropRect, options, exceptionState);
}

ScriptPromise ImageBitmapFactories::createImageBitmap(
    ScriptState* scriptState,
    EventTarget& eventTarget,
    ImageBitmapSource* bitmapSource,
    Optional<IntRect> cropRect,
    const ImageBitmapOptions& options,
    ExceptionState& exceptionState) {
  if (bitmapSource->isBlob())
    return createImageBitmapFromBlob(scriptState, eventTarget, bitmapSource,
                                     cropRect, options, exceptionState);

  return bitmapSource->createImageBitmap(scriptState, eventTarget, cropRect,
                                         options, exceptionState);
}

const char* ImageBitmapFactories::supplementName() {
  return "ImageBitmapFactories";
}

ImageBitmapFactories& ImageBitmapFactories::from(EventTarget& eventTarget) {
  if (LocalDOMWindow* window = eventTarget.toLocalDOMWindow())
    return fromInternal(*window);

  ASSERT(eventTarget.getExecutionContext()->isWorkerGlobalScope());
  return ImageBitmapFactories::fromInternal(
      *toWorkerGlobalScope(eventTarget.getExecutionContext()));
}

template <class GlobalObject>
ImageBitmapFactories& ImageBitmapFactories::fromInternal(GlobalObject& object) {
  ImageBitmapFactories* supplement = static_cast<ImageBitmapFactories*>(
      Supplement<GlobalObject>::from(object, supplementName()));
  if (!supplement) {
    supplement = new ImageBitmapFactories;
    Supplement<GlobalObject>::provideTo(object, supplementName(), supplement);
  }
  return *supplement;
}

void ImageBitmapFactories::addLoader(ImageBitmapLoader* loader) {
  m_pendingLoaders.add(loader);
}

void ImageBitmapFactories::didFinishLoading(ImageBitmapLoader* loader) {
  ASSERT(m_pendingLoaders.contains(loader));
  m_pendingLoaders.remove(loader);
}

ImageBitmapFactories::ImageBitmapLoader::ImageBitmapLoader(
    ImageBitmapFactories& factory,
    Optional<IntRect> cropRect,
    ScriptState* scriptState,
    const ImageBitmapOptions& options)
    : m_loader(
          FileReaderLoader::create(FileReaderLoader::ReadAsArrayBuffer, this)),
      m_factory(&factory),
      m_resolver(ScriptPromiseResolver::create(scriptState)),
      m_cropRect(cropRect),
      m_options(options) {}

void ImageBitmapFactories::ImageBitmapLoader::loadBlobAsync(
    ExecutionContext* context,
    Blob* blob) {
  m_loader->start(context, blob->blobDataHandle());
}

DEFINE_TRACE(ImageBitmapFactories) {
  visitor->trace(m_pendingLoaders);
  Supplement<LocalDOMWindow>::trace(visitor);
  Supplement<WorkerGlobalScope>::trace(visitor);
}

void ImageBitmapFactories::ImageBitmapLoader::rejectPromise() {
  m_resolver->reject(DOMException::create(
      InvalidStateError, "The source image cannot be decoded."));
  m_factory->didFinishLoading(this);
}

void ImageBitmapFactories::ImageBitmapLoader::didFinishLoading() {
  DOMArrayBuffer* arrayBuffer = m_loader->arrayBufferResult();
  if (!arrayBuffer) {
    rejectPromise();
    return;
  }
  scheduleAsyncImageBitmapDecoding(arrayBuffer);
}

void ImageBitmapFactories::ImageBitmapLoader::didFail(FileError::ErrorCode) {
  rejectPromise();
}

void ImageBitmapFactories::ImageBitmapLoader::scheduleAsyncImageBitmapDecoding(
    DOMArrayBuffer* arrayBuffer) {
  // For a 4000*4000 png image where each 10*10 tile is filled in by a random
  // RGBA value, the byteLength is around 2M, and it typically takes around
  // 4.5ms to decode on a current model of Linux desktop.
  const int longTaskByteLengthThreshold = 2000000;
  BackgroundTaskRunner::TaskSize taskSize =
      BackgroundTaskRunner::TaskSizeShortRunningTask;
  if (arrayBuffer->byteLength() >= longTaskByteLengthThreshold)
    taskSize = BackgroundTaskRunner::TaskSizeLongRunningTask;
  WebTaskRunner* taskRunner =
      Platform::current()->currentThread()->getWebTaskRunner();
  BackgroundTaskRunner::postOnBackgroundThread(
      BLINK_FROM_HERE,
      crossThreadBind(
          &ImageBitmapFactories::ImageBitmapLoader::decodeImageOnDecoderThread,
          wrapCrossThreadPersistent(this), crossThreadUnretained(taskRunner),
          wrapCrossThreadPersistent(arrayBuffer), m_options.premultiplyAlpha(),
          m_options.colorSpaceConversion()),
      taskSize);
}

void ImageBitmapFactories::ImageBitmapLoader::decodeImageOnDecoderThread(
    WebTaskRunner* taskRunner,
    DOMArrayBuffer* arrayBuffer,
    const String& premultiplyAlphaOption,
    const String& colorSpaceConversionOption) {
  ASSERT(!isMainThread());

  ImageDecoder::AlphaOption alphaOp = ImageDecoder::AlphaPremultiplied;
  if (premultiplyAlphaOption == "none")
    alphaOp = ImageDecoder::AlphaNotPremultiplied;
  ImageDecoder::ColorSpaceOption colorSpaceOp = ImageDecoder::ColorSpaceApplied;
  if (colorSpaceConversionOption == "none")
    colorSpaceOp = ImageDecoder::ColorSpaceIgnored;
  std::unique_ptr<ImageDecoder> decoder(ImageDecoder::create(
      SegmentReader::createFromSkData(SkData::MakeWithoutCopy(
          arrayBuffer->data(), arrayBuffer->byteLength())),
      true, alphaOp, colorSpaceOp));
  sk_sp<SkImage> frame;
  if (decoder) {
    frame = ImageBitmap::getSkImageFromDecoder(std::move(decoder));
  }
  taskRunner->postTask(
      BLINK_FROM_HERE,
      crossThreadBind(&ImageBitmapFactories::ImageBitmapLoader::
                          resolvePromiseOnOriginalThread,
                      wrapCrossThreadPersistent(this), std::move(frame)));
}

void ImageBitmapFactories::ImageBitmapLoader::resolvePromiseOnOriginalThread(
    sk_sp<SkImage> frame) {
  if (!frame) {
    rejectPromise();
    return;
  }
  ASSERT(frame->width() && frame->height());

  RefPtr<StaticBitmapImage> image = StaticBitmapImage::create(std::move(frame));
  image->setOriginClean(true);
  ImageBitmap* imageBitmap = ImageBitmap::create(image, m_cropRect, m_options);
  if (imageBitmap && imageBitmap->bitmapImage()) {
    m_resolver->resolve(imageBitmap);
  } else {
    rejectPromise();
    return;
  }
  m_factory->didFinishLoading(this);
}

DEFINE_TRACE(ImageBitmapFactories::ImageBitmapLoader) {
  visitor->trace(m_factory);
  visitor->trace(m_resolver);
}

}  // namespace blink
