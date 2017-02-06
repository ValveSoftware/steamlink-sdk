// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMetaElement_h
#define WebMetaElement_h

#include "WebElement.h"

namespace blink {

class HTMLMetaElement;

class WebMetaElement final : public WebElement {
public:
    WebMetaElement() : WebElement() { }
    WebMetaElement(const WebMetaElement& element) : WebElement(element) { }

    WebMetaElement& operator=(const WebMetaElement& element)
    {
        WebElement::assign(element);
        return *this;
    }
    void assign(const WebMetaElement& element) { WebElement::assign(element); }

    BLINK_EXPORT WebString computeEncoding() const;

#if BLINK_IMPLEMENTATION
    WebMetaElement(HTMLMetaElement*);
    WebMetaElement& operator=(HTMLMetaElement*);
    operator HTMLMetaElement*() const;
#endif
};

DECLARE_WEB_NODE_TYPE_CASTS(WebMetaElement);

} // namespace blink

#endif
