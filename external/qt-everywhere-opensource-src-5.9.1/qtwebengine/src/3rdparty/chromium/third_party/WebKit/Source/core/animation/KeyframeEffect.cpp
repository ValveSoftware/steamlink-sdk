/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/animation/KeyframeEffect.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/animation/AnimationEffectTiming.h"
#include "core/animation/EffectInput.h"
#include "core/animation/KeyframeEffectOptions.h"
#include "core/animation/KeyframeEffectReadOnly.h"
#include "core/animation/TimingInput.h"
#include "core/dom/Element.h"
#include "core/frame/UseCounter.h"

namespace blink {

KeyframeEffect* KeyframeEffect::create(
    Element* target,
    EffectModel* model,
    const Timing& timing,
    KeyframeEffectReadOnly::Priority priority,
    EventDelegate* eventDelegate) {
  return new KeyframeEffect(target, model, timing, priority, eventDelegate);
}

KeyframeEffect* KeyframeEffect::create(
    ExecutionContext* executionContext,
    Element* element,
    const DictionarySequenceOrDictionary& effectInput,
    double duration,
    ExceptionState& exceptionState) {
  DCHECK(RuntimeEnabledFeatures::webAnimationsAPIEnabled());
  if (element) {
    UseCounter::count(
        element->document(),
        UseCounter::AnimationConstructorKeyframeListEffectObjectTiming);
  }
  Timing timing;
  if (!TimingInput::convert(duration, timing, exceptionState))
    return nullptr;
  return create(element, EffectInput::convert(element, effectInput,
                                              executionContext, exceptionState),
                timing);
}

KeyframeEffect* KeyframeEffect::create(
    ExecutionContext* executionContext,
    Element* element,
    const DictionarySequenceOrDictionary& effectInput,
    const KeyframeEffectOptions& timingInput,
    ExceptionState& exceptionState) {
  DCHECK(RuntimeEnabledFeatures::webAnimationsAPIEnabled());
  if (element) {
    UseCounter::count(
        element->document(),
        UseCounter::AnimationConstructorKeyframeListEffectObjectTiming);
  }
  Timing timing;
  Document* document = element ? &element->document() : nullptr;
  if (!TimingInput::convert(timingInput, timing, document, exceptionState))
    return nullptr;
  return create(element, EffectInput::convert(element, effectInput,
                                              executionContext, exceptionState),
                timing);
}

KeyframeEffect* KeyframeEffect::create(
    ExecutionContext* executionContext,
    Element* element,
    const DictionarySequenceOrDictionary& effectInput,
    ExceptionState& exceptionState) {
  DCHECK(RuntimeEnabledFeatures::webAnimationsAPIEnabled());
  if (element) {
    UseCounter::count(
        element->document(),
        UseCounter::AnimationConstructorKeyframeListEffectNoTiming);
  }
  return create(element, EffectInput::convert(element, effectInput,
                                              executionContext, exceptionState),
                Timing());
}

KeyframeEffect::KeyframeEffect(Element* target,
                               EffectModel* model,
                               const Timing& timing,
                               KeyframeEffectReadOnly::Priority priority,
                               EventDelegate* eventDelegate)
    : KeyframeEffectReadOnly(target, model, timing, priority, eventDelegate) {}

KeyframeEffect::~KeyframeEffect() {}

AnimationEffectTiming* KeyframeEffect::timing() {
  return AnimationEffectTiming::create(this);
}

}  // namespace blink
