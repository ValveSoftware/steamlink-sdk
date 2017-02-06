// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UnderlyingValueOwner_h
#define UnderlyingValueOwner_h

#include "core/animation/TypedInterpolationValue.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include <memory>

namespace blink {

// Handles memory management of underlying InterpolationValues in applyStack()
// Ensures we perform copy on write if we are not the owner of an underlying InterpolationValue.
// This functions similar to a DataRef except on std::unique_ptr'd objects.
class UnderlyingValueOwner {
    WTF_MAKE_NONCOPYABLE(UnderlyingValueOwner);
    STACK_ALLOCATED();

public:
    UnderlyingValueOwner()
        : m_type(nullptr)
        , m_valueOwner(nullptr)
        , m_value(nullptr)
    { }

    operator bool() const
    {
        ASSERT(static_cast<bool>(m_type) == static_cast<bool>(m_value));
        return m_type;
    }

    const InterpolationType& type() const
    {
        ASSERT(m_type);
        return *m_type;
    }

    const InterpolationValue& value() const;

    void set(std::nullptr_t);
    void set(const InterpolationType&, const InterpolationValue&);
    void set(const InterpolationType&, InterpolationValue&&);
    void set(std::unique_ptr<TypedInterpolationValue>);
    void set(const TypedInterpolationValue*);

    InterpolationValue& mutableValue();

private:
    const InterpolationType* m_type;
    InterpolationValue m_valueOwner;
    const InterpolationValue* m_value;
};

} // namespace blink

#endif // UnderlyingValueOwner_h
