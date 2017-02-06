// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteFrameClient_h
#define RemoteFrameClient_h

#include "core/frame/FrameClient.h"
#include "core/frame/FrameTypes.h"
#include "core/loader/FrameLoaderTypes.h"
#include "public/platform/WebFocusType.h"

namespace blink {

class Event;
class IntRect;
class ResourceRequest;

class RemoteFrameClient : public FrameClient {
public:
    ~RemoteFrameClient() override { }

    virtual void navigate(const ResourceRequest&, bool shouldReplaceCurrentEntry) = 0;
    virtual void reload(FrameLoadType, ClientRedirectPolicy) = 0;
    virtual unsigned backForwardLength() = 0;

    // Forwards a postMessage for a remote frame.
    virtual void forwardPostMessage(MessageEvent*, PassRefPtr<SecurityOrigin> target, LocalFrame* sourceFrame) const = 0;

    // FIXME: Remove this method once we have input routing in the browser
    // process. See http://crbug.com/339659.
    virtual void forwardInputEvent(Event*) = 0;

    virtual void frameRectsChanged(const IntRect& frameRect) = 0;

    virtual void advanceFocus(WebFocusType, LocalFrame* source) = 0;

    virtual void visibilityChanged(bool visible) = 0;
};

} // namespace blink

#endif // RemoteFrameClient_h
