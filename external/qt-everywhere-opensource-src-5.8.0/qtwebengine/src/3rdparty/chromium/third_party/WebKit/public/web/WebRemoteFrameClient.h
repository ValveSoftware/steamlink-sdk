// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRemoteFrameClient_h
#define WebRemoteFrameClient_h

#include "public/platform/WebFocusType.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/web/WebDOMMessageEvent.h"
#include "public/web/WebFrame.h"

namespace blink {
class WebInputEvent;
enum class WebClientRedirectPolicy;
enum class WebFrameLoadType;
struct WebRect;

class WebRemoteFrameClient {
public:
    // Specifies the reason for the detachment.
    enum class DetachType { Remove, Swap };

    // Notify the embedder that it should remove this frame from the frame tree
    // and release any resources associated with it.
    virtual void frameDetached(DetachType) { }

    // Notifies the embedder that a postMessage was issued to a remote frame.
    virtual void forwardPostMessage(
        WebLocalFrame* sourceFrame,
        WebRemoteFrame* targetFrame,
        WebSecurityOrigin targetOrigin,
        WebDOMMessageEvent) {}

    // Send initial drawing parameters to a child frame that is being rendered
    // out of process.
    virtual void initializeChildFrame(float deviceScaleFactor) { }

    // A remote frame was asked to start a navigation.
    virtual void navigate(const WebURLRequest& request, bool shouldReplaceCurrentEntry) { }
    virtual void reload(WebFrameLoadType, WebClientRedirectPolicy) {}

    // FIXME: Remove this method once we have input routing in the browser
    // process. See http://crbug.com/339659.
    virtual void forwardInputEvent(const WebInputEvent*) { }

    virtual void frameRectsChanged(const WebRect&) { }

    virtual void visibilityChanged(bool visible) {}

    // This frame updated its opener to another frame.
    virtual void didChangeOpener(WebFrame* opener) { }

    // Continue sequential focus navigation in this frame.  This is called when
    // the |source| frame is searching for the next focusable element (e.g., in
    // response to <tab>) and encounters a remote frame.
    virtual void advanceFocus(WebFocusType type, WebLocalFrame* source) { }

    // This frame was focused by another frame.
    virtual void frameFocused() { }
};

} // namespace blink

#endif // WebRemoteFrameClient_h
