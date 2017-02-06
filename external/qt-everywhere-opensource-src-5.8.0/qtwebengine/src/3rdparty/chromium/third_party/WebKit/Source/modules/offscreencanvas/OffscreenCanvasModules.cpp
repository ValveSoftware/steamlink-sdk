// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/offscreencanvas/OffscreenCanvasModules.h"

#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/offscreencanvas/OffscreenCanvas.h"
#include "modules/offscreencanvas2d/OffscreenCanvasRenderingContext2D.h"

namespace blink {

void OffscreenCanvasModules::getContext(ScriptState* scriptState, OffscreenCanvas& offscreenCanvas, const String& id, const CanvasContextCreationAttributes& attributes, ExceptionState& exceptionState, OffscreenRenderingContext& result)
{
    if (offscreenCanvas.isNeutered()) {
        exceptionState.throwDOMException(InvalidStateError, "OffscreenCanvas object is detached");
        return;
    }

    CanvasRenderingContext* context = offscreenCanvas.getCanvasRenderingContext(scriptState, id, attributes);
    if (context)
        context->setOffscreenCanvasGetContextResult(result);
}

} // namespace blink
