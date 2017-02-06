// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UnderlyingLengthChecker_h
#define UnderlyingLengthChecker_h

#include "core/animation/InterpolableValue.h"
#include "core/animation/InterpolationType.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class UnderlyingLengthChecker : public InterpolationType::ConversionChecker {
public:
    static std::unique_ptr<UnderlyingLengthChecker> create(size_t underlyingLength)
    {
        return wrapUnique(new UnderlyingLengthChecker(underlyingLength));
    }

    static size_t getUnderlyingLength(const InterpolationValue& underlying)
    {
        if (!underlying)
            return 0;
        return toInterpolableList(*underlying.interpolableValue).length();
    }

    bool isValid(const InterpolationEnvironment&, const InterpolationValue& underlying) const final
    {
        return m_underlyingLength == getUnderlyingLength(underlying);
    }

private:
    UnderlyingLengthChecker(size_t underlyingLength)
        : m_underlyingLength(underlyingLength)
    {}

    size_t m_underlyingLength;
};

} // namespace blink

#endif // UnderlyingLengthChecker_h
