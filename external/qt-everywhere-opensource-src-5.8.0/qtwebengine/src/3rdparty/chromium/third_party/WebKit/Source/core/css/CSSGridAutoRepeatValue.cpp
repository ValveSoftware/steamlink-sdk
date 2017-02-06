// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSGridAutoRepeatValue.h"

#include "wtf/text/StringBuilder.h"

namespace blink {

String CSSGridAutoRepeatValue::customCSSText() const
{
    StringBuilder result;
    result.append("repeat(");
    result.append(getValueName(autoRepeatID()));
    result.append(", ");
    result.append(CSSValueList::customCSSText());
    result.append(")");
    return result.toString();
}

} // namespace blink
