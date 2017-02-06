// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ElementFullscreen.h"

#include "core/dom/Fullscreen.h"
#include "core/frame/UseCounter.h"

namespace blink {

void ElementFullscreen::requestFullscreen(Element& element)
{
    Fullscreen::from(element.document()).requestFullscreen(element, Fullscreen::UnprefixedRequest);
}

void ElementFullscreen::webkitRequestFullscreen(Element& element)
{
    if (element.isInShadowTree())
        UseCounter::count(element.document(), UseCounter::PrefixedElementRequestFullscreenInShadow);
    Fullscreen::from(element.document()).requestFullscreen(element, Fullscreen::PrefixedRequest);
}

} // namespace blink
