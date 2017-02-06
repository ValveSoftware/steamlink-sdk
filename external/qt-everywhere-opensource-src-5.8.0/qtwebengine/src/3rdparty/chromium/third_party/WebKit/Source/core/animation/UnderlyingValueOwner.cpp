// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/UnderlyingValueOwner.h"

#include <memory>

namespace blink {

struct NullValueWrapper {
    NullValueWrapper() : value(nullptr) {}
    const InterpolationValue value;
};

const InterpolationValue& UnderlyingValueOwner::value() const
{
    DEFINE_STATIC_LOCAL(NullValueWrapper, nullValueWrapper, ());
    return *this ? *m_value : nullValueWrapper.value;
}

void UnderlyingValueOwner::set(std::nullptr_t)
{
    m_type = nullptr;
    m_valueOwner.clear();
    m_value = nullptr;
}

void UnderlyingValueOwner::set(const InterpolationType& type, const InterpolationValue& value)
{
    ASSERT(value);
    m_type = &type;
    // By clearing m_valueOwner we will perform a copy before attempting to mutate m_value,
    // thus upholding the const contract for this instance of interpolationValue.
    m_valueOwner.clear();
    m_value = &value;
}

void UnderlyingValueOwner::set(const InterpolationType& type, InterpolationValue&& value)
{
    ASSERT(value);
    m_type = &type;
    m_valueOwner = std::move(value);
    m_value = &m_valueOwner;
}

void UnderlyingValueOwner::set(std::unique_ptr<TypedInterpolationValue> value)
{
    if (value)
        set(value->type(), std::move(value->mutableValue()));
    else
        set(nullptr);
}

void UnderlyingValueOwner::set(const TypedInterpolationValue* value)
{
    if (value)
        set(value->type(), value->value());
    else
        set(nullptr);
}

InterpolationValue& UnderlyingValueOwner::mutableValue()
{
    ASSERT(m_type && m_value);
    if (!m_valueOwner) {
        m_valueOwner = m_value->clone();
        m_value = &m_valueOwner;
    }
    return m_valueOwner;
}

} // namespace blink
