// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/offscreencanvas2d/OffscreenCanvasRenderingContext2D.h"

#include "bindings/modules/v8/OffscreenCanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContext.h"
#include "core/frame/ImageBitmap.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "wtf/Assertions.h"

#define UNIMPLEMENTED ASSERT_NOT_REACHED

namespace blink {

OffscreenCanvasRenderingContext2D::~OffscreenCanvasRenderingContext2D()
{
}

OffscreenCanvasRenderingContext2D::OffscreenCanvasRenderingContext2D(OffscreenCanvas* canvas, const CanvasContextCreationAttributes& attrs)
    : CanvasRenderingContext(nullptr, canvas)
    , m_hasAlpha(attrs.alpha())
{
}

DEFINE_TRACE(OffscreenCanvasRenderingContext2D)
{
    CanvasRenderingContext::trace(visitor);
    BaseRenderingContext2D::trace(visitor);
}

// BaseRenderingContext2D implementation
bool OffscreenCanvasRenderingContext2D::originClean() const
{
    return getOffscreenCanvas()->originClean();
}

void OffscreenCanvasRenderingContext2D::setOriginTainted()
{
    return getOffscreenCanvas()->setOriginTainted();
}

bool OffscreenCanvasRenderingContext2D::wouldTaintOrigin(CanvasImageSource* source, ExecutionContext* executionContext)
{
    if (executionContext->isWorkerGlobalScope()) {
        // Currently, we only support passing in ImageBitmap as source image in
        // drawImage() or createPattern() in a OffscreenCanvas2d in worker.
        ASSERT(source->isImageBitmap());
    }

    return CanvasRenderingContext::wouldTaintOrigin(source, executionContext->getSecurityOrigin());
}

int OffscreenCanvasRenderingContext2D::width() const
{
    return getOffscreenCanvas()->width();
}

int OffscreenCanvasRenderingContext2D::height() const
{
    return getOffscreenCanvas()->height();
}

bool OffscreenCanvasRenderingContext2D::hasImageBuffer() const
{
    return !!m_imageBuffer;
}

ImageBuffer* OffscreenCanvasRenderingContext2D::imageBuffer() const
{
    if (!m_imageBuffer) {
        // TODO: crbug.com/593514 Add support for GPU rendering
        OffscreenCanvasRenderingContext2D* nonConstThis = const_cast<OffscreenCanvasRenderingContext2D*>(this);
        nonConstThis->m_imageBuffer = ImageBuffer::create(IntSize(width(), height()), m_hasAlpha ? NonOpaque : Opaque, InitializeImagePixels);

        if (m_needsMatrixClipRestore) {
            restoreMatrixClipStack(m_imageBuffer->canvas());
            nonConstThis->m_needsMatrixClipRestore = false;
        }
    }

    return m_imageBuffer.get();
}

ImageBitmap* OffscreenCanvasRenderingContext2D::transferToImageBitmap(ExceptionState& exceptionState)
{
    if (!imageBuffer())
        return nullptr;
    // TODO: crbug.com/593514 Add support for GPU rendering
    RefPtr<SkImage> skImage = m_imageBuffer->newSkImageSnapshot(PreferNoAcceleration, SnapshotReasonTransferToImageBitmap);
    RefPtr<StaticBitmapImage> image = StaticBitmapImage::create(skImage.release());
    image->setOriginClean(this->originClean());
    m_imageBuffer.reset(); // "Transfer" means no retained buffer
    m_needsMatrixClipRestore = true;
    return ImageBitmap::create(image.release());
}

void OffscreenCanvasRenderingContext2D::setOffscreenCanvasGetContextResult(OffscreenRenderingContext& result)
{
    result.setOffscreenCanvasRenderingContext2D(this);
}

bool OffscreenCanvasRenderingContext2D::parseColorOrCurrentColor(Color& color, const String& colorString) const
{
    return ::blink::parseColorOrCurrentColor(color, colorString, nullptr);
}

SkCanvas* OffscreenCanvasRenderingContext2D::drawingCanvas() const
{
    ImageBuffer* buffer = imageBuffer();
    if (!buffer)
        return nullptr;
    return imageBuffer()->canvas();
}

SkCanvas* OffscreenCanvasRenderingContext2D::existingDrawingCanvas() const
{
    if (!m_imageBuffer)
        return nullptr;
    return m_imageBuffer->canvas();
}

void OffscreenCanvasRenderingContext2D::disableDeferral(DisableDeferralReason)
{ }

AffineTransform OffscreenCanvasRenderingContext2D::baseTransform() const
{
    if (!m_imageBuffer)
        return AffineTransform(); // identity
    return m_imageBuffer->baseTransform();
}

void OffscreenCanvasRenderingContext2D::didDraw(const SkIRect& dirtyRect)
{ }

bool OffscreenCanvasRenderingContext2D::stateHasFilter()
{
    // TODO: crbug.com/593838 make hasFilter accept nullptr
    // return state().hasFilter(nullptr, nullptr, IntSize(width(), height()), this);
    return false;
}

SkImageFilter* OffscreenCanvasRenderingContext2D::stateGetFilter()
{
    // TODO: make getFilter accept nullptr
    // return state().getFilter(nullptr, nullptr, IntSize(width(), height()), this);
    return nullptr;
}

void OffscreenCanvasRenderingContext2D::validateStateStack()
{
#if ENABLE(ASSERT)
    SkCanvas* skCanvas = existingDrawingCanvas();
    if (skCanvas) {
        ASSERT(static_cast<size_t>(skCanvas->getSaveCount()) == m_stateStack.size());
    }
#endif
}

bool OffscreenCanvasRenderingContext2D::isContextLost() const
{
    return false;
}

}
