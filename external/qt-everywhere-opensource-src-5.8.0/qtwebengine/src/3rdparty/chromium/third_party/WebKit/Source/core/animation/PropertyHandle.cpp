// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/PropertyHandle.h"

namespace blink {

bool PropertyHandle::operator==(const PropertyHandle& other) const
{
    if (m_handleType != other.m_handleType)
        return false;

    switch (m_handleType) {
    case HandleCSSProperty:
    case HandlePresentationAttribute:
        return m_cssProperty == other.m_cssProperty;
    case HandleSVGAttribute:
        return m_svgAttribute == other.m_svgAttribute;
    default:
        return true;
    }
}

unsigned PropertyHandle::hash() const
{
    switch (m_handleType) {
    case HandleCSSProperty:
        return m_cssProperty;
    case HandlePresentationAttribute:
        return -m_cssProperty;
    case HandleSVGAttribute:
        return QualifiedNameHash::hash(*m_svgAttribute);
    default:
        NOTREACHED();
        return 0;
    }
}

} // namespace blink
