// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/offscreencanvas/OffscreenCanvas.h"

#include "core/dom/ExceptionCode.h"
#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/canvas/CanvasRenderingContextFactory.h"
#include "wtf/MathExtras.h"
#include <memory>

namespace blink {

OffscreenCanvas::OffscreenCanvas(const IntSize& size)
    : m_size(size)
    , m_originClean(true)
{
}

OffscreenCanvas* OffscreenCanvas::create(unsigned width, unsigned height)
{
    return new OffscreenCanvas(IntSize(clampTo<int>(width), clampTo<int>(height)));
}

void OffscreenCanvas::setWidth(unsigned width)
{
    m_size.setWidth(clampTo<int>(width));
}

void OffscreenCanvas::setHeight(unsigned height)
{
    m_size.setHeight(clampTo<int>(height));
}

void OffscreenCanvas::setNeutered()
{
    ASSERT(!m_context);
    m_isNeutered = true;
    m_size.setWidth(0);
    m_size.setHeight(0);
}

ImageBitmap* OffscreenCanvas::transferToImageBitmap(ExceptionState& exceptionState)
{
    if (m_isNeutered) {
        exceptionState.throwDOMException(InvalidStateError, "Cannot transfer an ImageBitmap from a detached OffscreenCanvas");
        return nullptr;
    }
    if (!m_context) {
        exceptionState.throwDOMException(InvalidStateError, "Cannot transfer an ImageBitmap from an OffscreenCanvas with no context");
        return nullptr;
    }
    ImageBitmap* image = m_context->transferToImageBitmap(exceptionState);
    if (!image) {
        // Undocumented exception (not in spec)
        exceptionState.throwDOMException(V8GeneralError, "Out of memory");
    }
    return image;
}

CanvasRenderingContext* OffscreenCanvas::getCanvasRenderingContext(ScriptState* scriptState, const String& id, const CanvasContextCreationAttributes& attributes)
{
    CanvasRenderingContext::ContextType contextType = CanvasRenderingContext::contextTypeFromId(id);

    // Unknown type.
    if (contextType == CanvasRenderingContext::ContextTypeCount)
        return nullptr;

    CanvasRenderingContextFactory* factory = getRenderingContextFactory(contextType);
    if (!factory)
        return nullptr;

    if (m_context) {
        if (m_context->getContextType() != contextType) {
            factory->onError(this, "OffscreenCanvas has an existing context of a different type");
            return nullptr;
        }
    } else {
        m_context = factory->create(scriptState, this, attributes);
    }

    return m_context.get();
}

OffscreenCanvas::ContextFactoryVector& OffscreenCanvas::renderingContextFactories()
{
    DEFINE_STATIC_LOCAL(ContextFactoryVector, s_contextFactories, (CanvasRenderingContext::ContextTypeCount));
    return s_contextFactories;
}

CanvasRenderingContextFactory* OffscreenCanvas::getRenderingContextFactory(int type)
{
    ASSERT(type < CanvasRenderingContext::ContextTypeCount);
    return renderingContextFactories()[type].get();
}

void OffscreenCanvas::registerRenderingContextFactory(std::unique_ptr<CanvasRenderingContextFactory> renderingContextFactory)
{
    CanvasRenderingContext::ContextType type = renderingContextFactory->getContextType();
    ASSERT(type < CanvasRenderingContext::ContextTypeCount);
    ASSERT(!renderingContextFactories()[type]);
    renderingContextFactories()[type] = std::move(renderingContextFactory);
}

bool OffscreenCanvas::originClean() const
{
    // TODO(crbug.com/607575): Make Settings accessable in worker and use
    // disableReadingFromCanvas to determine originClean value.
    return m_originClean;
}

DEFINE_TRACE(OffscreenCanvas)
{
    visitor->trace(m_context);
}

} // namespace blink
