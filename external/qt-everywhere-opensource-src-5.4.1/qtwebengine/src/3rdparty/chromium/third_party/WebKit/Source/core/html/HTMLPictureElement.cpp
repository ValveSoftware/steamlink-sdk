// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/html/HTMLPictureElement.h"

#include "core/HTMLNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/html/HTMLImageElement.h"

namespace WebCore {

using namespace HTMLNames;

inline HTMLPictureElement::HTMLPictureElement(Document& document)
    : HTMLElement(pictureTag, document)
{
    ScriptWrappable::init(this);
}

DEFINE_NODE_FACTORY(HTMLPictureElement)

void HTMLPictureElement::sourceOrMediaChanged()
{
    for (HTMLImageElement* imageElement = Traversal<HTMLImageElement>::firstChild(*this); imageElement; imageElement = Traversal<HTMLImageElement>::nextSibling(*imageElement)) {
        imageElement->selectSourceURL(HTMLImageElement::UpdateNormal);
    }
}

} // namespace
