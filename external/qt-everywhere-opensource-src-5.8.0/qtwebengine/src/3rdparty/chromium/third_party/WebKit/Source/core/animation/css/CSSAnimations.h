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

#ifndef CSSAnimations_h
#define CSSAnimations_h

#include "core/animation/InertEffect.h"
#include "core/animation/Interpolation.h"
#include "core/animation/css/CSSAnimationData.h"
#include "core/animation/css/CSSAnimationUpdate.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/StylePropertySet.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "wtf/HashMap.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class CSSTransitionData;
class Element;
class StylePropertyShorthand;
class StyleResolver;

class CSSAnimations final {
    WTF_MAKE_NONCOPYABLE(CSSAnimations);
    DISALLOW_NEW();
public:
    CSSAnimations();

    bool isAnimationForInspector(const Animation&);
    bool isTransitionAnimationForInspector(const Animation&) const;

    static const StylePropertyShorthand& propertiesForTransitionAll();
    static bool isAnimatableProperty(CSSPropertyID);
    static void calculateUpdate(const Element* animatingElement, Element&, const ComputedStyle&, ComputedStyle* parentStyle, CSSAnimationUpdate&, StyleResolver*);

    void setPendingUpdate(const CSSAnimationUpdate& update)
    {
        clearPendingUpdate();
        m_pendingUpdate.copy(update);
    }
    void clearPendingUpdate()
    {
        m_pendingUpdate.clear();
    }
    void maybeApplyPendingUpdate(Element*);
    bool isEmpty() const { return m_runningAnimations.isEmpty() && m_transitions.isEmpty() && m_pendingUpdate.isEmpty(); }
    void cancel();

    DECLARE_TRACE();

private:
    class RunningAnimation final : public GarbageCollectedFinalized<RunningAnimation> {
    public:
        RunningAnimation(Animation* animation, NewCSSAnimation newAnimation)
            : animation(animation)
            , name(newAnimation.name)
            , nameIndex(newAnimation.nameIndex)
            , specifiedTiming(newAnimation.timing)
            , styleRule(newAnimation.styleRule)
            , styleRuleVersion(newAnimation.styleRuleVersion)
        {
        }

        void update(UpdatedCSSAnimation update)
        {
            ASSERT(update.animation == animation);
            styleRule = update.styleRule;
            styleRuleVersion = update.styleRuleVersion;
            specifiedTiming = update.specifiedTiming;
        }

        DEFINE_INLINE_TRACE()
        {
            visitor->trace(animation);
            visitor->trace(styleRule);
        }

        Member<Animation> animation;
        AtomicString name;
        size_t nameIndex;
        Timing specifiedTiming;
        Member<StyleRuleKeyframes> styleRule;
        unsigned styleRuleVersion;
    };

    struct RunningTransition {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    public:
        DEFINE_INLINE_TRACE()
        {
            visitor->trace(animation);
        }

        Member<Animation> animation;
        const AnimatableValue* from;
        const AnimatableValue* to;
        RefPtr<AnimatableValue> reversingAdjustedStartValue;
        double reversingShorteningFactor;
    };

    HeapVector<Member<RunningAnimation>> m_runningAnimations;

    using TransitionMap = HeapHashMap<CSSPropertyID, RunningTransition>;
    TransitionMap m_transitions;

    CSSAnimationUpdate m_pendingUpdate;

    ActiveInterpolationsMap m_previousActiveInterpolationsForAnimations;

    static void calculateCompositorAnimationUpdate(CSSAnimationUpdate&, const Element* animatingElement, Element&, const ComputedStyle&);
    static void calculateAnimationUpdate(CSSAnimationUpdate&, const Element* animatingElement, Element&, const ComputedStyle&, ComputedStyle* parentStyle, StyleResolver*);
    static void calculateTransitionUpdate(CSSAnimationUpdate&, const Element* animatingElement, const ComputedStyle&);
    static void calculateTransitionUpdateForProperty(CSSPropertyID, const CSSTransitionData&, size_t transitionIndex, const ComputedStyle& oldStyle, const ComputedStyle&, const TransitionMap* activeTransitions, CSSAnimationUpdate&, const Element*);

    static void calculateAnimationActiveInterpolations(CSSAnimationUpdate&, const Element* animatingElement);
    static void calculateTransitionActiveInterpolations(CSSAnimationUpdate&, const Element* animatingElement);

    class AnimationEventDelegate final : public AnimationEffect::EventDelegate {
    public:
        AnimationEventDelegate(Element* animationTarget, const AtomicString& name)
            : m_animationTarget(animationTarget)
            , m_name(name)
            , m_previousPhase(AnimationEffect::PhaseNone)
            , m_previousIteration(nullValue())
        {
        }
        bool requiresIterationEvents(const AnimationEffect&) override;
        void onEventCondition(const AnimationEffect&) override;
        DECLARE_VIRTUAL_TRACE();

    private:
        const Element& animationTarget() const { return *m_animationTarget; }
        EventTarget* eventTarget() const;
        Document& document() const { return m_animationTarget->document(); }

        void maybeDispatch(Document::ListenerType, const AtomicString& eventName, double elapsedTime);
        Member<Element> m_animationTarget;
        const AtomicString m_name;
        AnimationEffect::Phase m_previousPhase;
        double m_previousIteration;
    };

    class TransitionEventDelegate final : public AnimationEffect::EventDelegate {
    public:
        TransitionEventDelegate(Element* transitionTarget, CSSPropertyID property)
            : m_transitionTarget(transitionTarget)
            , m_property(property)
            , m_previousPhase(AnimationEffect::PhaseNone)
        {
        }
        bool requiresIterationEvents(const AnimationEffect&) override { return false; }
        void onEventCondition(const AnimationEffect&) override;
        DECLARE_VIRTUAL_TRACE();

    private:
        const Element& transitionTarget() const { return *m_transitionTarget; }
        EventTarget* eventTarget() const;
        PseudoId getPseudoId() const { return m_transitionTarget->getPseudoId(); }
        Document& document() const { return m_transitionTarget->document(); }

        Member<Element> m_transitionTarget;
        const CSSPropertyID m_property;
        AnimationEffect::Phase m_previousPhase;
    };
};

} // namespace blink

#endif
