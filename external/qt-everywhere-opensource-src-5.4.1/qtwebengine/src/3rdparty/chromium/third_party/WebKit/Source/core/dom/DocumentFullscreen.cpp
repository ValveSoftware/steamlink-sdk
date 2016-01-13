/*
 * Copyright (C) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "config.h"
#include "core/dom/DocumentFullscreen.h"

#include "core/dom/FullscreenElementStack.h"

namespace WebCore {

bool DocumentFullscreen::webkitIsFullScreen(Document& document)
{
    if (FullscreenElementStack* fullscreen = FullscreenElementStack::fromIfExists(document))
        return fullscreen->webkitIsFullScreen();
    return false;
}

bool DocumentFullscreen::webkitFullScreenKeyboardInputAllowed(Document& document)
{
    if (FullscreenElementStack* fullscreen = FullscreenElementStack::fromIfExists(document))
        return fullscreen->webkitFullScreenKeyboardInputAllowed();
    return false;
}

Element* DocumentFullscreen::webkitCurrentFullScreenElement(Document& document)
{
    if (FullscreenElementStack* fullscreen = FullscreenElementStack::fromIfExists(document))
        return fullscreen->webkitCurrentFullScreenElement();
    return 0;
}

void DocumentFullscreen::webkitCancelFullScreen(Document& document)
{
    FullscreenElementStack::from(document).webkitCancelFullScreen();
}

bool DocumentFullscreen::webkitFullscreenEnabled(Document& document)
{
    return FullscreenElementStack::webkitFullscreenEnabled(document);
}

Element* DocumentFullscreen::webkitFullscreenElement(Document& document)
{
    if (FullscreenElementStack* fullscreen = FullscreenElementStack::fromIfExists(document))
        return fullscreen->webkitFullscreenElement();
    return 0;
}

void DocumentFullscreen::webkitExitFullscreen(Document& document)
{
    FullscreenElementStack::from(document).webkitExitFullscreen();
}

} // namespace WebCore
