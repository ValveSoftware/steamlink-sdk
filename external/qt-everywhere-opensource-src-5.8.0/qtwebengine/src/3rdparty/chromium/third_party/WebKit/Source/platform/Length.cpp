/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "platform/Length.h"

#include "platform/CalculationValue.h"
#include "platform/animation/AnimationUtilities.h"

using namespace WTF;

namespace blink {

class CalculationValueHandleMap {
    USING_FAST_MALLOC(CalculationValueHandleMap);
    WTF_MAKE_NONCOPYABLE(CalculationValueHandleMap);
public:
    CalculationValueHandleMap()
        : m_index(1)
    {
    }

    int insert(PassRefPtr<CalculationValue> calcValue)
    {
        ASSERT(m_index);
        // FIXME calc(): https://bugs.webkit.org/show_bug.cgi?id=80489
        // This monotonically increasing handle generation scheme is potentially wasteful
        // of the handle space. Consider reusing empty handles.
        while (m_map.contains(m_index))
            m_index++;

        m_map.set(m_index, calcValue);

        return m_index;
    }

    void remove(int index)
    {
        ASSERT(m_map.contains(index));
        m_map.remove(index);
    }

    CalculationValue& get(int index)
    {
        ASSERT(m_map.contains(index));
        return *m_map.get(index);
    }

    void decrementRef(int index)
    {
        ASSERT(m_map.contains(index));
        CalculationValue* value = m_map.get(index);
        if (value->hasOneRef()) {
            // Force the CalculationValue destructor early to avoid a potential recursive call inside HashMap remove().
            m_map.set(index, nullptr);
            m_map.remove(index);
        } else {
            value->deref();
        }
    }

private:
    int m_index;
    HashMap<int, RefPtr<CalculationValue>> m_map;
};

static CalculationValueHandleMap& calcHandles()
{
    DEFINE_STATIC_LOCAL(CalculationValueHandleMap, handleMap, ());
    return handleMap;
}

Length::Length(PassRefPtr<CalculationValue> calc)
    : m_quirk(false)
    , m_type(Calculated)
    , m_isFloat(false)
{
    m_intValue = calcHandles().insert(calc);
}

Length Length::blendMixedTypes(const Length& from, double progress, ValueRange range) const
{
    ASSERT(from.isSpecified());
    ASSERT(isSpecified());
    PixelsAndPercent fromPixelsAndPercent = from.getPixelsAndPercent();
    PixelsAndPercent toPixelsAndPercent = getPixelsAndPercent();
    const float pixels = blink::blend(fromPixelsAndPercent.pixels, toPixelsAndPercent.pixels, progress);
    const float percent = blink::blend(fromPixelsAndPercent.percent, toPixelsAndPercent.percent, progress);
    return Length(CalculationValue::create(PixelsAndPercent(pixels, percent), range));
}

PixelsAndPercent Length::getPixelsAndPercent() const
{
    switch (type()) {
    case Fixed:
        return PixelsAndPercent(value(), 0);
    case Percent:
        return PixelsAndPercent(0, value());
    case Calculated:
        return getCalculationValue().getPixelsAndPercent();
    default:
        ASSERT_NOT_REACHED();
        return PixelsAndPercent(0, 0);
    }
}

Length Length::subtractFromOneHundredPercent() const
{
    PixelsAndPercent result = getPixelsAndPercent();
    result.pixels = -result.pixels;
    result.percent = 100 - result.percent;
    if (result.pixels && result.percent)
        return Length(CalculationValue::create(result, ValueRangeAll));
    if (result.percent)
        return Length(result.percent, Percent);
    return Length(result.pixels, Fixed);
}

Length Length::zoom(double factor) const
{
    switch (type()) {
    case Fixed:
        return Length(getFloatValue() * factor, Fixed);
    case Calculated: {
        PixelsAndPercent result = getPixelsAndPercent();
        result.pixels *= factor;
        return Length(CalculationValue::create(result, getCalculationValue().getValueRange()));
    }
    default:
        return *this;
    }
}

CalculationValue& Length::getCalculationValue() const
{
    ASSERT(isCalculated());
    return calcHandles().get(calculationHandle());
}

void Length::incrementCalculatedRef() const
{
    ASSERT(isCalculated());
    getCalculationValue().ref();
}

void Length::decrementCalculatedRef() const
{
    ASSERT(isCalculated());
    calcHandles().decrementRef(calculationHandle());
}

float Length::nonNanCalculatedValue(LayoutUnit maxValue) const
{
    ASSERT(isCalculated());
    float result = getCalculationValue().evaluate(maxValue.toFloat());
    if (std::isnan(result))
        return 0;
    return result;
}

bool Length::isCalculatedEqual(const Length& o) const
{
    return isCalculated() && (&getCalculationValue() == &o.getCalculationValue() || getCalculationValue() == o.getCalculationValue());
}

struct SameSizeAsLength {
    int32_t value;
    int32_t metaData;
};
static_assert(sizeof(Length) == sizeof(SameSizeAsLength), "length should stay small");

} // namespace blink
