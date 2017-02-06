// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSAnimationUpdate_h
#define CSSAnimationUpdate_h

#include "core/animation/AnimationStack.h"
#include "core/animation/InertEffect.h"
#include "core/animation/Interpolation.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/CSSPropertyEquality.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class Animation;

class NewCSSAnimation {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    NewCSSAnimation(AtomicString name, size_t nameIndex, const InertEffect& effect, Timing timing, StyleRuleKeyframes* styleRule)
        : name(name)
        , nameIndex(nameIndex)
        , effect(effect)
        , timing(timing)
        , styleRule(styleRule)
        , styleRuleVersion(this->styleRule->version())
    {
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(effect);
        visitor->trace(styleRule);
    }

    AtomicString name;
    size_t nameIndex;
    Member<const InertEffect> effect;
    Timing timing;
    Member<StyleRuleKeyframes> styleRule;
    unsigned styleRuleVersion;
};

class UpdatedCSSAnimation {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    UpdatedCSSAnimation(size_t index, Animation* animation, const InertEffect& effect, Timing specifiedTiming, StyleRuleKeyframes* styleRule)
        : index(index)
        , animation(animation)
        , effect(&effect)
        , specifiedTiming(specifiedTiming)
        , styleRule(styleRule)
        , styleRuleVersion(this->styleRule->version())
    {
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(animation);
        visitor->trace(effect);
        visitor->trace(styleRule);
    }

    size_t index;
    Member<Animation> animation;
    Member<const InertEffect> effect;
    Timing specifiedTiming;
    Member<StyleRuleKeyframes> styleRule;
    unsigned styleRuleVersion;
};

} // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::NewCSSAnimation);
WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::UpdatedCSSAnimation);

namespace blink {

// This class stores the CSS Animations/Transitions information we use during a style recalc.
// This includes updates to animations/transitions as well as the Interpolations to be applied.
class CSSAnimationUpdate final {
    DISALLOW_NEW();
    WTF_MAKE_NONCOPYABLE(CSSAnimationUpdate);
public:
    CSSAnimationUpdate()
    {
    }

    ~CSSAnimationUpdate()
    {
    }

    void copy(const CSSAnimationUpdate& update)
    {
        ASSERT(isEmpty());
        m_newAnimations = update.newAnimations();
        m_animationsWithUpdates = update.animationsWithUpdates();
        m_newTransitions = update.newTransitions();
        m_activeInterpolationsForAnimations = update.activeInterpolationsForAnimations();
        m_activeInterpolationsForTransitions = update.activeInterpolationsForTransitions();
        m_cancelledAnimationIndices = update.cancelledAnimationIndices();
        m_animationIndicesWithPauseToggled = update.animationIndicesWithPauseToggled();
        m_cancelledTransitions = update.cancelledTransitions();
        m_finishedTransitions = update.finishedTransitions();
        m_updatedCompositorKeyframes = update.updatedCompositorKeyframes();
    }

    void clear()
    {
        m_newAnimations.clear();
        m_animationsWithUpdates.clear();
        m_newTransitions.clear();
        m_activeInterpolationsForAnimations.clear();
        m_activeInterpolationsForTransitions.clear();
        m_cancelledAnimationIndices.clear();
        m_animationIndicesWithPauseToggled.clear();
        m_cancelledTransitions.clear();
        m_finishedTransitions.clear();
        m_updatedCompositorKeyframes.clear();
    }

    void startAnimation(const AtomicString& animationName, size_t nameIndex, const InertEffect& effect, const Timing& timing, StyleRuleKeyframes* styleRule)
    {
        m_newAnimations.append(NewCSSAnimation(animationName, nameIndex, effect, timing, styleRule));
    }
    // Returns whether animation has been suppressed and should be filtered during style application.
    bool isSuppressedAnimation(const Animation* animation) const { return m_suppressedAnimations.contains(animation); }
    void cancelAnimation(size_t index, const Animation& animation)
    {
        m_cancelledAnimationIndices.append(index);
        m_suppressedAnimations.add(&animation);
    }
    void toggleAnimationIndexPaused(size_t index)
    {
        m_animationIndicesWithPauseToggled.append(index);
    }
    void updateAnimation(size_t index, Animation* animation, const InertEffect& effect, const Timing& specifiedTiming,
        StyleRuleKeyframes* styleRule)
    {
        m_animationsWithUpdates.append(UpdatedCSSAnimation(index, animation, effect, specifiedTiming, styleRule));
        m_suppressedAnimations.add(animation);
    }
    void updateCompositorKeyframes(Animation* animation)
    {
        m_updatedCompositorKeyframes.append(animation);
    }

    void startTransition(CSSPropertyID id, const AnimatableValue* from, const AnimatableValue* to, PassRefPtr<AnimatableValue> reversingAdjustedStartValue, double reversingShorteningFactor, const InertEffect& effect)
    {
        NewTransition newTransition;
        newTransition.id = id;
        newTransition.from = from;
        newTransition.to = to;
        newTransition.reversingAdjustedStartValue = reversingAdjustedStartValue;
        newTransition.reversingShorteningFactor = reversingShorteningFactor;
        newTransition.effect = &effect;
        m_newTransitions.set(id, newTransition);
    }
    bool isCancelledTransition(CSSPropertyID id) const { return m_cancelledTransitions.contains(id); }
    void cancelTransition(CSSPropertyID id) { m_cancelledTransitions.add(id); }
    void finishTransition(CSSPropertyID id) { m_finishedTransitions.add(id); }

    const HeapVector<NewCSSAnimation>& newAnimations() const { return m_newAnimations; }
    const Vector<size_t>& cancelledAnimationIndices() const { return m_cancelledAnimationIndices; }
    const HeapHashSet<Member<const Animation>>& suppressedAnimations() const { return m_suppressedAnimations; }
    const Vector<size_t>& animationIndicesWithPauseToggled() const { return m_animationIndicesWithPauseToggled; }
    const HeapVector<UpdatedCSSAnimation>& animationsWithUpdates() const { return m_animationsWithUpdates; }
    const HeapVector<Member<Animation>>& updatedCompositorKeyframes() const { return m_updatedCompositorKeyframes; }

    struct NewTransition {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    public:
        DEFINE_INLINE_TRACE()
        {
            visitor->trace(effect);
        }

        CSSPropertyID id;
        const AnimatableValue* from;
        const AnimatableValue* to;
        RefPtr<AnimatableValue> reversingAdjustedStartValue;
        double reversingShorteningFactor;
        Member<const InertEffect> effect;
    };
    using NewTransitionMap = HeapHashMap<CSSPropertyID, NewTransition>;
    const NewTransitionMap& newTransitions() const { return m_newTransitions; }
    const HashSet<CSSPropertyID>& cancelledTransitions() const { return m_cancelledTransitions; }
    const HashSet<CSSPropertyID>& finishedTransitions() const { return m_finishedTransitions; }

    void adoptActiveInterpolationsForAnimations(ActiveInterpolationsMap& newMap) { newMap.swap(m_activeInterpolationsForAnimations); }
    void adoptActiveInterpolationsForTransitions(ActiveInterpolationsMap& newMap) { newMap.swap(m_activeInterpolationsForTransitions); }
    const ActiveInterpolationsMap& activeInterpolationsForAnimations() const { return m_activeInterpolationsForAnimations; }
    const ActiveInterpolationsMap& activeInterpolationsForTransitions() const { return m_activeInterpolationsForTransitions; }
    ActiveInterpolationsMap& activeInterpolationsForAnimations() { return m_activeInterpolationsForAnimations; }

    bool isEmpty() const
    {
        return m_newAnimations.isEmpty()
            && m_cancelledAnimationIndices.isEmpty()
            && m_suppressedAnimations.isEmpty()
            && m_animationIndicesWithPauseToggled.isEmpty()
            && m_animationsWithUpdates.isEmpty()
            && m_newTransitions.isEmpty()
            && m_cancelledTransitions.isEmpty()
            && m_finishedTransitions.isEmpty()
            && m_activeInterpolationsForAnimations.isEmpty()
            && m_activeInterpolationsForTransitions.isEmpty()
            && m_updatedCompositorKeyframes.isEmpty();
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_newTransitions);
        visitor->trace(m_newAnimations);
        visitor->trace(m_suppressedAnimations);
        visitor->trace(m_animationsWithUpdates);
        visitor->trace(m_updatedCompositorKeyframes);
    }

private:
    // Order is significant since it defines the order in which new animations
    // will be started. Note that there may be multiple animations present
    // with the same name, due to the way in which we split up animations with
    // incomplete keyframes.
    HeapVector<NewCSSAnimation> m_newAnimations;
    Vector<size_t> m_cancelledAnimationIndices;
    HeapHashSet<Member<const Animation>> m_suppressedAnimations;
    Vector<size_t> m_animationIndicesWithPauseToggled;
    HeapVector<UpdatedCSSAnimation> m_animationsWithUpdates;
    HeapVector<Member<Animation>> m_updatedCompositorKeyframes;

    NewTransitionMap m_newTransitions;
    HashSet<CSSPropertyID> m_cancelledTransitions;
    HashSet<CSSPropertyID> m_finishedTransitions;

    ActiveInterpolationsMap m_activeInterpolationsForAnimations;
    ActiveInterpolationsMap m_activeInterpolationsForTransitions;

    friend class PendingAnimationUpdate;
};

} // namespace blink

#endif
