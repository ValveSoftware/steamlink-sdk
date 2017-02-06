// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationEnvironment_h
#define InterpolationEnvironment_h

#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"

namespace blink {

class StyleResolverState;
class SVGPropertyBase;
class SVGElement;

class InterpolationEnvironment {
    STACK_ALLOCATED();
public:
    explicit InterpolationEnvironment(StyleResolverState& state)
        : m_state(&state)
        , m_svgElement(nullptr)
        , m_svgBaseValue(nullptr)
    { }

    explicit InterpolationEnvironment(SVGElement& svgElement, const SVGPropertyBase& svgBaseValue)
        : m_state(nullptr)
        , m_svgElement(&svgElement)
        , m_svgBaseValue(&svgBaseValue)
    { }

    StyleResolverState& state() { ASSERT(m_state); return *m_state; }
    const StyleResolverState& state() const { ASSERT(m_state); return *m_state; }

    SVGElement& svgElement() { ASSERT(m_svgElement); return *m_svgElement; }
    const SVGElement& svgElement() const { ASSERT(m_svgElement); return *m_svgElement; }

    const SVGPropertyBase& svgBaseValue() const { ASSERT(m_svgBaseValue); return *m_svgBaseValue; }

private:
    StyleResolverState* m_state;
    Member<SVGElement> m_svgElement;
    Member<const SVGPropertyBase> m_svgBaseValue;
};

} // namespace blink

#endif // InterpolationEnvironment_h
