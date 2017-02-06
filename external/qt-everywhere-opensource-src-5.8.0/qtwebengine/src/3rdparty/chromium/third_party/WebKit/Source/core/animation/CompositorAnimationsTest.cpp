/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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


#include "core/animation/CompositorAnimations.h"

#include "core/animation/Animation.h"
#include "core/animation/AnimationTimeline.h"
#include "core/animation/CompositorPendingAnimations.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/animatable/AnimatableDouble.h"
#include "core/animation/animatable/AnimatableFilterOperations.h"
#include "core/animation/animatable/AnimatableTransform.h"
#include "core/animation/animatable/AnimatableValueTestHelper.h"
#include "core/dom/Document.h"
#include "core/layout/LayoutObject.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/animation/CompositorAnimation.h"
#include "platform/animation/CompositorFloatAnimationCurve.h"
#include "platform/animation/CompositorFloatKeyframe.h"
#include "platform/geometry/FloatBox.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/filters/FilterOperations.h"
#include "platform/transforms/TransformOperations.h"
#include "platform/transforms/TranslateTransformOperation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/HashFunctions.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

class AnimationCompositorAnimationsTest : public ::testing::Test {
protected:
    RefPtr<TimingFunction> m_linearTimingFunction;
    RefPtr<TimingFunction> m_cubicEaseTimingFunction;
    RefPtr<TimingFunction> m_cubicCustomTimingFunction;
    RefPtr<TimingFunction> m_stepTimingFunction;

    Timing m_timing;
    CompositorAnimations::CompositorTiming m_compositorTiming;
    std::unique_ptr<AnimatableValueKeyframeVector> m_keyframeVector2;
    Persistent<AnimatableValueKeyframeEffectModel> m_keyframeAnimationEffect2;
    std::unique_ptr<AnimatableValueKeyframeVector> m_keyframeVector5;
    Persistent<AnimatableValueKeyframeEffectModel> m_keyframeAnimationEffect5;

    Persistent<Document> m_document;
    Persistent<Element> m_element;
    Persistent<AnimationTimeline> m_timeline;
    std::unique_ptr<DummyPageHolder> m_pageHolder;

    void SetUp() override
    {
        m_linearTimingFunction = LinearTimingFunction::shared();
        m_cubicEaseTimingFunction = CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseType::EASE);
        m_cubicCustomTimingFunction = CubicBezierTimingFunction::create(1, 2, 3, 4);
        m_stepTimingFunction = StepsTimingFunction::create(1, StepsTimingFunction::StepPosition::END);

        m_timing = createCompositableTiming();
        m_compositorTiming = CompositorAnimations::CompositorTiming();
        // Make sure the CompositableTiming is really compositable, otherwise
        // most other tests will fail.
        ASSERT(convertTimingForCompositor(m_timing, m_compositorTiming));

        m_keyframeVector2 = createCompositableFloatKeyframeVector(2);
        m_keyframeAnimationEffect2 = AnimatableValueKeyframeEffectModel::create(*m_keyframeVector2);

        m_keyframeVector5 = createCompositableFloatKeyframeVector(5);
        m_keyframeAnimationEffect5 = AnimatableValueKeyframeEffectModel::create(*m_keyframeVector5);

        m_pageHolder = DummyPageHolder::create();
        m_document = &m_pageHolder->document();
        m_document->animationClock().resetTimeForTesting();

        m_timeline = AnimationTimeline::create(m_document.get());
        m_timeline->resetForTesting();
        m_element = m_document->createElement("test", ASSERT_NO_EXCEPTION);
    }

public:
    bool convertTimingForCompositor(const Timing& t, CompositorAnimations::CompositorTiming& out)
    {
        return CompositorAnimations::convertTimingForCompositor(t, 0, out, 1);
    }
    bool isCandidateForAnimationOnCompositor(const Timing& timing, const EffectModel& effect)
    {
        return CompositorAnimations::isCandidateForAnimationOnCompositor(timing, *m_element.get(), nullptr, effect, 1);
    }
    void getAnimationOnCompositor(Timing& timing, AnimatableValueKeyframeEffectModel& effect, Vector<std::unique_ptr<CompositorAnimation>>& animations)
    {
        getAnimationOnCompositor(timing, effect, animations, 1);
    }
    void getAnimationOnCompositor(Timing& timing, AnimatableValueKeyframeEffectModel& effect, Vector<std::unique_ptr<CompositorAnimation>>& animations, double playerPlaybackRate)
    {
        CompositorAnimations::getAnimationOnCompositor(timing, 0, std::numeric_limits<double>::quiet_NaN(), 0, effect, animations, playerPlaybackRate);
    }
    bool getAnimationBounds(FloatBox& boundingBox, const EffectModel& effect, double minValue, double maxValue)
    {
        return CompositorAnimations::getAnimatedBoundingBox(boundingBox, effect, minValue, maxValue);
    }

    bool duplicateSingleKeyframeAndTestIsCandidateOnResult(AnimatableValueKeyframe* frame)
    {
        EXPECT_EQ(frame->offset(), 0);
        AnimatableValueKeyframeVector frames;
        RefPtr<Keyframe> second = frame->cloneWithOffset(1);

        frames.append(frame);
        frames.append(toAnimatableValueKeyframe(second.get()));
        return isCandidateForAnimationOnCompositor(m_timing, *AnimatableValueKeyframeEffectModel::create(frames));
    }

    // -------------------------------------------------------------------

    Timing createCompositableTiming()
    {
        Timing timing;
        timing.startDelay = 0;
        timing.fillMode = Timing::FillModeNone;
        timing.iterationStart = 0;
        timing.iterationCount = 1;
        timing.iterationDuration = 1.0;
        timing.playbackRate = 1.0;
        timing.direction = Timing::PlaybackDirectionNormal;
        ASSERT(m_linearTimingFunction);
        timing.timingFunction = m_linearTimingFunction;
        return timing;
    }

    PassRefPtr<AnimatableValueKeyframe> createReplaceOpKeyframe(CSSPropertyID id, AnimatableValue* value, double offset = 0)
    {
        RefPtr<AnimatableValueKeyframe> keyframe = AnimatableValueKeyframe::create();
        keyframe->setPropertyValue(id, value);
        keyframe->setComposite(EffectModel::CompositeReplace);
        keyframe->setOffset(offset);
        keyframe->setEasing(LinearTimingFunction::shared());
        return keyframe;
    }

    PassRefPtr<AnimatableValueKeyframe> createDefaultKeyframe(CSSPropertyID id, EffectModel::CompositeOperation op, double offset = 0)
    {
        RefPtr<AnimatableValue> value = nullptr;
        if (id == CSSPropertyTransform)
            value = AnimatableTransform::create(TransformOperations(), 1);
        else
            value = AnimatableDouble::create(10.0);

        RefPtr<AnimatableValueKeyframe> keyframe = createReplaceOpKeyframe(id, value.get(), offset);
        keyframe->setComposite(op);
        return keyframe;
    }

    std::unique_ptr<AnimatableValueKeyframeVector> createCompositableFloatKeyframeVector(size_t n)
    {
        Vector<double> values;
        for (size_t i = 0; i < n; i++) {
            values.append(static_cast<double>(i));
        }
        return createCompositableFloatKeyframeVector(values);
    }

    std::unique_ptr<AnimatableValueKeyframeVector> createCompositableFloatKeyframeVector(Vector<double>& values)
    {
        std::unique_ptr<AnimatableValueKeyframeVector> frames = wrapUnique(new AnimatableValueKeyframeVector);
        for (size_t i = 0; i < values.size(); i++) {
            double offset = 1.0 / (values.size() - 1) * i;
            RefPtr<AnimatableDouble> value = AnimatableDouble::create(values[i]);
            frames->append(createReplaceOpKeyframe(CSSPropertyOpacity, value.get(), offset).get());
        }
        return frames;
    }

    std::unique_ptr<AnimatableValueKeyframeVector> createCompositableTransformKeyframeVector(const Vector<TransformOperations>& values)
    {
        std::unique_ptr<AnimatableValueKeyframeVector> frames = wrapUnique(new AnimatableValueKeyframeVector);
        for (size_t i = 0; i < values.size(); ++i) {
            double offset = 1.0f / (values.size() - 1) * i;
            RefPtr<AnimatableTransform> value = AnimatableTransform::create(values[i], 1);
            frames->append(createReplaceOpKeyframe(CSSPropertyTransform, value.get(), offset).get());
        }
        return frames;
    }

    AnimatableValueKeyframeEffectModel* createKeyframeEffectModel(PassRefPtr<AnimatableValueKeyframe> prpFrom, PassRefPtr<AnimatableValueKeyframe> prpTo, PassRefPtr<AnimatableValueKeyframe> prpC = nullptr, PassRefPtr<AnimatableValueKeyframe> prpD = nullptr)
    {
        RefPtr<AnimatableValueKeyframe> from = prpFrom;
        RefPtr<AnimatableValueKeyframe> to = prpTo;
        RefPtr<AnimatableValueKeyframe> c = prpC;
        RefPtr<AnimatableValueKeyframe> d = prpD;

        EXPECT_EQ(from->offset(), 0);
        AnimatableValueKeyframeVector frames;
        frames.append(from);
        EXPECT_LE(from->offset(), to->offset());
        frames.append(to);
        if (c) {
            EXPECT_LE(to->offset(), c->offset());
            frames.append(c);
        }
        if (d) {
            frames.append(d);
            EXPECT_LE(c->offset(), d->offset());
            EXPECT_EQ(d->offset(), 1.0);
        } else {
            EXPECT_EQ(to->offset(), 1.0);
        }
        if (!HasFatalFailure()) {
            return AnimatableValueKeyframeEffectModel::create(frames);
        }
        return nullptr;
    }

    void simulateFrame(double time)
    {
        m_document->animationClock().updateTime(time);
        m_document->compositorPendingAnimations().update(false);
        m_timeline->serviceAnimations(TimingUpdateForAnimationFrame);
    }

    std::unique_ptr<CompositorAnimation> convertToCompositorAnimation(AnimatableValueKeyframeEffectModel& effect, double playerPlaybackRate)
    {
        Vector<std::unique_ptr<CompositorAnimation>> result;
        getAnimationOnCompositor(m_timing, effect, result, playerPlaybackRate);
        DCHECK_EQ(1U, result.size());
        return std::move(result[0]);
    }

    std::unique_ptr<CompositorAnimation> convertToCompositorAnimation(AnimatableValueKeyframeEffectModel& effect)
    {
        return convertToCompositorAnimation(effect, 1.0);
    }
};

class LayoutObjectProxy : public LayoutObject {
public:
    static LayoutObjectProxy* create(Node* node)
    {
        return new LayoutObjectProxy(node);
    }

    static void dispose(LayoutObjectProxy* proxy)
    {
        proxy->destroy();
    }

    const char* name() const override { return nullptr; }
    void layout() override { }

private:
    explicit LayoutObjectProxy(Node* node)
        : LayoutObject(node)
    {
    }
};

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorKeyframeMultipleCSSProperties)
{
    RefPtr<AnimatableValueKeyframe> keyframeGoodMultiple = createDefaultKeyframe(CSSPropertyOpacity, EffectModel::CompositeReplace);
    keyframeGoodMultiple->setPropertyValue(CSSPropertyTransform, AnimatableTransform::create(TransformOperations(), 1).get());
    EXPECT_TRUE(duplicateSingleKeyframeAndTestIsCandidateOnResult(keyframeGoodMultiple.get()));

    RefPtr<AnimatableValueKeyframe> keyframeBadMultipleID = createDefaultKeyframe(CSSPropertyColor, EffectModel::CompositeReplace);
    keyframeBadMultipleID->setPropertyValue(CSSPropertyOpacity, AnimatableDouble::create(10.0).get());
    EXPECT_FALSE(duplicateSingleKeyframeAndTestIsCandidateOnResult(keyframeBadMultipleID.get()));
}

TEST_F(AnimationCompositorAnimationsTest, isNotCandidateForCompositorAnimationTransformDependsOnBoxSize)
{
    TransformOperations ops;
    ops.operations().append(TranslateTransformOperation::create(Length(2, Fixed), Length(2, Fixed), TransformOperation::TranslateX));
    RefPtr<AnimatableValueKeyframe> goodKeyframe = createReplaceOpKeyframe(CSSPropertyTransform, AnimatableTransform::create(ops, 1).get());
    EXPECT_TRUE(duplicateSingleKeyframeAndTestIsCandidateOnResult(goodKeyframe.get()));

    ops.operations().append(TranslateTransformOperation::create(Length(50, Percent), Length(2, Fixed), TransformOperation::TranslateX));
    RefPtr<AnimatableValueKeyframe> badKeyframe = createReplaceOpKeyframe(CSSPropertyTransform, AnimatableTransform::create(ops, 1).get());
    EXPECT_FALSE(duplicateSingleKeyframeAndTestIsCandidateOnResult(badKeyframe.get()));

    TransformOperations ops2;
    Length calcLength = Length(100, Percent).blend(Length(100, Fixed), 0.5, ValueRangeAll);
    ops2.operations().append(TranslateTransformOperation::create(calcLength, Length(0, Fixed), TransformOperation::TranslateX));
    RefPtr<AnimatableValueKeyframe> badKeyframe2 = createReplaceOpKeyframe(CSSPropertyTransform, AnimatableTransform::create(ops2, 1).get());
    EXPECT_FALSE(duplicateSingleKeyframeAndTestIsCandidateOnResult(badKeyframe2.get()));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorKeyframeEffectModelMultipleFramesOkay)
{
    AnimatableValueKeyframeVector framesSame;
    framesSame.append(createDefaultKeyframe(CSSPropertyOpacity, EffectModel::CompositeReplace, 0.0).get());
    framesSame.append(createDefaultKeyframe(CSSPropertyOpacity, EffectModel::CompositeReplace, 1.0).get());
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *AnimatableValueKeyframeEffectModel::create(framesSame)));

    AnimatableValueKeyframeVector framesMixed;
    framesMixed.append(createDefaultKeyframe(CSSPropertyOpacity, EffectModel::CompositeReplace, 0.0).get());
    framesMixed.append(createDefaultKeyframe(CSSPropertyTransform, EffectModel::CompositeReplace, 1.0).get());
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *AnimatableValueKeyframeEffectModel::create(framesMixed)));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorKeyframeEffectModel)
{
    AnimatableValueKeyframeVector framesSame;
    framesSame.append(createDefaultKeyframe(CSSPropertyColor, EffectModel::CompositeReplace, 0.0).get());
    framesSame.append(createDefaultKeyframe(CSSPropertyColor, EffectModel::CompositeReplace, 1.0).get());
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *AnimatableValueKeyframeEffectModel::create(framesSame)));

    AnimatableValueKeyframeVector framesMixedProperties;
    framesMixedProperties.append(createDefaultKeyframe(CSSPropertyOpacity, EffectModel::CompositeReplace, 0.0).get());
    framesMixedProperties.append(createDefaultKeyframe(CSSPropertyColor, EffectModel::CompositeReplace, 1.0).get());
    EXPECT_FALSE(isCandidateForAnimationOnCompositor(m_timing, *AnimatableValueKeyframeEffectModel::create(framesMixedProperties)));
}

TEST_F(AnimationCompositorAnimationsTest, AnimatedBoundingBox)
{
    Vector<TransformOperations> transformVector;
    transformVector.append(TransformOperations());
    transformVector.last().operations().append(TranslateTransformOperation::create(Length(0, Fixed), Length(0, Fixed), 0.0, TransformOperation::Translate3D));
    transformVector.append(TransformOperations());
    transformVector.last().operations().append(TranslateTransformOperation::create(Length(200, Fixed), Length(200, Fixed), 0.0, TransformOperation::Translate3D));
    std::unique_ptr<AnimatableValueKeyframeVector> frames = createCompositableTransformKeyframeVector(transformVector);
    FloatBox bounds;
    EXPECT_TRUE(getAnimationBounds(bounds, *AnimatableValueKeyframeEffectModel::create(*frames), 0, 1));
    EXPECT_EQ(FloatBox(0.0f, 0.f, 0.0f, 200.0f, 200.0f, 0.0f), bounds);
    bounds = FloatBox();
    EXPECT_TRUE(getAnimationBounds(bounds, *AnimatableValueKeyframeEffectModel::create(*frames), -1, 1));
    EXPECT_EQ(FloatBox(-200.0f, -200.0, 0.0, 400.0f, 400.0f, 0.0f), bounds);
    transformVector.append(TransformOperations());
    transformVector.last().operations().append(TranslateTransformOperation::create(Length(-300, Fixed), Length(-400, Fixed), 1.0f, TransformOperation::Translate3D));
    bounds = FloatBox();
    frames = createCompositableTransformKeyframeVector(transformVector);
    EXPECT_TRUE(getAnimationBounds(bounds, *AnimatableValueKeyframeEffectModel::create(*frames), 0, 1));
    EXPECT_EQ(FloatBox(-300.0f, -400.f, 0.0f, 500.0f, 600.0f, 1.0f), bounds);
    bounds = FloatBox();
    EXPECT_TRUE(getAnimationBounds(bounds, *AnimatableValueKeyframeEffectModel::create(*frames), -1, 2));
    EXPECT_EQ(FloatBox(-1300.0f, -1600.f, 0.0f, 1500.0f, 1800.0f, 3.0f), bounds);
}

TEST_F(AnimationCompositorAnimationsTest, ConvertTimingForCompositorStartDelay)
{
    m_timing.iterationDuration = 20.0;

    m_timing.startDelay = 2.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(-2.0, m_compositorTiming.scaledTimeOffset);

    m_timing.startDelay = -2.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(2.0, m_compositorTiming.scaledTimeOffset);
}

TEST_F(AnimationCompositorAnimationsTest, ConvertTimingForCompositorIterationStart)
{
    m_timing.iterationStart = 2.2;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
}

TEST_F(AnimationCompositorAnimationsTest, ConvertTimingForCompositorIterationCount)
{
    m_timing.iterationCount = 5.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_EQ(5, m_compositorTiming.adjustedIterationCount);

    m_timing.iterationCount = 5.5;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_EQ(5.5, m_compositorTiming.adjustedIterationCount);

    // EXPECT_DEATH tests are flaky on Android.
#if ENABLE(ASSERT) && !OS(ANDROID)
    m_timing.iterationCount = -1;
    EXPECT_DEATH(convertTimingForCompositor(m_timing, m_compositorTiming), "");
#endif

    m_timing.iterationCount = std::numeric_limits<double>::infinity();
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_EQ(-1, m_compositorTiming.adjustedIterationCount);

    m_timing.iterationCount = std::numeric_limits<double>::infinity();
    m_timing.iterationDuration = 5.0;
    m_timing.startDelay = -6.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(6.0, m_compositorTiming.scaledTimeOffset);
    EXPECT_EQ(-1, m_compositorTiming.adjustedIterationCount);
}

TEST_F(AnimationCompositorAnimationsTest, ConvertTimingForCompositorIterationsAndStartDelay)
{
    m_timing.iterationCount = 4.0;
    m_timing.iterationDuration = 5.0;

    m_timing.startDelay = 6.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(-6.0, m_compositorTiming.scaledTimeOffset);
    EXPECT_DOUBLE_EQ(4.0, m_compositorTiming.adjustedIterationCount);

    m_timing.startDelay = -6.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(6.0, m_compositorTiming.scaledTimeOffset);
    EXPECT_DOUBLE_EQ(4.0, m_compositorTiming.adjustedIterationCount);

    m_timing.startDelay = 21.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
}

TEST_F(AnimationCompositorAnimationsTest, ConvertTimingForCompositorPlaybackRate)
{
    m_timing.playbackRate = 1.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(1.0, m_compositorTiming.playbackRate);

    m_timing.playbackRate = -2.3;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(-2.3, m_compositorTiming.playbackRate);

    m_timing.playbackRate = 1.6;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(1.6, m_compositorTiming.playbackRate);
}

TEST_F(AnimationCompositorAnimationsTest, ConvertTimingForCompositorDirection)
{
    m_timing.direction = Timing::PlaybackDirectionNormal;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_EQ(m_compositorTiming.direction, Timing::PlaybackDirectionNormal);

    m_timing.direction = Timing::PlaybackDirectionAlternate;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_EQ(m_compositorTiming.direction, Timing::PlaybackDirectionAlternate);

    m_timing.direction = Timing::PlaybackDirectionAlternateReverse;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_EQ(m_compositorTiming.direction, Timing::PlaybackDirectionAlternateReverse);

    m_timing.direction = Timing::PlaybackDirectionReverse;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_EQ(m_compositorTiming.direction, Timing::PlaybackDirectionReverse);
}

TEST_F(AnimationCompositorAnimationsTest, ConvertTimingForCompositorDirectionIterationsAndStartDelay)
{
    m_timing.direction = Timing::PlaybackDirectionAlternate;
    m_timing.iterationCount = 4.0;
    m_timing.iterationDuration = 5.0;
    m_timing.startDelay = -6.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(6.0, m_compositorTiming.scaledTimeOffset);
    EXPECT_EQ(4, m_compositorTiming.adjustedIterationCount);
    EXPECT_EQ(m_compositorTiming.direction, Timing::PlaybackDirectionAlternate);

    m_timing.direction = Timing::PlaybackDirectionAlternate;
    m_timing.iterationCount = 4.0;
    m_timing.iterationDuration = 5.0;
    m_timing.startDelay = -11.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(11.0, m_compositorTiming.scaledTimeOffset);
    EXPECT_EQ(4, m_compositorTiming.adjustedIterationCount);
    EXPECT_EQ(m_compositorTiming.direction, Timing::PlaybackDirectionAlternate);

    m_timing.direction = Timing::PlaybackDirectionAlternateReverse;
    m_timing.iterationCount = 4.0;
    m_timing.iterationDuration = 5.0;
    m_timing.startDelay = -6.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(6.0, m_compositorTiming.scaledTimeOffset);
    EXPECT_EQ(4, m_compositorTiming.adjustedIterationCount);
    EXPECT_EQ(m_compositorTiming.direction, Timing::PlaybackDirectionAlternateReverse);

    m_timing.direction = Timing::PlaybackDirectionAlternateReverse;
    m_timing.iterationCount = 4.0;
    m_timing.iterationDuration = 5.0;
    m_timing.startDelay = -11.0;
    EXPECT_TRUE(convertTimingForCompositor(m_timing, m_compositorTiming));
    EXPECT_DOUBLE_EQ(11.0, m_compositorTiming.scaledTimeOffset);
    EXPECT_EQ(4, m_compositorTiming.adjustedIterationCount);
    EXPECT_EQ(m_compositorTiming.direction, Timing::PlaybackDirectionAlternateReverse);
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorTimingFunctionLinear)
{
    m_timing.timingFunction = m_linearTimingFunction;
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2));
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorTimingFunctionCubic)
{
    m_timing.timingFunction = m_cubicEaseTimingFunction;
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2));
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5));

    m_timing.timingFunction = m_cubicCustomTimingFunction;
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2));
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorTimingFunctionSteps)
{
    m_timing.timingFunction = m_stepTimingFunction;
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2));
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorTimingFunctionChainedLinear)
{
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2));
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorNonLinearTimingFunctionOnFirstOrLastFrame)
{
    (*m_keyframeVector2)[0]->setEasing(m_cubicEaseTimingFunction.get());
    m_keyframeAnimationEffect2 = AnimatableValueKeyframeEffectModel::create(*m_keyframeVector2);

    (*m_keyframeVector5)[3]->setEasing(m_cubicEaseTimingFunction.get());
    m_keyframeAnimationEffect5 = AnimatableValueKeyframeEffectModel::create(*m_keyframeVector5);

    m_timing.timingFunction = m_cubicEaseTimingFunction;
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2));
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5));

    m_timing.timingFunction = m_cubicCustomTimingFunction;
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2));
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorTimingFunctionChainedCubicMatchingOffsets)
{
    (*m_keyframeVector2)[0]->setEasing(m_cubicEaseTimingFunction.get());
    m_keyframeAnimationEffect2 = AnimatableValueKeyframeEffectModel::create(*m_keyframeVector2);
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2));

    (*m_keyframeVector2)[0]->setEasing(m_cubicCustomTimingFunction.get());
    m_keyframeAnimationEffect2 = AnimatableValueKeyframeEffectModel::create(*m_keyframeVector2);
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2));

    (*m_keyframeVector5)[0]->setEasing(m_cubicEaseTimingFunction.get());
    (*m_keyframeVector5)[1]->setEasing(m_cubicCustomTimingFunction.get());
    (*m_keyframeVector5)[2]->setEasing(m_cubicCustomTimingFunction.get());
    (*m_keyframeVector5)[3]->setEasing(m_cubicCustomTimingFunction.get());
    m_keyframeAnimationEffect5 = AnimatableValueKeyframeEffectModel::create(*m_keyframeVector5);
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorTimingFunctionMixedGood)
{
    (*m_keyframeVector5)[0]->setEasing(m_linearTimingFunction.get());
    (*m_keyframeVector5)[1]->setEasing(m_cubicEaseTimingFunction.get());
    (*m_keyframeVector5)[2]->setEasing(m_cubicEaseTimingFunction.get());
    (*m_keyframeVector5)[3]->setEasing(m_linearTimingFunction.get());
    m_keyframeAnimationEffect5 = AnimatableValueKeyframeEffectModel::create(*m_keyframeVector5);
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositorTimingFunctionWithStepOkay)
{
    (*m_keyframeVector2)[0]->setEasing(m_stepTimingFunction.get());
    m_keyframeAnimationEffect2 = AnimatableValueKeyframeEffectModel::create(*m_keyframeVector2);
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect2));

    (*m_keyframeVector5)[0]->setEasing(m_stepTimingFunction.get());
    (*m_keyframeVector5)[1]->setEasing(m_linearTimingFunction.get());
    (*m_keyframeVector5)[2]->setEasing(m_cubicEaseTimingFunction.get());
    (*m_keyframeVector5)[3]->setEasing(m_linearTimingFunction.get());
    m_keyframeAnimationEffect5 = AnimatableValueKeyframeEffectModel::create(*m_keyframeVector5);
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5));

    (*m_keyframeVector5)[0]->setEasing(m_linearTimingFunction.get());
    (*m_keyframeVector5)[1]->setEasing(m_stepTimingFunction.get());
    (*m_keyframeVector5)[2]->setEasing(m_cubicEaseTimingFunction.get());
    (*m_keyframeVector5)[3]->setEasing(m_linearTimingFunction.get());
    m_keyframeAnimationEffect5 = AnimatableValueKeyframeEffectModel::create(*m_keyframeVector5);
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5));

    (*m_keyframeVector5)[0]->setEasing(m_linearTimingFunction.get());
    (*m_keyframeVector5)[1]->setEasing(m_cubicEaseTimingFunction.get());
    (*m_keyframeVector5)[2]->setEasing(m_cubicEaseTimingFunction.get());
    (*m_keyframeVector5)[3]->setEasing(m_stepTimingFunction.get());
    m_keyframeAnimationEffect5 = AnimatableValueKeyframeEffectModel::create(*m_keyframeVector5);
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(m_timing, *m_keyframeAnimationEffect5));
}

TEST_F(AnimationCompositorAnimationsTest, isCandidateForAnimationOnCompositor)
{
    Timing linearTiming(createCompositableTiming());

    AnimatableValueKeyframeVector basicFramesVector;
    basicFramesVector.append(createDefaultKeyframe(CSSPropertyOpacity, EffectModel::CompositeReplace, 0.0).get());
    basicFramesVector.append(createDefaultKeyframe(CSSPropertyOpacity, EffectModel::CompositeReplace, 1.0).get());

    AnimatableValueKeyframeVector nonBasicFramesVector;
    nonBasicFramesVector.append(createDefaultKeyframe(CSSPropertyOpacity, EffectModel::CompositeReplace, 0.0).get());
    nonBasicFramesVector.append(createDefaultKeyframe(CSSPropertyOpacity, EffectModel::CompositeReplace, 0.5).get());
    nonBasicFramesVector.append(createDefaultKeyframe(CSSPropertyOpacity, EffectModel::CompositeReplace, 1.0).get());

    basicFramesVector[0]->setEasing(m_linearTimingFunction.get());
    AnimatableValueKeyframeEffectModel* basicFrames = AnimatableValueKeyframeEffectModel::create(basicFramesVector);
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(linearTiming, *basicFrames));

    basicFramesVector[0]->setEasing(CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseType::EASE_IN));
    basicFrames = AnimatableValueKeyframeEffectModel::create(basicFramesVector);
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(linearTiming, *basicFrames));

    nonBasicFramesVector[0]->setEasing(m_linearTimingFunction.get());
    nonBasicFramesVector[1]->setEasing(CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseType::EASE_IN));
    AnimatableValueKeyframeEffectModel* nonBasicFrames = AnimatableValueKeyframeEffectModel::create(nonBasicFramesVector);
    EXPECT_TRUE(isCandidateForAnimationOnCompositor(linearTiming, *nonBasicFrames));
}

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------

TEST_F(AnimationCompositorAnimationsTest, createSimpleOpacityAnimation)
{
    // KeyframeEffect to convert
    AnimatableValueKeyframeEffectModel* effect = createKeyframeEffectModel(
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));

    std::unique_ptr<CompositorAnimation> animation = convertToCompositorAnimation(*effect);
    EXPECT_EQ(CompositorTargetProperty::OPACITY, animation->targetProperty());
    EXPECT_EQ(1.0, animation->iterations());
    EXPECT_EQ(0, animation->timeOffset());
    EXPECT_EQ(CompositorAnimation::Direction::NORMAL, animation->getDirection());
    EXPECT_EQ(1.0, animation->playbackRate());

    std::unique_ptr<CompositorFloatAnimationCurve> keyframedFloatCurve = animation->floatCurveForTesting();

    Vector<CompositorFloatKeyframe> keyframes = keyframedFloatCurve->keyframesForTesting();
    ASSERT_EQ(2UL, keyframes.size());

    EXPECT_EQ(0, keyframes[0].time);
    EXPECT_EQ(2.0f, keyframes[0].value);
    EXPECT_TRUE(keyframedFloatCurve->keyframeHasLinearTimingFunctionForTesting(0));

    EXPECT_EQ(1.0, keyframes[1].time);
    EXPECT_EQ(5.0f, keyframes[1].value);
    EXPECT_EQ(CubicBezierTimingFunction::EaseType::EASE, keyframedFloatCurve->getKeyframeEaseTypeForTesting(1));
}

TEST_F(AnimationCompositorAnimationsTest, createSimpleOpacityAnimationDuration)
{
    // KeyframeEffect to convert
    AnimatableValueKeyframeEffectModel* effect = createKeyframeEffectModel(
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));

    const double duration = 10.0;
    m_timing.iterationDuration = duration;

    std::unique_ptr<CompositorAnimation> animation = convertToCompositorAnimation(*effect);
    std::unique_ptr<CompositorFloatAnimationCurve> keyframedFloatCurve = animation->floatCurveForTesting();

    Vector<CompositorFloatKeyframe> keyframes = keyframedFloatCurve->keyframesForTesting();
    ASSERT_EQ(2UL, keyframes.size());

    EXPECT_EQ(duration, keyframes[1].time);
}

TEST_F(AnimationCompositorAnimationsTest, createMultipleKeyframeOpacityAnimationLinear)
{
    // KeyframeEffect to convert
    AnimatableValueKeyframeEffectModel* effect = createKeyframeEffectModel(
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(-1.0).get(), 0.25),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(20.0).get(), 0.5),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));

    m_timing.iterationCount = 5;
    m_timing.direction = Timing::PlaybackDirectionAlternate;
    m_timing.playbackRate = 2.0;

    std::unique_ptr<CompositorAnimation> animation = convertToCompositorAnimation(*effect);
    EXPECT_EQ(CompositorTargetProperty::OPACITY, animation->targetProperty());
    EXPECT_EQ(5.0, animation->iterations());
    EXPECT_EQ(0, animation->timeOffset());
    EXPECT_EQ(CompositorAnimation::Direction::ALTERNATE_NORMAL, animation->getDirection());
    EXPECT_EQ(2.0, animation->playbackRate());

    std::unique_ptr<CompositorFloatAnimationCurve> keyframedFloatCurve = animation->floatCurveForTesting();

    Vector<CompositorFloatKeyframe> keyframes = keyframedFloatCurve->keyframesForTesting();
    ASSERT_EQ(4UL, keyframes.size());

    EXPECT_EQ(0, keyframes[0].time);
    EXPECT_EQ(2.0f, keyframes[0].value);
    EXPECT_TRUE(keyframedFloatCurve->keyframeHasLinearTimingFunctionForTesting(0));

    EXPECT_EQ(0.25, keyframes[1].time);
    EXPECT_EQ(-1.0f, keyframes[1].value);
    EXPECT_TRUE(keyframedFloatCurve->keyframeHasLinearTimingFunctionForTesting(1));

    EXPECT_EQ(0.5, keyframes[2].time);
    EXPECT_EQ(20.0f, keyframes[2].value);
    EXPECT_TRUE(keyframedFloatCurve->keyframeHasLinearTimingFunctionForTesting(2));

    EXPECT_EQ(1.0, keyframes[3].time);
    EXPECT_EQ(5.0f, keyframes[3].value);
    EXPECT_EQ(CubicBezierTimingFunction::EaseType::EASE, keyframedFloatCurve->getKeyframeEaseTypeForTesting(3));
}

TEST_F(AnimationCompositorAnimationsTest, createSimpleOpacityAnimationStartDelay)
{
    // KeyframeEffect to convert
    AnimatableValueKeyframeEffectModel* effect = createKeyframeEffectModel(
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));

    const double startDelay = 3.25;

    m_timing.iterationCount = 5.0;
    m_timing.iterationDuration = 1.75;
    m_timing.startDelay = startDelay;

    std::unique_ptr<CompositorAnimation> animation = convertToCompositorAnimation(*effect);

    EXPECT_EQ(CompositorTargetProperty::OPACITY, animation->targetProperty());
    EXPECT_EQ(5.0, animation->iterations());
    EXPECT_EQ(-startDelay, animation->timeOffset());

    std::unique_ptr<CompositorFloatAnimationCurve> keyframedFloatCurve = animation->floatCurveForTesting();

    Vector<CompositorFloatKeyframe> keyframes = keyframedFloatCurve->keyframesForTesting();
    ASSERT_EQ(2UL, keyframes.size());

    EXPECT_EQ(1.75, keyframes[1].time);
    EXPECT_EQ(5.0f, keyframes[1].value);
}

TEST_F(AnimationCompositorAnimationsTest, createMultipleKeyframeOpacityAnimationChained)
{
    // KeyframeEffect to convert
    AnimatableValueKeyframeVector frames;
    frames.append(createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0));
    frames.append(createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(-1.0).get(), 0.25));
    frames.append(createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(20.0).get(), 0.5));
    frames.append(createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));
    frames[0]->setEasing(m_cubicEaseTimingFunction.get());
    frames[1]->setEasing(m_linearTimingFunction.get());
    frames[2]->setEasing(m_cubicCustomTimingFunction.get());
    AnimatableValueKeyframeEffectModel* effect = AnimatableValueKeyframeEffectModel::create(frames);

    m_timing.timingFunction = m_linearTimingFunction.get();
    m_timing.iterationDuration = 2.0;
    m_timing.iterationCount = 10;
    m_timing.direction = Timing::PlaybackDirectionAlternate;

    std::unique_ptr<CompositorAnimation> animation = convertToCompositorAnimation(*effect);
    EXPECT_EQ(CompositorTargetProperty::OPACITY, animation->targetProperty());
    EXPECT_EQ(10.0, animation->iterations());
    EXPECT_EQ(0, animation->timeOffset());
    EXPECT_EQ(CompositorAnimation::Direction::ALTERNATE_NORMAL, animation->getDirection());
    EXPECT_EQ(1.0, animation->playbackRate());

    std::unique_ptr<CompositorFloatAnimationCurve> keyframedFloatCurve = animation->floatCurveForTesting();

    Vector<CompositorFloatKeyframe> keyframes = keyframedFloatCurve->keyframesForTesting();
    ASSERT_EQ(4UL, keyframes.size());

    EXPECT_EQ(0, keyframes[0].time);
    EXPECT_EQ(2.0f, keyframes[0].value);
    EXPECT_EQ(CubicBezierTimingFunction::EaseType::EASE, keyframedFloatCurve->getKeyframeEaseTypeForTesting(0));

    EXPECT_EQ(0.5, keyframes[1].time);
    EXPECT_EQ(-1.0f, keyframes[1].value);
    EXPECT_TRUE(keyframedFloatCurve->keyframeHasLinearTimingFunctionForTesting(1));

    EXPECT_EQ(1.0, keyframes[2].time);
    EXPECT_EQ(20.0f, keyframes[2].value);
    EXPECT_EQ(CubicBezierTimingFunction::EaseType::CUSTOM, keyframedFloatCurve->getKeyframeEaseTypeForTesting(2));

    EXPECT_EQ(2.0, keyframes[3].time);
    EXPECT_EQ(5.0f, keyframes[3].value);
    EXPECT_EQ(CubicBezierTimingFunction::EaseType::EASE, keyframedFloatCurve->getKeyframeEaseTypeForTesting(3));
}

TEST_F(AnimationCompositorAnimationsTest, createReversedOpacityAnimation)
{
    RefPtr<TimingFunction> cubicEasyFlipTimingFunction = CubicBezierTimingFunction::create(0.0, 0.0, 0.0, 1.0);

    // KeyframeEffect to convert
    AnimatableValueKeyframeVector frames;
    frames.append(createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0));
    frames.append(createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(-1.0).get(), 0.25));
    frames.append(createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(20.0).get(), 0.5));
    frames.append(createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));
    frames[0]->setEasing(CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseType::EASE_IN));
    frames[1]->setEasing(m_linearTimingFunction.get());
    frames[2]->setEasing(cubicEasyFlipTimingFunction.get());
    AnimatableValueKeyframeEffectModel* effect = AnimatableValueKeyframeEffectModel::create(frames);

    m_timing.timingFunction = m_linearTimingFunction.get();
    m_timing.iterationCount = 10;
    m_timing.direction = Timing::PlaybackDirectionAlternateReverse;

    std::unique_ptr<CompositorAnimation> animation = convertToCompositorAnimation(*effect);
    EXPECT_EQ(CompositorTargetProperty::OPACITY, animation->targetProperty());
    EXPECT_EQ(10.0, animation->iterations());
    EXPECT_EQ(0, animation->timeOffset());
    EXPECT_EQ(CompositorAnimation::Direction::ALTERNATE_REVERSE, animation->getDirection());
    EXPECT_EQ(1.0, animation->playbackRate());

    std::unique_ptr<CompositorFloatAnimationCurve> keyframedFloatCurve = animation->floatCurveForTesting();

    Vector<CompositorFloatKeyframe> keyframes = keyframedFloatCurve->keyframesForTesting();
    ASSERT_EQ(4UL, keyframes.size());

    EXPECT_TRUE(keyframedFloatCurve->curveHasLinearTimingFunctionForTesting());

    EXPECT_EQ(0, keyframes[0].time);
    EXPECT_EQ(2.0f, keyframes[0].value);
    EXPECT_EQ(CubicBezierTimingFunction::EaseType::EASE_IN, keyframedFloatCurve->getKeyframeEaseTypeForTesting(0));

    EXPECT_EQ(0.25, keyframes[1].time);
    EXPECT_EQ(-1.0f, keyframes[1].value);
    EXPECT_TRUE(keyframedFloatCurve->keyframeHasLinearTimingFunctionForTesting(1));

    EXPECT_EQ(0.5, keyframes[2].time);
    EXPECT_EQ(20.0f, keyframes[2].value);
    EXPECT_EQ(CubicBezierTimingFunction::EaseType::CUSTOM, keyframedFloatCurve->getKeyframeEaseTypeForTesting(2));

    EXPECT_EQ(1.0, keyframes[3].time);
    EXPECT_EQ(5.0f, keyframes[3].value);
    EXPECT_EQ(CubicBezierTimingFunction::EaseType::EASE, keyframedFloatCurve->getKeyframeEaseTypeForTesting(3));
}

TEST_F(AnimationCompositorAnimationsTest, createReversedOpacityAnimationNegativeStartDelay)
{
    // KeyframeEffect to convert
    AnimatableValueKeyframeEffectModel* effect = createKeyframeEffectModel(
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));

    const double negativeStartDelay = -3;

    m_timing.iterationCount = 5.0;
    m_timing.iterationDuration = 1.5;
    m_timing.startDelay = negativeStartDelay;
    m_timing.direction = Timing::PlaybackDirectionAlternateReverse;

    std::unique_ptr<CompositorAnimation> animation = convertToCompositorAnimation(*effect);
    EXPECT_EQ(CompositorTargetProperty::OPACITY, animation->targetProperty());
    EXPECT_EQ(5.0, animation->iterations());
    EXPECT_EQ(-negativeStartDelay, animation->timeOffset());
    EXPECT_EQ(CompositorAnimation::Direction::ALTERNATE_REVERSE, animation->getDirection());
    EXPECT_EQ(1.0, animation->playbackRate());

    std::unique_ptr<CompositorFloatAnimationCurve> keyframedFloatCurve = animation->floatCurveForTesting();

    Vector<CompositorFloatKeyframe> keyframes = keyframedFloatCurve->keyframesForTesting();
    ASSERT_EQ(2UL, keyframes.size());
}

TEST_F(AnimationCompositorAnimationsTest, createSimpleOpacityAnimationPlaybackRates)
{
    // KeyframeEffect to convert
    AnimatableValueKeyframeEffectModel* effect = createKeyframeEffectModel(
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));

    const double playbackRate = 2;
    const double playerPlaybackRate = -1.5;

    m_timing.playbackRate = playbackRate;

    std::unique_ptr<CompositorAnimation> animation = convertToCompositorAnimation(*effect, playerPlaybackRate);
    EXPECT_EQ(CompositorTargetProperty::OPACITY, animation->targetProperty());
    EXPECT_EQ(1.0, animation->iterations());
    EXPECT_EQ(0, animation->timeOffset());
    EXPECT_EQ(CompositorAnimation::Direction::NORMAL, animation->getDirection());
    EXPECT_EQ(playbackRate * playerPlaybackRate, animation->playbackRate());

    std::unique_ptr<CompositorFloatAnimationCurve> keyframedFloatCurve = animation->floatCurveForTesting();

    Vector<CompositorFloatKeyframe> keyframes = keyframedFloatCurve->keyframesForTesting();
    ASSERT_EQ(2UL, keyframes.size());
}

TEST_F(AnimationCompositorAnimationsTest, createSimpleOpacityAnimationFillModeNone)
{
    // KeyframeEffect to convert
    AnimatableValueKeyframeEffectModel* effect = createKeyframeEffectModel(
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));

    m_timing.fillMode = Timing::FillModeNone;

    std::unique_ptr<CompositorAnimation> animation = convertToCompositorAnimation(*effect);
    EXPECT_EQ(CompositorAnimation::FillMode::NONE, animation->getFillMode());
}

TEST_F(AnimationCompositorAnimationsTest, createSimpleOpacityAnimationFillModeAuto)
{
    // KeyframeEffect to convert
    AnimatableValueKeyframeEffectModel* effect = createKeyframeEffectModel(
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));

    m_timing.fillMode = Timing::FillModeAuto;

    std::unique_ptr<CompositorAnimation> animation = convertToCompositorAnimation(*effect);
    EXPECT_EQ(CompositorTargetProperty::OPACITY, animation->targetProperty());
    EXPECT_EQ(1.0, animation->iterations());
    EXPECT_EQ(0, animation->timeOffset());
    EXPECT_EQ(CompositorAnimation::Direction::NORMAL, animation->getDirection());
    EXPECT_EQ(1.0, animation->playbackRate());
    EXPECT_EQ(CompositorAnimation::FillMode::NONE, animation->getFillMode());
}

TEST_F(AnimationCompositorAnimationsTest, createSimpleOpacityAnimationWithTimingFunction)
{
    // KeyframeEffect to convert
    AnimatableValueKeyframeEffectModel* effect = createKeyframeEffectModel(
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(2.0).get(), 0),
        createReplaceOpKeyframe(CSSPropertyOpacity, AnimatableDouble::create(5.0).get(), 1.0));

    m_timing.timingFunction = m_cubicCustomTimingFunction;

    std::unique_ptr<CompositorAnimation> animation = convertToCompositorAnimation(*effect);

    std::unique_ptr<CompositorFloatAnimationCurve> keyframedFloatCurve = animation->floatCurveForTesting();

    Vector<CompositorFloatKeyframe> keyframes = keyframedFloatCurve->keyframesForTesting();
    ASSERT_EQ(2UL, keyframes.size());

    EXPECT_EQ(CubicBezierTimingFunction::EaseType::CUSTOM, keyframedFloatCurve->getCurveEaseTypeForTesting());

    EXPECT_EQ(0, keyframes[0].time);
    EXPECT_EQ(2.0f, keyframes[0].value);
    EXPECT_TRUE(keyframedFloatCurve->keyframeHasLinearTimingFunctionForTesting(0));

    EXPECT_EQ(1.0, keyframes[1].time);
    EXPECT_EQ(5.0f, keyframes[1].value);
    EXPECT_EQ(CubicBezierTimingFunction::EaseType::EASE, keyframedFloatCurve->getKeyframeEaseTypeForTesting(1));
}

TEST_F(AnimationCompositorAnimationsTest, cancelIncompatibleCompositorAnimations)
{
    Persistent<Element> element = m_document->createElement("shared", ASSERT_NO_EXCEPTION);

    LayoutObjectProxy* layoutObject = LayoutObjectProxy::create(element.get());
    element->setLayoutObject(layoutObject);

    AnimatableValueKeyframeVector keyFrames;
    keyFrames.append(createDefaultKeyframe(CSSPropertyOpacity, EffectModel::CompositeReplace, 0.0).get());
    keyFrames.append(createDefaultKeyframe(CSSPropertyOpacity, EffectModel::CompositeReplace, 1.0).get());
    EffectModel* animationEffect1 = AnimatableValueKeyframeEffectModel::create(keyFrames);
    EffectModel* animationEffect2 = AnimatableValueKeyframeEffectModel::create(keyFrames);

    Timing timing;
    timing.iterationDuration = 1.f;

    // The first animation for opacity is ok to run on compositor.
    KeyframeEffect* keyframeEffect1 = KeyframeEffect::create(element.get(), animationEffect1, timing);
    Animation* animation1 = m_timeline->play(keyframeEffect1);
    EXPECT_TRUE(CompositorAnimations::isCandidateForAnimationOnCompositor(timing, *element.get(), animation1, *animationEffect1, 1));

    // simulate KeyframeEffect::maybeStartAnimationOnCompositor
    Vector<int> compositorAnimationIds;
    compositorAnimationIds.append(1);
    keyframeEffect1->setCompositorAnimationIdsForTesting(compositorAnimationIds);
    EXPECT_TRUE(animation1->hasActiveAnimationsOnCompositor());

    // The second animation for opacity is not ok to run on compositor.
    KeyframeEffect* keyframeEffect2 = KeyframeEffect::create(element.get(), animationEffect2, timing);
    Animation* animation2 = m_timeline->play(keyframeEffect2);
    EXPECT_FALSE(CompositorAnimations::isCandidateForAnimationOnCompositor(timing, *element.get(), animation2, *animationEffect2, 1));
    EXPECT_FALSE(animation2->hasActiveAnimationsOnCompositor());

    // A fallback to blink implementation needed, so cancel all compositor-side opacity animations for this element.
    animation2->cancelIncompatibleAnimationsOnCompositor();

    EXPECT_FALSE(animation1->hasActiveAnimationsOnCompositor());
    EXPECT_FALSE(animation2->hasActiveAnimationsOnCompositor());

    simulateFrame(0);
    EXPECT_EQ(2U, element->elementAnimations()->animations().size());
    simulateFrame(1.);

    element->setLayoutObject(nullptr);
    LayoutObjectProxy::dispose(layoutObject);

    ThreadHeap::collectAllGarbage();
    EXPECT_TRUE(element->elementAnimations()->animations().isEmpty());
}

} // namespace blink
