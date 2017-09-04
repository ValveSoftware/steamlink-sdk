// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/offscreencanvas2d/OffscreenCanvasRenderingContext2D.h"

#include "bindings/modules/v8/OffscreenCanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContext.h"
#include "core/frame/ImageBitmap.h"
#include "core/frame/Settings.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerSettings.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/gpu/AcceleratedImageBufferSurface.h"
#include "wtf/Assertions.h"
#include "wtf/CurrentTime.h"

#define UNIMPLEMENTED ASSERT_NOT_REACHED

namespace blink {

OffscreenCanvasRenderingContext2D::~OffscreenCanvasRenderingContext2D() {}

OffscreenCanvasRenderingContext2D::OffscreenCanvasRenderingContext2D(
    ScriptState* scriptState,
    OffscreenCanvas* canvas,
    const CanvasContextCreationAttributes& attrs)
    : CanvasRenderingContext(nullptr, canvas, attrs) {
  ExecutionContext* executionContext = scriptState->getExecutionContext();
  if (executionContext->isDocument()) {
    if (toDocument(executionContext)->settings()->disableReadingFromCanvas())
      canvas->setDisableReadingFromCanvasTrue();
    return;
  }

  WorkerSettings* workerSettings =
      toWorkerGlobalScope(executionContext)->workerSettings();
  if (workerSettings && workerSettings->disableReadingFromCanvas())
    canvas->setDisableReadingFromCanvasTrue();
}

DEFINE_TRACE(OffscreenCanvasRenderingContext2D) {
  CanvasRenderingContext::trace(visitor);
  BaseRenderingContext2D::trace(visitor);
}

void OffscreenCanvasRenderingContext2D::commit(ScriptState* scriptState,
                                               ExceptionState& exceptionState) {
  UseCounter::Feature feature = UseCounter::OffscreenCanvasCommit2D;
  UseCounter::count(scriptState->getExecutionContext(), feature);
  if (!getOffscreenCanvas()->hasPlaceholderCanvas()) {
    // If an OffscreenCanvas has no associated canvas Id, it indicates that
    // it is not an OffscreenCanvas created by transfering control from html
    // canvas.
    exceptionState.throwDOMException(InvalidStateError,
                                     "Commit() was called on a context whose "
                                     "OffscreenCanvas is not associated with a "
                                     "canvas element.");
    return;
  }
  double commitStartTime = WTF::monotonicallyIncreasingTime();
  RefPtr<StaticBitmapImage> image = this->transferToStaticBitmapImage();
  getOffscreenCanvas()->getOrCreateFrameDispatcher()->dispatchFrame(
      std::move(image), commitStartTime);
}

// BaseRenderingContext2D implementation
bool OffscreenCanvasRenderingContext2D::originClean() const {
  return getOffscreenCanvas()->originClean();
}

void OffscreenCanvasRenderingContext2D::setOriginTainted() {
  return getOffscreenCanvas()->setOriginTainted();
}

bool OffscreenCanvasRenderingContext2D::wouldTaintOrigin(
    CanvasImageSource* source,
    ExecutionContext* executionContext) {
  if (executionContext->isWorkerGlobalScope()) {
    // We only support passing in ImageBitmap and OffscreenCanvases as source
    // images in drawImage() or createPattern() in a OffscreenCanvas2d in
    // worker.
    DCHECK(source->isImageBitmap() || source->isOffscreenCanvas());
  }

  return CanvasRenderingContext::wouldTaintOrigin(
      source, executionContext->getSecurityOrigin());
}

int OffscreenCanvasRenderingContext2D::width() const {
  return getOffscreenCanvas()->width();
}

int OffscreenCanvasRenderingContext2D::height() const {
  return getOffscreenCanvas()->height();
}

bool OffscreenCanvasRenderingContext2D::hasImageBuffer() const {
  return !!m_imageBuffer;
}

void OffscreenCanvasRenderingContext2D::reset() {
  m_imageBuffer = nullptr;
  BaseRenderingContext2D::reset();
}

ImageBuffer* OffscreenCanvasRenderingContext2D::imageBuffer() const {
  if (!m_imageBuffer) {
    IntSize surfaceSize(width(), height());
    OpacityMode opacityMode = hasAlpha() ? NonOpaque : Opaque;
    std::unique_ptr<ImageBufferSurface> surface;
    if (RuntimeEnabledFeatures::accelerated2dCanvasEnabled()) {
      surface.reset(
          new AcceleratedImageBufferSurface(surfaceSize, opacityMode));
    }

    if (!surface || !surface->isValid()) {
      surface.reset(new UnacceleratedImageBufferSurface(
          surfaceSize, opacityMode, InitializeImagePixels));
    }

    OffscreenCanvasRenderingContext2D* nonConstThis =
        const_cast<OffscreenCanvasRenderingContext2D*>(this);
    nonConstThis->m_imageBuffer = ImageBuffer::create(std::move(surface));

    if (m_needsMatrixClipRestore) {
      restoreMatrixClipStack(m_imageBuffer->canvas());
      nonConstThis->m_needsMatrixClipRestore = false;
    }
  }

  return m_imageBuffer.get();
}

RefPtr<StaticBitmapImage>
OffscreenCanvasRenderingContext2D::transferToStaticBitmapImage() {
  if (!imageBuffer())
    return nullptr;
  sk_sp<SkImage> skImage = m_imageBuffer->newSkImageSnapshot(
      PreferAcceleration, SnapshotReasonTransferToImageBitmap);
  RefPtr<StaticBitmapImage> image =
      StaticBitmapImage::create(std::move(skImage));
  image->setOriginClean(this->originClean());
  return image;
}

ImageBitmap* OffscreenCanvasRenderingContext2D::transferToImageBitmap(
    ScriptState* scriptState) {
  UseCounter::Feature feature =
      UseCounter::OffscreenCanvasTransferToImageBitmap2D;
  UseCounter::count(scriptState->getExecutionContext(), feature);
  RefPtr<StaticBitmapImage> image = transferToStaticBitmapImage();
  if (!image)
    return nullptr;
  m_imageBuffer.reset();  // "Transfer" means no retained buffer
  m_needsMatrixClipRestore = true;
  return ImageBitmap::create(std::move(image));
}

PassRefPtr<Image> OffscreenCanvasRenderingContext2D::getImage(
    AccelerationHint hint,
    SnapshotReason reason) const {
  if (!imageBuffer())
    return nullptr;
  sk_sp<SkImage> skImage = m_imageBuffer->newSkImageSnapshot(hint, reason);
  RefPtr<StaticBitmapImage> image =
      StaticBitmapImage::create(std::move(skImage));
  return image;
}

ImageData* OffscreenCanvasRenderingContext2D::toImageData(
    SnapshotReason reason) const {
  if (!imageBuffer())
    return nullptr;
  sk_sp<SkImage> snapshot =
      m_imageBuffer->newSkImageSnapshot(PreferNoAcceleration, reason);
  ImageData* imageData = nullptr;
  if (snapshot) {
    imageData = ImageData::create(this->getOffscreenCanvas()->size());
    SkImageInfo imageInfo =
        SkImageInfo::Make(this->width(), this->height(), kRGBA_8888_SkColorType,
                          kUnpremul_SkAlphaType);
    snapshot->readPixels(imageInfo, imageData->data()->data(),
                         imageInfo.minRowBytes(), 0, 0);
  }
  return imageData;
}

void OffscreenCanvasRenderingContext2D::setOffscreenCanvasGetContextResult(
    OffscreenRenderingContext& result) {
  result.setOffscreenCanvasRenderingContext2D(this);
}

bool OffscreenCanvasRenderingContext2D::parseColorOrCurrentColor(
    Color& color,
    const String& colorString) const {
  return ::blink::parseColorOrCurrentColor(color, colorString, nullptr);
}

SkCanvas* OffscreenCanvasRenderingContext2D::drawingCanvas() const {
  ImageBuffer* buffer = imageBuffer();
  if (!buffer)
    return nullptr;
  return imageBuffer()->canvas();
}

SkCanvas* OffscreenCanvasRenderingContext2D::existingDrawingCanvas() const {
  if (!m_imageBuffer)
    return nullptr;
  return m_imageBuffer->canvas();
}

void OffscreenCanvasRenderingContext2D::disableDeferral(DisableDeferralReason) {
}

AffineTransform OffscreenCanvasRenderingContext2D::baseTransform() const {
  if (!m_imageBuffer)
    return AffineTransform();  // identity
  return m_imageBuffer->baseTransform();
}

void OffscreenCanvasRenderingContext2D::didDraw(const SkIRect& dirtyRect) {}

bool OffscreenCanvasRenderingContext2D::stateHasFilter() {
  // TODO: crbug.com/593838 make hasFilter accept nullptr
  // return state().hasFilter(nullptr, nullptr, IntSize(width(), height()),
  // this);
  return false;
}

sk_sp<SkImageFilter> OffscreenCanvasRenderingContext2D::stateGetFilter() {
  // TODO: make getFilter accept nullptr
  // return state().getFilter(nullptr, nullptr, IntSize(width(), height()),
  // this);
  return nullptr;
}

void OffscreenCanvasRenderingContext2D::validateStateStack() const {
#if DCHECK_IS_ON()
  if (SkCanvas* skCanvas = existingDrawingCanvas()) {
    DCHECK_EQ(static_cast<size_t>(skCanvas->getSaveCount()),
              m_stateStack.size() + 1);
  }
#endif
}

bool OffscreenCanvasRenderingContext2D::isContextLost() const {
  return false;
}

bool OffscreenCanvasRenderingContext2D::isPaintable() const {
  return this->imageBuffer();
}
}
