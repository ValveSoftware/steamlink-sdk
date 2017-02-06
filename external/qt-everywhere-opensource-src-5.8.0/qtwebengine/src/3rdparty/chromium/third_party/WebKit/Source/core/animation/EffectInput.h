// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EffectInput_h
#define EffectInput_h

#include "core/CoreExport.h"
#include "core/animation/EffectModel.h"
#include "wtf/Allocator.h"
#include "wtf/Vector.h"

namespace blink {

class EffectModel;
class EffectModelOrDictionarySequenceOrDictionary;
class Dictionary;
class Element;
class ExceptionState;
class ExecutionContext;

class CORE_EXPORT EffectInput {
    STATIC_ONLY(EffectInput);
public:
    // TODO(alancutter): Replace Element* parameter with Document&.
    static EffectModel* convert(Element*, const EffectModelOrDictionarySequenceOrDictionary&, ExecutionContext*, ExceptionState&);

private:
    static EffectModel* convertArrayForm(Element&, const Vector<Dictionary>& keyframes, ExceptionState&);
    static EffectModel* convertObjectForm(Element&, const Dictionary& keyframe, ExceptionState&);
};

} // namespace blink

#endif
