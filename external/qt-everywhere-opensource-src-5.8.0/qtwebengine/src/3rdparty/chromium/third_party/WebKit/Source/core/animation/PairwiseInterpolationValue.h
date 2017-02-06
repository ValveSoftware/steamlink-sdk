// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PairwiseInterpolationValue_h
#define PairwiseInterpolationValue_h

#include "core/animation/InterpolableValue.h"
#include "core/animation/NonInterpolableValue.h"
#include "platform/heap/Handle.h"
#include <memory>

namespace blink {

// Represents the smooth interpolation between an adjacent pair of PropertySpecificKeyframes.
struct PairwiseInterpolationValue {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

    PairwiseInterpolationValue(std::unique_ptr<InterpolableValue> startInterpolableValue, std::unique_ptr<InterpolableValue> endInterpolableValue, PassRefPtr<NonInterpolableValue> nonInterpolableValue = nullptr)
        : startInterpolableValue(std::move(startInterpolableValue))
        , endInterpolableValue(std::move(endInterpolableValue))
        , nonInterpolableValue(std::move(nonInterpolableValue))
    { }

    PairwiseInterpolationValue(std::nullptr_t) { }

    PairwiseInterpolationValue(PairwiseInterpolationValue&& other)
        : startInterpolableValue(std::move(other.startInterpolableValue))
        , endInterpolableValue(std::move(other.endInterpolableValue))
        , nonInterpolableValue(other.nonInterpolableValue.release())
    { }

    operator bool() const { return startInterpolableValue.get(); }

    std::unique_ptr<InterpolableValue> startInterpolableValue;
    std::unique_ptr<InterpolableValue> endInterpolableValue;
    RefPtr<NonInterpolableValue> nonInterpolableValue;
};

} // namespace blink

#endif // PairwiseInterpolationValue_h
