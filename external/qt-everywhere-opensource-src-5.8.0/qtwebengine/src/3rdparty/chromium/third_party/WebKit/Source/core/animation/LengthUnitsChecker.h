// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthUnitsChecker_h
#define LengthUnitsChecker_h

#include "core/animation/InterpolationType.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/resolver/StyleResolverState.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class LengthUnitsChecker : public InterpolationType::ConversionChecker {
public:
    static std::unique_ptr<LengthUnitsChecker> maybeCreate(CSSLengthArray&& lengthArray, const StyleResolverState& state)
    {
        bool create = false;
        size_t lastIndex = 0;
        for (size_t i = 0; i < lengthArray.values.size(); i++) {
            if (i == CSSPrimitiveValue::UnitTypePercentage || !lengthArray.typeFlags.get(i))
                continue;
            lengthArray.values[i] = lengthUnit(i, state.cssToLengthConversionData());
            create = true;
            lastIndex = i;
        }
        if (!create)
            return nullptr;
        return wrapUnique(new LengthUnitsChecker(std::move(lengthArray), lastIndex));
    }

    bool isValid(const InterpolationEnvironment& environment, const InterpolationValue& underlying) const final
    {
        for (size_t i = 0; i <= m_lastIndex; i++) {
            if (i == CSSPrimitiveValue::UnitTypePercentage || !m_lengthArray.typeFlags.get(i))
                continue;
            if (m_lengthArray.values[i] != lengthUnit(i, environment.state().cssToLengthConversionData()))
                return false;
        }
        return true;
    }

    static double lengthUnit(size_t lengthUnitType, const CSSToLengthConversionData& conversionData)
    {
        return conversionData.zoomedComputedPixels(1, CSSPrimitiveValue::lengthUnitTypeToUnitType(static_cast<CSSPrimitiveValue::LengthUnitType>(lengthUnitType)));
    }

private:
    LengthUnitsChecker(CSSPrimitiveValue::CSSLengthArray&& lengthArray, size_t lastIndex)
        : m_lengthArray(std::move(lengthArray))
        , m_lastIndex(lastIndex)
    { }

    const CSSLengthArray m_lengthArray;
    const size_t m_lastIndex;
};

} // namespace blink

#endif // LengthUnitsChecker_h
