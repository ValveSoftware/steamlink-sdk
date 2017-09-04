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

#ifndef KeyframeEffect_h
#define KeyframeEffect_h

#include "core/CoreExport.h"
#include "core/animation/AnimationEffectTiming.h"
#include "core/animation/EffectModel.h"
#include "core/animation/KeyframeEffectReadOnly.h"

namespace blink {

class Element;
class ExceptionState;
class KeyframeEffectOptions;

// Represents the effect of an Animation on an Element's properties.
// http://w3c.github.io/web-animations/#keyframe-effect
class CORE_EXPORT KeyframeEffect final : public KeyframeEffectReadOnly {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static KeyframeEffect* create(Element*,
                                EffectModel*,
                                const Timing&,
                                KeyframeEffectReadOnly::Priority =
                                    KeyframeEffectReadOnly::DefaultPriority,
                                EventDelegate* = nullptr);
  // Web Animations API Bindings constructors.
  static KeyframeEffect* create(
      ExecutionContext*,
      Element*,
      const DictionarySequenceOrDictionary& effectInput,
      double duration,
      ExceptionState&);
  static KeyframeEffect* create(
      ExecutionContext*,
      Element*,
      const DictionarySequenceOrDictionary& effectInput,
      const KeyframeEffectOptions& timingInput,
      ExceptionState&);
  static KeyframeEffect* create(
      ExecutionContext*,
      Element*,
      const DictionarySequenceOrDictionary& effectInput,
      ExceptionState&);

  ~KeyframeEffect() override;

  bool isKeyframeEffect() const override { return true; }

  AnimationEffectTiming* timing() override;

 private:
  KeyframeEffect(Element*,
                 EffectModel*,
                 const Timing&,
                 KeyframeEffectReadOnly::Priority,
                 EventDelegate*);
};

DEFINE_TYPE_CASTS(KeyframeEffect,
                  AnimationEffectReadOnly,
                  animationNode,
                  animationNode->isKeyframeEffect(),
                  animationNode.isKeyframeEffect());

}  // namespace blink

#endif  // KeyframeEffect_h
