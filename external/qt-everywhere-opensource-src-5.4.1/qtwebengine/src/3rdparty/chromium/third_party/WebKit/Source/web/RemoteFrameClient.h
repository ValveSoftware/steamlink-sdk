// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteFrameClient_h
#define RemoteFrameClient_h

#include "core/frame/FrameClient.h"

namespace blink {

class WebRemoteFrameImpl;

class RemoteFrameClient : public WebCore::FrameClient {
public:
    explicit RemoteFrameClient(WebRemoteFrameImpl*);

    // FrameClient overrides:
    virtual WebCore::Frame* opener() const OVERRIDE;
    virtual void setOpener(WebCore::Frame*) OVERRIDE;

    virtual WebCore::Frame* parent() const OVERRIDE;
    virtual WebCore::Frame* top() const OVERRIDE;
    virtual WebCore::Frame* previousSibling() const OVERRIDE;
    virtual WebCore::Frame* nextSibling() const OVERRIDE;
    virtual WebCore::Frame* firstChild() const OVERRIDE;
    virtual WebCore::Frame* lastChild() const OVERRIDE;

    WebRemoteFrameImpl* webFrame() const { return m_webFrame; }

private:
    WebRemoteFrameImpl* m_webFrame;
};

} // namespace blink

#endif // RemoteFrameClient_h
