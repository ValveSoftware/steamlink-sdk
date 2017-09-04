// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintPropertyTreePrinter_h
#define PaintPropertyTreePrinter_h

#include "core/CoreExport.h"
#include "wtf/text/WTFString.h"

#ifndef NDEBUG

namespace blink {

class ClipPaintPropertyNode;
class FrameView;
class EffectPaintPropertyNode;
class PropertyTreeState;
class ScrollPaintPropertyNode;
class TransformPaintPropertyNode;

}  // namespace blink

// Outside the blink namespace for ease of invocation from gdb.
CORE_EXPORT void showAllPropertyTrees(const blink::FrameView& rootFrame);
CORE_EXPORT void showTransformPropertyTree(const blink::FrameView& rootFrame);
CORE_EXPORT void showClipPropertyTree(const blink::FrameView& rootFrame);
CORE_EXPORT void showEffectPropertyTree(const blink::FrameView& rootFrame);
CORE_EXPORT void showScrollPropertyTree(const blink::FrameView& rootFrame);
CORE_EXPORT String
transformPropertyTreeAsString(const blink::FrameView& rootFrame);
CORE_EXPORT String clipPropertyTreeAsString(const blink::FrameView& rootFrame);
CORE_EXPORT String
effectPropertyTreeAsString(const blink::FrameView& rootFrame);
CORE_EXPORT String
scrollPropertyTreeAsString(const blink::FrameView& rootFrame);

CORE_EXPORT void showPaintPropertyPath(
    const blink::TransformPaintPropertyNode*);
CORE_EXPORT void showPaintPropertyPath(const blink::ClipPaintPropertyNode*);
CORE_EXPORT void showPaintPropertyPath(const blink::EffectPaintPropertyNode*);
CORE_EXPORT void showPaintPropertyPath(const blink::ScrollPaintPropertyNode*);
CORE_EXPORT String
transformPaintPropertyPathAsString(const blink::TransformPaintPropertyNode*);
CORE_EXPORT String
clipPaintPropertyPathAsString(const blink::ClipPaintPropertyNode*);
CORE_EXPORT String
effectPaintPropertyPathAsString(const blink::EffectPaintPropertyNode*);
CORE_EXPORT String
scrollPaintPropertyPathAsString(const blink::ScrollPaintPropertyNode*);

CORE_EXPORT void showPropertyTreeState(const blink::PropertyTreeState&);
CORE_EXPORT String propertyTreeStateAsString(const blink::PropertyTreeState&);

CORE_EXPORT String paintPropertyTreeGraph(const blink::FrameView&);

#endif  // ifndef NDEBUG

#endif  // PaintPropertyTreePrinter_h
