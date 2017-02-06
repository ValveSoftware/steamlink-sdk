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

#include "core/animation/KeyframeEffectModel.h"

#include "core/animation/AnimationEffect.h"
#include "core/animation/CompositorAnimations.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/css/CSSPropertyEquality.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "platform/animation/AnimationUtilities.h"
#include "platform/geometry/FloatBox.h"
#include "platform/transforms/TransformationMatrix.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/StringHash.h"

namespace blink {

PropertyHandleSet KeyframeEffectModelBase::properties() const
{
    PropertyHandleSet result;
    for (const auto& keyframe : m_keyframes) {
        for (const auto& property : keyframe->properties())
            result.add(property);
    }
    return result;
}

void KeyframeEffectModelBase::setFrames(KeyframeVector& keyframes)
{
    // TODO(samli): Should also notify/invalidate the animation
    m_keyframes = keyframes;
    m_keyframeGroups = nullptr;
    m_interpolationEffect.clear();
    m_lastFraction = std::numeric_limits<double>::quiet_NaN();
}

bool KeyframeEffectModelBase::sample(int iteration, double fraction, double iterationDuration, Vector<RefPtr<Interpolation>>& result) const
{
    ASSERT(iteration >= 0);
    ASSERT(!isNull(fraction));
    ensureKeyframeGroups();
    ensureInterpolationEffectPopulated();

    bool changed = iteration != m_lastIteration || fraction != m_lastFraction || iterationDuration != m_lastIterationDuration;
    m_lastIteration = iteration;
    m_lastFraction = fraction;
    m_lastIterationDuration = iterationDuration;
    m_interpolationEffect.getActiveInterpolations(fraction, iterationDuration, result);
    return changed;
}

void KeyframeEffectModelBase::forceConversionsToAnimatableValues(Element& element, const ComputedStyle* baseStyle)
{
    ensureKeyframeGroups();
    // TODO(alancutter): Defer compositor keyframe snapshotting to style resolve to remove the Element and ComputedStyle dependency here.
    snapshotAllCompositorKeyframes(element, baseStyle);
    ensureInterpolationEffectPopulated();
}

bool KeyframeEffectModelBase::snapshotNeutralCompositorKeyframes(Element& element, const ComputedStyle& oldStyle, const ComputedStyle& newStyle)
{
    bool updated = false;
    ensureKeyframeGroups();
    for (CSSPropertyID property : CompositorAnimations::compositableProperties) {
        if (CSSPropertyEquality::propertiesEqual(property, oldStyle, newStyle))
            continue;
        PropertySpecificKeyframeGroup* keyframeGroup = m_keyframeGroups->get(PropertyHandle(property));
        if (!keyframeGroup)
            continue;
        for (auto& keyframe : keyframeGroup->m_keyframes) {
            if (keyframe->isNeutral())
                updated |= keyframe->populateAnimatableValue(property, element, &newStyle, true);
        }
    }
    return updated;
}

bool KeyframeEffectModelBase::snapshotAllCompositorKeyframes(Element& element, const ComputedStyle* baseStyle)
{
    bool updated = false;
    ensureKeyframeGroups();
    for (CSSPropertyID property : CompositorAnimations::compositableProperties) {
        PropertySpecificKeyframeGroup* keyframeGroup = m_keyframeGroups->get(PropertyHandle(property));
        if (!keyframeGroup)
            continue;
        for (auto& keyframe : keyframeGroup->m_keyframes)
            updated |= keyframe->populateAnimatableValue(property, element, baseStyle, true);
    }
    return updated;
}

KeyframeEffectModelBase::KeyframeVector KeyframeEffectModelBase::normalizedKeyframes(const KeyframeVector& keyframes)
{
    double lastOffset = 0;
    KeyframeVector result;
    result.reserveCapacity(keyframes.size());

    for (const auto& keyframe : keyframes) {
        double offset = keyframe->offset();
        if (!isNull(offset)) {
            ASSERT(offset >= 0);
            ASSERT(offset <= 1);
            ASSERT(offset >= lastOffset);
            lastOffset = offset;
        }
        result.append(keyframe->clone());
    }

    if (result.isEmpty())
        return result;

    if (isNull(result.last()->offset()))
        result.last()->setOffset(1);

    if (result.size() > 1 && isNull(result[0]->offset()))
        result.first()->setOffset(0);

    size_t lastIndex = 0;
    lastOffset = result.first()->offset();
    for (size_t i = 1; i < result.size(); ++i) {
        double offset = result[i]->offset();
        if (!isNull(offset)) {
            for (size_t j = 1; j < i - lastIndex; ++j)
                result[lastIndex + j]->setOffset(lastOffset + (offset - lastOffset) * j / (i - lastIndex));
            lastIndex = i;
            lastOffset = offset;
        }
    }

    return result;
}

bool KeyframeEffectModelBase::isTransformRelatedEffect() const
{
    return affects(PropertyHandle(CSSPropertyTransform))
        || affects(PropertyHandle(CSSPropertyRotate))
        || affects(PropertyHandle(CSSPropertyScale))
        || affects(PropertyHandle(CSSPropertyTranslate));
}

void KeyframeEffectModelBase::ensureKeyframeGroups() const
{
    if (m_keyframeGroups)
        return;

    m_keyframeGroups = wrapUnique(new KeyframeGroupMap);
    RefPtr<TimingFunction> zeroOffsetEasing = m_defaultKeyframeEasing;
    for (const auto& keyframe : normalizedKeyframes(getFrames())) {
        if (keyframe->offset() == 0)
            zeroOffsetEasing = &keyframe->easing();

        for (const PropertyHandle& property : keyframe->properties()) {
            KeyframeGroupMap::iterator groupIter = m_keyframeGroups->find(property);
            PropertySpecificKeyframeGroup* group;
            if (groupIter == m_keyframeGroups->end())
                group = m_keyframeGroups->add(property, wrapUnique(new PropertySpecificKeyframeGroup)).storedValue->value.get();
            else
                group = groupIter->value.get();

            group->appendKeyframe(keyframe->createPropertySpecificKeyframe(property));
        }
    }

    // Add synthetic keyframes.
    m_hasSyntheticKeyframes = false;
    for (const auto& entry : *m_keyframeGroups) {
        if (entry.value->addSyntheticKeyframeIfRequired(zeroOffsetEasing))
            m_hasSyntheticKeyframes = true;

        entry.value->removeRedundantKeyframes();
    }
}

void KeyframeEffectModelBase::ensureInterpolationEffectPopulated() const
{
    if (m_interpolationEffect.isPopulated())
        return;

    for (const auto& entry : *m_keyframeGroups) {
        const PropertySpecificKeyframeVector& keyframes = entry.value->keyframes();
        for (size_t i = 0; i < keyframes.size() - 1; i++) {
            size_t startIndex = i;
            size_t endIndex = i+1;
            double startOffset = keyframes[startIndex]->offset();
            double endOffset = keyframes[endIndex]->offset();
            double applyFrom = startOffset;
            double applyTo = endOffset;

            if (i == 0) {
                applyFrom = -std::numeric_limits<double>::infinity();
                ASSERT(startOffset == 0.0);
                if (endOffset == 0.0) {
                    ASSERT(keyframes[endIndex + 1]->offset() != 0.0);
                    endIndex = startIndex;
                }
            }
            if (i == keyframes.size() - 2) {
                applyTo = std::numeric_limits<double>::infinity();
                ASSERT(endOffset == 1.0);
                if (startOffset == 1.0) {
                    ASSERT(keyframes[startIndex - 1]->offset() != 1.0);
                    startIndex = endIndex;
                }
            }

            if (applyFrom != applyTo) {
                m_interpolationEffect.addInterpolationsFromKeyframes(
                    entry.key, *keyframes[startIndex], *keyframes[endIndex], applyFrom, applyTo);
            }
            // else the interpolation will never be used in sampling
        }
    }

    m_interpolationEffect.setPopulated();
}

bool KeyframeEffectModelBase::isReplaceOnly()
{
    ensureKeyframeGroups();
    for (const auto& entry : *m_keyframeGroups) {
        for (const auto& keyframe : entry.value->keyframes()) {
            if (keyframe->composite() != EffectModel::CompositeReplace)
                return false;
        }
    }
    return true;
}

Keyframe::PropertySpecificKeyframe::PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, EffectModel::CompositeOperation composite)
    : m_offset(offset)
    , m_easing(easing)
    , m_composite(composite)
{
    ASSERT(!isNull(offset));
}

void KeyframeEffectModelBase::PropertySpecificKeyframeGroup::appendKeyframe(PassRefPtr<Keyframe::PropertySpecificKeyframe> keyframe)
{
    ASSERT(m_keyframes.isEmpty() || m_keyframes.last()->offset() <= keyframe->offset());
    m_keyframes.append(keyframe);
}

void KeyframeEffectModelBase::PropertySpecificKeyframeGroup::removeRedundantKeyframes()
{
    // As an optimization, removes interior keyframes that have the same offset
    // as both their neighbours, as they will never be used by sample().
    // Note that synthetic keyframes must be added before this method is
    // called.
    ASSERT(m_keyframes.size() >= 2);
    for (int i = m_keyframes.size() - 2; i > 0; --i) {
        double offset = m_keyframes[i]->offset();
        bool hasSameOffsetAsPreviousNeighbor = m_keyframes[i - 1]->offset() == offset;
        bool hasSameOffsetAsNextNeighbor = m_keyframes[i + 1]->offset() == offset;
        if (hasSameOffsetAsPreviousNeighbor && hasSameOffsetAsNextNeighbor)
            m_keyframes.remove(i);
    }
    ASSERT(m_keyframes.size() >= 2);
}

bool KeyframeEffectModelBase::PropertySpecificKeyframeGroup::addSyntheticKeyframeIfRequired(PassRefPtr<TimingFunction> zeroOffsetEasing)
{
    ASSERT(!m_keyframes.isEmpty());

    bool addedSyntheticKeyframe = false;

    if (m_keyframes.first()->offset() != 0.0) {
        m_keyframes.insert(0, m_keyframes.first()->neutralKeyframe(0, zeroOffsetEasing));
        addedSyntheticKeyframe = true;
    }
    if (m_keyframes.last()->offset() != 1.0) {
        appendKeyframe(m_keyframes.last()->neutralKeyframe(1, nullptr));
        addedSyntheticKeyframe = true;
    }

    return addedSyntheticKeyframe;
}

} // namespace blink
