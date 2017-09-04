/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/HTMLCanvasElement.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptController.h"
#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/fileapi/File.h"
#include "core/frame/ImageBitmap.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/html/ImageData.h"
#include "core/html/canvas/CanvasAsyncBlobCreator.h"
#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasFontCache.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/canvas/CanvasRenderingContextFactory.h"
#include "core/imagebitmap/ImageBitmapOptions.h"
#include "core/layout/HitTestCanvasResult.h"
#include "core/layout/LayoutHTMLCanvas.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintTiming.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/Canvas2DImageBufferSurface.h"
#include "platform/graphics/CanvasMetrics.h"
#include "platform/graphics/ExpensiveCanvasHeuristicParameters.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/RecordingImageBufferSurface.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/gpu/AcceleratedImageBufferSurface.h"
#include "platform/image-encoders/ImageEncoderUtils.h"
#include "platform/transforms/AffineTransform.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTraceLocation.h"
#include "public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom-blink.h"
#include "wtf/CheckedNumeric.h"
#include "wtf/PtrUtil.h"
#include <math.h>
#include <memory>
#include <v8.h>

namespace blink {

using namespace HTMLNames;

namespace {

// These values come from the WhatWG spec.
const int DefaultWidth = 300;
const int DefaultHeight = 150;

#if OS(ANDROID)
// We estimate that the max limit for android phones is a quarter of that for
// desktops based on local experimental results on Android One.
const int MaxGlobalAcceleratedImageBufferCount = 25;
#else
const int MaxGlobalAcceleratedImageBufferCount = 100;
#endif

// We estimate the max limit of GPU allocated memory for canvases before Chrome
// becomes laggy by setting the total allocated memory for accelerated canvases
// to be equivalent to memory used by 100 accelerated canvases, each has a size
// of 1000*500 and 2d context.
// Each such canvas occupies 4000000 = 1000 * 500 * 2 * 4 bytes, where 2 is the
// gpuBufferCount in ImageBuffer::updateGPUMemoryUsage() and 4 means four bytes
// per pixel per buffer.
const int MaxGlobalGPUMemoryUsage =
    4000000 * MaxGlobalAcceleratedImageBufferCount;

// A default value of quality argument for toDataURL and toBlob
// It is in an invalid range (outside 0.0 - 1.0) so that it will not be
// misinterpreted as a user-input value
const int UndefinedQualityValue = -1.0;

PassRefPtr<Image> createTransparentImage(const IntSize& size) {
  DCHECK(ImageBuffer::canCreateImageBuffer(size));
  sk_sp<SkSurface> surface =
      SkSurface::MakeRasterN32Premul(size.width(), size.height());
  return StaticBitmapImage::create(surface->makeImageSnapshot());
}

}  // namespace

inline HTMLCanvasElement::HTMLCanvasElement(Document& document)
    : HTMLElement(canvasTag, document),
      ContextLifecycleObserver(&document),
      PageVisibilityObserver(document.page()),
      m_size(DefaultWidth, DefaultHeight),
      m_context(this, nullptr),
      m_ignoreReset(false),
      m_externallyAllocatedMemory(0),
      m_originClean(true),
      m_didFailToCreateImageBuffer(false),
      m_imageBufferIsClear(false),
      m_numFramesSinceLastRenderingModeSwitch(0),
      m_pendingRenderingModeSwitch(false) {
  CanvasMetrics::countCanvasContextUsage(CanvasMetrics::CanvasCreated);
  UseCounter::count(document, UseCounter::HTMLCanvasElement);
}

DEFINE_NODE_FACTORY(HTMLCanvasElement)

HTMLCanvasElement::~HTMLCanvasElement() {
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      -m_externallyAllocatedMemory);
}

void HTMLCanvasElement::dispose() {
  releasePlaceholderFrame();

  if (m_context) {
    m_context->detachCanvas();
    m_context = nullptr;
  }
  m_imageBuffer = nullptr;
}

void HTMLCanvasElement::parseAttribute(const QualifiedName& name,
                                       const AtomicString& oldValue,
                                       const AtomicString& value) {
  if (name == widthAttr || name == heightAttr)
    reset();
  HTMLElement::parseAttribute(name, oldValue, value);
}

LayoutObject* HTMLCanvasElement::createLayoutObject(
    const ComputedStyle& style) {
  LocalFrame* frame = document().frame();
  if (frame && frame->script().canExecuteScripts(NotAboutToExecuteScript))
    return new LayoutHTMLCanvas(this);
  return HTMLElement::createLayoutObject(style);
}

Node::InsertionNotificationRequest HTMLCanvasElement::insertedInto(
    ContainerNode* node) {
  setIsInCanvasSubtree(true);
  return HTMLElement::insertedInto(node);
}

void HTMLCanvasElement::setHeight(int value, ExceptionState& exceptionState) {
  if (surfaceLayerBridge()) {
    // The existence of surfaceLayerBridge indicates that
    // canvas.transferControlToOffscreen has been called.
    exceptionState.throwDOMException(InvalidStateError,
                                     "Resizing is not allowed for a canvas "
                                     "that has transferred its control to "
                                     "offscreen.");
    return;
  }
  setIntegralAttribute(heightAttr, value);
}

void HTMLCanvasElement::setWidth(int value, ExceptionState& exceptionState) {
  if (surfaceLayerBridge()) {
    // Same comment as above.
    exceptionState.throwDOMException(InvalidStateError,
                                     "Resizing is not allowed for a canvas "
                                     "that has transferred its control to "
                                     "offscreen.");
    return;
  }
  setIntegralAttribute(widthAttr, value);
}

void HTMLCanvasElement::setSize(const IntSize& newSize) {
  if (newSize == size())
    return;
  m_ignoreReset = true;
  setIntegralAttribute(widthAttr, newSize.width());
  setIntegralAttribute(heightAttr, newSize.height());
  m_ignoreReset = false;
  reset();
}

HTMLCanvasElement::ContextFactoryVector&
HTMLCanvasElement::renderingContextFactories() {
  DCHECK(isMainThread());
  DEFINE_STATIC_LOCAL(ContextFactoryVector, s_contextFactories,
                      (CanvasRenderingContext::ContextTypeCount));
  return s_contextFactories;
}

CanvasRenderingContextFactory* HTMLCanvasElement::getRenderingContextFactory(
    int type) {
  DCHECK(type < CanvasRenderingContext::ContextTypeCount);
  return renderingContextFactories()[type].get();
}

void HTMLCanvasElement::registerRenderingContextFactory(
    std::unique_ptr<CanvasRenderingContextFactory> renderingContextFactory) {
  CanvasRenderingContext::ContextType type =
      renderingContextFactory->getContextType();
  DCHECK(type < CanvasRenderingContext::ContextTypeCount);
  DCHECK(!renderingContextFactories()[type]);
  renderingContextFactories()[type] = std::move(renderingContextFactory);
}

CanvasRenderingContext* HTMLCanvasElement::getCanvasRenderingContext(
    const String& type,
    const CanvasContextCreationAttributes& attributes) {
  CanvasRenderingContext::ContextType contextType =
      CanvasRenderingContext::contextTypeFromId(type);

  // Unknown type.
  if (contextType == CanvasRenderingContext::ContextTypeCount)
    return nullptr;

  // Log the aliased context type used.
  if (!m_context) {
    DEFINE_STATIC_LOCAL(
        EnumerationHistogram, contextTypeHistogram,
        ("Canvas.ContextType", CanvasRenderingContext::ContextTypeCount));
    contextTypeHistogram.count(contextType);
  }

  contextType = CanvasRenderingContext::resolveContextTypeAliases(contextType);

  CanvasRenderingContextFactory* factory =
      getRenderingContextFactory(contextType);
  if (!factory)
    return nullptr;

  // FIXME - The code depends on the context not going away once created, to
  // prevent JS from seeing a dangling pointer. So for now we will disallow the
  // context from being changed once it is created.
  if (m_context) {
    if (m_context->getContextType() == contextType)
      return m_context.get();

    factory->onError(this,
                     "Canvas has an existing context of a different type");
    return nullptr;
  }

  m_context = factory->create(this, attributes, document());
  if (!m_context)
    return nullptr;

  if (m_context->is3d()) {
    updateExternallyAllocatedMemory();
  }
  setNeedsCompositingUpdate();

  return m_context.get();
}

bool HTMLCanvasElement::shouldBeDirectComposited() const {
  return (m_context && m_context->isAccelerated()) ||
         (hasImageBuffer() && buffer()->isExpensiveToPaint()) ||
         (!!m_surfaceLayerBridge);
}

bool HTMLCanvasElement::isPaintable() const {
  return (m_context && m_context->isPaintable()) ||
         ImageBuffer::canCreateImageBuffer(size());
}

bool HTMLCanvasElement::isAccelerated() const {
  return m_context && m_context->isAccelerated();
}

void HTMLCanvasElement::didDraw(const FloatRect& rect) {
  if (rect.isEmpty())
    return;
  m_imageBufferIsClear = false;
  clearCopiedImage();
  if (layoutObject())
    layoutObject()->setMayNeedPaintInvalidation();
  if (m_context && m_context->is2d() && m_context->shouldAntialias() &&
      page() && page()->deviceScaleFactor() > 1.0f) {
    FloatRect inflatedRect = rect;
    inflatedRect.inflate(1);
    m_dirtyRect.unite(inflatedRect);
  } else {
    m_dirtyRect.unite(rect);
  }
  if (m_context && m_context->is2d() && hasImageBuffer())
    buffer()->didDraw(rect);
}

void HTMLCanvasElement::didFinalizeFrame() {
  notifyListenersCanvasChanged();

  if (m_dirtyRect.isEmpty())
    return;

  // Propagate the m_dirtyRect accumulated so far to the compositor
  // before restarting with a blank dirty rect.
  FloatRect srcRect(0, 0, size().width(), size().height());

  LayoutBox* ro = layoutBox();
  // Canvas content updates do not need to be propagated as
  // paint invalidations if the canvas is accelerated, since
  // the canvas contents are sent separately through a texture layer.
  if (ro && (!m_context || !m_context->isAccelerated())) {
    // If ro->contentBoxRect() is larger than srcRect the canvas's image is
    // being stretched, so we need to account for color bleeding caused by the
    // interpollation filter.
    if (ro->contentBoxRect().width() > srcRect.width() ||
        ro->contentBoxRect().height() > srcRect.height()) {
      m_dirtyRect.inflate(0.5);
    }

    m_dirtyRect.intersect(srcRect);
    LayoutRect mappedDirtyRect(enclosingIntRect(
        mapRect(m_dirtyRect, srcRect, FloatRect(ro->contentBoxRect()))));
    // For querying PaintLayer::compositingState()
    // FIXME: is this invalidation using the correct compositing state?
    DisableCompositingQueryAsserts disabler;
    ro->invalidatePaintRectangle(mappedDirtyRect);
  }
  m_dirtyRect = FloatRect();

  m_numFramesSinceLastRenderingModeSwitch++;
  if (RuntimeEnabledFeatures::
          enableCanvas2dDynamicRenderingModeSwitchingEnabled() &&
      !RuntimeEnabledFeatures::canvas2dFixedRenderingModeEnabled()) {
    if (m_context->is2d() && buffer() && buffer()->isAccelerated() &&
        m_numFramesSinceLastRenderingModeSwitch >=
            ExpensiveCanvasHeuristicParameters::MinFramesBeforeSwitch &&
        !m_pendingRenderingModeSwitch) {
      if (!m_context->isAccelerationOptimalForCanvasContent()) {
        // The switch must be done asynchronously in order to avoid switching
        // during the paint invalidation step.
        Platform::current()->currentThread()->getWebTaskRunner()->postTask(
            BLINK_FROM_HERE,
            WTF::bind(
                [](WeakPtr<ImageBuffer> buffer) {
                  if (buffer) {
                    buffer->disableAcceleration();
                  }
                },
                m_imageBuffer->m_weakPtrFactory.createWeakPtr()));
        m_numFramesSinceLastRenderingModeSwitch = 0;
        m_pendingRenderingModeSwitch = true;
      }
    }
  }

  if (m_pendingRenderingModeSwitch && buffer() && !buffer()->isAccelerated()) {
    m_pendingRenderingModeSwitch = false;
  }

  m_context->incrementFrameCount();
}

void HTMLCanvasElement::didDisableAcceleration() {
  // We must force a paint invalidation on the canvas even if it's
  // content did not change because it layer was destroyed.
  didDraw(FloatRect(0, 0, size().width(), size().height()));
}

void HTMLCanvasElement::restoreCanvasMatrixClipStack(SkCanvas* canvas) const {
  if (m_context)
    m_context->restoreCanvasMatrixClipStack(canvas);
}

void HTMLCanvasElement::doDeferredPaintInvalidation() {
  DCHECK(!m_dirtyRect.isEmpty());
  if (!m_context->is2d()) {
    didFinalizeFrame();
  } else {
    DCHECK(hasImageBuffer());
    FloatRect srcRect(0, 0, size().width(), size().height());
    m_dirtyRect.intersect(srcRect);
    LayoutBox* lb = layoutBox();
    if (lb) {
      FloatRect mappedDirtyRect =
          mapRect(m_dirtyRect, srcRect, FloatRect(lb->contentBoxRect()));
      if (m_context->isAccelerated()) {
        // Accelerated 2D canvases need the dirty rect to be expressed relative
        // to the content box, as opposed to the layout box.
        mappedDirtyRect.move(-lb->contentBoxOffset());
      }
      m_imageBuffer->finalizeFrame(mappedDirtyRect);
    } else {
      m_imageBuffer->finalizeFrame(m_dirtyRect);
    }
  }
  DCHECK(m_dirtyRect.isEmpty());
}

void HTMLCanvasElement::reset() {
  if (m_ignoreReset)
    return;

  m_dirtyRect = FloatRect();

  bool ok;
  bool hadImageBuffer = hasImageBuffer();

  int w = getAttribute(widthAttr).toInt(&ok);
  if (!ok || w < 0)
    w = DefaultWidth;

  int h = getAttribute(heightAttr).toInt(&ok);
  if (!ok || h < 0)
    h = DefaultHeight;

  if (m_context && m_context->is2d())
    m_context->reset();

  IntSize oldSize = size();
  IntSize newSize(w, h);

  // If the size of an existing buffer matches, we can just clear it instead of
  // reallocating.  This optimization is only done for 2D canvases for now.
  if (hadImageBuffer && oldSize == newSize && m_context && m_context->is2d() &&
      !buffer()->isRecording()) {
    if (!m_imageBufferIsClear) {
      m_imageBufferIsClear = true;
      m_context->clearRect(0, 0, width(), height());
    }
    return;
  }

  setSurfaceSize(newSize);

  if (m_context && m_context->is3d() && oldSize != size())
    m_context->reshape(width(), height());

  if (LayoutObject* layoutObject = this->layoutObject()) {
    if (layoutObject->isCanvas()) {
      if (oldSize != size()) {
        toLayoutHTMLCanvas(layoutObject)->canvasSizeChanged();
        if (layoutBox() && layoutBox()->hasAcceleratedCompositing())
          layoutBox()->contentChanged(CanvasChanged);
      }
      if (hadImageBuffer)
        layoutObject->setShouldDoFullPaintInvalidation();
    }
  }
}

bool HTMLCanvasElement::paintsIntoCanvasBuffer() const {
  if (placeholderFrame())
    return false;
  DCHECK(m_context);
  if (!m_context->isAccelerated())
    return true;
  if (layoutBox() && layoutBox()->hasAcceleratedCompositing())
    return false;

  return true;
}

void HTMLCanvasElement::notifyListenersCanvasChanged() {
  if (m_listeners.size() == 0)
    return;

  if (!originClean()) {
    m_listeners.clear();
    return;
  }

  bool listenerNeedsNewFrameCapture = false;
  for (const CanvasDrawListener* listener : m_listeners) {
    if (listener->needsNewFrame()) {
      listenerNeedsNewFrameCapture = true;
    }
  }

  if (listenerNeedsNewFrameCapture) {
    SourceImageStatus status;
    RefPtr<Image> sourceImage = getSourceImageForCanvas(
        &status, PreferNoAcceleration, SnapshotReasonCanvasListenerCapture,
        FloatSize());
    if (status != NormalSourceImageStatus)
      return;
    sk_sp<SkImage> image = sourceImage->imageForCurrentFrame();
    for (CanvasDrawListener* listener : m_listeners) {
      if (listener->needsNewFrame()) {
        listener->sendNewFrame(image);
      }
    }
  }
}

void HTMLCanvasElement::paint(GraphicsContext& context, const LayoutRect& r) {
  // FIXME: crbug.com/438240; there is a bug with the new CSS blending and
  // compositing feature.
  if (!m_context && !placeholderFrame())
    return;

  const ComputedStyle* style = ensureComputedStyle();
  SkFilterQuality filterQuality =
      (style && style->imageRendering() == ImageRenderingPixelated)
          ? kNone_SkFilterQuality
          : kLow_SkFilterQuality;

  if (is3D()) {
    m_context->setFilterQuality(filterQuality);
  } else if (hasImageBuffer()) {
    m_imageBuffer->setFilterQuality(filterQuality);
  }

  if (hasImageBuffer() && !m_imageBufferIsClear)
    PaintTiming::from(document()).markFirstContentfulPaint();

  if (!paintsIntoCanvasBuffer() && !document().printing())
    return;

  if (placeholderFrame()) {
    DCHECK(document().printing());
    context.drawImage(placeholderFrame().get(), pixelSnappedIntRect(r));
    return;
  }

  // TODO(junov): Paint is currently only implemented by ImageBitmap contexts.
  // We could improve the abstraction by making all context types paint
  // themselves (implement paint()).
  if (m_context->paint(context, pixelSnappedIntRect(r)))
    return;

  m_context->paintRenderingResultsToCanvas(FrontBuffer);
  if (hasImageBuffer()) {
    if (!context.contextDisabled()) {
      SkBlendMode compositeOperator =
          !m_context || m_context->creationAttributes().alpha()
              ? SkBlendMode::kSrcOver
              : SkBlendMode::kSrc;
      buffer()->draw(context, pixelSnappedIntRect(r), 0, compositeOperator);
    }
  } else {
    // When alpha is false, we should draw to opaque black.
    if (!m_context->creationAttributes().alpha())
      context.fillRect(FloatRect(r), Color(0, 0, 0));
  }

  if (is3D() && paintsIntoCanvasBuffer())
    m_context->markLayerComposited();
}

bool HTMLCanvasElement::is3D() const {
  return m_context && m_context->is3d();
}

bool HTMLCanvasElement::isAnimated2D() const {
  return m_context && m_context->is2d() && hasImageBuffer() &&
         m_imageBuffer->wasDrawnToAfterSnapshot();
}

void HTMLCanvasElement::setSurfaceSize(const IntSize& size) {
  m_size = size;
  m_didFailToCreateImageBuffer = false;
  discardImageBuffer();
  clearCopiedImage();
  if (m_context && m_context->is2d() && m_context->isContextLost()) {
    m_context->didSetSurfaceSize();
  }
}

const AtomicString HTMLCanvasElement::imageSourceURL() const {
  return AtomicString(
      toDataURLInternal(ImageEncoderUtils::DefaultMimeType, 0, FrontBuffer));
}

void HTMLCanvasElement::prepareSurfaceForPaintingIfNeeded() const {
  DCHECK(m_context &&
         m_context->is2d());  // This function is called by the 2d context
  if (buffer())
    m_imageBuffer->prepareSurfaceForPaintingIfNeeded();
}

ImageData* HTMLCanvasElement::toImageData(SourceDrawingBuffer sourceBuffer,
                                          SnapshotReason reason) const {
  ImageData* imageData;
  if (is3D()) {
    // Get non-premultiplied data because of inaccurate premultiplied alpha
    // conversion of buffer()->toDataURL().
    imageData = m_context->paintRenderingResultsToImageData(sourceBuffer);
    if (imageData)
      return imageData;

    m_context->paintRenderingResultsToCanvas(sourceBuffer);
    imageData = ImageData::create(m_size);
    if (imageData && hasImageBuffer()) {
      sk_sp<SkImage> snapshot =
          buffer()->newSkImageSnapshot(PreferNoAcceleration, reason);
      if (snapshot) {
        SkImageInfo imageInfo = SkImageInfo::Make(
            width(), height(), kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
        snapshot->readPixels(imageInfo, imageData->data()->data(),
                             imageInfo.minRowBytes(), 0, 0);
      }
    }
    return imageData;
  }

  imageData = ImageData::create(m_size);

  if ((!m_context || !imageData) && !placeholderFrame())
    return imageData;

  DCHECK((m_context && m_context->is2d()) || placeholderFrame());
  sk_sp<SkImage> snapshot;
  if (hasImageBuffer()) {
    snapshot = buffer()->newSkImageSnapshot(PreferNoAcceleration, reason);
  } else if (placeholderFrame()) {
    snapshot = placeholderFrame()->imageForCurrentFrame();
  }

  if (snapshot) {
    SkImageInfo imageInfo = SkImageInfo::Make(
        width(), height(), kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
    snapshot->readPixels(imageInfo, imageData->data()->data(),
                         imageInfo.minRowBytes(), 0, 0);
  }

  return imageData;
}

String HTMLCanvasElement::toDataURLInternal(
    const String& mimeType,
    const double& quality,
    SourceDrawingBuffer sourceBuffer) const {
  if (!isPaintable())
    return String("data:,");

  String encodingMimeType = ImageEncoderUtils::toEncodingMimeType(
      mimeType, ImageEncoderUtils::EncodeReasonToDataURL);

  Optional<ScopedUsHistogramTimer> timer;
  if (encodingMimeType == "image/png") {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, scopedUsCounterPNG,
        new CustomCountHistogram("Blink.Canvas.ToDataURL.PNG", 0, 10000000,
                                 50));
    timer.emplace(scopedUsCounterPNG);
  } else if (encodingMimeType == "image/jpeg") {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, scopedUsCounterJPEG,
        new CustomCountHistogram("Blink.Canvas.ToDataURL.JPEG", 0, 10000000,
                                 50));
    timer.emplace(scopedUsCounterJPEG);
  } else if (encodingMimeType == "image/webp") {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, scopedUsCounterWEBP,
        new CustomCountHistogram("Blink.Canvas.ToDataURL.WEBP", 0, 10000000,
                                 50));
    timer.emplace(scopedUsCounterWEBP);
  } else {
    // Currently we only support three encoding types.
    NOTREACHED();
  }

  ImageData* imageData = toImageData(sourceBuffer, SnapshotReasonToDataURL);

  if (!imageData)  // allocation failure
    return String("data:,");

  return ImageDataBuffer(imageData->size(), imageData->data()->data())
      .toDataURL(encodingMimeType, quality);
}

String HTMLCanvasElement::toDataURL(const String& mimeType,
                                    const ScriptValue& qualityArgument,
                                    ExceptionState& exceptionState) const {
  if (!originClean()) {
    exceptionState.throwSecurityError("Tainted canvases may not be exported.");
    return String();
  }

  double quality = UndefinedQualityValue;
  if (!qualityArgument.isEmpty()) {
    v8::Local<v8::Value> v8Value = qualityArgument.v8Value();
    if (v8Value->IsNumber()) {
      quality = v8Value.As<v8::Number>()->Value();
    }
  }
  return toDataURLInternal(mimeType, quality, BackBuffer);
}

void HTMLCanvasElement::toBlob(BlobCallback* callback,
                               const String& mimeType,
                               const ScriptValue& qualityArgument,
                               ExceptionState& exceptionState) {
  if (!originClean()) {
    exceptionState.throwSecurityError("Tainted canvases may not be exported.");
    return;
  }

  if (!isPaintable()) {
    // If the canvas element's bitmap has no pixels
    TaskRunnerHelper::get(TaskType::CanvasBlobSerialization, &document())
        ->postTask(BLINK_FROM_HERE,
                   WTF::bind(&BlobCallback::handleEvent,
                             wrapPersistent(callback), nullptr));
    return;
  }

  double startTime = WTF::monotonicallyIncreasingTime();
  double quality = UndefinedQualityValue;
  if (!qualityArgument.isEmpty()) {
    v8::Local<v8::Value> v8Value = qualityArgument.v8Value();
    if (v8Value->IsNumber()) {
      quality = v8Value.As<v8::Number>()->Value();
    }
  }

  String encodingMimeType = ImageEncoderUtils::toEncodingMimeType(
      mimeType, ImageEncoderUtils::EncodeReasonToBlobCallback);

  ImageData* imageData = toImageData(BackBuffer, SnapshotReasonToBlob);

  if (!imageData) {
    // ImageData allocation faillure
    TaskRunnerHelper::get(TaskType::CanvasBlobSerialization, &document())
        ->postTask(BLINK_FROM_HERE,
                   WTF::bind(&BlobCallback::handleEvent,
                             wrapPersistent(callback), nullptr));
    return;
  }

  CanvasAsyncBlobCreator* asyncCreator = CanvasAsyncBlobCreator::create(
      imageData->data(), encodingMimeType, imageData->size(), callback,
      startTime, &document());

  asyncCreator->scheduleAsyncBlobCreation(quality);
}

void HTMLCanvasElement::addListener(CanvasDrawListener* listener) {
  m_listeners.add(listener);
}

void HTMLCanvasElement::removeListener(CanvasDrawListener* listener) {
  m_listeners.remove(listener);
}

SecurityOrigin* HTMLCanvasElement::getSecurityOrigin() const {
  return document().getSecurityOrigin();
}

bool HTMLCanvasElement::originClean() const {
  if (document().settings() &&
      document().settings()->disableReadingFromCanvas())
    return false;
  return m_originClean;
}

bool HTMLCanvasElement::shouldAccelerate(const IntSize& size) const {
  if (m_context && !m_context->is2d())
    return false;

  if (RuntimeEnabledFeatures::forceDisplayList2dCanvasEnabled())
    return false;

  if (!RuntimeEnabledFeatures::accelerated2dCanvasEnabled())
    return false;

  // The following is necessary for handling the special case of canvases in the
  // dev tools overlay, which run in a process that supports accelerated 2d
  // canvas but in a special compositing context that does not.
  if (layoutBox() && !layoutBox()->hasAcceleratedCompositing())
    return false;

  CheckedNumeric<int> checkedCanvasPixelCount = size.width();
  checkedCanvasPixelCount *= size.height();
  if (!checkedCanvasPixelCount.IsValid())
    return false;
  int canvasPixelCount = checkedCanvasPixelCount.ValueOrDie();

  if (RuntimeEnabledFeatures::displayList2dCanvasEnabled()) {
#if 0
        // TODO(junov): re-enable this code once we solve the problem of recording
        // GPU-backed images to an SkPicture for cross-context rendering crbug.com/490328

        // If the compositor provides GPU acceleration to display list canvases, we
        // prefer that over direct acceleration.
        if (document().viewportDescription().matchesHeuristicsForGpuRasterization())
            return false;
#endif
    // If the GPU resources would be very expensive, prefer a display list.
    if (canvasPixelCount > ExpensiveCanvasHeuristicParameters::
                               PreferDisplayListOverGpuSizeThreshold)
      return false;
  }

  // Do not use acceleration for small canvas.
  Settings* settings = document().settings();
  if (!settings ||
      canvasPixelCount < settings->minimumAccelerated2dCanvasSize())
    return false;

  // When GPU allocated memory runs low (due to having created too many
  // accelerated canvases), the compositor starves and browser becomes laggy.
  // Thus, we should stop allocating more GPU memory to new canvases created
  // when the current memory usage exceeds the threshold.
  if (ImageBuffer::getGlobalGPUMemoryUsage() >= MaxGlobalGPUMemoryUsage)
    return false;

  // Allocating too many GPU resources can makes us run into the driver's
  // resource limits. So we need to keep the number of texture resources
  // under tight control
  if (ImageBuffer::getGlobalAcceleratedImageBufferCount() >=
      MaxGlobalAcceleratedImageBufferCount)
    return false;

  return true;
}

class UnacceleratedSurfaceFactory
    : public RecordingImageBufferFallbackSurfaceFactory {
 public:
  virtual std::unique_ptr<ImageBufferSurface> createSurface(
      const IntSize& size,
      OpacityMode opacityMode,
      sk_sp<SkColorSpace> colorSpace,
      SkColorType colorType) {
    return wrapUnique(new UnacceleratedImageBufferSurface(
        size, opacityMode, InitializeImagePixels, colorSpace, colorType));
  }

  virtual ~UnacceleratedSurfaceFactory() {}
};

bool HTMLCanvasElement::shouldUseDisplayList(const IntSize& deviceSize) {
  if (m_context->colorSpace() != kLegacyCanvasColorSpace)
    return false;

  if (RuntimeEnabledFeatures::forceDisplayList2dCanvasEnabled())
    return true;

  if (!RuntimeEnabledFeatures::displayList2dCanvasEnabled())
    return false;

  return true;
}

std::unique_ptr<ImageBufferSurface>
HTMLCanvasElement::createWebGLImageBufferSurface(const IntSize& deviceSize,
                                                 OpacityMode opacityMode) {
  DCHECK(is3D());
  // If 3d, but the use of the canvas will be for non-accelerated content
  // then make a non-accelerated ImageBuffer. This means copying the internal
  // Image will require a pixel readback, but that is unavoidable in this case.
  auto surface = wrapUnique(new AcceleratedImageBufferSurface(
      deviceSize, opacityMode, m_context->skColorSpace(),
      m_context->colorType()));
  if (surface->isValid())
    return std::move(surface);
  return nullptr;
}

std::unique_ptr<ImageBufferSurface>
HTMLCanvasElement::createAcceleratedImageBufferSurface(
    const IntSize& deviceSize,
    OpacityMode opacityMode,
    int* msaaSampleCount) {
  if (!shouldAccelerate(deviceSize))
    return nullptr;

  if (document().settings())
    *msaaSampleCount =
        document().settings()->accelerated2dCanvasMSAASampleCount();

  // Avoid creating |contextProvider| until we're sure we want to try use it,
  // since it costs us GPU memory.
  std::unique_ptr<WebGraphicsContext3DProvider> contextProvider(
      Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
  if (!contextProvider) {
    CanvasMetrics::countCanvasContextUsage(
        CanvasMetrics::Accelerated2DCanvasGPUContextLost);
    return nullptr;
  }

  if (contextProvider->isSoftwareRendering())
    return nullptr;  // Don't use accelerated canvas with swiftshader.

  std::unique_ptr<ImageBufferSurface> surface =
      wrapUnique(new Canvas2DImageBufferSurface(
          std::move(contextProvider), deviceSize, *msaaSampleCount, opacityMode,
          Canvas2DLayerBridge::EnableAcceleration, m_context->skColorSpace(),
          m_context->colorType()));
  if (!surface->isValid()) {
    CanvasMetrics::countCanvasContextUsage(
        CanvasMetrics::GPUAccelerated2DCanvasImageBufferCreationFailed);
    return nullptr;
  }

  CanvasMetrics::countCanvasContextUsage(
      CanvasMetrics::GPUAccelerated2DCanvasImageBufferCreated);
  return surface;
}

std::unique_ptr<ImageBufferSurface>
HTMLCanvasElement::createUnacceleratedImageBufferSurface(
    const IntSize& deviceSize,
    OpacityMode opacityMode) {
  if (shouldUseDisplayList(deviceSize)) {
    auto surface = wrapUnique(new RecordingImageBufferSurface(
        deviceSize, wrapUnique(new UnacceleratedSurfaceFactory), opacityMode,
        m_context->skColorSpace(), m_context->colorType()));
    if (surface->isValid()) {
      CanvasMetrics::countCanvasContextUsage(
          CanvasMetrics::DisplayList2DCanvasImageBufferCreated);
      return std::move(surface);
    }
    // We fallback to a non-display-list surface without recording a metric
    // here.
  }

  auto surfaceFactory = makeUnique<UnacceleratedSurfaceFactory>();
  auto surface = surfaceFactory->createSurface(deviceSize, opacityMode,
                                               m_context->skColorSpace(),
                                               m_context->colorType());
  if (surface->isValid()) {
    CanvasMetrics::countCanvasContextUsage(
        CanvasMetrics::Unaccelerated2DCanvasImageBufferCreated);
    return surface;
  }

  CanvasMetrics::countCanvasContextUsage(
      CanvasMetrics::Unaccelerated2DCanvasImageBufferCreationFailed);
  return nullptr;
}

void HTMLCanvasElement::createImageBuffer() {
  createImageBufferInternal(nullptr);
  if (m_didFailToCreateImageBuffer && m_context->is2d() && !size().isEmpty())
    m_context->loseContext(CanvasRenderingContext::SyntheticLostContext);
}

void HTMLCanvasElement::createImageBufferInternal(
    std::unique_ptr<ImageBufferSurface> externalSurface) {
  DCHECK(!m_imageBuffer);

  m_didFailToCreateImageBuffer = true;
  m_imageBufferIsClear = true;

  if (!ImageBuffer::canCreateImageBuffer(size()))
    return;

  OpacityMode opacityMode =
      !m_context || m_context->creationAttributes().alpha() ? NonOpaque
                                                            : Opaque;
  int msaaSampleCount = 0;
  std::unique_ptr<ImageBufferSurface> surface;
  if (externalSurface) {
    if (externalSurface->isValid())
      surface = std::move(externalSurface);
  } else if (is3D()) {
    surface = createWebGLImageBufferSurface(size(), opacityMode);
  } else {
    surface = createAcceleratedImageBufferSurface(size(), opacityMode,
                                                  &msaaSampleCount);
    if (!surface) {
      surface = createUnacceleratedImageBufferSurface(size(), opacityMode);
    }
  }
  if (!surface)
    return;
  DCHECK(surface->isValid());
  m_imageBuffer = ImageBuffer::create(std::move(surface));
  DCHECK(m_imageBuffer);
  m_imageBuffer->setClient(this);

  m_didFailToCreateImageBuffer = false;

  updateExternallyAllocatedMemory();

  if (is3D()) {
    // Early out for WebGL canvases
    return;
  }

  m_imageBuffer->setClient(this);
  // Enabling MSAA overrides a request to disable antialiasing. This is true
  // regardless of whether the rendering mode is accelerated or not. For
  // consistency, we don't want to apply AA in accelerated canvases but not in
  // unaccelerated canvases.
  if (!msaaSampleCount && document().settings() &&
      !document().settings()->antialiased2dCanvasEnabled())
    m_context->setShouldAntialias(false);

  if (m_context)
    setNeedsCompositingUpdate();
}

void HTMLCanvasElement::notifySurfaceInvalid() {
  if (m_context && m_context->is2d())
    m_context->loseContext(CanvasRenderingContext::RealLostContext);
}

DEFINE_TRACE(HTMLCanvasElement) {
  visitor->trace(m_listeners);
  visitor->trace(m_context);
  ContextLifecycleObserver::trace(visitor);
  PageVisibilityObserver::trace(visitor);
  HTMLElement::trace(visitor);
}

DEFINE_TRACE_WRAPPERS(HTMLCanvasElement) {
  visitor->traceWrappers(m_context);
  HTMLElement::traceWrappers(visitor);
}

void HTMLCanvasElement::updateExternallyAllocatedMemory() const {
  int bufferCount = 0;
  if (m_imageBuffer) {
    bufferCount++;
    if (m_imageBuffer->isAccelerated()) {
      // The number of internal GPU buffers vary between one (stable
      // non-displayed state) and three (triple-buffered animations).
      // Adding 2 is a pessimistic but relevant estimate.
      // Note: These buffers might be allocated in GPU memory.
      bufferCount += 2;
    }
  }
  if (m_copiedImage)
    bufferCount++;

  // Four bytes per pixel per buffer.
  CheckedNumeric<intptr_t> checkedExternallyAllocatedMemory = 4 * bufferCount;
  if (is3D())
    checkedExternallyAllocatedMemory +=
        m_context->externallyAllocatedBytesPerPixel();

  checkedExternallyAllocatedMemory *= width();
  checkedExternallyAllocatedMemory *= height();
  intptr_t externallyAllocatedMemory =
      checkedExternallyAllocatedMemory.ValueOrDefault(
          std::numeric_limits<intptr_t>::max());

  // Subtracting two intptr_t that are known to be positive will never
  // underflow.
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      externallyAllocatedMemory - m_externallyAllocatedMemory);
  m_externallyAllocatedMemory = externallyAllocatedMemory;
}

SkCanvas* HTMLCanvasElement::drawingCanvas() const {
  return buffer() ? m_imageBuffer->canvas() : nullptr;
}

void HTMLCanvasElement::disableDeferral(DisableDeferralReason reason) const {
  if (buffer())
    m_imageBuffer->disableDeferral(reason);
}

SkCanvas* HTMLCanvasElement::existingDrawingCanvas() const {
  if (!hasImageBuffer())
    return nullptr;

  return m_imageBuffer->canvas();
}

ImageBuffer* HTMLCanvasElement::buffer() const {
  DCHECK(m_context);
  DCHECK(m_context->getContextType() !=
         CanvasRenderingContext::ContextImageBitmap);
  if (!hasImageBuffer() && !m_didFailToCreateImageBuffer)
    const_cast<HTMLCanvasElement*>(this)->createImageBuffer();
  return m_imageBuffer.get();
}

void HTMLCanvasElement::createImageBufferUsingSurfaceForTesting(
    std::unique_ptr<ImageBufferSurface> surface) {
  discardImageBuffer();
  setIntegralAttribute(widthAttr, surface->size().width());
  setIntegralAttribute(heightAttr, surface->size().height());
  createImageBufferInternal(std::move(surface));
}

void HTMLCanvasElement::ensureUnacceleratedImageBuffer() {
  DCHECK(m_context);
  if ((hasImageBuffer() && !m_imageBuffer->isAccelerated()) ||
      m_didFailToCreateImageBuffer)
    return;
  discardImageBuffer();
  OpacityMode opacityMode =
      m_context->creationAttributes().alpha() ? NonOpaque : Opaque;
  m_imageBuffer = ImageBuffer::create(size(), opacityMode);
  m_didFailToCreateImageBuffer = !m_imageBuffer;
}

PassRefPtr<Image> HTMLCanvasElement::copiedImage(
    SourceDrawingBuffer sourceBuffer,
    AccelerationHint hint) const {
  if (!isPaintable())
    return nullptr;
  if (!m_context)
    return createTransparentImage(size());

  if (m_context->getContextType() ==
      CanvasRenderingContext::ContextImageBitmap) {
    RefPtr<Image> image =
        m_context->getImage(hint, SnapshotReasonGetCopiedImage);
    if (image)
      return m_context->getImage(hint, SnapshotReasonGetCopiedImage);
    // Special case: transferFromImageBitmap is not yet called.
    sk_sp<SkSurface> surface =
        SkSurface::MakeRasterN32Premul(width(), height());
    return StaticBitmapImage::create(surface->makeImageSnapshot());
  }

  bool needToUpdate = !m_copiedImage;
  // The concept of SourceDrawingBuffer is valid on only WebGL.
  if (m_context->is3d())
    needToUpdate |= m_context->paintRenderingResultsToCanvas(sourceBuffer);
  if (needToUpdate && buffer()) {
    m_copiedImage =
        buffer()->newImageSnapshot(hint, SnapshotReasonGetCopiedImage);
    updateExternallyAllocatedMemory();
  }
  return m_copiedImage;
}

void HTMLCanvasElement::discardImageBuffer() {
  m_imageBuffer.reset();
  m_dirtyRect = FloatRect();
  updateExternallyAllocatedMemory();
}

void HTMLCanvasElement::clearCopiedImage() {
  if (m_copiedImage) {
    m_copiedImage.clear();
    updateExternallyAllocatedMemory();
  }
}

AffineTransform HTMLCanvasElement::baseTransform() const {
  DCHECK(hasImageBuffer() && !m_didFailToCreateImageBuffer);
  return m_imageBuffer->baseTransform();
}

void HTMLCanvasElement::pageVisibilityChanged() {
  if (!m_context)
    return;

  bool hidden = !page()->isPageVisible();
  m_context->setIsHidden(hidden);
  if (hidden) {
    clearCopiedImage();
    if (is3D()) {
      discardImageBuffer();
    }
  }
}

void HTMLCanvasElement::contextDestroyed() {
  if (m_context)
    m_context->stop();
}

void HTMLCanvasElement::styleDidChange(const ComputedStyle* oldStyle,
                                       const ComputedStyle& newStyle) {
  if (m_context)
    m_context->styleDidChange(oldStyle, newStyle);
}

void HTMLCanvasElement::didMoveToNewDocument(Document& oldDocument) {
  ContextLifecycleObserver::setContext(&document());
  PageVisibilityObserver::setContext(document().page());
  HTMLElement::didMoveToNewDocument(oldDocument);
}

PassRefPtr<Image> HTMLCanvasElement::getSourceImageForCanvas(
    SourceImageStatus* status,
    AccelerationHint hint,
    SnapshotReason reason,
    const FloatSize&) const {
  if (!width() || !height()) {
    *status = ZeroSizeCanvasSourceImageStatus;
    return nullptr;
  }

  if (!isPaintable()) {
    *status = InvalidSourceImageStatus;
    return nullptr;
  }

  if (placeholderFrame()) {
    *status = NormalSourceImageStatus;
    return placeholderFrame();
  }

  if (!m_context) {
    *status = NormalSourceImageStatus;
    return createTransparentImage(size());
  }

  if (m_context->getContextType() == CanvasRenderingContext::ContextImageBitmap)
    return m_context->getImage(hint, reason);

  sk_sp<SkImage> skImage;
  if (m_context->is3d()) {
    // Because WebGL sources always require making a copy of the back buffer, we
    // use paintRenderingResultsToCanvas instead of getImage in order to keep a
    // cached copy of the backing in the canvas's ImageBuffer.
    renderingContext()->paintRenderingResultsToCanvas(BackBuffer);
    skImage = hasImageBuffer()
                  ? buffer()->newSkImageSnapshot(hint, reason)
                  : createTransparentImage(size())->imageForCurrentFrame();
  } else {
    if (ExpensiveCanvasHeuristicParameters::
            DisableAccelerationToAvoidReadbacks &&
        !RuntimeEnabledFeatures::canvas2dFixedRenderingModeEnabled() &&
        hint == PreferNoAcceleration && m_context->isAccelerated() &&
        hasImageBuffer())
      buffer()->disableAcceleration();
    RefPtr<blink::Image> image = renderingContext()->getImage(hint, reason);
    skImage = image ? image->imageForCurrentFrame()
                    : createTransparentImage(size())->imageForCurrentFrame();
  }

  if (skImage) {
    *status = NormalSourceImageStatus;
    return StaticBitmapImage::create(std::move(skImage));
  }

  *status = InvalidSourceImageStatus;
  return nullptr;
}

bool HTMLCanvasElement::wouldTaintOrigin(SecurityOrigin*) const {
  return !originClean();
}

FloatSize HTMLCanvasElement::elementSize(const FloatSize&) const {
  if (m_context &&
      m_context->getContextType() ==
          CanvasRenderingContext::ContextImageBitmap) {
    RefPtr<Image> image =
        m_context->getImage(PreferNoAcceleration, SnapshotReasonDrawImage);
    if (image)
      return FloatSize(image->width(), image->height());
    return FloatSize(0, 0);
  }
  return FloatSize(width(), height());
}

IntSize HTMLCanvasElement::bitmapSourceSize() const {
  return IntSize(width(), height());
}

ScriptPromise HTMLCanvasElement::createImageBitmap(
    ScriptState* scriptState,
    EventTarget& eventTarget,
    Optional<IntRect> cropRect,
    const ImageBitmapOptions& options,
    ExceptionState& exceptionState) {
  DCHECK(eventTarget.toLocalDOMWindow());
  if ((cropRect &&
       !ImageBitmap::isSourceSizeValid(cropRect->width(), cropRect->height(),
                                       exceptionState)) ||
      !ImageBitmap::isSourceSizeValid(bitmapSourceSize().width(),
                                      bitmapSourceSize().height(),
                                      exceptionState))
    return ScriptPromise();
  if (!ImageBitmap::isResizeOptionValid(options, exceptionState))
    return ScriptPromise();
  return ImageBitmapSource::fulfillImageBitmap(
      scriptState,
      isPaintable() ? ImageBitmap::create(this, cropRect, options) : nullptr);
}

bool HTMLCanvasElement::isOpaque() const {
  return m_context && !m_context->creationAttributes().alpha();
}

bool HTMLCanvasElement::isSupportedInteractiveCanvasFallback(
    const Element& element) {
  if (!element.isDescendantOf(this))
    return false;

  // An element is a supported interactive canvas fallback element if it is one
  // of the following:
  // https://html.spec.whatwg.org/multipage/scripting.html#supported-interactive-canvas-fallback-element

  // An a element that represents a hyperlink and that does not have any img
  // descendants.
  if (isHTMLAnchorElement(element))
    return !Traversal<HTMLImageElement>::firstWithin(element);

  // A button element
  if (isHTMLButtonElement(element))
    return true;

  // An input element whose type attribute is in one of the Checkbox or Radio
  // Button states.  An input element that is a button but its type attribute is
  // not in the Image Button state.
  if (isHTMLInputElement(element)) {
    const HTMLInputElement& inputElement = toHTMLInputElement(element);
    if (inputElement.type() == InputTypeNames::checkbox ||
        inputElement.type() == InputTypeNames::radio ||
        inputElement.isTextButton())
      return true;
  }

  // A select element with a "multiple" attribute or with a display size greater
  // than 1.
  if (isHTMLSelectElement(element)) {
    const HTMLSelectElement& selectElement = toHTMLSelectElement(element);
    if (selectElement.isMultiple() || selectElement.size() > 1)
      return true;
  }

  // An option element that is in a list of options of a select element with a
  // "multiple" attribute or with a display size greater than 1.
  if (isHTMLOptionElement(element) && element.parentNode() &&
      isHTMLSelectElement(*element.parentNode())) {
    const HTMLSelectElement& selectElement =
        toHTMLSelectElement(*element.parentNode());
    if (selectElement.isMultiple() || selectElement.size() > 1)
      return true;
  }

  // An element that would not be interactive content except for having the
  // tabindex attribute specified.
  if (element.fastHasAttribute(HTMLNames::tabindexAttr))
    return true;

  // A non-interactive table, caption, thead, tbody, tfoot, tr, td, or th
  // element.
  if (isHTMLTableElement(element) ||
      element.hasTagName(HTMLNames::captionTag) ||
      element.hasTagName(HTMLNames::theadTag) ||
      element.hasTagName(HTMLNames::tbodyTag) ||
      element.hasTagName(HTMLNames::tfootTag) ||
      element.hasTagName(HTMLNames::trTag) ||
      element.hasTagName(HTMLNames::tdTag) ||
      element.hasTagName(HTMLNames::thTag))
    return true;

  return false;
}

HitTestCanvasResult* HTMLCanvasElement::getControlAndIdIfHitRegionExists(
    const LayoutPoint& location) {
  if (m_context && m_context->is2d())
    return m_context->getControlAndIdIfHitRegionExists(location);
  return HitTestCanvasResult::create(String(), nullptr);
}

String HTMLCanvasElement::getIdFromControl(const Element* element) {
  if (m_context)
    return m_context->getIdFromControl(element);
  return String();
}

bool HTMLCanvasElement::createSurfaceLayer() {
  DCHECK(!m_surfaceLayerBridge);
  mojom::blink::OffscreenCanvasSurfacePtr service;
  Platform::current()->interfaceProvider()->getInterface(
      mojo::GetProxy(&service));
  m_surfaceLayerBridge =
      wrapUnique(new CanvasSurfaceLayerBridge(std::move(service)));
  return m_surfaceLayerBridge->createSurfaceLayer(this->width(),
                                                  this->height());
}

}  // namespace blink
