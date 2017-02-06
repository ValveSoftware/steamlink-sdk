// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WindowPaintWorklet_h
#define WindowPaintWorklet_h

#include "core/frame/DOMWindowProperty.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class DOMWindow;
class ExecutionContext;
class PaintWorklet;
class Worklet;

class MODULES_EXPORT WindowPaintWorklet final : public GarbageCollected<WindowPaintWorklet>, public Supplement<LocalDOMWindow>, public DOMWindowProperty {
    USING_GARBAGE_COLLECTED_MIXIN(WindowPaintWorklet);
public:
    static WindowPaintWorklet& from(LocalDOMWindow&);
    static Worklet* paintWorklet(ExecutionContext*, DOMWindow&);
    PaintWorklet* paintWorklet(ExecutionContext*) const;

    DECLARE_TRACE();

private:
    explicit WindowPaintWorklet(LocalDOMWindow&);
    static const char* supplementName();

    mutable Member<PaintWorklet> m_paintWorklet;
};

} // namespace blink

#endif // WindowPaintWorklet_h
