// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLRubyElement_h
#define HTMLRubyElement_h

#include "core/html/HTMLElement.h"

namespace WebCore {

// <ruby> is an HTMLElement in script, but we use a separate interface here
// so HTMLElement's createRenderer doesn't need to know about it.
class HTMLRubyElement FINAL : public HTMLElement {
public:
    DECLARE_NODE_FACTORY(HTMLRubyElement);

private:
    explicit HTMLRubyElement(Document&);

    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;
};

} // namespace

#endif
