// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorAnimationAgent.h"

#include "core/animation/Animation.h"
#include "core/animation/AnimationEffect.h"
#include "core/animation/AnimationEffectTiming.h"
#include "core/animation/ComputedTimingProperties.h"
#include "core/animation/EffectModel.h"
#include "core/animation/ElementAnimation.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/StringKeyframe.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorStyleSheet.h"
#include "platform/Decimal.h"
#include "platform/animation/TimingFunction.h"
#include "platform/v8_inspector/public/V8InspectorSession.h"
#include "wtf/text/Base64.h"
#include <memory>

namespace AnimationAgentState {
static const char animationAgentEnabled[] = "animationAgentEnabled";
static const char animationAgentPlaybackRate[] = "animationAgentPlaybackRate";
}

namespace blink {

InspectorAnimationAgent::InspectorAnimationAgent(InspectedFrames* inspectedFrames, InspectorDOMAgent* domAgent, InspectorCSSAgent* cssAgent, V8InspectorSession* v8Session)
    : m_inspectedFrames(inspectedFrames)
    , m_domAgent(domAgent)
    , m_cssAgent(cssAgent)
    , m_v8Session(v8Session)
    , m_isCloning(false)
{
}

void InspectorAnimationAgent::restore()
{
    if (m_state->booleanProperty(AnimationAgentState::animationAgentEnabled, false)) {
        ErrorString error;
        enable(&error);
        double playbackRate = 1;
        m_state->getNumber(AnimationAgentState::animationAgentPlaybackRate, &playbackRate);
        setPlaybackRate(nullptr, playbackRate);
    }
}

void InspectorAnimationAgent::enable(ErrorString*)
{
    m_state->setBoolean(AnimationAgentState::animationAgentEnabled, true);
    m_instrumentingAgents->addInspectorAnimationAgent(this);
}

void InspectorAnimationAgent::disable(ErrorString*)
{
    setPlaybackRate(nullptr, 1);
    for (const auto& clone : m_idToAnimationClone.values())
        clone->cancel();
    m_state->setBoolean(AnimationAgentState::animationAgentEnabled, false);
    m_instrumentingAgents->removeInspectorAnimationAgent(this);
    m_idToAnimation.clear();
    m_idToAnimationType.clear();
    m_idToAnimationClone.clear();
    m_clearedAnimations.clear();
}

void InspectorAnimationAgent::didCommitLoadForLocalFrame(LocalFrame* frame)
{
    if (frame == m_inspectedFrames->root()) {
        m_idToAnimation.clear();
        m_idToAnimationType.clear();
        m_idToAnimationClone.clear();
        m_clearedAnimations.clear();
    }
    double playbackRate = 1;
    m_state->getNumber(AnimationAgentState::animationAgentPlaybackRate, &playbackRate);
    setPlaybackRate(nullptr, playbackRate);
}

static std::unique_ptr<protocol::Animation::AnimationEffect> buildObjectForAnimationEffect(KeyframeEffect* effect, bool isTransition)
{
    ComputedTimingProperties computedTiming = effect->getComputedTiming();
    double delay = computedTiming.delay();
    double duration = computedTiming.duration().getAsUnrestrictedDouble();
    String easing = effect->specifiedTiming().timingFunction->toString();

    if (isTransition) {
        // Obtain keyframes and convert keyframes back to delay
        ASSERT(effect->model()->isKeyframeEffectModel());
        const KeyframeEffectModelBase* model = toKeyframeEffectModelBase(effect->model());
        Vector<RefPtr<Keyframe>> keyframes = KeyframeEffectModelBase::normalizedKeyframesForInspector(model->getFrames());
        if (keyframes.size() == 3) {
            delay = keyframes.at(1)->offset() * duration;
            duration -= delay;
            easing = keyframes.at(1)->easing().toString();
        } else {
            easing = keyframes.at(0)->easing().toString();
        }
    }

    std::unique_ptr<protocol::Animation::AnimationEffect> animationObject = protocol::Animation::AnimationEffect::create()
        .setDelay(delay)
        .setEndDelay(computedTiming.endDelay())
        .setPlaybackRate(computedTiming.playbackRate())
        .setIterationStart(computedTiming.iterationStart())
        .setIterations(computedTiming.iterations())
        .setDuration(duration)
        .setDirection(computedTiming.direction())
        .setFill(computedTiming.fill())
        .setBackendNodeId(DOMNodeIds::idForNode(effect->target()))
        .setEasing(easing).build();
    return animationObject;
}

static std::unique_ptr<protocol::Animation::KeyframeStyle> buildObjectForStringKeyframe(const StringKeyframe* keyframe)
{
    Decimal decimal = Decimal::fromDouble(keyframe->offset() * 100);
    String offset = decimal.toString();
    offset.append("%");

    std::unique_ptr<protocol::Animation::KeyframeStyle> keyframeObject = protocol::Animation::KeyframeStyle::create()
        .setOffset(offset)
        .setEasing(keyframe->easing().toString()).build();
    return keyframeObject;
}

static std::unique_ptr<protocol::Animation::KeyframesRule> buildObjectForAnimationKeyframes(const KeyframeEffect* effect)
{
    if (!effect || !effect->model() || !effect->model()->isKeyframeEffectModel())
        return nullptr;
    const KeyframeEffectModelBase* model = toKeyframeEffectModelBase(effect->model());
    Vector<RefPtr<Keyframe>> normalizedKeyframes = KeyframeEffectModelBase::normalizedKeyframesForInspector(model->getFrames());
    std::unique_ptr<protocol::Array<protocol::Animation::KeyframeStyle>> keyframes = protocol::Array<protocol::Animation::KeyframeStyle>::create();

    for (const auto& keyframe : normalizedKeyframes) {
        // Ignore CSS Transitions
        if (!keyframe.get()->isStringKeyframe())
            continue;
        const StringKeyframe* stringKeyframe = toStringKeyframe(keyframe.get());
        keyframes->addItem(buildObjectForStringKeyframe(stringKeyframe));
    }
    return protocol::Animation::KeyframesRule::create().setKeyframes(std::move(keyframes)).build();
}

std::unique_ptr<protocol::Animation::Animation> InspectorAnimationAgent::buildObjectForAnimation(blink::Animation& animation)
{
    const Element* element = toKeyframeEffect(animation.effect())->target();
    CSSAnimations& cssAnimations = element->elementAnimations()->cssAnimations();
    std::unique_ptr<protocol::Animation::KeyframesRule> keyframeRule = nullptr;
    String animationType;

    if (cssAnimations.isTransitionAnimationForInspector(animation)) {
        // CSS Transitions
        animationType = AnimationType::CSSTransition;
    } else {
        // Keyframe based animations
        keyframeRule = buildObjectForAnimationKeyframes(toKeyframeEffect(animation.effect()));
        animationType = cssAnimations.isAnimationForInspector(animation) ? AnimationType::CSSAnimation : AnimationType::WebAnimation;
    }

    String id = String::number(animation.sequenceNumber());
    m_idToAnimation.set(id, &animation);
    m_idToAnimationType.set(id, animationType);

    std::unique_ptr<protocol::Animation::AnimationEffect> animationEffectObject = buildObjectForAnimationEffect(toKeyframeEffect(animation.effect()), animationType == AnimationType::CSSTransition);
    animationEffectObject->setKeyframesRule(std::move(keyframeRule));

    std::unique_ptr<protocol::Animation::Animation> animationObject = protocol::Animation::Animation::create()
        .setId(id)
        .setName(animation.id())
        .setPausedState(animation.paused())
        .setPlayState(animation.playState())
        .setPlaybackRate(animation.playbackRate())
        .setStartTime(normalizedStartTime(animation))
        .setCurrentTime(animation.currentTime())
        .setSource(std::move(animationEffectObject))
        .setType(animationType).build();
    if (animationType != AnimationType::WebAnimation)
        animationObject->setCssId(createCSSId(animation));
    return animationObject;
}

void InspectorAnimationAgent::getPlaybackRate(ErrorString*, double* playbackRate)
{
    *playbackRate = referenceTimeline().playbackRate();
}

void InspectorAnimationAgent::setPlaybackRate(ErrorString*, double playbackRate)
{
    for (LocalFrame* frame : *m_inspectedFrames)
        frame->document()->timeline().setPlaybackRate(playbackRate);
    m_state->setNumber(AnimationAgentState::animationAgentPlaybackRate, playbackRate);
}

void InspectorAnimationAgent::getCurrentTime(ErrorString* errorString, const String& id, double* currentTime)
{
    blink::Animation* animation = assertAnimation(errorString, id);
    if (!animation)
        return;
    if (m_idToAnimationClone.get(id))
        animation = m_idToAnimationClone.get(id);

    if (animation->paused()) {
        *currentTime = animation->currentTime();
    } else {
        // Use startTime where possible since currentTime is limited.
        *currentTime = animation->timeline()->currentTime() - animation->startTime();
    }
}

void InspectorAnimationAgent::setPaused(ErrorString* errorString, std::unique_ptr<protocol::Array<String>> animationIds, bool paused)
{
    for (size_t i = 0; i < animationIds->length(); ++i) {
        String animationId = animationIds->get(i);
        blink::Animation* animation = assertAnimation(errorString, animationId);
        if (!animation)
            return;
        blink::Animation* clone = animationClone(animation);
        if (!clone) {
            *errorString = "Failed to clone detached animation";
            return;
        }
        if (paused && !clone->paused()) {
            // Ensure we restore a current time if the animation is limited.
            double currentTime = clone->timeline()->currentTime() - clone->startTime();
            clone->pause();
            clone->setCurrentTime(currentTime);
        } else if (!paused && clone->paused()) {
            clone->unpause();
        }
    }
}

blink::Animation* InspectorAnimationAgent::animationClone(blink::Animation* animation)
{
    const String id = String::number(animation->sequenceNumber());
    if (!m_idToAnimationClone.get(id)) {
        KeyframeEffect* oldEffect = toKeyframeEffect(animation->effect());
        ASSERT(oldEffect->model()->isKeyframeEffectModel());
        KeyframeEffectModelBase* oldModel = toKeyframeEffectModelBase(oldEffect->model());
        EffectModel* newModel = nullptr;
        // Clone EffectModel.
        // TODO(samli): Determine if this is an animations bug.
        if (oldModel->isStringKeyframeEffectModel()) {
            StringKeyframeEffectModel* oldStringKeyframeModel = toStringKeyframeEffectModel(oldModel);
            KeyframeVector oldKeyframes = oldStringKeyframeModel->getFrames();
            StringKeyframeVector newKeyframes;
            for (auto& oldKeyframe : oldKeyframes)
                newKeyframes.append(toStringKeyframe(oldKeyframe.get()));
            StringKeyframeEffectModel* newStringKeyframeModel = StringKeyframeEffectModel::create(newKeyframes);
            // TODO(samli): This shouldn't be required.
            Element* element = oldEffect->target();
            if (!element)
                return nullptr;
            newStringKeyframeModel->forceConversionsToAnimatableValues(*element, element->computedStyle());
            newModel = newStringKeyframeModel;
        } else if (oldModel->isAnimatableValueKeyframeEffectModel()) {
            AnimatableValueKeyframeEffectModel* oldAnimatableValueKeyframeModel = toAnimatableValueKeyframeEffectModel(oldModel);
            KeyframeVector oldKeyframes = oldAnimatableValueKeyframeModel->getFrames();
            AnimatableValueKeyframeVector newKeyframes;
            for (auto& oldKeyframe : oldKeyframes)
                newKeyframes.append(toAnimatableValueKeyframe(oldKeyframe.get()));
            newModel = AnimatableValueKeyframeEffectModel::create(newKeyframes);
        }

        KeyframeEffect* newEffect = KeyframeEffect::create(oldEffect->target(), newModel, oldEffect->specifiedTiming());
        m_isCloning = true;
        blink::Animation* clone = blink::Animation::create(newEffect, animation->timeline());
        m_isCloning = false;
        m_idToAnimationClone.set(id, clone);
        m_idToAnimation.set(String::number(clone->sequenceNumber()), clone);
        clone->play();
        clone->setStartTime(animation->startTime());

        animation->setEffectSuppressed(true);
    }
    return m_idToAnimationClone.get(id);
}

void InspectorAnimationAgent::seekAnimations(ErrorString* errorString, std::unique_ptr<protocol::Array<String>> animationIds, double currentTime)
{
    for (size_t i = 0; i < animationIds->length(); ++i) {
        String animationId = animationIds->get(i);
        blink::Animation* animation = assertAnimation(errorString, animationId);
        if (!animation)
            return;
        blink::Animation* clone = animationClone(animation);
        if (!clone) {
            *errorString = "Failed to clone a detached animation.";
            return;
        }
        if (!clone->paused())
            clone->play();
        clone->setCurrentTime(currentTime);
    }
}

void InspectorAnimationAgent::releaseAnimations(ErrorString* errorString, std::unique_ptr<protocol::Array<String>> animationIds)
{
    for (size_t i = 0; i < animationIds->length(); ++i) {
        String animationId = animationIds->get(i);
        blink::Animation* animation = m_idToAnimation.get(animationId);
        if (animation)
            animation->setEffectSuppressed(false);
        blink::Animation* clone = m_idToAnimationClone.get(animationId);
        if (clone)
            clone->cancel();
        m_idToAnimationClone.remove(animationId);
        m_idToAnimation.remove(animationId);
        m_idToAnimationType.remove(animationId);
        m_clearedAnimations.add(animationId);
    }
}


void InspectorAnimationAgent::setTiming(ErrorString* errorString, const String& animationId, double duration, double delay)
{
    blink::Animation* animation = assertAnimation(errorString, animationId);
    if (!animation)
        return;

    animation = animationClone(animation);
    NonThrowableExceptionState exceptionState;

    String type = m_idToAnimationType.get(animationId);
    if (type == AnimationType::CSSTransition) {
        KeyframeEffect* effect = toKeyframeEffect(animation->effect());
        KeyframeEffectModelBase* model = toKeyframeEffectModelBase(effect->model());
        const AnimatableValueKeyframeEffectModel* oldModel = toAnimatableValueKeyframeEffectModel(model);
        // Refer to CSSAnimations::calculateTransitionUpdateForProperty() for the structure of transitions.
        const KeyframeVector& frames = oldModel->getFrames();
        ASSERT(frames.size() == 3);
        KeyframeVector newFrames;
        for (int i = 0; i < 3; i++)
            newFrames.append(toAnimatableValueKeyframe(frames[i]->clone().get()));
        // Update delay, represented by the distance between the first two keyframes.
        newFrames[1]->setOffset(delay / (delay + duration));
        model->setFrames(newFrames);

        AnimationEffectTiming* timing = animation->effect()->timing();
        UnrestrictedDoubleOrString unrestrictedDuration;
        unrestrictedDuration.setUnrestrictedDouble(duration + delay);
        timing->setDuration(unrestrictedDuration, exceptionState);
    } else {
        AnimationEffectTiming* timing = animation->effect()->timing();
        UnrestrictedDoubleOrString unrestrictedDuration;
        unrestrictedDuration.setUnrestrictedDouble(duration);
        timing->setDuration(unrestrictedDuration, exceptionState);
        timing->setDelay(delay);
    }
}

void InspectorAnimationAgent::resolveAnimation(ErrorString* errorString, const String& animationId, std::unique_ptr<protocol::Runtime::RemoteObject>* result)
{
    blink::Animation* animation = assertAnimation(errorString, animationId);
    if (!animation)
        return;
    if (m_idToAnimationClone.get(animationId))
        animation = m_idToAnimationClone.get(animationId);
    const Element* element = toKeyframeEffect(animation->effect())->target();
    Document* document = element->ownerDocument();
    LocalFrame* frame = document ? document->frame() : nullptr;
    ScriptState* scriptState = frame ? ScriptState::forMainWorld(frame) : nullptr;
    if (!scriptState) {
        *errorString = "Element not associated with a document.";
        return;
    }

    ScriptState::Scope scope(scriptState);
    m_v8Session->releaseObjectGroup("animation");
    *result = m_v8Session->wrapObject(scriptState->context(), toV8(animation, scriptState->context()->Global(), scriptState->isolate()), "animation");
    if (!*result)
        *errorString = "Element not associated with a document.";
}

static CSSPropertyID animationProperties[] = {
    CSSPropertyAnimationDelay,
    CSSPropertyAnimationDirection,
    CSSPropertyAnimationDuration,
    CSSPropertyAnimationFillMode,
    CSSPropertyAnimationIterationCount,
    CSSPropertyAnimationName,
    CSSPropertyAnimationTimingFunction
};

static CSSPropertyID transitionProperties[] = {
    CSSPropertyTransitionDelay,
    CSSPropertyTransitionDuration,
    CSSPropertyTransitionProperty,
    CSSPropertyTransitionTimingFunction,
};

static void addStringToDigestor(WebCryptoDigestor* digestor, const String& string)
{
    digestor->consume(reinterpret_cast<const unsigned char*>(string.ascii().data()), string.length());
}

String InspectorAnimationAgent::createCSSId(blink::Animation& animation)
{
    String type = m_idToAnimationType.get(String::number(animation.sequenceNumber()));
    ASSERT(type != AnimationType::WebAnimation);

    KeyframeEffect* effect = toKeyframeEffect(animation.effect());
    Vector<CSSPropertyID> cssProperties;
    if (type == AnimationType::CSSAnimation) {
        for (CSSPropertyID property : animationProperties)
            cssProperties.append(property);
    } else {
        for (CSSPropertyID property : transitionProperties)
            cssProperties.append(property);
        cssProperties.append(cssPropertyID(animation.id()));
    }

    Element* element = effect->target();
    HeapVector<Member<CSSStyleDeclaration>> styles = m_cssAgent->matchingStyles(element);
    std::unique_ptr<WebCryptoDigestor> digestor = createDigestor(HashAlgorithmSha1);
    addStringToDigestor(digestor.get(), type);
    addStringToDigestor(digestor.get(), animation.id());
    for (CSSPropertyID property : cssProperties) {
        CSSStyleDeclaration* style = m_cssAgent->findEffectiveDeclaration(property, styles);
        // Ignore inline styles.
        if (!style || !style->parentStyleSheet() || !style->parentRule() || style->parentRule()->type() != CSSRule::STYLE_RULE)
            continue;
        addStringToDigestor(digestor.get(), getPropertyNameString(property));
        addStringToDigestor(digestor.get(), m_cssAgent->styleSheetId(style->parentStyleSheet()));
        addStringToDigestor(digestor.get(), toCSSStyleRule(style->parentRule())->selectorText());
    }
    DigestValue digestResult;
    finishDigestor(digestor.get(), digestResult);
    return base64Encode(reinterpret_cast<const char*>(digestResult.data()), 10);
}

void InspectorAnimationAgent::didCreateAnimation(unsigned sequenceNumber)
{
    if (m_isCloning)
        return;
    frontend()->animationCreated(String::number(sequenceNumber));
}

void InspectorAnimationAgent::animationPlayStateChanged(blink::Animation* animation, blink::Animation::AnimationPlayState oldPlayState, blink::Animation::AnimationPlayState newPlayState)
{
    const String& animationId = String::number(animation->sequenceNumber());
    if (m_idToAnimation.get(animationId) || m_clearedAnimations.contains(animationId))
        return;
    if (newPlayState == blink::Animation::Running || newPlayState == blink::Animation::Finished)
        frontend()->animationStarted(buildObjectForAnimation(*animation));
    else if (newPlayState == blink::Animation::Idle || newPlayState == blink::Animation::Paused)
        frontend()->animationCanceled(animationId);
}

void InspectorAnimationAgent::didClearDocumentOfWindowObject(LocalFrame* frame)
{
    if (!m_state->booleanProperty(AnimationAgentState::animationAgentEnabled, false))
        return;
    ASSERT(frame->document());
    frame->document()->timeline().setPlaybackRate(referenceTimeline().playbackRate());
}

blink::Animation* InspectorAnimationAgent::assertAnimation(ErrorString* errorString, const String& id)
{
    blink::Animation* animation = m_idToAnimation.get(id);
    if (!animation) {
        *errorString = "Could not find animation with given id";
        return nullptr;
    }
    return animation;
}

AnimationTimeline& InspectorAnimationAgent::referenceTimeline()
{
    return m_inspectedFrames->root()->document()->timeline();
}

double InspectorAnimationAgent::normalizedStartTime(blink::Animation& animation)
{
    if (referenceTimeline().playbackRate() == 0)
        return animation.startTime() + referenceTimeline().currentTime() - animation.timeline()->currentTime();
    return animation.startTime() + (animation.timeline()->zeroTime() - referenceTimeline().zeroTime()) * 1000 * referenceTimeline().playbackRate();
}

DEFINE_TRACE(InspectorAnimationAgent)
{
    visitor->trace(m_inspectedFrames);
    visitor->trace(m_domAgent);
    visitor->trace(m_cssAgent);
    visitor->trace(m_idToAnimation);
    visitor->trace(m_idToAnimationClone);
    InspectorBaseAgent::trace(visitor);
}

} // namespace blink
