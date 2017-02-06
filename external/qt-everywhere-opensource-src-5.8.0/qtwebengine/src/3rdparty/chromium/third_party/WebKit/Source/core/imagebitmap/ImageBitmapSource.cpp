// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/imagebitmap/ImageBitmapSource.h"

#include "core/frame/ImageBitmap.h"
#include "core/imagebitmap/ImageBitmapOptions.h"

namespace blink {

ScriptPromise ImageBitmapSource::fulfillImageBitmap(ScriptState* scriptState, ImageBitmap* imageBitmap)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (imageBitmap && imageBitmap->bitmapImage()) {
        resolver->resolve(imageBitmap);
    } else {
        resolver->reject(ScriptValue(scriptState, v8::Null(scriptState->isolate())));
    }
    return promise;
}

ScriptPromise ImageBitmapSource::createImageBitmap(ScriptState* scriptState, EventTarget& eventTarget, int sx, int sy, int sw, int sh, const ImageBitmapOptions& options, ExceptionState& exceptionState)
{
    return ScriptPromise();
}

} // namespace blink
