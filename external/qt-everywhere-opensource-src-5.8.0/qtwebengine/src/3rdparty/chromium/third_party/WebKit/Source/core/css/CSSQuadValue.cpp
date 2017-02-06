// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSQuadValue.h"

#include "wtf/text/StringBuilder.h"

namespace blink {

String CSSQuadValue::customCSSText() const
{
    String top = m_top->cssText();
    String right = m_right->cssText();
    String bottom = m_bottom->cssText();
    String left = m_left->cssText();

    if (m_serializationType == TypeForSerialization::SerializeAsRect)
        return "rect(" + top + ' ' + right + ' ' + bottom + ' ' + left + ')';

    StringBuilder result;
    // reserve space for the four strings, plus three space separator characters.
    result.reserveCapacity(top.length() + right.length() + bottom.length() + left.length() + 3);
    result.append(top);
    if (right != top || bottom != top || left != top) {
        result.append(' ');
        result.append(right);
        if (bottom != top || right != left) {
            result.append(' ');
            result.append(bottom);
            if (left != right) {
                result.append(' ');
                result.append(left);
            }
        }
    }
    return result.toString();
}

DEFINE_TRACE_AFTER_DISPATCH(CSSQuadValue)
{
    visitor->trace(m_top);
    visitor->trace(m_right);
    visitor->trace(m_bottom);
    visitor->trace(m_left);
    CSSValue::traceAfterDispatch(visitor);
}

} // namespace blink
