// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLCanvasElementModule_h
#define HTMLCanvasElementModule_h

#include "core/html/HTMLCanvasElement.h"
#include "modules/ModulesExport.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CanvasContextCreationAttributes;
class HTMLCanvasElement;
class ScriptState;
class OffscreenCanvas;

class MODULES_EXPORT HTMLCanvasElementModule {
    STATIC_ONLY(HTMLCanvasElementModule);

    friend class HTMLCanvasElementModuleTest;

public:
    static void getContext(HTMLCanvasElement&, const String&, const CanvasContextCreationAttributes&, RenderingContext&);
    static OffscreenCanvas* transferControlToOffscreen(HTMLCanvasElement&, ExceptionState&);

private:
    static OffscreenCanvas* transferControlToOffscreenInternal(HTMLCanvasElement&, ExceptionState&);
};

}

#endif
