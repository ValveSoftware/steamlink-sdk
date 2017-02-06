// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/canvas/HTMLCanvasElementModule.h"

#include "core/dom/DOMNodeIds.h"
#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/offscreencanvas/OffscreenCanvas.h"

namespace blink {

void HTMLCanvasElementModule::getContext(HTMLCanvasElement& canvas, const String& type, const CanvasContextCreationAttributes& attributes, RenderingContext& result)
{
    CanvasRenderingContext* context = canvas.getCanvasRenderingContext(type, attributes);
    if (context) {
        context->setCanvasGetContextResult(result);
    }
}

OffscreenCanvas* HTMLCanvasElementModule::transferControlToOffscreen(HTMLCanvasElement& canvas, ExceptionState& exceptionState)
{
    if (!canvas.createSurfaceLayer()) {
        exceptionState.throwDOMException(V8GeneralError, "Offscreen canvas creation failed due to an internal timeout.");
        return nullptr;
    }

    return transferControlToOffscreenInternal(canvas, exceptionState);
}

OffscreenCanvas* HTMLCanvasElementModule::transferControlToOffscreenInternal(HTMLCanvasElement& canvas, ExceptionState& exceptionState)
{
    if (canvas.renderingContext()) {
        exceptionState.throwDOMException(InvalidStateError, "Cannot transfer control from a canvas that has a rendering context.");
        return nullptr;
    }
    OffscreenCanvas* offscreenCanvas = OffscreenCanvas::create(canvas.width(), canvas.height());
    offscreenCanvas->setAssociatedCanvasId(DOMNodeIds::idForNode(&canvas));
    return offscreenCanvas;
}

}
