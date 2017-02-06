// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLUnknownElement.h"

#include "core/frame/UseCounter.h"

namespace blink {

HTMLUnknownElement::HTMLUnknownElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    if (tagName.localName() == "data")
        UseCounter::count(document, UseCounter::DataElement);
    else if (tagName.localName() == "time")
        UseCounter::count(document, UseCounter::TimeElement);
    else if (tagName.localName() == "menuitem")
        UseCounter::count(document, UseCounter::MenuItemElement);
}

} // namespace blink
