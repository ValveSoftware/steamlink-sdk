// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationValue_h
#define InterpolationValue_h

#include "core/animation/InterpolableValue.h"
#include "core/animation/NonInterpolableValue.h"
#include "platform/heap/Handle.h"
#include <memory>

namespace blink {

// Represents a (non-strict) subset of a PropertySpecificKeyframe's value broken down into interpolable and non-interpolable parts.
// InterpolationValues can be composed together to represent a whole PropertySpecificKeyframe value.
struct InterpolationValue {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

    explicit InterpolationValue(std::unique_ptr<InterpolableValue> interpolableValue, PassRefPtr<NonInterpolableValue> nonInterpolableValue = nullptr)
        : interpolableValue(std::move(interpolableValue))
        , nonInterpolableValue(nonInterpolableValue)
    { }

    InterpolationValue(std::nullptr_t) { }

    InterpolationValue(InterpolationValue&& other)
        : interpolableValue(std::move(other.interpolableValue))
        , nonInterpolableValue(other.nonInterpolableValue.release())
    { }

    void operator=(InterpolationValue&& other)
    {
        interpolableValue = std::move(other.interpolableValue);
        nonInterpolableValue = other.nonInterpolableValue.release();
    }

    operator bool() const { return interpolableValue.get(); }

    InterpolationValue clone() const
    {
        return InterpolationValue(interpolableValue ? interpolableValue->clone() : nullptr, nonInterpolableValue);
    }

    void clear()
    {
        interpolableValue.reset();
        nonInterpolableValue.clear();
    }

    std::unique_ptr<InterpolableValue> interpolableValue;
    RefPtr<NonInterpolableValue> nonInterpolableValue;
};

} // namespace blink

#endif // InterpolationValue_h
