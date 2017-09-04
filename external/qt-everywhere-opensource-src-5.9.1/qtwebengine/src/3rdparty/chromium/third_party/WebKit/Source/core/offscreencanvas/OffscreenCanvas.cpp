// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/offscreencanvas/OffscreenCanvas.h"

#include "core/dom/ExceptionCode.h"
#include "core/fileapi/Blob.h"
#include "core/html/ImageData.h"
#include "core/html/canvas/CanvasAsyncBlobCreator.h"
#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/canvas/CanvasRenderingContextFactory.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/OffscreenCanvasFrameDispatcherImpl.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/image-encoders/ImageEncoderUtils.h"
#include "wtf/MathExtras.h"
#include <memory>

namespace blink {

OffscreenCanvas::OffscreenCanvas(const IntSize& size)
    : m_size(size), m_originClean(true) {}

OffscreenCanvas* OffscreenCanvas::create(unsigned width, unsigned height) {
  return new OffscreenCanvas(
      IntSize(clampTo<int>(width), clampTo<int>(height)));
}

void OffscreenCanvas::setWidth(unsigned width) {
  IntSize newSize = m_size;
  newSize.setWidth(clampTo<int>(width));
  setSize(newSize);
}

void OffscreenCanvas::setHeight(unsigned height) {
  IntSize newSize = m_size;
  newSize.setHeight(clampTo<int>(height));
  setSize(newSize);
}

void OffscreenCanvas::setSize(const IntSize& size) {
  if (m_context) {
    if (m_context->is3d()) {
      if (size != m_size)
        m_context->reshape(size.width(), size.height());
    } else if (m_context->is2d()) {
      m_context->reset();
    }
  }
  m_size = size;
}

void OffscreenCanvas::setNeutered() {
  ASSERT(!m_context);
  m_isNeutered = true;
  m_size.setWidth(0);
  m_size.setHeight(0);
}

ImageBitmap* OffscreenCanvas::transferToImageBitmap(
    ScriptState* scriptState,
    ExceptionState& exceptionState) {
  if (m_isNeutered) {
    exceptionState.throwDOMException(
        InvalidStateError,
        "Cannot transfer an ImageBitmap from a detached OffscreenCanvas");
    return nullptr;
  }
  if (!m_context) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "Cannot transfer an ImageBitmap from an "
                                     "OffscreenCanvas with no context");
    return nullptr;
  }
  ImageBitmap* image = m_context->transferToImageBitmap(scriptState);
  if (!image) {
    // Undocumented exception (not in spec)
    exceptionState.throwDOMException(V8Error, "Out of memory");
  }
  return image;
}

PassRefPtr<Image> OffscreenCanvas::getSourceImageForCanvas(
    SourceImageStatus* status,
    AccelerationHint hint,
    SnapshotReason reason,
    const FloatSize& size) const {
  if (!m_context) {
    *status = InvalidSourceImageStatus;
    return nullptr;
  }
  if (!size.width() || !size.height()) {
    *status = ZeroSizeCanvasSourceImageStatus;
    return nullptr;
  }
  RefPtr<Image> image = m_context->getImage(hint, reason);
  if (!image) {
    *status = InvalidSourceImageStatus;
  } else {
    *status = NormalSourceImageStatus;
  }
  return image.release();
}

bool OffscreenCanvas::isOpaque() const {
  if (!m_context)
    return false;
  return !m_context->creationAttributes().hasAlpha();
}

CanvasRenderingContext* OffscreenCanvas::getCanvasRenderingContext(
    ScriptState* scriptState,
    const String& id,
    const CanvasContextCreationAttributes& attributes) {
  CanvasRenderingContext::ContextType contextType =
      CanvasRenderingContext::contextTypeFromId(id);

  // Unknown type.
  if (contextType == CanvasRenderingContext::ContextTypeCount)
    return nullptr;

  CanvasRenderingContextFactory* factory =
      getRenderingContextFactory(contextType);
  if (!factory)
    return nullptr;

  if (m_context) {
    if (m_context->getContextType() != contextType) {
      factory->onError(
          this, "OffscreenCanvas has an existing context of a different type");
      return nullptr;
    }
  } else {
    m_context = factory->create(scriptState, this, attributes);
  }

  return m_context.get();
}

OffscreenCanvas::ContextFactoryVector&
OffscreenCanvas::renderingContextFactories() {
  DEFINE_STATIC_LOCAL(ContextFactoryVector, s_contextFactories,
                      (CanvasRenderingContext::ContextTypeCount));
  return s_contextFactories;
}

CanvasRenderingContextFactory* OffscreenCanvas::getRenderingContextFactory(
    int type) {
  ASSERT(type < CanvasRenderingContext::ContextTypeCount);
  return renderingContextFactories()[type].get();
}

void OffscreenCanvas::registerRenderingContextFactory(
    std::unique_ptr<CanvasRenderingContextFactory> renderingContextFactory) {
  CanvasRenderingContext::ContextType type =
      renderingContextFactory->getContextType();
  ASSERT(type < CanvasRenderingContext::ContextTypeCount);
  ASSERT(!renderingContextFactories()[type]);
  renderingContextFactories()[type] = std::move(renderingContextFactory);
}

bool OffscreenCanvas::originClean() const {
  return m_originClean && !m_disableReadingFromCanvas;
}

bool OffscreenCanvas::isPaintable() const {
  if (!m_context)
    return ImageBuffer::canCreateImageBuffer(m_size);
  return m_context->isPaintable();
}

bool OffscreenCanvas::isAccelerated() const {
  return m_context && m_context->isAccelerated();
}

OffscreenCanvasFrameDispatcher* OffscreenCanvas::getOrCreateFrameDispatcher() {
  if (!m_frameDispatcher) {
    // The frame dispatcher connects the current thread of OffscreenCanvas
    // (either main or worker) to the browser process and remains unchanged
    // throughout the lifetime of this OffscreenCanvas.
    m_frameDispatcher = wrapUnique(new OffscreenCanvasFrameDispatcherImpl(
        m_clientId, m_sinkId, m_localId, m_nonceHigh, m_nonceLow,
        m_placeholderCanvasId, width(), height()));
  }
  return m_frameDispatcher.get();
}

ScriptPromise OffscreenCanvas::convertToBlob(ScriptState* scriptState,
                                             const ImageEncodeOptions& options,
                                             ExceptionState& exceptionState) {
  if (this->isNeutered()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "OffscreenCanvas object is detached.");
    return exceptionState.reject(scriptState);
  }

  if (!this->originClean()) {
    exceptionState.throwSecurityError(
        "Tainted OffscreenCanvas may not be exported.");
    return exceptionState.reject(scriptState);
  }

  if (!this->isPaintable()) {
    return ScriptPromise();
  }

  double startTime = WTF::monotonicallyIncreasingTime();
  String encodingMimeType = ImageEncoderUtils::toEncodingMimeType(
      options.type(), ImageEncoderUtils::EncodeReasonConvertToBlobPromise);

  ImageData* imageData = nullptr;
  if (this->renderingContext()) {
    imageData = this->renderingContext()->toImageData(SnapshotReasonUnknown);
  }
  if (!imageData) {
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);

  Document* document =
      scriptState->getExecutionContext()->isDocument()
          ? static_cast<Document*>(scriptState->getExecutionContext())
          : nullptr;

  CanvasAsyncBlobCreator* asyncCreator = CanvasAsyncBlobCreator::create(
      imageData->data(), encodingMimeType, imageData->size(), startTime,
      document, resolver);

  asyncCreator->scheduleAsyncBlobCreation(options.quality());

  return resolver->promise();
}

DEFINE_TRACE(OffscreenCanvas) {
  visitor->trace(m_context);
  visitor->trace(m_executionContext);
  EventTarget::trace(visitor);
}

}  // namespace blink
