// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WindowGetComputedStyle_h
#define WindowGetComputedStyle_h

#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/cssom/ComputedStylePropertyMap.h"

namespace blink {

class WindowGetComputedStyle {
    STATIC_ONLY(WindowGetComputedStyle);
public:
    static StylePropertyMap* getComputedStyleMap(const DOMWindow&, Element* element, const String& pseudoElement)
    {
        DCHECK(element);
        CSSComputedStyleDeclaration* computedStyleDeclaration = CSSComputedStyleDeclaration::create(element, false, pseudoElement);
        return ComputedStylePropertyMap::create(computedStyleDeclaration);
    }
};

} // namespace blink

#endif // WindowGetComputedStyle_h
