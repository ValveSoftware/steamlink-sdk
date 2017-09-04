// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KeyframeEffectReadOnly_h
#define KeyframeEffectReadOnly_h

#include "core/CoreExport.h"
#include "core/animation/AnimationEffectReadOnly.h"
#include "core/animation/EffectModel.h"

namespace blink {

class DictionarySequenceOrDictionary;
class Element;
class ExceptionState;
class ExecutionContext;
class KeyframeEffectOptions;
class PropertyHandle;
class SampledEffect;

// Represents the effect of an Animation on an Element's properties.
// http://w3c.github.io/web-animations/#the-keyframeeffect-interfaces
class CORE_EXPORT KeyframeEffectReadOnly : public AnimationEffectReadOnly {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum Priority { DefaultPriority, TransitionPriority };

  static KeyframeEffectReadOnly* create(Element*,
                                        EffectModel*,
                                        const Timing&,
                                        Priority = DefaultPriority,
                                        EventDelegate* = nullptr);
  // Web Animations API Bindings constructors.
  static KeyframeEffectReadOnly* create(
      ExecutionContext*,
      Element*,
      const DictionarySequenceOrDictionary& effectInput,
      double duration,
      ExceptionState&);
  static KeyframeEffectReadOnly* create(
      ExecutionContext*,
      Element*,
      const DictionarySequenceOrDictionary& effectInput,
      const KeyframeEffectOptions& timingInput,
      ExceptionState&);
  static KeyframeEffectReadOnly* create(
      ExecutionContext*,
      Element*,
      const DictionarySequenceOrDictionary& effectInput,
      ExceptionState&);

  ~KeyframeEffectReadOnly() override {}

  bool isKeyframeEffectReadOnly() const override { return true; }

  bool affects(PropertyHandle) const;
  const EffectModel* model() const { return m_model.get(); }
  EffectModel* model() { return m_model.get(); }
  void setModel(EffectModel* model) { m_model = model; }
  Priority getPriority() const { return m_priority; }
  Element* target() const { return m_target; }

  void notifySampledEffectRemovedFromAnimationStack();

  bool isCandidateForAnimationOnCompositor(double animationPlaybackRate) const;
  // Must only be called once.
  bool maybeStartAnimationOnCompositor(int group,
                                       double startTime,
                                       double timeOffset,
                                       double animationPlaybackRate);
  bool hasActiveAnimationsOnCompositor() const;
  bool hasActiveAnimationsOnCompositor(CSSPropertyID) const;
  bool cancelAnimationOnCompositor();
  void restartAnimationOnCompositor();
  void cancelIncompatibleAnimationsOnCompositor();
  void pauseAnimationForTestingOnCompositor(double pauseTime);

  void attachCompositedLayers();

  void setCompositorAnimationIdsForTesting(
      const Vector<int>& compositorAnimationIds) {
    m_compositorAnimationIds = compositorAnimationIds;
  }

  DECLARE_VIRTUAL_TRACE();

  void downgradeToNormal() { m_priority = DefaultPriority; }

 protected:
  KeyframeEffectReadOnly(Element*,
                         EffectModel*,
                         const Timing&,
                         Priority,
                         EventDelegate*);

  void applyEffects();
  void clearEffects();
  void updateChildrenAndEffects() const override;
  void attach(Animation*) override;
  void detach() override;
  void specifiedTimingChanged() override;
  double calculateTimeToEffectChange(bool forwards,
                                     double inheritedTime,
                                     double timeToNextIteration) const override;
  virtual bool hasIncompatibleStyle();
  bool hasMultipleTransformProperties() const;

 private:
  Member<Element> m_target;
  Member<EffectModel> m_model;
  Member<SampledEffect> m_sampledEffect;

  Priority m_priority;

  Vector<int> m_compositorAnimationIds;
};

DEFINE_TYPE_CASTS(KeyframeEffectReadOnly,
                  AnimationEffectReadOnly,
                  animationNode,
                  animationNode->isKeyframeEffectReadOnly(),
                  animationNode.isKeyframeEffectReadOnly());

}  // namespace blink

#endif  // KeyframeEffectReadOnly_h
