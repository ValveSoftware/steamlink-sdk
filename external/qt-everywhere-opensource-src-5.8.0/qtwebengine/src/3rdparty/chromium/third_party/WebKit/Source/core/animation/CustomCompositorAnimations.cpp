// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CustomCompositorAnimations.h"

#include "core/animation/Animation.h"
#include "core/animation/AnimationTimeline.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/animatable/AnimatableDouble.h"
#include "core/animation/animatable/AnimatableTransform.h"
#include "core/animation/animatable/AnimatableValue.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/CompositorMutation.h"
#include "platform/transforms/Matrix3DTransformOperation.h"
#include "platform/transforms/MatrixTransformOperation.h"
#include "platform/transforms/TransformOperations.h"

namespace blink {

namespace {

// Create keyframe effect with zero duration, fill mode forward, and two key
// frames with same value. This corresponding animation is always running and by
// updating the key frames we are able to control the applied value.
static KeyframeEffect* createInfiniteKeyFrameEffect(Element& element, CSSPropertyID propertyId, PassRefPtr<AnimatableValue> value)
{
    AnimatableValueKeyframeVector keyframes(2);
    keyframes[0] = AnimatableValueKeyframe::create();
    keyframes[0]->setOffset(0.0);
    keyframes[0]->setPropertyValue(propertyId, value.get());
    keyframes[1] = AnimatableValueKeyframe::create();
    keyframes[1]->setOffset(1.0);
    keyframes[1]->setPropertyValue(propertyId, value.get());
    keyframes[1]->setComposite(EffectModel::CompositeReplace);

    Timing timing;
    timing.iterationDuration = 0;
    timing.fillMode = Timing::FillModeForwards;

    AnimatableValueKeyframeEffectModel* effectModel = AnimatableValueKeyframeEffectModel::create(keyframes);
    return KeyframeEffect::create(&element, effectModel, timing);
}

static KeyframeEffect* updateInfiniteKeyframeEffect(const KeyframeEffect& keyframeEffect, CSSPropertyID propertyId, PassRefPtr<AnimatableValue> value)
{
    const KeyframeVector& oldFrames = toAnimatableValueKeyframeEffectModel(keyframeEffect.model())->getFrames();
    AnimatableValueKeyframeVector keyframes(2);
    keyframes[0] = toAnimatableValueKeyframe(oldFrames[0]->clone().get());
    keyframes[1] = toAnimatableValueKeyframe(oldFrames[1]->clone().get());
    keyframes[0]->setPropertyValue(propertyId, value.get());
    keyframes[1]->setPropertyValue(propertyId, value.get());

    AnimatableValueKeyframeEffectModel* effectModel = AnimatableValueKeyframeEffectModel::create(keyframes);
    return KeyframeEffect::create(keyframeEffect.target(), effectModel, keyframeEffect.specifiedTiming());
}

static Animation* createOrUpdateAnimation(Animation* animation, Element& element, CSSPropertyID propertyId, PassRefPtr<AnimatableValue> newValue)
{
    if (!animation) {
        KeyframeEffect* keyframeEffect = createInfiniteKeyFrameEffect(element, propertyId, newValue);
        return element.document().timeline().play(keyframeEffect);
    }
    KeyframeEffect* keyframeEffect = updateInfiniteKeyframeEffect(*toKeyframeEffect(animation->effect()), propertyId, newValue);
    animation->setEffect(keyframeEffect);
    return animation;
}

} // namespace

void CustomCompositorAnimations::applyUpdate(Element& element, const CompositorMutation& mutation)
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"), "CustomCompositorAnimations::applyUpdate");

    if (mutation.isOpacityMutated()) {
        RefPtr<AnimatableValue> animatableValue = AnimatableDouble::create(mutation.opacity());
        m_animation = createOrUpdateAnimation(m_animation, element, CSSPropertyOpacity, animatableValue.release());
    }
    if (mutation.isTransformMutated()) {
        TransformOperations ops;
        ops.operations().append(Matrix3DTransformOperation::create(TransformationMatrix(mutation.transform())));
        RefPtr<AnimatableValue> animatableValue = AnimatableTransform::create(ops, 1);
        m_animation = createOrUpdateAnimation(m_animation, element, CSSPropertyTransform, animatableValue.release());
    }
}

} // namespace blink
